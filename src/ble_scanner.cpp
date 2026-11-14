#include "ble_scanner.h"
#include "iphone_tracker.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <atomic>
#include <cstring>

namespace {
constexpr size_t kCapacity = 32;
constexpr uint32_t kScanSeconds = 5;
struct Observation {
    char address[18];
    uint8_t addressType;
    int rssi;
    char name[33];
};

// Only the BLE task writes during a scan. The loop reads after the completion
// flag's release/acquire handoff; a new scan starts only after output finishes.
Observation observations[kCapacity]{};
size_t used = 0;
uint32_t overflowReports = 0;
std::atomic<uint32_t> reportCount{0};
std::atomic<bool> finished{false};
BLEScan* scanner = nullptr;
uint32_t scans = 0;
uint8_t lastDevices = 0;
size_t printIndex = 0;
bool scanning = false;
bool printing = false;
bool failed = false;
bool stalled = false;
uint32_t startedMs = 0;
uint32_t retryMs = 0;

class Callbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) override {
        reportCount.fetch_add(1, std::memory_order_relaxed);
        auto nativeAddress = device.getAddress();
        iphone_tracker::observe(*nativeAddress.getNative(),
                                static_cast<uint8_t>(device.getAddressType()),
                                device.getRSSI());
        const auto address = device.getAddress().toString();
        const auto type = static_cast<uint8_t>(device.getAddressType());
        size_t index = 0;
        for (; index < used; ++index) {
            if (observations[index].addressType == type &&
                address == observations[index].address) {
                break;
            }
        }
        if (index == kCapacity) {
            ++overflowReports;
            return;
        }
        auto& observation = observations[index];
        if (index == used) {
            ++used;
            observation = {};
            memcpy(observation.address, address.c_str(), 17);
            observation.addressType = type;
        }
        observation.rssi = device.getRSSI();
        if (device.haveName()) {
            const auto name = device.getName();
            const size_t length = name.size() < 32 ? name.size() : 32;
            for (size_t i = 0; i < length; ++i) {
                const auto c = static_cast<unsigned char>(name[i]);
                // Prevent advertised control characters from altering a terminal.
                observation.name[i] = (c >= 32 && c <= 126 && c != '"') ? c : '?';
            }
            observation.name[length] = '\0';
        }
    }
} callbacks;

void complete(BLEScanResults) {
    finished.store(true, std::memory_order_release);
}

const char* addressTypeName(uint8_t type) {
    switch (type) {
        case 0: return "public";
        case 1: return "random";
        case 2: return "public-identity";
        case 3: return "random-identity";
        default: return "unknown";
    }
}
}  // namespace

namespace ble_scanner {
void begin() {
    BLEDevice::init("");
    scanner = BLEDevice::getScan();
    // Receiving duplicates prevents this pinned library from accumulating an
    // unbounded internal address map. We retain at most 32 entries ourselves.
    scanner->setAdvertisedDeviceCallbacks(&callbacks, true);
    scanner->setActiveScan(false);  // Listen only; no scan requests or connections.
    scanner->setInterval(100);
    scanner->setWindow(80);
    Serial.println("BLE passive scan: 5 s rounds, at most 32 address/type entries.");
}

void poll() {
    const uint32_t now = millis();
    if (scanning) {
        if (!finished.load(std::memory_order_acquire)) {
            // Do not risk a blocking restart if the stack never completes.
            if (!stalled && static_cast<uint32_t>(now - startedMs) > 10000) {
                stalled = true;
                Serial.println("BLE scan completion missing; reset if it persists.");
            }
            return;
        }
        scanning = false;
        stalled = false;
        printing = true;
        printIndex = 0;
        ++scans;
        lastDevices = static_cast<uint8_t>(used);
        Serial.printf("Scan %lu: %lu reports, %u retained address/type entries, "
                      "%lu reports outside capacity\n",
                      static_cast<unsigned long>(scans),
                      static_cast<unsigned long>(reportCount.load()),
                      static_cast<unsigned>(used),
                      static_cast<unsigned long>(overflowReports));
    }
    if (printing) {
        if (printIndex < used) {
            const auto& observation = observations[printIndex++];
            Serial.printf("  %s type=%s(%u) RSSI=%d dBm name=\"%s\"\n",
                          observation.address, addressTypeName(observation.addressType),
                          observation.addressType, observation.rssi,
                          observation.name[0] ? observation.name : "<not advertised>");
            return;  // One line per loop keeps the screen responsive.
        }
        printing = false;
    }
    if (failed && static_cast<uint32_t>(now - retryMs) < 2000) {
        return;
    }
    used = 0;
    overflowReports = 0;
    reportCount.store(0);
    finished.store(false);
    startedMs = now;
    scanning = scanner->start(kScanSeconds, complete, false);
    failed = !scanning;
    if (failed) {
        retryMs = now;
        Serial.println("BLE scan start failed; retrying in 2 s.");
    }
}

uint32_t completedScans() { return scans; }
uint32_t reports() { return reportCount.load(std::memory_order_relaxed); }
uint8_t lastDeviceCount() { return lastDevices; }
bool healthy() { return !failed && !stalled; }
const char* status() {
    if (stalled) return "stalled";
    if (failed) return "retrying";
    return scanning ? "listening" : "reporting";
}
}  // namespace ble_scanner

#include "ble_scanner.h"
#include "iphone_tracker.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <esp_timer.h>
#include <atomic>
#include <cstring>
#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/ble_hs_adv.h>
#endif

namespace {
constexpr size_t kCapacity = ble_scanner::kObservationCapacity;
constexpr uint32_t kScanSeconds = 5;

// Only the BLE task writes during a scan. The loop reads after the completion
// flag's release/acquire handoff; a new scan starts only after output finishes.
ble_scanner::Observation observations[kCapacity]{};
ble_scanner::Snapshot completedSnapshot;
size_t used = 0;
uint32_t overflowReports = 0;
std::atomic<uint32_t> reportCount{0};
std::atomic<bool> finished{false};
#if defined(CONFIG_BLUEDROID_ENABLED)
BLEScan* scanner = nullptr;
#endif
uint32_t scans = 0;
uint8_t lastDevices = 0;
size_t printIndex = 0;
bool scanning = false;
bool printing = false;
bool failed = false;
bool stalled = false;
uint32_t startedMs = 0;
uint32_t retryMs = 0;

void record(const uint8_t canonical[6], uint8_t type, int rssi,
            const uint8_t* name, size_t nameLength) {
    reportCount.fetch_add(1, std::memory_order_relaxed);
    iphone_tracker::observe(canonical, type, rssi);
    char address[18];
    snprintf(address, sizeof(address), "%02x:%02x:%02x:%02x:%02x:%02x",
             canonical[0], canonical[1], canonical[2], canonical[3], canonical[4], canonical[5]);
    size_t index = 0;
    for (; index < used; ++index) {
        if (observations[index].addressType == type &&
            std::strcmp(address, observations[index].address) == 0) break;
    }
    if (index == kCapacity) { ++overflowReports; return; }
    auto& observation = observations[index];
    if (index == used) {
        ++used;
        observation = {};
        std::memcpy(observation.address, address, sizeof(address));
        observation.addressType = type;
    }
    observation.rssi = rssi;
    observation.lastSeenUptimeMs = static_cast<uint64_t>(esp_timer_get_time()) / 1000;
    if (name && nameLength) {
        const size_t length = nameLength < 32 ? nameLength : 32;
        for (size_t i = 0; i < length; ++i) {
            const auto c = name[i];
            observation.name[i] = (c >= 32 && c <= 126 && c != '"') ? c : '?';
        }
        observation.name[length] = '\0';
    }
}

#if defined(CONFIG_NIMBLE_ENABLED)
// The bundled C++ NimBLE scanner retains an internal address map without a
// public capacity setter in core 3.3.12. Use the bundled GAP API directly so
// every advertisement reaches IRK matching without accumulating that map.
int scanEvent(ble_gap_event* event, void*) {
    if (event->type == BLE_GAP_EVENT_DISC) {
        const auto& report = event->disc;
        uint8_t canonical[6];
        iphone_presence::canonicalAddress(report.addr.val, true, canonical);
        ble_hs_adv_fields fields{};
        const bool parsed = ble_hs_adv_parse_fields(&fields, report.data, report.length_data) == 0;
        record(canonical, report.addr.type, report.rssi,
               parsed ? fields.name : nullptr, parsed ? fields.name_len : 0);
    } else if (event->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        finished.store(true, std::memory_order_release);
    }
    return 0;
}

bool startScan() {
    uint8_t ownAddressType;
    if (ble_hs_id_infer_auto(0, &ownAddressType) != 0) return false;
    ble_gap_disc_params params{};
    params.itvl = 160;  // 100 ms in 0.625-ms units.
    params.window = 128; // 80 ms.
    params.passive = 1;
    params.filter_duplicates = 0;
    return ble_gap_disc(ownAddressType, kScanSeconds * 1000, &params, scanEvent, nullptr) == 0;
}
#else
class Callbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) override {
        auto address = device.getAddress();
        const auto name = device.getName();
        record(address.getNative(), static_cast<uint8_t>(device.getAddressType()), device.getRSSI(),
               reinterpret_cast<const uint8_t*>(name.c_str()), name.length());
    }
} callbacks;
void complete(BLEScanResults) { finished.store(true, std::memory_order_release); }
bool startScan() { return scanner->start(kScanSeconds, complete, false); }
#endif

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
#if defined(CONFIG_BLUEDROID_ENABLED)
    scanner = BLEDevice::getScan();
    scanner->setAdvertisedDeviceCallbacks(&callbacks, true);
    scanner->setActiveScan(false);
    scanner->setInterval(100);
    scanner->setWindow(80);
#endif
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
        completedSnapshot.count = used;
        std::memcpy(completedSnapshot.entries, observations, used * sizeof(observations[0]));
        completedSnapshot.completedUptimeMs = static_cast<uint64_t>(esp_timer_get_time()) / 1000;
        completedSnapshot.reports = reportCount.load();
        completedSnapshot.outsideCapacity = overflowReports;
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
    scanning = startScan();
    failed = !scanning;
    if (failed) {
        retryMs = now;
        Serial.println("BLE scan start failed; retrying in 2 s.");
    }
}

uint32_t completedScans() { return scans; }
uint32_t reports() { return reportCount.load(std::memory_order_relaxed); }
uint8_t lastDeviceCount() { return lastDevices; }
const Snapshot& snapshot() { return completedSnapshot; }
bool healthy() { return !failed && !stalled; }
const char* status() {
    if (stalled) return "stalled";
    if (failed) return "retrying";
    return scanning ? "listening" : "reporting";
}
}  // namespace ble_scanner

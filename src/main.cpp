#include <Arduino.h>
#include "ble_scanner.h"
#include "iphone_tracker.h"
#include "network_dashboard.h"
#include "settings_store.h"
#include "oled_ui.h"

namespace {
constexpr uint32_t kUpdateMs = 1000;
uint32_t lastUpdateMs = 0;
uint32_t stageShownMs = 0;
enum class Startup { Title, Phone, Settings, Bluetooth, Network, Running };
Startup startup = Startup::Title;
bool bleStarted = false;
bool networkStarted = false;

void showStatus(uint32_t now) {
    Serial.printf("Heltec V3 | uptime %lu s | OLED %s | BLE %s | scans %lu | reports %lu\n",
                  static_cast<unsigned long>(now / 1000),
                  oled_ui::ready() ? "initialized" : "unavailable",
                  ble_scanner::status(),
                  static_cast<unsigned long>(ble_scanner::completedScans()),
                  static_cast<unsigned long>(ble_scanner::reports()));
    const auto& phone = iphone_tracker::presence();
    const auto& stats = iphone_tracker::stats();
    char mac[18] = "--";
    if (phone.seen()) {
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                 stats.address[0], stats.address[1], stats.address[2],
                 stats.address[3], stats.address[4], stats.address[5]);
    }
    Serial.printf("iPhone: %s", iphone_tracker::status());
    if (phone.seen()) {
        Serial.printf(" | last seen %llu s ago | last matching RSSI %d dBm",
                      static_cast<unsigned long long>(phone.ageMs() / 1000),
                      phone.rssi());
        Serial.printf(" | latest matching MAC (rotating) %s | last seen at uptime %llu s"
                      " | matching reports %llu | observed address changes %llu",
                      mac, static_cast<unsigned long long>(stats.lastSeenUptimeMs / 1000),
                      static_cast<unsigned long long>(stats.matches),
                      static_cast<unsigned long long>(stats.addressChanges));
    } else {
        Serial.print(" | latest matching MAC -- | matching reports 0 | observed address changes 0");
    }
    Serial.println();
    Serial.printf("Network: %s\n", network_dashboard::status());
}

void nextStage(Startup next, const char* label, unsigned completed) {
    startup = next;
    stageShownMs = millis();
    oled_ui::progress(label, completed);
}

void advanceStartup() {
    if (startup == Startup::Running) return;
    if (startup == Startup::Title) {
        if (oled_ui::titleComplete()) nextStage(Startup::Phone, "IRK tracker", 0);
        return;
    }
    // Let the user read each stage while the loop keeps servicing anything
    // already initialized. Never wait here for Wi-Fi association or HTTPS.
    if (static_cast<uint32_t>(millis() - stageShownMs) < 200) return;
    switch (startup) {
        case Startup::Phone:
            iphone_tracker::begin();
            nextStage(Startup::Settings, "Saved settings", 1);
            break;
        case Startup::Settings:
            settings_store::begin(iphone_tracker::presence().timeoutMs() / 1000);
            iphone_tracker::setTimeoutMs(settings_store::phoneTimeoutMs());
            nextStage(Startup::Bluetooth, "Bluetooth scanner", 2);
            break;
        case Startup::Bluetooth:
            ble_scanner::begin();
            bleStarted = true;
            nextStage(Startup::Network, "Wi-Fi / HTTPS", 3);
            break;
        case Startup::Network:
            network_dashboard::begin();
            networkStarted = true;
            startup = Startup::Running;
            oled_ui::initialized();
            break;
        default: break;
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    oled_ui::begin();
}

void loop() {
    advanceStartup();
    if (bleStarted) {
        ble_scanner::poll();
        iphone_tracker::setTimeoutMs(settings_store::phoneTimeoutMs());
        iphone_tracker::poll(ble_scanner::healthy());
    }
    if (networkStarted) network_dashboard::poll();
    oled_ui::poll();
    const uint32_t now = millis();
    if (startup == Startup::Running && static_cast<uint32_t>(now - lastUpdateMs) >= kUpdateMs) {
        lastUpdateMs = now;
        showStatus(now);
    }
    delay(10);
}

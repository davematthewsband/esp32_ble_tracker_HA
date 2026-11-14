#include "oled_ui.h"
#include "page_button.h"
#include "ble_scanner.h"
#include "iphone_tracker.h"
#include "network_dashboard.h"
#include "wifi_diagnostics.h"

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <esp_timer.h>

namespace {
// Official Heltec V3: active-low PRG/USER_SW is GPIO0. It remains a boot strap
// at reset; firmware only reads it and never drives it or adds a long-press action.
constexpr uint8_t kPrg = 0;
constexpr uint8_t kSda = 17, kScl = 18, kReset = 21, kVext = 36;
Adafruit_SSD1306 display(128, 64, &Wire, kReset);
PageButton button;
bool available = false;
bool done = false;
bool runningPages = false;
uint8_t page = 0;
uint32_t startedMs = 0, completedMs = 0, refreshedMs = 0;

void clear() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
}

void drawPage() {
    if (!available) return;
    clear();
    if (page == 0) {
        const auto& phone = iphone_tracker::presence();
        const auto& stats = iphone_tracker::stats();
        display.println("PHONE 1/3  PRG >");
        display.print("State: ");
        display.println(phone.state() == iphone_presence::State::NotSeenRecently ? "not seen" : iphone_tracker::status());
        display.println("Last MAC (rotating):");
        if (phone.seen()) {
            char mac[18];
            snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", stats.address[0],
                     stats.address[1], stats.address[2], stats.address[3], stats.address[4], stats.address[5]);
            display.println(mac);
            display.printf("Seen: %llu s ago\n", static_cast<unsigned long long>(phone.ageMs() / 1000));
            display.printf("Last RSSI:%d dBm\n", phone.rssi());
        } else {
            display.println("--"); display.println("Seen: never"); display.println("Last RSSI: --");
        }
        display.print("BLE: "); display.println(ble_scanner::status());
        display.println("PRG: Wi-Fi / System");
    } else if (page == 1) {
        const auto wifi = network_dashboard::connectionInfo();
        display.println("WI-FI 2/3  PRG >");
        display.print("State: ");
        display.println(!wifi.enabled ? "disabled" : wifi.connected ? "connected" : "connecting");
        display.print("IP: "); display.println(wifi.connected ? wifi.ip : "--");
        if (wifi.connected) display.printf("WiFi RSSI: %d dBm\n", wifi.rssi);
        else display.println("WiFi RSSI: --");
        display.print("HTTPS: ");
        display.println(!wifi.enabled ? "disabled" : wifi.httpsReady ? "ready" : "waiting");
        display.printf("Last reason: %u\n", wifi.lastDisconnectReason);
        display.println(wifi_diagnostics::message(wifi.lastDisconnectReason));
        display.println("PRG: System / Phone");
    } else {
        display.println("SYSTEM 3/3  PRG >");
        display.printf("Uptime: %llu s\n", static_cast<unsigned long long>(esp_timer_get_time() / 1000000));
        display.printf("Scans: %lu\n", static_cast<unsigned long>(ble_scanner::completedScans()));
        display.printf("Reports: %lu\n", static_cast<unsigned long>(ble_scanner::reports()));
        display.printf("Free heap: %luK\n", static_cast<unsigned long>(ESP.getFreeHeap() / 1024));
        display.printf("Min heap: %luK\n", static_cast<unsigned long>(ESP.getMinFreeHeap() / 1024));
        display.print("BLE: "); display.println(ble_scanner::status());
        display.println("PRG: Phone / Wi-Fi");
    }
    display.display();
}
}  // namespace

namespace oled_ui {
bool begin() {
    pinMode(kPrg, INPUT_PULLUP);
    button.update(digitalRead(kPrg) == LOW, millis());
    digitalWrite(kVext, LOW); pinMode(kVext, OUTPUT);
    delay(100);
    Wire.begin(kSda, kScl); Wire.setTimeOut(100);
    digitalWrite(kReset, HIGH); pinMode(kReset, OUTPUT); delay(1);
    digitalWrite(kReset, LOW); delay(10); digitalWrite(kReset, HIGH); delay(10);
    Wire.beginTransmission(0x3C);
    if (Wire.endTransmission() == 0)
        available = display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);
    startedMs = millis();
    if (available) {
        display.setTextColor(SSD1306_WHITE); display.setTextWrap(false);
        clear();
        display.println("Heltec WiFi LoRa V3");
        display.setTextSize(2); display.setCursor(0, 16); display.println("BLE");
        display.setCursor(0, 34); display.println("TRACKER");
        display.setTextSize(1); display.setCursor(0, 56); display.println("Starting...");
        display.display();
    }
    return available;
}
bool ready() { return available; }
bool titleComplete() { return static_cast<uint32_t>(millis() - startedMs) >= 1000; }
void progress(const char* stage, unsigned completed) {
    if (!available) return;
    clear();
    display.println("BLE TRACKER"); display.println("Initializing..."); display.println();
    display.println(stage);
    display.printf("%u/4 stages done\n", completed);
    if (completed == 4) {
        const auto wifi = network_dashboard::connectionInfo();
        display.print("Wi-Fi: ");
        display.println(!wifi.enabled ? "disabled" : wifi.connected ? "connected" : "connecting");
    } else display.println("Starting services...");
    display.display();
}
void initialized() {
    done = true;
    completedMs = millis();
    progress("Services started", 4);
}
void poll() {
    const uint32_t now = millis();
    const bool pressed = button.update(digitalRead(kPrg) == LOW, now);
    if (!done) return;
    if (!runningPages) {
        if (static_cast<uint32_t>(now - completedMs) < 750) return;
        runningPages = true;
        page = 0;
        refreshedMs = now;
        drawPage();
    } else if (pressed) {
        page = (page + 1) % 3;
        refreshedMs = now;
        drawPage();
    } else if (static_cast<uint32_t>(now - refreshedMs) >= 1000) {
        refreshedMs = now;
        drawPage();
    }
}
}  // namespace oled_ui

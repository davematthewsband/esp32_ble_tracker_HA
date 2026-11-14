#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

namespace {
// Heltec's V3 OLED uses different I2C pins from the generic SDA/SCL.
constexpr uint8_t kSda = 17;
constexpr uint8_t kScl = 18;
constexpr uint8_t kReset = 21;
constexpr uint8_t kVext = 36;  // LOW enables the display's power rail.
constexpr uint8_t kAddress = 0x3C;
constexpr uint32_t kUpdateMs = 1000;

Adafruit_SSD1306 display(128, 64, &Wire, kReset);
bool displayReady = false;
uint32_t lastUpdateMs = 0;

void showStatus(uint32_t now) {
    Serial.printf("Heltec V3 bring-up | uptime %lu s | OLED %s\n",
                  static_cast<unsigned long>(now / 1000),
                  displayReady ? "initialized" : "unavailable");
    if (!displayReady) {
        return;
    }
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Heltec WiFi LoRa V3");
    display.println("Serial / OLED test");
    display.println();
    display.print("Uptime: ");
    display.print(now / 1000);
    display.println(" s");
    display.display();
}
}  // namespace

void setup() {
    Serial.begin(115200);
    // Do not wait for a serial monitor: the screen must work standalone.
    digitalWrite(kVext, LOW);
    pinMode(kVext, OUTPUT);
    delay(100);
    Wire.begin(kSda, kScl);
    Wire.setTimeOut(100);

    // Reset before probing: an ACK alone does not prove visible OLED output.
    digitalWrite(kReset, HIGH);
    pinMode(kReset, OUTPUT);
    delay(1);
    digitalWrite(kReset, LOW);
    delay(10);
    digitalWrite(kReset, HIGH);
    delay(10);
    Wire.beginTransmission(kAddress);
    if (Wire.endTransmission() == 0) {
        // Pins and reset were set above; keep the library from restarting Wire.
        displayReady = display.begin(SSD1306_SWITCHCAPVCC, kAddress, false, false);
    }
    if (displayReady) {
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setTextWrap(false);
    }
    showStatus(millis());
}

void loop() {
    const uint32_t now = millis();
    // Unsigned subtraction keeps the interval correct across millis() rollover.
    if (static_cast<uint32_t>(now - lastUpdateMs) >= kUpdateMs) {
        lastUpdateMs = now;
        showStatus(now);
    }
    delay(10);
}

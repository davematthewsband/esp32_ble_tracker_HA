#include "iphone_tracker.h"

#include <Arduino.h>
#include <mbedtls/aes.h>
#include <esp_timer.h>
#include <atomic>

#if __has_include("config.local.h")
#include "config.local.h"
#endif
#ifndef IPHONE_IRK_BASE64
#define IPHONE_IRK_BASE64 ""
#endif
#ifndef IPHONE_TIMEOUT_MS
#define IPHONE_TIMEOUT_MS 120000UL
#endif
static_assert(IPHONE_TIMEOUT_MS > 0 && IPHONE_TIMEOUT_MS < 0x80000000UL,
              "Presence timeout must be between 1 and 2147483647 ms");

namespace {
uint8_t irk[16]{};
bool ready = false;  // Set before the scanner starts, then immutable.
std::atomic<bool> cryptoFailed{false};
portMUX_TYPE matchMux = portMUX_INITIALIZER_UNLOCKED;
bool pending = false;
iphone_presence::MatchStats callbackStats;
iphone_presence::MatchStats latestStats;
iphone_presence::Presence tracked(IPHONE_TIMEOUT_MS);

bool encrypt(const uint8_t key[16], const uint8_t input[16], uint8_t output[16]) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    const bool ok = mbedtls_aes_setkey_enc(&aes, key, 128) == 0 &&
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input, output) == 0;
    mbedtls_aes_free(&aes);
    if (!ok) cryptoFailed.store(true);
    return ok;
}
}  // namespace

namespace iphone_tracker {
void begin() {
    if (!iphone_presence::selfTest(encrypt)) {
        Serial.println("iPhone tracker disabled: AES/address self-test failed.");
        return;
    }
    if (!iphone_presence::decodeMacIrk(IPHONE_IRK_BASE64, irk)) {
        Serial.println("iPhone tracker disabled: missing/invalid local Remote IRK.");
        return;
    }
    ready = true;
    Serial.printf("iPhone tracker configured; timeout %lu ms; waiting for a match.\n",
                  static_cast<unsigned long>(IPHONE_TIMEOUT_MS));
}

void observe(const uint8_t address[6], uint8_t type, int rssi) {
    if (!ready || cryptoFailed.load() ||
        !iphone_presence::resolves(irk, address, type, encrypt)) return;
    const uint64_t uptimeMs = static_cast<uint64_t>(esp_timer_get_time()) / 1000;
    portENTER_CRITICAL(&matchMux);
    callbackStats.observe(address, rssi, uptimeMs);
    pending = true;
    portEXIT_CRITICAL(&matchMux);
}

void poll(bool scannerHealthy) {
    bool gotMatch;
    portENTER_CRITICAL(&matchMux);
    gotMatch = pending;
    if (gotMatch) latestStats = callbackStats;
    pending = false;
    portEXIT_CRITICAL(&matchMux);
    if (gotMatch) {
        tracked.observe(static_cast<uint32_t>(latestStats.lastSeenUptimeMs),
                        latestStats.rssi);
    }
    // Sample time after the mailbox snapshot so a newly received timestamp
    // cannot appear to be in the future and look like a millis() rollover.
    const uint32_t now = millis();
    tracked.tick(now, scannerHealthy && ready && !cryptoFailed.load());
}

const iphone_presence::Presence& presence() { return tracked; }
void setTimeoutMs(uint32_t timeoutMs) { tracked.setTimeoutMs(timeoutMs); }
const iphone_presence::MatchStats& stats() { return latestStats; }
const char* status() {
    switch (tracked.state()) {
        case iphone_presence::State::Present: return "present";
        case iphone_presence::State::NotSeenRecently: return "not seen recently";
        default: return "unknown";
    }
}
}  // namespace iphone_tracker

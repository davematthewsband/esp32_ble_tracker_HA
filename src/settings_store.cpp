#include "settings_store.h"
#include <Arduino.h>
#include <Preferences.h>
#include <freertos/semphr.h>
#include <atomic>

namespace {
SemaphoreHandle_t mutex = nullptr;
device_settings::Config current;
std::atomic<uint32_t> timeoutMs{120000};
}

namespace settings_store {
void begin(uint32_t defaultTimeoutSeconds) {
    if (defaultTimeoutSeconds >= 5 && defaultTimeoutSeconds <= 3600)
        current.phoneTimeoutSeconds = defaultTimeoutSeconds;
    timeoutMs.store(current.phoneTimeoutSeconds * 1000);
    mutex = xSemaphoreCreateMutex();
    if (!mutex) { Serial.println("Settings storage unavailable: no mutex memory."); return; }
    Preferences nvs;
    if (!nvs.begin("ble-tracker", true)) return; // No saved settings is normal.
    const size_t length = nvs.getBytesLength("settings");
    if (length && length <= device_settings::kMaxJson) {
        auto* json = static_cast<char*>(malloc(length + 1));
        if (json) {
            if (nvs.getBytes("settings", json, length) == length) {
                json[length] = '\0';
                if (!device_settings::parse(json, length, current))
                    Serial.println("Saved settings invalid; using defaults without overwriting storage.");
            }
            free(json);
        }
    }
    nvs.end();
    timeoutMs.store(current.phoneTimeoutSeconds * 1000);
}

bool copy(device_settings::Config& output) {
    if (!mutex || xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE) return false;
    output = current;
    xSemaphoreGive(mutex);
    return true;
}

bool replace(const device_settings::Config& config) {
    if (!mutex) return false;
    auto* json = static_cast<char*>(malloc(device_settings::kMaxJson + 1));
    if (!json) return false;
    size_t length;
    if (!device_settings::serialize(config, json, device_settings::kMaxJson + 1, length)) {
        free(json); return false;
    }
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) != pdTRUE) { free(json); return false; }
    Preferences nvs;
    const bool opened = nvs.begin("ble-tracker", false);
    const bool saved = opened && nvs.putBytes("settings", json, length) == length;
    if (opened) nvs.end();
    if (saved) {
        current = config;
        timeoutMs.store(current.phoneTimeoutSeconds * 1000);
    }
    xSemaphoreGive(mutex);
    free(json);
    return saved;
}
uint32_t phoneTimeoutMs() { return timeoutMs.load(); }
}  // namespace settings_store

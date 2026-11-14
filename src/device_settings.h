#pragma once

#include "dashboard_protocol.h"
#include <stddef.h>
#include <stdint.h>

namespace device_settings {
constexpr size_t kMaxLabels = 16;
constexpr size_t kMaxJson = 8192;
struct Label {
    char identity[24]{}; // "iphone" or "mac:<type>:AA:BB:CC:DD:EE:FF"
    char name[33]{};
    char room[33]{};
};
struct Config {
    char scannerName[33] = "BLE tracker";
    char scannerRoom[33]{};
    uint32_t phoneTimeoutSeconds = 120;
    size_t labelCount = 0;
    Label labels[kMaxLabels]{};
};
// Strict versioned schema; rejected input never changes output.
bool parse(const char* json, size_t length, Config& output);
bool serialize(const Config& config, char* json, size_t capacity, size_t& length);
}  // namespace device_settings

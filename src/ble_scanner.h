#pragma once

#include <stdint.h>

namespace ble_scanner {
void begin();
void poll();
uint32_t completedScans();
uint32_t reports();
uint8_t lastDeviceCount();
const char* status();
bool healthy();
}  // namespace ble_scanner

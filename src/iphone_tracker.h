#pragma once

#include "iphone_presence.h"

namespace iphone_tracker {
void begin();
// BLE task only. Called before any diagnostic-table capacity checks.
void observe(const uint8_t address[6], uint8_t type, int rssi);
// Main loop only; no serial or display I/O in the BLE callback.
void poll(bool scannerHealthy);
void setTimeoutMs(uint32_t timeoutMs);
const iphone_presence::Presence& presence();
const iphone_presence::MatchStats& stats();
const char* status();
}  // namespace iphone_tracker

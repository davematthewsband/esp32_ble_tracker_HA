#pragma once

#include <stdint.h>
#include <stddef.h>

namespace ble_scanner {
constexpr size_t kObservationCapacity = 32;
struct Observation {
    char address[18]{};
    uint8_t addressType = 0;
    int rssi = 0;
    char name[33]{};
    uint64_t lastSeenUptimeMs = 0;
};
struct Snapshot {
    Observation entries[kObservationCapacity]{};
    size_t count = 0;
    uint64_t completedUptimeMs = 0;
    uint32_t reports = 0;
    uint32_t outsideCapacity = 0;
};
// Main-loop access only. Updated after completion; never read the live BLE table.
const Snapshot& snapshot();
void begin();
void poll();
uint32_t completedScans();
uint32_t reports();
uint8_t lastDeviceCount();
const char* status();
bool healthy();
}  // namespace ble_scanner

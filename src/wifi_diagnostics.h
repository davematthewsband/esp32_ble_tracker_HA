#pragma once
#include <stdint.h>

namespace wifi_diagnostics {
// ESP-IDF wifi_err_reason_t numeric values. Keep messages within an OLED row;
// the raw code is displayed separately so unrecognized codes remain useful.
inline const char* message(uint16_t reason) {
    switch (reason) {
        case 0: return "No disconnect yet";
        case 2: return "Auth expired";
        case 3: return "Auth leave";
        case 4: return "AP inactivity";
        case 5: return "AP full";
        case 8: return "Station left";
        case 15: case 204: return "Handshake timeout";
        case 16: return "Key update timeout";
        case 18: case 19: case 20: case 21: case 22: case 24: return "Security rejected";
        case 23: return "802.1X auth failed";
        case 200: return "Beacon timeout";
        case 201: return "Network not found";
        case 202: return "Auth failed";
        case 203: return "Association failed";
        case 205: return "Connection failed";
        case 207: return "Roaming";
        case 208: return "AP comeback too long";
        case 209: return "SA query timeout";
        case 210: return "Security mismatch";
        case 211: return "Security threshold";
        case 212: return "Signal below limit";
        default: return "Unknown reason";
    }
}

inline bool retryLeave(uint16_t reason, uint32_t now, uint32_t retryAt, bool requested) {
    // A deliberate disconnect during retry must not replace the AP failure
    // being investigated. Ignore only the local-leave code in a short window.
    return requested && reason == 8 && static_cast<uint32_t>(now - retryAt) < 1000;
}
}  // namespace wifi_diagnostics

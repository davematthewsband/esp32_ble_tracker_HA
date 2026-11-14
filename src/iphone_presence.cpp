#include "iphone_presence.h"

#include <cstring>

namespace iphone_presence {
namespace {
int digit(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
}  // namespace

bool decodeMacIrk(const char* base64, uint8_t key[16]) {
    // A macOS plist Remote IRK is exactly 16 bytes, base64-encoded in LE order.
    // Reject noncanonical padding, wrong lengths and embedded whitespace.
    if (!base64 || std::strlen(base64) != 24 ||
        base64[22] != '=' || base64[23] != '=') return false;
    uint8_t decoded[16]{};
    uint32_t accumulator = 0;
    unsigned bits = 0;
    unsigned count = 0;
    for (unsigned i = 0; i < 22; ++i) {
        const int value = digit(base64[i]);
        if (value < 0) return false;
        accumulator = (accumulator << 6) | static_cast<unsigned>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            decoded[count++] = static_cast<uint8_t>(accumulator >> bits);
        }
    }
    if ((accumulator & 0x0F) != 0 || count != 16) return false;
    for (unsigned i = 0; i < 16; ++i) key[i] = decoded[15 - i];
    return true;
}

bool resolves(const uint8_t key[16], const uint8_t address[6],
              uint8_t addressType, EncryptBlock encrypt) {
    // Native Bluedroid bytes are in printed MAC order. Only unresolved random
    // addresses (type 1) with prand's high bits 01 can be RPAs.
    if (addressType != 1 || (address[0] & 0xC0) != 0x40) return false;
    const uint32_t random = ((address[0] & 0x3F) << 16) |
                            (address[1] << 8) | address[2];
    if (random == 0 || random == 0x3FFFFF) return false;
    uint8_t input[16]{};
    uint8_t output[16]{};
    std::memcpy(input + 13, address, 3);
    return encrypt(key, input, output) &&
           std::memcmp(output + 13, address + 3, 3) == 0;
}

bool selfTest(EncryptBlock encrypt) {
    // Bluetooth Core v5.4 Vol 3, Part H, Appendix D.7 (public test key).
    const uint8_t key[16] = {0xec,0x02,0x34,0xa3,0x57,0xc8,0xad,0x05,
                             0x34,0x10,0x10,0xa6,0x0a,0x39,0x7d,0x9b};
    const uint8_t address[6] = {0x70,0x81,0x94,0x0d,0xfb,0xaa};
    return resolves(key, address, 1, encrypt);
}

void MatchStats::observe(const uint8_t matchedAddress[6], int matchedRssi,
                         uint64_t uptimeMs) {
    if (matches && std::memcmp(address, matchedAddress, sizeof(address)) != 0) {
        ++addressChanges;
    }
    std::memcpy(address, matchedAddress, sizeof(address));
    ++matches;
    lastSeenUptimeMs = uptimeMs;
    rssi = matchedRssi;
}

void Presence::observe(uint32_t now, int rssi) {
    seen_ = true;
    expired_ = false;
    lastTick_ = now;
    ageMs_ = 0;
    rssi_ = rssi;
}

void Presence::tick(uint32_t now, bool scannerHealthy) {
    healthy_ = scannerHealthy;
    if (!seen_) return;
    // Called from loop, at least once per millis() cycle. Accumulating into
    // 64 bits avoids a stale match becoming present again after 49.7 days.
    ageMs_ += static_cast<uint32_t>(now - lastTick_);
    lastTick_ = now;
    if (ageMs_ >= timeoutMs_) expired_ = true;
}

State Presence::state() const {
    if (!seen_ || !healthy_) return State::Unknown;
    return expired_ ? State::NotSeenRecently : State::Present;
}
}  // namespace iphone_presence

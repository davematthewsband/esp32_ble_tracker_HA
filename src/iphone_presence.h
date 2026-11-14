#pragma once

#include <stdint.h>

namespace iphone_presence {
// Pure protocol code: AES input/output use conventional most-significant-byte
// first order. The backend supplies encryption, not a home-grown AES algorithm.
using EncryptBlock = bool (*)(const uint8_t key[16], const uint8_t input[16],
                             uint8_t output[16]);
bool decodeMacIrk(const char* base64, uint8_t key[16]);
bool resolves(const uint8_t key[16], const uint8_t address[6],
              uint8_t addressType, EncryptBlock encrypt);
bool selfTest(EncryptBlock encrypt);

enum class State { Unknown, Present, NotSeenRecently };

// Fixed-size statistics for verified matches only. Record every callback before
// handing a snapshot to the loop, so coalescing updates cannot lose counts.
struct MatchStats {
    uint8_t address[6]{};
    uint64_t matches = 0;
    uint64_t addressChanges = 0;
    uint64_t lastSeenUptimeMs = 0;
    int rssi = 0;

    void observe(const uint8_t matchedAddress[6], int matchedRssi, uint64_t uptimeMs);
};

class Presence {
public:
    explicit Presence(uint32_t timeoutMs) : timeoutMs_(timeoutMs) {}
    void observe(uint32_t now, int rssi);
    void tick(uint32_t now, bool scannerHealthy);
    State state() const;
    bool seen() const { return seen_; }
    uint64_t ageMs() const { return ageMs_; }
    int rssi() const { return rssi_; }

private:
    uint32_t timeoutMs_;
    uint32_t lastTick_ = 0;
    uint64_t ageMs_ = 0;
    int rssi_ = 0;
    bool seen_ = false;
    bool healthy_ = true;
    bool expired_ = false;
};
}  // namespace iphone_presence

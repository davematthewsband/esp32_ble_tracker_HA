#include "iphone_presence.h"

#include <CommonCrypto/CommonCryptor.h>
#include <cassert>
#include <cstring>
#include <iostream>

using namespace iphone_presence;

bool encrypt(const uint8_t key[16], const uint8_t input[16], uint8_t output[16]) {
    size_t written = 0;
    return CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionECBMode,
                   key, 16, nullptr, input, 16, output, 16, &written) == kCCSuccess &&
           written == 16;
}

bool failingEncrypt(const uint8_t[16], const uint8_t[16], uint8_t[16]) {
    return false;
}

int main() {
    // Public Bluetooth Core v5.4 Vol 3 Part H D.7 example, not a personal key.
    const uint8_t key[16] = {0xec,0x02,0x34,0xa3,0x57,0xc8,0xad,0x05,
                             0x34,0x10,0x10,0xa6,0x0a,0x39,0x7d,0x9b};
    uint8_t address[6] = {0x70,0x81,0x94,0x0d,0xfb,0xaa};
    assert(selfTest(encrypt));
    assert(!selfTest(failingEncrypt));
    assert(resolves(key, address, 1, encrypt));
    const uint8_t nimbleAddress[6] = {0xaa,0xfb,0x0d,0x94,0x81,0x70};
    uint8_t canonical[6];
    canonicalAddress(nimbleAddress, true, canonical);
    assert(std::memcmp(canonical, address, 6) == 0);
    assert(resolves(key, canonical, 1, encrypt));
    canonicalAddress(address, false, canonical);
    assert(std::memcmp(canonical, address, 6) == 0);
    uint8_t wrongKey[16]{};
    assert(!resolves(wrongKey, address, 1, encrypt));
    uint8_t reversedKey[16];
    for (unsigned i = 0; i < 16; ++i) reversedKey[i] = key[15 - i];
    assert(!resolves(reversedKey, address, 1, encrypt));
    for (uint8_t type : {0, 2, 3}) assert(!resolves(key, address, type, encrypt));
    address[5] ^= 1;
    assert(!resolves(key, address, 1, encrypt));
    address[5] ^= 1;
    address[0] = 0x30;  // Nonresolvable random address.
    assert(!resolves(key, address, 1, encrypt));
    address[0] = 0xf0;  // Static random address.
    assert(!resolves(key, address, 1, encrypt));
    const uint8_t invalidRandom[6] = {0x40,0,0,0,0,0};
    assert(!resolves(key, invalidRandom, 1, encrypt));
    const uint8_t invalidOnes[6] = {0x7f,0xff,0xff,0,0,0};
    assert(!resolves(key, invalidOnes, 1, encrypt));

    // The public example key represented in macOS plist byte order.
    uint8_t decoded[16]{};
    assert(decodeMacIrk("m305CqYQEDQFrchXozQC7A==", decoded));
    assert(std::memcmp(decoded, key, 16) == 0);
    assert(!decodeMacIrk(nullptr, decoded));
    for (const char* bad : {"", "short", "m305CqYQEDQFrchXozQC7A=",
                            "m305CqYQEDQFrchXozQC7B==",  // Nonzero padding bits.
                            "m305CqYQEDQFrchXozQC7!==",
                            "m305CqYQEDQFrchXozQC7A==\n"}) {
        assert(!decodeMacIrk(bad, decoded));
    }

    Presence phone(120000);
    phone.tick(130000, true);
    assert(phone.state() == State::Unknown && !phone.seen());
    phone.observe(130000, -65);
    phone.tick(249999, true);
    assert(phone.state() == State::Present && phone.ageMs() == 119999);
    phone.tick(250000, true);
    assert(phone.state() == State::NotSeenRecently);
    phone.observe(250001, -70);
    phone.tick(250002, true);
    assert(phone.state() == State::Present && phone.rssi() == -70);
    phone.tick(250003, false);
    assert(phone.state() == State::Unknown);
    phone.tick(370001, true);
    assert(phone.state() == State::NotSeenRecently);

    Presence rollover(100);
    rollover.observe(UINT32_MAX - 50, -80);
    rollover.tick(48, true);
    assert(rollover.ageMs() == 99 && rollover.state() == State::Present);
    rollover.tick(49, true);
    assert(rollover.state() == State::NotSeenRecently);
    rollover.tick(UINT32_MAX - 50, true);
    rollover.tick(48, true);  // Another full millis cycle cannot resurrect it.
    assert(rollover.ageMs() > UINT32_MAX);
    assert(rollover.state() == State::NotSeenRecently);
    rollover.observe(49, -60);
    rollover.tick(50, true);
    assert(rollover.state() == State::Present && rollover.ageMs() == 1);

    Presence atBoot(100);
    atBoot.observe(0, -50);  // Timestamp zero is a valid match, not a sentinel.
    atBoot.tick(0, true);
    assert(atBoot.state() == State::Present);
    atBoot.observe(99, -55);  // Repeated matching address extends presence.
    atBoot.tick(100, true);
    assert(atBoot.state() == State::Present && atBoot.ageMs() == 1);
    atBoot.tick(101, false);
    assert(atBoot.state() == State::Unknown);
    atBoot.tick(102, true);
    assert(atBoot.state() == State::Present && atBoot.ageMs() == 3);
    atBoot.setTimeoutMs(2);
    assert(atBoot.state() == State::NotSeenRecently);
    atBoot.setTimeoutMs(100);
    assert(atBoot.state() == State::Present && atBoot.ageMs() == 3);
    MatchStats stats;
    assert(stats.matches == 0 && stats.addressChanges == 0);
    assert(stats.lastSeenUptimeMs == 0);
    const uint8_t firstAddress[6] = {0x70,0x81,0x94,0x0d,0xfb,0xaa};
    const uint8_t secondAddress[6] = {0x41,0x22,0x33,0x44,0x55,0x66};
    stats.observe(firstAddress, -66, 0);
    assert(stats.matches == 1 && stats.addressChanges == 0);
    assert(std::memcmp(stats.address, firstAddress, 6) == 0);
    assert(stats.rssi == -66 && stats.lastSeenUptimeMs == 0);
    // Multiple callbacks can precede a loop snapshot. Every report must count.
    stats.observe(firstAddress, -60, 10);
    stats.observe(secondAddress, -70, 20);
    stats.observe(firstAddress, -62, 30);
    assert(stats.matches == 4 && stats.addressChanges == 2);
    auto snapshot = stats;
    const uint64_t afterRollover = static_cast<uint64_t>(UINT32_MAX) + 1000;
    stats.observe(secondAddress, -80, afterRollover);
    assert(stats.matches == 5 && stats.addressChanges == 3);
    assert(stats.lastSeenUptimeMs == afterRollover && stats.rssi == -80);
    // A displayed snapshot stays coherent while new callbacks update the source.
    assert(snapshot.matches == 4 && snapshot.addressChanges == 2);
    assert(snapshot.lastSeenUptimeMs == 30 && snapshot.rssi == -62);
    assert(std::memcmp(snapshot.address, firstAddress, 6) == 0);

    std::cout << "PASS: Bluetooth ah vector, macOS key order, rejection cases, "
                 "presence timing, scanner failure, rollover and match statistics\n";
}

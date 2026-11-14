#include "wifi_diagnostics.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main() {
    using namespace wifi_diagnostics;
    assert(!std::strcmp(message(201), "Network not found"));
    assert(!std::strcmp(message(202), "Auth failed"));
    assert(!std::strcmp(message(203), "Association failed"));
    assert(!std::strcmp(message(15), message(204)));
    assert(!std::strcmp(message(210), "Security mismatch"));
    assert(!std::strcmp(message(0), "No disconnect yet"));
    assert(!std::strcmp(message(65535), "Unknown reason"));
    for (unsigned i = 0; i <= 255; ++i) assert(std::strlen(message(i)) <= 21);
    assert(retryLeave(8, 500, 100, true));
    assert(!retryLeave(8, 1100, 100, true));
    assert(!retryLeave(202, 500, 100, true));
    assert(!retryLeave(8, 500, 100, false));
    assert(retryLeave(8, 20, UINT32_MAX - 30, true));
    assert(!retryLeave(8, 2000, UINT32_MAX - 30, true));
    std::cout << "PASS: Wi-Fi reason messages, OLED widths and retry-leave filtering\n";
}

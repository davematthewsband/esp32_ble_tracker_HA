#include "dashboard_protocol.h"

#include <cassert>
#include <cstring>
#include <iostream>

using namespace dashboard_protocol;
int main() {
    char buffer[256];
    JsonWriter out(buffer, sizeof(buffer));
    out.raw("{\"name\":");
    out.text("</script>\"\\\n\x01&");
    out.raw(",\"rssi\":"); out.integer(-127);
    out.raw(",\"count\":"); out.number(UINT64_MAX); out.raw("}");
    assert(out.ok());
    assert(std::strcmp(buffer, "{\"name\":\"\\u003c/script\\u003e\\\"\\\\\\u000a\\u0001\\u0026\",\"rssi\":-127,\"count\":18446744073709551615}") == 0);
    char exact[3];
    JsonWriter exactWriter(exact, sizeof(exact));
    exactWriter.text("");
    assert(exactWriter.ok() && exactWriter.size() == 2);
    char tiny[3];
    JsonWriter overflow(tiny, sizeof(tiny));
    overflow.text("X");
    assert(!overflow.ok());
    assert(tiny[2] == '\0');
    JsonWriter zero(nullptr, 0);
    zero.text("a");
    assert(!zero.ok());

    // Worst-case 32-entry names expand through escaping; stay inside the actual
    // snapshot capacity without truncating advertiser-controlled data into JSON.
    char large[12288];
    JsonWriter full(large, sizeof(large));
    full.raw("{\"observations\":[");
    for (unsigned i = 0; i < 32; ++i) {
        if (i) full.raw(",");
        full.raw("{\"address\":\"70:81:94:0d:fb:aa\",\"name\":");
        full.text("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
        full.raw(",\"lastSeenAtMs\":"); full.number(UINT64_MAX);
        full.raw(",\"addressType\":3,\"rssi\":-127}");
    }
    full.raw("]}");
    assert(full.ok() && full.size() < 11200); // Leave room for system/phone fields.
    assert(equalAuthorization("Basic abc123", "Basic abc123"));
    assert(!equalAuthorization("Basic abc124", "Basic abc123"));
    assert(!equalAuthorization("Basic abc123extra", "Basic abc123"));
    assert(!equalAuthorization("", "Basic abc123"));

    LoginLimiter limiter;
    for (int i = 0; i < 8; ++i) {
        assert(!limiter.blocked(100)); limiter.failure(100);
    }
    assert(limiter.blocked(30099));
    assert(!limiter.blocked(30100));
    LoginLimiter rollover;
    for (int i = 0; i < 8; ++i) rollover.failure(UINT32_MAX - 50);
    assert(rollover.blocked(20));
    assert(!rollover.blocked(30000));
    std::cout << "PASS: safe bounded JSON, authentication comparisons and login limits\n";
}

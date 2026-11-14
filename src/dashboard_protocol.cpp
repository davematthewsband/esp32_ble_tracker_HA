#include "dashboard_protocol.h"

#include <cstdio>
#include <cstring>

namespace dashboard_protocol {
JsonWriter::JsonWriter(char* buffer, size_t capacity) : buffer_(buffer), capacity_(capacity) {
    if (capacity_) buffer_[0] = '\0';
    else ok_ = false;
}
void JsonWriter::put(char value) {
    if (!ok_) return;
    if (size_ + 1 >= capacity_) { ok_ = false; return; }
    buffer_[size_++] = value;
    buffer_[size_] = '\0';
}
void JsonWriter::raw(const char* value) { while (*value) put(*value++); }
void JsonWriter::text(const char* value) {
    constexpr char hex[] = "0123456789abcdef";
    put('"');
    for (; *value; ++value) {
        const auto c = static_cast<unsigned char>(*value);
        if (c == '"' || c == '\\') { put('\\'); put(c); }
        else if (c < 32 || c == '<' || c == '>' || c == '&') {
            raw("\\u00"); put(hex[c >> 4]); put(hex[c & 15]);
        } else put(c);
    }
    put('"');
}
void JsonWriter::number(uint64_t value) {
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
    raw(buffer);
}
void JsonWriter::integer(int value) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", value);
    raw(buffer);
}
bool equalAuthorization(const char* supplied, const char* expected) {
    const size_t length = std::strlen(expected);
    if (std::strlen(supplied) != length) return false;
    unsigned difference = 0;
    for (size_t i = 0; i < length; ++i) difference |= supplied[i] ^ expected[i];
    return difference == 0;
}
void LoginLimiter::refresh(uint32_t now) {
    if (!failures_ || static_cast<uint32_t>(now - windowStart_) >= 30000) {
        windowStart_ = now;
        failures_ = 0;
    }
}
bool LoginLimiter::blocked(uint32_t now) { refresh(now); return failures_ >= 8; }
void LoginLimiter::failure(uint32_t now) { refresh(now); if (failures_ < 8) ++failures_; }
}  // namespace dashboard_protocol

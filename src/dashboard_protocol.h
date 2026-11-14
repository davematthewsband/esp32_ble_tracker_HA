#pragma once

#include <stddef.h>
#include <stdint.h>

namespace dashboard_protocol {
// A fixed-capacity writer. Once full it fails closed; never serve partial JSON.
class JsonWriter {
public:
    JsonWriter(char* buffer, size_t capacity);
    void raw(const char* value);
    void text(const char* value);
    void number(uint64_t value);
    void integer(int value);
    bool ok() const { return ok_; }
    size_t size() const { return size_; }
private:
    void put(char value);
    char* buffer_;
    size_t capacity_;
    size_t size_ = 0;
    bool ok_ = true;
};

bool equalAuthorization(const char* supplied, const char* expected);
// A bounded, global failed-login window. All requests run on the server task.
class LoginLimiter {
public:
    bool blocked(uint32_t now);
    void failure(uint32_t now);
private:
    void refresh(uint32_t now);
    uint32_t windowStart_ = 0;
    unsigned failures_ = 0;
};
}  // namespace dashboard_protocol

#include "device_settings.h"

#include <cJSON.h>
#include <cmath>
#include <cstring>

namespace device_settings {
namespace {
bool boundedStructure(const char* json, size_t length) {
    if (!length || length > kMaxJson || std::strlen(json) != length) return false;
    bool quoted = false, escaped = false;
    unsigned depth = 0, commas = 0;
    for (size_t i = 0; i < length; ++i) {
        const char c = json[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') {
                if (i + 5 < length && std::strncmp(json + i, "\\u0000", 6) == 0) return false;
                escaped = true;
            } else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') { if (++depth > 4) return false; }
        else if (c == '}' || c == ']') { if (!depth) return false; --depth; }
        else if (c == ',' && ++commas > 80) return false;
    }
    return !quoted && depth == 0;
}

bool fields(const cJSON* object, const char* const* names, size_t count) {
    if (!cJSON_IsObject(object)) return false;
    unsigned seen = 0;
    for (auto* item = object->child; item; item = item->next) {
        size_t index = 0;
        for (; index < count; ++index) if (std::strcmp(item->string, names[index]) == 0) break;
        if (index == count || (seen & (1U << index))) return false;
        seen |= 1U << index;
    }
    return seen == (1U << count) - 1;
}

bool text(const cJSON* item, char output[33], bool allowEmpty) {
    if (!cJSON_IsString(item)) return false;
    const size_t length = std::strlen(item->valuestring);
    if (length > 32 || (!length && !allowEmpty)) return false;
    // cJSON preserves raw UTF-8 without validating it. Reject malformed sequences,
    // overlong encodings, surrogate values and controls before persistence.
    for (size_t i = 0; i < length;) {
        const auto first = static_cast<uint8_t>(item->valuestring[i++]);
        if (first < 32 || first == 127) return false;
        if (first < 128) continue;
        unsigned extra;
        uint32_t code, minimum;
        if (first >= 0xc2 && first <= 0xdf) { extra = 1; code = first & 0x1f; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { extra = 2; code = first & 0x0f; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { extra = 3; code = first & 7; minimum = 0x10000; }
        else return false;
        if (i + extra > length) return false;
        while (extra--) {
            const auto next = static_cast<uint8_t>(item->valuestring[i++]);
            if ((next & 0xc0) != 0x80) return false;
            code = (code << 6) | (next & 0x3f);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
    }
    std::memcpy(output, item->valuestring, length + 1);
    return true;
}

bool identity(const cJSON* item, char output[24]) {
    if (!cJSON_IsString(item)) return false;
    const char* value = item->valuestring;
    if (std::strcmp(value, "iphone") == 0) { std::strcpy(output, value); return true; }
    if (std::strlen(value) != 23 || std::strncmp(value, "mac:", 4) ||
        value[4] < '0' || value[4] > '3' || value[5] != ':') return false;
    for (size_t i = 6; i < 23; ++i) {
        if ((i - 6) % 3 == 2) { if (value[i] != ':') return false; }
        else if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'A' && value[i] <= 'F'))) return false;
    }
    std::memcpy(output, value, 24);
    return true;
}
}  // namespace

bool parse(const char* json, size_t length, Config& output) {
    if (!boundedStructure(json, length)) return false;
    cJSON* root = cJSON_ParseWithLengthOpts(json, length + 1, nullptr, true);
    if (!root) return false;
    Config candidate;
    constexpr const char* top[] = {"schemaVersion", "scannerName", "scannerRoom", "phoneTimeoutSeconds", "devices"};
    constexpr const char* labelFields[] = {"identity", "name", "room"};
    auto* version = cJSON_GetObjectItemCaseSensitive(root, "schemaVersion");
    auto* timeout = cJSON_GetObjectItemCaseSensitive(root, "phoneTimeoutSeconds");
    auto* devices = cJSON_GetObjectItemCaseSensitive(root, "devices");
    bool valid = fields(root, top, 5) && cJSON_IsNumber(version) && version->valuedouble == 1 &&
        cJSON_IsNumber(timeout) && std::isfinite(timeout->valuedouble) &&
        timeout->valuedouble >= 5 && timeout->valuedouble <= 3600 &&
        std::floor(timeout->valuedouble) == timeout->valuedouble &&
        text(cJSON_GetObjectItemCaseSensitive(root, "scannerName"), candidate.scannerName, false) &&
        text(cJSON_GetObjectItemCaseSensitive(root, "scannerRoom"), candidate.scannerRoom, true) &&
        cJSON_IsArray(devices) && cJSON_GetArraySize(devices) <= static_cast<int>(kMaxLabels);
    if (valid) {
        candidate.phoneTimeoutSeconds = static_cast<uint32_t>(timeout->valuedouble);
        for (auto* item = devices->child; item && valid; item = item->next) {
            auto& label = candidate.labels[candidate.labelCount];
            valid = fields(item, labelFields, 3) &&
                identity(cJSON_GetObjectItemCaseSensitive(item, "identity"), label.identity) &&
                text(cJSON_GetObjectItemCaseSensitive(item, "name"), label.name, false) &&
                text(cJSON_GetObjectItemCaseSensitive(item, "room"), label.room, true);
            for (size_t i = 0; i < candidate.labelCount; ++i)
                if (std::strcmp(candidate.labels[i].identity, label.identity) == 0) valid = false;
            if (valid) ++candidate.labelCount;
        }
    }
    cJSON_Delete(root);
    if (valid) output = candidate;
    return valid;
}

bool serialize(const Config& config, char* json, size_t capacity, size_t& length) {
    dashboard_protocol::JsonWriter out(json, capacity);
    out.raw("{\"schemaVersion\":1,\"scannerName\":"); out.text(config.scannerName);
    out.raw(",\"scannerRoom\":"); out.text(config.scannerRoom);
    out.raw(",\"phoneTimeoutSeconds\":"); out.number(config.phoneTimeoutSeconds);
    out.raw(",\"devices\":[");
    for (size_t i = 0; i < config.labelCount; ++i) {
        if (i) out.raw(",");
        out.raw("{\"identity\":"); out.text(config.labels[i].identity);
        out.raw(",\"name\":"); out.text(config.labels[i].name);
        out.raw(",\"room\":"); out.text(config.labels[i].room); out.raw("}");
    }
    out.raw("]}");
    length = out.size();
    return out.ok();
}
}  // namespace device_settings

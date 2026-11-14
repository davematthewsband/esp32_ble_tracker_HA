#include "device_settings.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

using namespace device_settings;
std::string document(const std::string& name = "Living room scanner",
                     const std::string& devices = "[]", const std::string& timeout = "120") {
    return "{\"schemaVersion\":1,\"scannerName\":\"" + name +
        "\",\"scannerRoom\":\"Living room\",\"phoneTimeoutSeconds\":" + timeout + ",\"devices\":" + devices + "}";
}
bool parseText(const std::string& json, Config& config) { return parse(json.c_str(), json.size(), config); }

int main() {
    Config settings;
    const std::string label = "{\"identity\":\"iphone\",\"name\":\"My phone\",\"room\":\"Living room\"}";
    assert(parseText(document("Scanner", "[" + label + "]"), settings));
    assert(settings.labelCount == 1 && !std::strcmp(settings.labels[0].identity, "iphone"));
    char output[kMaxJson + 1]; size_t length;
    assert(serialize(settings, output, sizeof(output), length));
    Config restored;
    assert(parse(output, length, restored));
    assert(!std::strcmp(restored.labels[0].room, "Living room"));
    assert(parseText(document("Caf\xc3\xa9"), restored));
    const auto unchanged = settings;
    const auto reject = [&](const std::string& json) {
        assert(!parseText(json, settings));
        assert(settings.labelCount == unchanged.labelCount);
        assert(!std::strcmp(settings.scannerName, unchanged.scannerName));
    };
    reject(document("", "[]"));
    reject(document(std::string(33, 'x')));
    reject(document("bad\\u0000name"));
    reject(document("bad\\nname"));
    reject(document("bad\xc0\xaf")); // Invalid overlong UTF-8.
    reject(document("Scanner", "[]", "4"));
    reject(document("Scanner", "[]", "3601"));
    reject(document("Scanner", "[]", "5.5"));
    reject(document("Scanner", "[]", "\"120\""));
    reject(document("Scanner", "[" + label + "," + label + "]"));
    reject("{\"schemaVersion\":2}");
    auto extra = document(); extra.insert(extra.size() - 1, ",\"WIFI_PASSWORD\":\"secret\""); reject(extra);
    auto duplicate = document(); duplicate.insert(duplicate.size() - 1, ",\"scannerName\":\"duplicate\""); reject(duplicate);
    reject(document() + " trailing");
    reject(std::string(9000, ' '));
    reject(std::string(100, '[') + std::string(100, ']'));
    std::string badIdentity = label;
    badIdentity.replace(badIdentity.find("iphone"), 6, "mac:1:aa:bb:cc:dd:ee:ff");
    reject(document("Scanner", "[" + badIdentity + "]"));

    Config full;
    full.labelCount = kMaxLabels;
    for (size_t i = 0; i < kMaxLabels; ++i) {
        snprintf(full.labels[i].identity, sizeof(full.labels[i].identity), "mac:1:41:22:33:44:55:%02X", static_cast<unsigned>(i));
        std::memset(full.labels[i].name, '<', 32);
        std::memset(full.labels[i].room, '>', 32);
    }
    assert(serialize(full, output, sizeof(output), length));
    assert(parse(output, length, restored) && restored.labelCount == kMaxLabels);
    assert(std::strstr(output, "WIFI_PASSWORD") == nullptr);
    std::string tooMany = "[";
    for (size_t i = 0; i < 17; ++i) {
        if (i) tooMany += ',';
        tooMany += "{\"identity\":\"mac:1:41:22:33:44:55:";
        char suffix[3]; snprintf(suffix, sizeof(suffix), "%02X", static_cast<unsigned>(i));
        tooMany += suffix; tooMany += "\",\"name\":\"Device\",\"room\":\"\"}";
    }
    tooMany += ']';
    reject(document("Scanner", tooMany));
    std::cout << "PASS: settings round trips, schema/size/UTF-8 limits, duplicates and secret-field rejection\n";
}

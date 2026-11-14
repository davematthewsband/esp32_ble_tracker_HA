#pragma once
#include "device_settings.h"

namespace settings_store {
void begin(uint32_t defaultTimeoutSeconds);
bool copy(device_settings::Config& output);
bool replace(const device_settings::Config& config);
uint32_t phoneTimeoutMs();
}  // namespace settings_store

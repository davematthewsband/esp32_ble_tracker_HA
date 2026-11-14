#pragma once
#include <stdint.h>

namespace oled_ui {
bool begin();
bool ready();
void progress(const char* stage, unsigned completed);
// Mark initialization complete. Progress stays visible briefly while services
// run; no waiting for Wi-Fi association or a serial monitor.
void initialized();
// Called from loop. Handles splash, initialization, PRG and periodic rendering.
void poll();
bool titleComplete();
}  // namespace oled_ui

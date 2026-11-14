#pragma once

namespace network_dashboard {
void begin();
// Main loop only. Publish snapshots; HTTPS handlers never read live scan state.
void poll();
const char* status();
}  // namespace network_dashboard

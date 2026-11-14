#pragma once

namespace network_dashboard {
struct ConnectionInfo {
    bool enabled = false;
    bool connected = false;
    bool httpsReady = false;
    char ip[16]{};
    int rssi = 0;
};
// Main-loop access only. No passwords, IRK or private TLS material.
ConnectionInfo connectionInfo();
void begin();
// Main loop only. Publish snapshots; HTTPS handlers never read live scan state.
void poll();
const char* status();
}  // namespace network_dashboard

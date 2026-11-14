#pragma once

// tools/setup_network.py creates include/config.local.network.h (ignored).
// WPA2/WPA3 personal network, 2.4 GHz. No open-network or hotspot fallback.
#define WIFI_SSID "YOUR_IOT_SSID"
#define WIFI_PASSWORD ""
#define TRACKER_HOSTNAME "ble-tracker"
#define DASHBOARD_USERNAME "admin"
// Generate a unique password of at least 24 characters; never use this example.
#define DASHBOARD_PASSWORD ""

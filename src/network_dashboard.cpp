#include "network_dashboard.h"
#include "ble_scanner.h"
#include "iphone_tracker.h"
#include "dashboard_protocol.h"
#include "dashboard_assets.h"
#include "settings_store.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_https_server.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <mbedtls/base64.h>
#include <freertos/semphr.h>
#include <cstring>

#if defined(CONFIG_MBEDTLS_SSL_PROTO_TLS1) || defined(CONFIG_MBEDTLS_SSL_PROTO_TLS1_1)
#error "Dashboard requires an SDK without legacy TLS 1.0/1.1 enabled"
#endif

#if __has_include("config.local.network.h")
#include "config.local.network.h"
#endif
#if __has_include("config.local.tls.h")
#include "config.local.tls.h"
#else
static const char kTlsCertificate[] = "";
static const char kTlsPrivateKey[] = "";
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef TRACKER_HOSTNAME
#define TRACKER_HOSTNAME "ble-tracker"
#endif
#ifndef DASHBOARD_USERNAME
#define DASHBOARD_USERNAME ""
#endif
#ifndef DASHBOARD_PASSWORD
#define DASHBOARD_PASSWORD ""
#endif

namespace {
constexpr size_t kJsonCapacity = 12288;
char staging[kJsonCapacity];
char published[kJsonCapacity];
size_t publishedLength = 0;
SemaphoreHandle_t snapshotMutex = nullptr;
httpd_handle_t server = nullptr;
char expectedAuthorization[192]{};
char csrfToken[65]{};
dashboard_protocol::LoginLimiter limiter;
bool enabled = false;
bool connected = false;
bool mdnsStarted = false;
uint32_t lastConnectMs = 0;
uint32_t lastSnapshotMs = 0;
uint32_t lastServerAttemptMs = 0;

void securityHeaders(httpd_req_t* request) {
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(request, "X-Frame-Options", "DENY");
    httpd_resp_set_hdr(request, "Referrer-Policy", "no-referrer");
    httpd_resp_set_hdr(request, "Content-Security-Policy",
        "default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; "
        "frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
    httpd_resp_set_hdr(request, "Connection", "close");
}

bool authorized(httpd_req_t* request) {
    securityHeaders(request);
    const uint32_t now = millis();
    if (limiter.blocked(now)) {
        httpd_resp_set_status(request, "429 Too Many Requests");
        httpd_resp_set_hdr(request, "Retry-After", "30");
        httpd_resp_sendstr(request, "Login temporarily limited. Retry in 30 seconds.");
        return false;
    }
    char supplied[sizeof(expectedAuthorization)]{};
    const size_t length = httpd_req_get_hdr_value_len(request, "Authorization");
    if (length && length < sizeof(supplied) &&
        httpd_req_get_hdr_value_str(request, "Authorization", supplied, sizeof(supplied)) == ESP_OK &&
        dashboard_protocol::equalAuthorization(supplied, expectedAuthorization)) return true;
    // Missing credentials are a normal browser challenge, not a failed guess.
    if (length) limiter.failure(now);
    httpd_resp_set_status(request, "401 Unauthorized");
    httpd_resp_set_hdr(request, "WWW-Authenticate", "Basic realm=\"BLE tracker\", charset=\"UTF-8\"");
    httpd_resp_sendstr(request, "Authentication required.");
    return false;
}

esp_err_t page(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_sendstr(request, kDashboardHtml);
}
esp_err_t script(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;
    httpd_resp_set_type(request, "text/javascript; charset=utf-8");
    return httpd_resp_sendstr(request, kDashboardJs);
}
esp_err_t style(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;
    httpd_resp_set_type(request, "text/css; charset=utf-8");
    return httpd_resp_sendstr(request, kDashboardCss);
}
esp_err_t api(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;
    // At most one handler runs at a time. Never hold the snapshot mutex while
    // sending to a potentially slow client, or allocate an unbounded response.
    auto* response = static_cast<char*>(malloc(kJsonCapacity));
    if (!response) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_sendstr(request, "Low memory");
    }
    size_t length = 0;
    if (xSemaphoreTake(snapshotMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        length = publishedLength;
        if (length) std::memcpy(response, published, length);
        xSemaphoreGive(snapshotMutex);
    }
    esp_err_t result;
    if (!length) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        result = httpd_resp_sendstr(request, "Status not ready");
    }
    else {
        httpd_resp_set_type(request, "application/json");
        result = httpd_resp_send(request, response, length);
    }
    free(response);
    return result;
}

esp_err_t settingsGet(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;
    device_settings::Config config;
    auto* json = static_cast<char*>(malloc(device_settings::kMaxJson + 1));
    size_t length = 0;
    if (!json || !settings_store::copy(config) ||
        !device_settings::serialize(config, json, device_settings::kMaxJson + 1, length)) {
        free(json);
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_sendstr(request, "Settings unavailable");
    }
    httpd_resp_set_hdr(request, "X-CSRF-Token", csrfToken);
    httpd_resp_set_type(request, "application/json");
    const auto result = httpd_resp_send(request, json, length);
    free(json);
    return result;
}

esp_err_t settingsPost(httpd_req_t* request) {
    if (!authorized(request)) return ESP_OK;
    char token[65]{};
    if (httpd_req_get_hdr_value_len(request, "X-CSRF-Token") != 64 ||
        httpd_req_get_hdr_value_str(request, "X-CSRF-Token", token, sizeof(token)) != ESP_OK ||
        !dashboard_protocol::equalAuthorization(token, csrfToken)) {
        httpd_resp_set_status(request, "403 Forbidden");
        return httpd_resp_sendstr(request, "Reload the dashboard before changing settings.");
    }
    char contentType[48]{};
    if (httpd_req_get_hdr_value_str(request, "Content-Type", contentType, sizeof(contentType)) != ESP_OK ||
        (std::strcmp(contentType, "application/json") &&
         std::strcmp(contentType, "application/json; charset=utf-8"))) {
        httpd_resp_set_status(request, "415 Unsupported Media Type");
        return httpd_resp_sendstr(request, "Settings must be JSON.");
    }
    if (request->content_len == 0 || request->content_len > device_settings::kMaxJson) {
        httpd_resp_set_status(request, "413 Content Too Large");
        return httpd_resp_sendstr(request, "Settings must contain at most 8 KiB.");
    }
    auto* json = static_cast<char*>(malloc(request->content_len + 1));
    if (!json) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_sendstr(request, "Low memory");
    }
    size_t received = 0;
    const uint32_t started = millis();
    while (received < request->content_len && static_cast<uint32_t>(millis() - started) < 5000) {
        const int count = httpd_req_recv(request, json + received, request->content_len - received);
        if (count <= 0) break;
        received += count;
    }
    json[received] = '\0';
    device_settings::Config candidate;
    const bool valid = received == request->content_len && device_settings::parse(json, received, candidate);
    free(json);
    if (!valid) {
        httpd_resp_set_status(request, "400 Bad Request");
        return httpd_resp_sendstr(request, "Invalid schema, duplicate/unknown fields, unsupported identity or settings limits. Nothing saved.");
    }
    if (!settings_store::replace(candidate)) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_sendstr(request, "Settings could not be saved. Previous active settings retained.");
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"saved\":true}");
}

bool startServer() {
    // TLS handshakes need heap beyond static build usage. Fail closed and retry
    // rather than starting a plaintext fallback if memory is too constrained.
    if (ESP.getFreeHeap() < 75000 || ESP.getMaxAllocHeap() < 40000) return false;
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    config.servercert = reinterpret_cast<const uint8_t*>(kTlsCertificate);
    config.servercert_len = sizeof(kTlsCertificate);
    config.prvtkey_pem = reinterpret_cast<const uint8_t*>(kTlsPrivateKey);
    config.prvtkey_len = sizeof(kTlsPrivateKey);
    config.transport_mode = HTTPD_SSL_TRANSPORT_SECURE;
    config.port_secure = 443;
    config.httpd.max_open_sockets = 2;
    config.httpd.max_uri_handlers = 6;
    config.httpd.max_resp_headers = 12;
    config.httpd.backlog_conn = 2;
    config.httpd.lru_purge_enable = true;
    config.httpd.recv_wait_timeout = 3;
    config.httpd.send_wait_timeout = 3;
    config.tls_handshake_timeout_ms = 5000;
    if (httpd_ssl_start(&server, &config) != ESP_OK) { server = nullptr; return false; }
    const httpd_uri_t routes[] = {
        { .uri = "/", .method = HTTP_GET, .handler = page, .user_ctx = nullptr },
        { .uri = "/app.js", .method = HTTP_GET, .handler = script, .user_ctx = nullptr },
        { .uri = "/style.css", .method = HTTP_GET, .handler = style, .user_ctx = nullptr },
        { .uri = "/api/status", .method = HTTP_GET, .handler = api, .user_ctx = nullptr },
        { .uri = "/api/settings", .method = HTTP_GET, .handler = settingsGet, .user_ctx = nullptr },
        { .uri = "/api/settings", .method = HTTP_POST, .handler = settingsPost, .user_ctx = nullptr },
    };
    for (const auto& route : routes) {
        if (httpd_register_uri_handler(server, &route) != ESP_OK) {
            httpd_ssl_stop(server); server = nullptr; return false;
        }
    }
    Serial.printf("HTTPS dashboard ready: https://%s.local/ (IP %s)\n",
                  TRACKER_HOSTNAME, WiFi.localIP().toString().c_str());
    return true;
}

void publishSnapshot() {
    using dashboard_protocol::JsonWriter;
    const uint64_t uptime = static_cast<uint64_t>(esp_timer_get_time()) / 1000;
    const auto& phone = iphone_tracker::presence();
    const auto& stats = iphone_tracker::stats();
    const auto& scan = ble_scanner::snapshot();
    JsonWriter out(staging, sizeof(staging));
    out.raw("{\"schemaVersion\":1,\"uptimeMs\":"); out.number(uptime);
    out.raw(",\"freeHeap\":"); out.number(ESP.getFreeHeap());
    out.raw(",\"minFreeHeap\":"); out.number(ESP.getMinFreeHeap());
    out.raw(",\"largestFreeBlock\":"); out.number(ESP.getMaxAllocHeap());
    out.raw(",\"wifi\":{\"connected\":"); out.raw(connected ? "true" : "false");
    out.raw(",\"ip\":"); out.text(connected ? WiFi.localIP().toString().c_str() : "");
    out.raw(",\"rssi\":"); if (connected) out.integer(WiFi.RSSI()); else out.raw("null");
    out.raw("},\"scanner\":{\"status\":"); out.text(ble_scanner::status());
    out.raw(",\"scans\":"); out.number(ble_scanner::completedScans());
    out.raw(",\"reports\":"); out.number(ble_scanner::reports());
    out.raw(",\"snapshotCompletedAtMs\":"); out.number(scan.completedUptimeMs);
    out.raw(",\"outsideCapacityReports\":"); out.number(scan.outsideCapacity);
    out.raw("},\"phone\":{\"status\":"); out.text(iphone_tracker::status());
    out.raw(",\"seen\":"); out.raw(phone.seen() ? "true" : "false");
    out.raw(",\"latestMatchingMac\":");
    if (phone.seen()) {
        char mac[18];
        snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", stats.address[0],
                 stats.address[1], stats.address[2], stats.address[3], stats.address[4], stats.address[5]);
        out.text(mac);
    } else out.raw("null");
    out.raw(",\"lastSeenAgoMs\":"); if (phone.seen()) out.number(phone.ageMs()); else out.raw("null");
    out.raw(",\"lastSeenUptimeMs\":"); if (phone.seen()) out.number(stats.lastSeenUptimeMs); else out.raw("null");
    out.raw(",\"lastRssi\":"); if (phone.seen()) out.integer(phone.rssi()); else out.raw("null");
    out.raw(",\"matchingReports\":"); out.number(stats.matches);
    out.raw(",\"addressChanges\":"); out.number(stats.addressChanges);
    out.raw("},\"observations\":[");
    for (size_t i = 0; i < scan.count; ++i) {
        const auto& item = scan.entries[i];
        if (i) out.raw(",");
        out.raw("{\"address\":"); out.text(item.address);
        out.raw(",\"addressType\":"); out.number(item.addressType);
        out.raw(",\"name\":"); out.text(item.name);
        out.raw(",\"rssi\":"); out.integer(item.rssi);
        out.raw(",\"lastSeenAtMs\":"); out.number(item.lastSeenUptimeMs);
        out.raw("}");
    }
    out.raw("]}");
    if (xSemaphoreTake(snapshotMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        publishedLength = out.ok() ? out.size() : 0;
        if (publishedLength) std::memcpy(published, staging, publishedLength);
        xSemaphoreGive(snapshotMutex);
    }
}

bool validSettings() {
    const size_t ssidLength = std::strlen(WIFI_SSID);
    const size_t passwordLength = std::strlen(WIFI_PASSWORD);
    const size_t userLength = std::strlen(DASHBOARD_USERNAME);
    const size_t webPasswordLength = std::strlen(DASHBOARD_PASSWORD);
    if (!ssidLength || ssidLength > 32 || passwordLength < 8 || passwordLength > 63 ||
        !userLength || userLength > 32 || webPasswordLength < 24 || webPasswordLength > 64 ||
        std::strchr(DASHBOARD_USERNAME, ':') || !*kTlsCertificate || !*kTlsPrivateKey) return false;
    const size_t hostnameLength = std::strlen(TRACKER_HOSTNAME);
    if (!hostnameLength || hostnameLength > 63 || TRACKER_HOSTNAME[0] == '-' ||
        TRACKER_HOSTNAME[hostnameLength - 1] == '-') return false;
    for (const char* c = TRACKER_HOSTNAME; *c; ++c)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '-')) return false;
    char credentials[100];
    const int length = snprintf(credentials, sizeof(credentials), "%s:%s", DASHBOARD_USERNAME, DASHBOARD_PASSWORD);
    if (length < 0 || static_cast<size_t>(length) >= sizeof(credentials)) return false;
    std::memcpy(expectedAuthorization, "Basic ", 6);
    size_t encoded = 0;
    const int result = mbedtls_base64_encode(reinterpret_cast<uint8_t*>(expectedAuthorization + 6),
        sizeof(expectedAuthorization) - 7, &encoded, reinterpret_cast<const uint8_t*>(credentials), length);
    std::memset(credentials, 0, sizeof(credentials));
    expectedAuthorization[6 + encoded] = '\0';
    return result == 0;
}
}  // namespace

namespace network_dashboard {
void begin() {
    if (!validSettings()) {
        Serial.println("Wi-Fi/HTTPS disabled: fill valid private network, login and TLS configuration.");
        return;
    }
    snapshotMutex = xSemaphoreCreateMutex();
    if (!snapshotMutex) { Serial.println("Wi-Fi/HTTPS disabled: no snapshot mutex memory."); return; }
    uint8_t random[32];
    esp_fill_random(random, sizeof(random));
    for (size_t i = 0; i < sizeof(random); ++i)
        snprintf(csrfToken + i * 2, 3, "%02x", random[i]);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(TRACKER_HOSTNAME);
    WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);
    WiFi.setAutoReconnect(false);
    enabled = true;
    lastConnectMs = millis();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Wi-Fi station connecting; credentials are not logged.");
}

void poll() {
    if (!enabled) return;
    const uint32_t now = millis();
    const bool wasConnected = connected;
    connected = WiFi.status() == WL_CONNECTED;
    if (connected && !wasConnected) {
        Serial.printf("Wi-Fi connected: IP %s | RSSI %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        lastServerAttemptMs = now - 15000;
        if (!mdnsStarted && MDNS.begin(TRACKER_HOSTNAME)) {
            MDNS.addService("https", "tcp", 443);
            mdnsStarted = true;
        }
    }
    if (!connected && wasConnected) {
        Serial.println("Wi-Fi disconnected; BLE scanning continues.");
        if (mdnsStarted) { MDNS.end(); mdnsStarted = false; }
    }
    if (!connected && static_cast<uint32_t>(now - lastConnectMs) >= 30000) {
        lastConnectMs = now;
        WiFi.disconnect(false, false);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        Serial.println("Wi-Fi connection retry.");
    }
    if (connected && !server && static_cast<uint32_t>(now - lastServerAttemptMs) >= 15000) {
        lastServerAttemptMs = now;
        if (!startServer()) Serial.println("HTTPS not ready (TLS/server resources); retry in 15 s.");
    }
    if (static_cast<uint32_t>(now - lastSnapshotMs) >= 1000) {
        lastSnapshotMs = now;
        publishSnapshot();
    }
}

const char* status() {
    if (!enabled) return "disabled";
    if (!connected) return "connecting";
    return server ? "HTTPS ready" : "HTTPS waiting";
}

ConnectionInfo connectionInfo() {
    ConnectionInfo info;
    info.enabled = enabled;
    info.connected = connected;
    info.httpsReady = connected && server;
    if (connected) {
        snprintf(info.ip, sizeof(info.ip), "%s", WiFi.localIP().toString().c_str());
        info.rssi = WiFi.RSSI();
    }
    return info;
}
}  // namespace network_dashboard

# Wi-Fi dashboard and saved settings

The two agreed stages add a local signal dashboard, saved device labels/manual room assignments, and versioned configuration backup/import. Observation data is read-only; explicit authenticated settings saves are supported. No OTA firmware endpoint is provided.

## Private setup

The installation uses three ignored files, separate from public examples:

- `include/config.local.h`: existing Remote IRK and presence timeout, unchanged by network setup.
- `include/config.local.network.h`: Wi-Fi SSID/password, dashboard hostname and login.
- `include/config.local.tls.h`: generated server certificate and its private key.

`.local-network/` stores the matching certificate/key and certificate identity. Files are created with owner-only permissions; the directory is mode 0700. Firmware binaries contain secrets and remain ignored too. These are local-file protections, not ESP32 flash encryption or secure boot. No irreversible security fuses have been changed.

The helper has already prepared this checkout's SSID and certificate. Put the Wi-Fi password and a unique dashboard password of **at least 24 characters** into the local network file. The helper generates a 32-character dashboard password initially; preserve its strength if replacing it. The example [network header](../include/network.example.h) contains no working credentials. Do not copy local secrets into public examples.

For a fresh checkout, run the following from the repository root, replacing the SSID:

```sh
python3 tools/setup_network.py --ssid YOUR_IOT_SSID
```

The Wi-Fi password prompt is hidden. Alternatively, `--prepare` leaves a new password blank for direct editing. Re-running setup preserves existing passwords and TLS material unless the hostname/IP changes or `--renew` is requested. It never edits the IRK configuration. The helper requires OpenSSL and verifies that private outputs are ignored by Git.

Only 2.4 GHz WPA2/WPA3 Personal networks are supported. Open networks, WEP, captive portals and enterprise authentication are not implemented. `WIFI_PASSWORD` must be 8–63 UTF-8 bytes. The hostname is a lowercase DNS label (default `ble-tracker`). Incomplete credentials, a short dashboard password or missing certificate/key disable the entire network stage; serial/OLED/BLE operation continues. Credentials are not printed or sent in the status API.

## Build, upload and open

The project pins Espressif32 **8.0.0**, Arduino **3.3.12**, and the corresponding ESP-IDF **5.5.5** libraries. This replaces the previous Arduino 2.0.9 baseline for the network stage. See [PlatformIO release notes](https://github.com/platformio/platform-espressif32/releases/tag/v8.0.0). The prebuilt S3 SDK enables TLS 1.2, not TLS 1.0/1.1; compilation rejects those legacy TLS options if enabled by a later SDK. This is a pinned baseline, not a guarantee against future security advisories.

```sh
PLATFORMIO_CORE_DIR="$PWD/.pio-core" /Users/finnmainstone/.platformio/penv/bin/platformio run -e heltec_v3
```

After a user-performed or separately agreed upload, open the serial monitor at 115200 baud. It will show network state and, once ready, the HTTPS hostname and IP. The OLED keeps its phone statistics; network diagnostics are on serial and the dashboard. Use the [README upload instructions](../README.md#build-and-first-hardware-check).

Browse to **https://ble-tracker.local/** and use the username/password from the private network header. The page/assets, `/api/status` and `/api/settings` all require authentication. Settings writes additionally require a per-boot CSRF token obtained from the authenticated settings response; no token is included in exported configuration. A normal browser login challenge is expected. Basic authentication is carried only inside TLS; no credentials are stored in JavaScript or browser local storage. Browsers can retain Basic credentials for their session, so close the browser session when finished on a shared computer.

Your laptop must be able to reach the ESP32 on the IoT LAN. If VLAN isolation blocks access, the relevant network paths are laptop → ESP32 TCP 443 and mDNS for `.local` discovery; the firmware does not change router/firewall settings. There is no plaintext port 80, open hotspot, cloud relay, automatic port forwarding or router configuration. Keep this service on the intended local network.

## Certificate trust

Each installation gets its own self-signed **ECDSA P-256 / SHA-256** server certificate, valid for 365 days, with SANs for the configured hostname and `hostname.local`. Its key is unique to that installation. A self-signed certificate encrypts traffic, but a browser needs an explicit trust decision to verify it.

Use the certificate produced locally in `.local-network/server.crt`. Inspect its identity, dates and SHA-256 fingerprint:

```sh
openssl x509 -in .local-network/server.crt -noout -subject -dates -fingerprint -sha256
```

On macOS, import that exact certificate into your login keychain and explicitly trust it for SSL, then reopen the browser. Verify the certificate/fingerprint presented by the scanner matches the locally generated certificate before entering the dashboard password. Do not accept an unrelated warning just to make the page load. Trust the server certificate for this device, not a downloaded third-party CA.

For a CLI check that pins the generated certificate and prompts for the dashboard password:

```sh
curl --cacert .local-network/server.crt --user admin https://ble-tracker.local/api/status
```

Use your configured username if changed. No password belongs in shell command arguments. Do not use `curl -k` as an acceptance check.

If `.local` is unavailable, reserve an IPv4 address in the router and add that address to the certificate, for example:

```sh
python3 tools/setup_network.py --prepare --ip 192.0.2.10
```

`192.0.2.10` is an example, not your network address. This regenerates TLS material and requires rebuilding/uploading and trusting the replacement certificate. It does not change the ESP32's DHCP configuration or set a static IP. Use `--renew` to renew a certificate while preserving login/network credentials; repeat any `--ip` argument used previously. No online certificate renewal is implemented.

## Dashboard and observation limits

The dashboard refreshes every three seconds. It separates **ESP32 Wi-Fi RSSI** (scanner → access point) from **BLE RSSI** (advertiser → scanner). The Bluetooth table shows only the latest completed five-second round, capped at 32 address/type pairs. It defaults to strongest-first sorting; recent/name sorting is available too. Retention is first-come, not strongest-32 sampling, so sorting cannot reveal devices omitted from a full table. Last-seen age and overflow-report counts keep that limitation visible.

The phone card shows IRK-based state, latest matching rotating MAC, historical RSSI, last-seen age/uptime, report count and observed address transitions. A dashboard disconnect replaces the live phone headline with unknown/offline and explicitly marks remaining values historical. No wall clock or automatic room-location inference is introduced. Saved manual room labels are clearly marked.

Browser rendering uses text nodes for advertiser-controlled names and addresses; no HTML insertion. All resources are served by the ESP32 itself. Responses use no-store caching, a restrictive same-origin content policy, anti-framing and no CORS permission. Eight failed credential attempts within a 30-second window block new authentication attempts until that window resets. This is a global bounded limiter, so repeated bad attempts can temporarily block legitimate users too; it is not a substitute for LAN access restrictions.

## Device names, rooms and configuration backup

Use **Name device** beside a Bluetooth observation, or **Name this phone**, to select an identity and save a display name/manual room. Existing saved devices also appear below the editor, so labels can be managed while a device is absent. Up to **16** labels are retained. Names and room labels allow up to **32 UTF-8 bytes**, with controls and malformed UTF-8 rejected. A manual room is a description you assigned, not a measured location.

The identity `iphone` attaches to the configured IRK without copying that key into the settings file. Ordinary identities use `mac:<addressType>:<UPPERCASE_MAC>`; a rotating address can stop matching such a label. An observation matching the latest phone RPA receives the iPhone label. Other observations remain address-based. The table still shows the advertised address/type; a label does not prove physical identity.

Scanner name, scanner room and the **5–3600 second** phone timeout are editable too. On first boot the timeout uses the valid compile-time value (otherwise 120 seconds); a saved timeout takes precedence on later firmware uploads. Changes apply on the main loop without resetting last-seen history. Increasing/decreasing the timeout can therefore change whether an existing observation is considered recent.

**Download settings JSON** exports the currently saved nonsecret configuration. **Import settings JSON** asks before replacing all saved scanner settings and labels. The ESP32 validates the whole file before writing anything: schema/version, required/unknown/duplicate fields, identity format, duplicate identities, integer timeout bounds, UTF-8, nesting, item count and size (8 KiB maximum). Extra fields such as Wi-Fi passwords or IRKs are rejected. The server returns an error and retains active settings on failed validation or storage failure. Saves are complete replacements; the last successful save wins if multiple dashboards edit at once.

The exported schema is:

```json
{
  "schemaVersion": 1,
  "scannerName": "Living room scanner",
  "scannerRoom": "Living room",
  "phoneTimeoutSeconds": 120,
  "devices": [
    {"identity": "iphone", "name": "My phone", "room": "Living room"}
  ]
}
```

Settings are serialized to a single NVS blob under namespace `ble-tracker`, key `settings`. The new in-memory state is applied only after a successful write. An ordinary upload with the same NVS layout normally preserves it; full flash erase, erasing NVS or changing the partition layout may remove it. Invalid/unsupported saved data is left in storage and defaults are used rather than silently overwriting it. Unsupported future JSON versions are rejected; cross-version migration is not claimed.

Backups contain labels and addresses but exclude live scan history, Wi-Fi/login credentials, CSRF tokens, TLS keys and IRKs. Keep the JSON private nonetheless. Back up the private headers/certificate separately through your own secure storage if you need to recover the whole installation; an ordinary settings export cannot restore those secrets.

## Runtime architecture

Wi-Fi starts in station mode only, requires at least WPA2, does not persist new Wi-Fi credentials to NVS, and retries connection every 30 seconds without a wait loop. BLE scanning and phone timing remain separate from Wi-Fi reachability. The new Arduino core uses NimBLE; the application uses its native GAP scan callback to avoid the bundled C++ scanner's uncapped map, and explicitly converts native little-endian addresses into printed MAC order before IRK resolution. Advertisements still reach IRK matching before the diagnostic table cap.

The BLE task writes a fixed observation table. Scan completion hands it to the main loop, which publishes a completed-round snapshot. The main loop serializes system/phone/BLE status into a bounded 12 KiB JSON buffer and copies it under a mutex. The HTTPS worker reads only that protected snapshot, releases the mutex before any socket output and never reads live scan/phone state. No key, network password, login password or TLS key is part of that payload.

The TLS server permits two client sockets, a small backlog and bounded timeouts. It starts only with sufficient free heap/largest free block and otherwise retries every 15 seconds without a plaintext fallback. Free heap, minimum heap and largest free block are shown in the dashboard. Actual TLS handshake memory and BLE/Wi-Fi coexistence still need physical verification. No unattended recovery reboot is introduced.

## Validation

Run the following software checks:

```sh
python3 tests/run_tests.py
python3 -m unittest discover -s tests -p 'test_network_setup.py' -v
node tests/test_dashboard_ui.js
python3 tests/build_network_fixture.py
git diff --check
```

The C++ tests cover cryptographic vectors, native address ordering, timing/statistics, bounded JSON escaping, login windows and real cJSON settings round trips/rejections. Host tests download matching cJSON 1.7.19 sources on first use into the ignored cache and verify pinned checksums. Setup tests generate real certificates in temporary directories, verify hostname/IP SANs and confirm local-file permissions and preserved credentials. UI tests exercise strongest/recent sorting, hostile names as plain text, empty scans, offline/recovery states, CSRF-bearing saves and backup/import flows with a minimal DOM harness; they are not browser visual QA. The enabled-network fixture uses throwaway credentials, checks the actual TLS server is linked, and never accesses the board. The normal project can legitimately compile with networking disabled if private configuration is incomplete.

After uploading, verify on the real board:

1. BLE/iPhone recognition still works after the core/backend upgrade and while Wi-Fi connects.
2. HTTPS presents the generated certificate and the expected login challenge. Unauthenticated requests to all five GET routes (`/`, `/app.js`, `/style.css`, `/api/status`, `/api/settings`) receive 401; bad credentials eventually receive 429; correct credentials succeed after the window resets.
3. The table, phone statistics and separate Wi-Fi RSSI update. Leave the dashboard open for several minutes and watch minimum heap, scans and phone matches for stalls/resets.
4. Disconnect the access point: the page becomes offline/unknown, BLE continues, and networking recovers when Wi-Fi returns. An incorrect password must not open a hotspot or HTTP fallback.
5. Confirm TLS 1.0/1.1 and plaintext HTTP cannot connect. An authenticated settings POST without the correct CSRF token must receive 403. Unknown POST/PUT routes must not change settings.
6. Save names/rooms and a timeout, reset the board, then perform an ordinary firmware upload and confirm persistence. Download a backup, edit/import it and confirm the replacement. Invalid/oversized/duplicate/secret-bearing JSON must leave the last valid settings active.

Software checks passed on 14 November 2026, including the enabled-network fixture. The current private-config build reports 81,916 bytes static RAM (25.0%) and 1,289,176 bytes application flash (38.6%). These values do not measure runtime TLS/NVS heap.

These physical network/TLS/NVS checks remain pending. The assistant has not flashed firmware, joined the Wi-Fi network or written labels/settings to the board.

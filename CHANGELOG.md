# Changelog

Notable implemented changes are recorded here. Future work belongs in [the roadmap](docs/roadmap.md).

## Unreleased

### Added

- Wi-Fi disconnect reason codes and short explanations on serial and the PRG Wi-Fi OLED page. The last reason survives connection retries/recovery; deliberate local retry disconnects are filtered briefly so they do not hide the failure under investigation. Event callbacks copy only numeric codes, never credentials.

- OLED startup title/initialization progress and debounced PRG-button cycling through Phone, Wi-Fi and System pages. Wi-Fi shows connection state, IP, RSSI and HTTPS readiness; System shows uptime/scans/memory. Startup does not wait for association, held presses do not repeat, and page changes do not write settings or expose passwords/IRKs.
- Editable scanner/device names and manual room labels, persisted in NVS with the phone timeout. Added authenticated/CSRF-protected settings saves and versioned JSON download/import with strict bounds/schema validation; exports exclude credentials, keys and IRKs. Invalid imports retain active settings.

- Station-only WPA2/WPA3 Wi-Fi and an authenticated HTTPS observation dashboard with separate Wi-Fi/BLE RSSI, strongest-first BLE observations, phone statistics and memory/scan health. Browser disconnects mark presence unconfirmed instead of retaining a live-present headline.
- Private network/certificate setup helper, per-installation ECDSA server certificate and dashboard password, bounded login attempts, protected status snapshots, and host tests for JSON escaping, authentication limits, private-file handling and dashboard offline behavior. Network configuration stays disabled when incomplete or invalid.
- Phone statistics: latest matching rotating MAC, last matching RSSI and last-seen age on the OLED; serial output also reports last detection uptime, total matching advertisement reports and observed address transitions. Counters include every matching callback, even between display updates, and reset on reboot.
- Local iPhone presence from macOS Remote IRK configuration: AES-based RPA resolution before diagnostic capacity limits, 120-second configurable last-seen timeout, unknown/present/not-seen status, last-seen age and historical matching RSSI on serial/OLED. Scanner failure produces unknown status.
- Placeholder configuration, strict key-format conversion, firmware AES startup self-test, and sanitized host tests using the Bluetooth D.7 vector plus timing/rollover and failure cases. Personal keys remain only in ignored local configuration and build output.

- Project learnings page recording the user-observed USB-A to USB-C power/serial success and USB-C to USB-C failure, with README connection guidance and the cause left unconfirmed.
- Passive BLE scanning in asynchronous five-second rounds, with a fixed 32-entry observation table, local address/type/name/RSSI diagnostics and OLED scan counters. Callback output is deferred to the main loop; start failures retry and missing scan completion is reported.

- Minimal PlatformIO/Arduino firmware for the confirmed Heltec WiFi LoRa 32 V3 target, with serial heartbeat, OLED status and uptime, explicit display power/reset handling, and serial fallback when OLED initialization fails.
- Pinned build dependencies and build, upload and serial-monitor instructions for a later hardware test.
- Project README describing the Heltec V3 hardware, learning goals and proposed BLE-to-MQTT-to-Home-Assistant approach.
- Repository working agreement covering discussion before edits, changelog maintenance, privacy and validation.
- Staged learning roadmap with acceptance checks and open decisions.
- Ignore rules for build output, local credentials, device configuration and diagnostic captures.

### Changed

- Updated the network-facing build to Espressif32 8.0.0 / Arduino 3.3.12 / ESP-IDF 5.5.5, with TLS 1.2 and legacy TLS disabled. Adapted scanning to native NimBLE GAP reports, explicitly normalized address byte order and retained the fixed observation capacity without an internal device map.

The user reported the preceding serial/OLED firmware working after their own flash. Earlier live serial inspection also showed fresh IRK matches. The upgraded BLE backend, Wi-Fi/HTTPS, memory under live TLS load, NVS persistence and dashboard behavior on the physical board remain unverified; no Home Assistant deployment has been performed.

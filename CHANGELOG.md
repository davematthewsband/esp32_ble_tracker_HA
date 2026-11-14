# Changelog

Notable implemented changes are recorded here. Future work belongs in [the roadmap](docs/roadmap.md).

## Unreleased

### Added

- Project learnings page recording the user-observed USB-A to USB-C power/serial success and USB-C to USB-C failure, with README connection guidance and the cause left unconfirmed.
- Passive BLE scanning in asynchronous five-second rounds, with a fixed 32-entry observation table, local address/type/name/RSSI diagnostics and OLED scan counters. Callback output is deferred to the main loop; start failures retry and missing scan completion is reported.

- Minimal PlatformIO/Arduino firmware for the confirmed Heltec WiFi LoRa 32 V3 target, with serial heartbeat, OLED status and uptime, explicit display power/reset handling, and serial fallback when OLED initialization fails.
- Pinned build dependencies and build, upload and serial-monitor instructions for a later hardware test.
- Project README describing the Heltec V3 hardware, learning goals and proposed BLE-to-MQTT-to-Home-Assistant approach.
- Repository working agreement covering discussion before edits, changelog maintenance, privacy and validation.
- Staged learning roadmap with acceptance checks and open decisions.
- Ignore rules for build output, local credentials, device configuration and diagnostic captures.

The user reported the preceding serial/OLED firmware working after their own flash. BLE reception and the new scan screen remain hardware-unverified; no Home Assistant deployment has been performed.

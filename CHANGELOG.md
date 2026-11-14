# Changelog

Notable implemented changes are recorded here. Future work belongs in [the roadmap](docs/roadmap.md).

## Unreleased

### Added

- Minimal PlatformIO/Arduino firmware for the confirmed Heltec WiFi LoRa 32 V3 target, with serial heartbeat, OLED status and uptime, explicit display power/reset handling, and serial fallback when OLED initialization fails.
- Pinned build dependencies and build, upload and serial-monitor instructions for a later hardware test.
- Project README describing the Heltec V3 hardware, learning goals and proposed BLE-to-MQTT-to-Home-Assistant approach.
- Repository working agreement covering discussion before edits, changelog maintenance, privacy and validation.
- Staged learning roadmap with acceptance checks and open decisions.
- Ignore rules for build output, local credentials, device configuration and diagnostic captures.

No flashing, physical hardware validation or deployment has been performed.

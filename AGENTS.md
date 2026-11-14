# Working on ESP32 BLE Tracker for Home Assistant

## Agree the change before editing

- Inspect relevant material read-only, then explain the proposed change, scope and meaningful effects.
- Wait for the user to naturally agree or refine the proposal before changing files or configuration. Investigation does not authorize implementation.
- Keep this conversational: present the proposal and wait, without a formal approval ritual or repeated reminders.
- Once agreed, complete routine steps within that scope without asking again. Discuss material scope changes before implementing them.
- Do not commit, push, publish, flash firmware, deploy to Home Assistant or restart services unless included in the agreed scope. The user plans to publish this repository themselves.

## Project and learning priorities

- This is a learning project for custom ESP32 firmware that observes Bluetooth Low Energy advertisements and sends selected observations to Home Assistant.
- Target hardware: Heltec WiFi LoRa 32 V3 with ESP32-S3 and onboard OLED. Verify the exact board revision and official pin definitions before implementing hardware support.
- Development stack: PlatformIO with Arduino and C++. The firmware implements serial/OLED status and passive BLE scanning with bounded local diagnostics and local IRK-based iPhone last-seen presence.
- Build in small, explainable stages: serial/OLED bring-up, BLE scanning, selected-device presence, then Wi-Fi/MQTT and Home Assistant discovery.
- Explain important choices and tradeoffs. Prefer understandable modules over a large framework or copying an existing tracker wholesale.
- LoRa, Bluetooth proxying, active GATT connections and room-level positioning are outside the initial scope.
- Read README.md, CHANGELOG.md and docs/roadmap.md before making changes. Keep them aligned with actual implementation.

## Development and privacy

- Use local files, terminal tools and APIs. Do not use Computer Use or ask the user to enable it.
- Keep Wi-Fi/MQTT credentials, IRKs, real device identifiers, captures and installation-specific configuration out of version control. The local Remote IRK is in ignored `include/config.local.h`; never print it or include firmware binaries in shared artifacts. Provide placeholder examples when configuration is introduced.
- Scan results can contain nearby people's device identifiers. Keep diagnostic output deliberate, bounded and local by default; publish only explicitly selected devices to MQTT.
- Do not equate an observed BLE address with a stable personal identity. Account for rotating addresses and devices that do not advertise consistently.
- Treat RSSI as signal strength, not a reliable distance measurement. A missed advertisement does not immediately establish absence.
- Use current official hardware, library and Home Assistant documentation. Verify board pins, display power/reset behavior and dependency compatibility before implementation.
- Keep scan callbacks short and memory use bounded. Validate Wi-Fi/BLE coexistence on the actual board when that stage is reached.
- The dashboard is HTTPS-only. Observation endpoints are read-only; settings writes require authentication, a per-boot CSRF token and full schema/size validation before NVS replacement. Keep every data/asset/settings endpoint authenticated; never add plaintext HTTP or an open configuration hotspot as a fallback. Wi-Fi/login configuration and TLS keys live in ignored `include/config.local.*` and `.local-network/`. Preserve user edits to those files and never print their contents.
- The pinned Arduino 3.3.12 uses NimBLE. Convert its native little-endian addresses before IRK resolution and use GAP callbacks directly to avoid the bundled C++ scanner's uncapped result map. Publish completed snapshots from the main loop; HTTPS handlers must not read live BLE/phone state.

## Changelog and validation

- Update CHANGELOG.md under Unreleased for notable implemented features, fixes, behavior changes and removals. Record plans in the roadmap, not as completed changelog entries.
- Keep release versions and dates for an explicitly agreed release; do not imply publication or deployment without evidence.
- Run checks appropriate to the final change. Documentation-only work needs link, command and whitespace checks; it does not need firmware tests.
- Once firmware exists, compile for the exact target and add meaningful tests for pure presence/timing/message logic where useful. Do not add tests that only mirror implementation.
- Distinguish documentation checks, successful compilation, flashing, real BLE reception, MQTT delivery and Home Assistant entity behavior. Success at one stage does not prove the next.
- Review final changes for secrets, generated files and unrelated edits. Report what was checked and any unavailable hardware validation.

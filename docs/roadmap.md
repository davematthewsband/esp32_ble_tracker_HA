# Learning roadmap

Stages below distinguish implemented software from future work and physical acceptance. Agree each implementation scope in conversation before editing. Keep the first experiments small enough to explain and troubleshoot independently.

## 1. Board bring-up

Implemented: the user confirmed `wifi_lora_32_v3`; a PlatformIO/Arduino project targets that board, with serial output and OLED uptime using official V3 display pins, reset and power control. Local compilation passed on 14 November 2026. A finer PCB revision is not established. The user subsequently reported the serial/OLED firmware working after flashing. This is user-reported bring-up acceptance, separate from BLE validation.

Acceptance: the project compiles for the selected board; after an agreed flash, the physical board shows the expected serial and OLED output. Record actual dependency versions and reproducible build/upload instructions.

The OLED now has a timed startup title, initialization progress and Phone → Wi-Fi → System pages controlled by PRG. Host button tests cover bounce, holds, initial held state and rollover; physical startup/page navigation and network-state display acceptance remain pending.

## 2. Observe BLE advertisements

Implemented in software: passive asynchronous five-second scans, a fixed 32-entry table, local name/address/type/RSSI diagnostics and OLED scan counters. The bundled Arduino BLE library is used without its internal accumulating results map. Local compilation passed on 14 November 2026. Real BLE reception and physical acceptance remain pending.

Acceptance: the board receives real advertisements; a known test device can be identified from evidence; serial and display remain responsive. Keep raw observations out of version control.

## 3. Track one selected device

Implemented in software: selected iPhone recognition using a locally configured macOS Remote IRK, AES RPA resolution, configurable 120-second timeout and explicit unknown/present/not-seen states on OLED and serial. Resolution runs before the scan diagnostic table limit. The public Bluetooth D.7 vector and host timing/failure/rollover tests pass; real iPhone matching, timeout behavior and address rotation still need hardware verification.

Phone statistics retain the latest matching rotating MAC, historical RSSI, last-seen age and uptime, total matching reports and observed address transitions. Host tests cover repeated reports, address transitions, coherent snapshots and 64-bit detection uptime. Physical screen/serial verification remains pending.

Acceptance: detection updates last-seen, brief gaps do not cause immediate absence, and the configured timeout produces the expected transition. Verify statistics across real address changes without treating an observed transition count as a count of unique addresses. Check timer rollover and matching logic with meaningful local tests where practical, then verify against the device.

## 4. Send observations over MQTT

Before MQTT, the agreed networking stages are:

1. Implemented in software: station-only secure Wi-Fi, authenticated HTTPS observation dashboard, private local credentials/certificate, bounded strongest-first observations and separate Wi-Fi/BLE signal strengths. The dependency upgrade changes the BLE backend to native NimBLE GAP callbacks. Software checks are separate from physical coexistence and TLS acceptance; see [network dashboard](network-dashboard.md).
2. Implemented in software: editable scanner/device names/manual rooms and phone timeout, persisted in NVS with validated versioned JSON import/export. Writes require authenticated HTTPS and a per-boot CSRF token. Exports exclude network/login passwords, TLS keys and IRKs. Full flash erase can remove NVS settings. Physical save/reboot/upload persistence and import acceptance remain pending.

Introduce local Wi-Fi and broker configuration with placeholder examples. Publish only the selected device's presence and RSSI at a bounded rate. Plan reconnect backoff, broker availability and stale-state handling without blocking scans.

Acceptance: the broker receives the documented payloads; Wi-Fi and MQTT recover after interruption; BLE reception continues during network activity on this board. Check memory use and avoid treating network loss as evidence that the tracked device left.

## 5. Integrate with Home Assistant

Choose suitable MQTT entities, add stable discovery identifiers, and distinguish scanner availability from selected-device presence. Handle discovery after Home Assistant restarts and document retained-message behavior and removal.

Acceptance: entities appear through MQTT discovery, reflect real observations and timeouts, recover after restarts, and show scanner outages appropriately. Record actual Home Assistant verification separately from firmware compilation and broker tests.

## Decisions to make when relevant

- First target: iPhone using Remote IRK; validate real advertisements and tune the timeout.
- Exact board revision and USB connection/upload details.
- OLED: pinned Adafruit libraries; BLE: bundled Arduino core library. Reassess dependencies when requirements change.
- Existing MQTT broker availability and connection requirements.
- Presence timeout, publication cadence and OLED contents based on observations.

LoRa, active GATT connections, Bluetooth proxying and multi-scanner room estimation are outside these initial stages.

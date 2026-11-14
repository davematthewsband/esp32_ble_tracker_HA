# Learning roadmap

Stages below distinguish implemented software from future work and physical acceptance. Agree each implementation scope in conversation before editing. Keep the first experiments small enough to explain and troubleshoot independently.

## 1. Board bring-up

Implemented: the user confirmed `wifi_lora_32_v3`; a PlatformIO/Arduino project targets that board, with serial output and OLED uptime using official V3 display pins, reset and power control. Local compilation passed on 14 November 2026. A finer PCB revision is not established. Flashing and physical acceptance remain pending.

Acceptance: the project compiles for the selected board; after an agreed flash, the physical board shows the expected serial and OLED output. Record actual dependency versions and reproducible build/upload instructions.

## 2. Observe BLE advertisements

Add bounded BLE scanning and local diagnostics for available names, addresses, address types and RSSI. Explain which fields are advertised and which are absent. Keep scan callbacks short and avoid an ever-growing device list.

Acceptance: the board receives real advertisements; a known test device can be identified from evidence; serial and display remain responsive. Keep raw observations out of version control.

## 3. Track one selected device

Choose a target and matching rule based on its actual advertisements. Implement last-seen timing and a configurable absence timeout, with explicit startup/unknown behavior. Explain limitations from address rotation, advertising intervals and signal loss.

Acceptance: detection updates last-seen, brief gaps do not cause immediate absence, and the configured timeout produces the expected transition. Check timer rollover and matching logic with meaningful local tests where practical, then verify against the device.

## 4. Send observations over MQTT

Introduce local Wi-Fi and broker configuration with placeholder examples. Publish only the selected device's presence and RSSI at a bounded rate. Plan reconnect backoff, broker availability and stale-state handling without blocking scans.

Acceptance: the broker receives the documented payloads; Wi-Fi and MQTT recover after interruption; BLE reception continues during network activity on this board. Check memory use and avoid treating network loss as evidence that the tracked device left.

## 5. Integrate with Home Assistant

Choose suitable MQTT entities, add stable discovery identifiers, and distinguish scanner availability from selected-device presence. Handle discovery after Home Assistant restarts and document retained-message behavior and removal.

Acceptance: entities appear through MQTT discovery, reflect real observations and timeouts, recover after restarts, and show scanner outages appropriately. Record actual Home Assistant verification separately from firmware compilation and broker tests.

## Decisions to make when relevant

- First target: beacon, phone or sensor, and the tracking behavior desired.
- Exact board revision and USB connection/upload details.
- BLE and OLED libraries after checking current compatibility.
- Existing MQTT broker availability and connection requirements.
- Presence timeout, publication cadence and OLED contents based on observations.

LoRa, active GATT connections, Bluetooth proxying and multi-scanner room estimation are outside these initial stages.

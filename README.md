# ESP32 BLE Tracker for Home Assistant

A personal learning project: build small pieces of custom ESP32 firmware to understand how Bluetooth Low Energy (BLE) observations can become presence information in Home Assistant.

Existing projects are useful references, but the aim here is to understand and implement the steps ourselves using established hardware and networking libraries.

## Current status

Documentation scaffold only. There is no firmware, PlatformIO configuration or Home Assistant configuration yet. Nothing has been flashed or hardware-tested by this project.

## Hardware

- **Heltec WiFi LoRa 32 V3**: ESP32-S3, Wi-Fi, BLE and onboard 0.96-inch OLED.
- The OLED will show local scan and connection status as the project develops.
- The LoRa radio is not part of the initial plan.

Verify the exact board revision before selecting pins or display initialization. See the [official Heltec product information](https://heltec.org/project/wifi-lora-32-v3/).

## Intended approach

Use **C++ with the Arduino framework in PlatformIO**, adding one working piece at a time:

1. Confirm serial output and OLED operation.
2. Observe BLE advertisements locally: available names, identifiers and RSSI.
3. Select one device and derive presence from recent observations and a timeout.
4. Connect to Wi-Fi and publish selected-device presence and signal strength through MQTT.
5. Add Home Assistant MQTT discovery and verify reconnect and availability behavior.

Intended data flow:

```text
Selected BLE advertiser → Heltec scanner → Wi-Fi → MQTT broker → Home Assistant
                              ↓
                         Serial / OLED
```

An MQTT broker and Home Assistant's MQTT integration will be needed for the network stage. Their setup is not implemented here yet. See [Home Assistant MQTT](https://www.home-assistant.io/integrations/mqtt/) and [MQTT device tracking](https://www.home-assistant.io/integrations/device_tracker.mqtt/).

## What “tracking” means initially

The first goal is whether a selected advertiser has been seen recently by one scanner. It is not precise positioning. RSSI varies with orientation, walls and interference; it should not be presented as an accurate distance.

The first target device is still to be chosen. A beacon, phone or sensor may advertise differently, and rotating BLE addresses can make address-only tracking unsuitable. We will inspect real observations before choosing a matching rule.

## Configuration and privacy

Credentials, real tracked-device identifiers and raw scan captures belong in local ignored files. Future configuration examples will use placeholders. MQTT output should be limited to deliberately selected devices.

## Project documents

- [Working agreement](AGENTS.md): discuss changes before implementation and validate each agreed stage.
- [Learning roadmap](docs/roadmap.md): proposed experiments and their acceptance checks.
- [Changelog](CHANGELOG.md): completed notable changes.

The user will initialise and publish the Git repository when ready. There are no build or flashing commands to run at this stage.

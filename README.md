# ESP32 BLE Tracker for Home Assistant

A personal learning project: build small pieces of custom ESP32 firmware to understand how Bluetooth Low Energy (BLE) observations can become presence information in Home Assistant.

Existing projects are useful references, but the aim here is to understand and implement the steps ourselves using established hardware and networking libraries.

## Current status

Initial PlatformIO firmware implements serial output and an OLED status screen with a one-second uptime update. BLE scanning, Wi-Fi, MQTT and Home Assistant configuration are future stages. Nothing has been flashed or hardware-tested by this project.

Local compilation passed on 14 November 2026 using PlatformIO Core 6.2.0 and the pinned dependencies below. Build-reported static RAM use is 20,464 bytes (6.2%); application flash use is 302,141 bytes (9.0% of the configured application partition). These figures do not measure runtime heap or prove hardware operation.

## Hardware

- **Heltec WiFi LoRa 32 V3**: ESP32-S3, Wi-Fi, BLE and onboard 0.96-inch OLED.
- The OLED will show local scan and connection status as the project develops.
- The LoRa radio is not part of the initial plan.

The user confirmed the board marking as `wifi_lora_32_v3`. This project targets that V3 board; no finer PCB revision has been established. See the [official Heltec product information](https://heltec.org/project/wifi-lora-32-v3/).

## Build and first hardware check

Use PlatformIO Core (`pio`) or the PlatformIO VS Code extension. On this Mac the existing executable is `/Users/finnmainstone/.platformio/penv/bin/platformio`; substitute that path if `pio` is not on your PATH.

```sh
pio run -e heltec_v3
```

The project pins Espressif32 platform **6.3.2** (the locally installed baseline, Arduino core 2.0.9), Adafruit SSD1306 **2.5.17**, Adafruit GFX **1.13.0** and Adafruit BusIO **1.17.4**. This platform is deliberately an older baseline, not a claim to use the latest release. The first build may need internet access to download dependencies.

For an isolated PlatformIO tool cache inside this ignored project directory, prefix commands with `PLATFORMIO_CORE_DIR="$PWD/.pio-core"`.

The following commands are for a later, agreed hardware test. Uploading replaces the current firmware; no upload has been performed here. Connect a USB data cable, list ports, then replace `YOUR_PORT` with the board's port:

```sh
pio device list
pio run -e heltec_v3 -t upload --upload-port YOUR_PORT
pio device monitor --port YOUR_PORT --baud 115200
```

The firmware uses the board's UART serial connection at 115200 baud. Expect a repeating `Heltec V3 bring-up` line and an OLED showing `Heltec WiFi LoRa V3`, `Serial / OLED test` and advancing uptime. Exit the monitor with Ctrl+C. If automatic upload fails, hold PRG/BOOT, tap RESET, release PRG/BOOT and retry. Close any serial monitor before uploading.

The OLED uses I2C SDA **17**, SCL **18**, reset **21**, address **0x3C**, and active-low Vext power **36**. These come from [Heltec's V3 pin definitions](https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series/blob/master/variants/heltec_wifi_lora_32_V3/pins_arduino.h) and [display initialization](https://github.com/HelTecAutomation/Heltec_ESP32/blob/master/src/heltec.cpp). Generic `SDA`/`SCL` are different pins on this board. Startup enables display power, resets the controller and checks its I2C response before allocating the display buffer. A missing display leaves serial heartbeat output running with `OLED unavailable`; initialization does not prove visible screen output. After resolving a display connection issue, reset the board to retry initialization.

`setup()` initializes the hardware once. `loop()` refreshes the status once per second without waiting for a serial monitor. The display driver handles pixels and I2C commands; the application owns power/reset sequencing and the status content.

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

The repository has an initial Git commit. The user manages subsequent commits and publication.

# ESP32 BLE Tracker for Home Assistant

A personal learning project: build small pieces of custom ESP32 firmware to understand how Bluetooth Low Energy (BLE) observations can become presence information in Home Assistant.

Existing projects are useful references, but the aim here is to understand and implement the steps ourselves using established hardware and networking libraries.

## Current status

The firmware implements passive BLE scanning, local serial diagnostics and an OLED scan status screen refreshed once per second. Wi-Fi, MQTT and selected-device presence are future stages. The user reported that the preceding serial/OLED firmware worked after flashing; BLE reception has not yet been hardware-verified.

The serial/OLED baseline compiled locally on 14 November 2026 using PlatformIO Core 6.2.0 and the pinned dependencies below. The BLE stage also compiled successfully on that date: build-reported static RAM is 46,408 bytes (14.2%), and application flash is 910,069 bytes (27.2% of the application partition). These figures do not measure runtime heap, radio reception or runtime stability.

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

The following commands are for a hardware test performed by the user or separately agreed in chat. Uploading replaces the current firmware. The user flashed the earlier serial/OLED stage; the BLE stage has not been uploaded by the assistant. Connect a USB data cable, list ports, then replace `YOUR_PORT` with the board's port:

Use a known data-capable USB-A to USB-C cable for bring-up: USB-C to USB-C provided neither power nor data in the user's tested setup. See [USB cable learnings](docs/learnings.md#usb-cable-choice) for the observation and its limits.

```sh
pio device list
pio run -e heltec_v3 -t upload --upload-port YOUR_PORT
pio device monitor --port YOUR_PORT --baud 115200
```

The firmware uses the board's UART serial connection at 115200 baud. Expect a repeating `Heltec V3` status line and an OLED showing BLE status, completed scans, report count, the number of address/type entries retained in the previous round, and uptime. Exit the monitor with Ctrl+C. If automatic upload fails, hold PRG/BOOT, tap RESET, release PRG/BOOT and retry. Close any serial monitor before uploading.

The OLED uses I2C SDA **17**, SCL **18**, reset **21**, address **0x3C**, and active-low Vext power **36**. These come from [Heltec's V3 pin definitions](https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series/blob/master/variants/heltec_wifi_lora_32_V3/pins_arduino.h) and [display initialization](https://github.com/HelTecAutomation/Heltec_ESP32/blob/master/src/heltec.cpp). Generic `SDA`/`SCL` are different pins on this board. Startup enables display power, resets the controller and checks its I2C response before allocating the display buffer. A missing display leaves serial heartbeat output running with `OLED unavailable`; initialization does not prove visible screen output. After resolving a display connection issue, reset the board to retry initialization.

`setup()` initializes the hardware once. `loop()` polls the asynchronous scanner and refreshes the status once per second without waiting for a serial monitor. The display driver handles pixels and I2C commands; the application owns power/reset sequencing and the status content.

## BLE scanning experiment

The scanner uses the BLE library bundled with the pinned Arduino core; no additional BLE dependency is needed. See [Espressif’s scanner implementation for core 2.0.9](https://github.com/espressif/arduino-esp32/blob/2.0.9/libraries/BLE/src/BLEScan.cpp).

- Passive five-second rounds listen for legacy BLE advertisements without sending scan requests or making connections. Scan interval is 100 ms; listening window is 80 ms.
- The callback counts reports and retains at most **32 address/type pairs per round**. Repeat reports update RSSI and any advertised name. A fixed table is reused each round; the library’s duplicate-report mode avoids its internal growing result map.
- Completion hands the table back to the main loop, which prints one entry per loop iteration before starting the next round. The OLED continues updating while scans run. There is a short listening gap while the previous round is printed.
- Serial entries show address, address type, most recently observed RSSI in dBm and any name observed during that round. Names are limited to 32 bytes and non-ASCII/control characters are replaced with `?`. Raw manufacturer/service payloads are not logged.
- `Reports` counts received callback reports in the current or just-completed round, including repeats. `Last kept` is the previous round’s retained address/type count, capped at 32; neither is a count of people or stable physical devices. Reports from addresses outside a full table are counted separately in serial output.
- A scan start failure retries after two seconds. If a scan fails to complete after ten seconds, the OLED shows `stalled`; reset the board if it persists. The firmware avoids attempting a potentially blocking restart while the stack still owns a scan.

Many advertisers have no name; passive scanning also misses names available only in a scan response. Phones may rotate addresses or stop advertising. This experiment cannot determine presence reliably yet, and RSSI is not a distance estimate.

To check on hardware after uploading this stage: keep only the V3 connected, open the 115200-baud monitor, and watch several rounds. Confirm that the completed-scan count and uptime advance, serial summaries arrive about every five seconds, and a known advertiser can be identified when nearby. More than 32 address/type pairs must not increase the retained table size. Leave it running for several minutes and check for resets or stalls. An empty scan is a valid result, not proof that no Bluetooth devices are nearby. Raw serial output contains nearby device identifiers: keep it local and out of Git.

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
- [Project learnings](docs/learnings.md): practical observations and troubleshooting notes.
- [Changelog](CHANGELOG.md): completed notable changes.

The repository has an initial Git commit. The user manages subsequent commits and publication.

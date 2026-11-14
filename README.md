# ESP32 BLE Tracker for Home Assistant

A personal learning project: build small pieces of custom ESP32 firmware to understand how Bluetooth Low Energy (BLE) observations can become presence information in Home Assistant.

Existing projects are useful references, but the aim here is to understand and implement the steps ourselves using established hardware and networking libraries.

## Current status

The firmware implements passive BLE scanning, local serial diagnostics and an OLED scan status screen refreshed once per second. Local IRK-based iPhone presence and an authenticated HTTPS dashboard, saved labels/manual rooms and configuration backup/import are implemented in software. MQTT is a future stage. The user reported that the preceding serial/OLED firmware worked after flashing; Earlier live serial inspection showed fresh IRK matches; the upgraded BLE backend and Wi-Fi/HTTPS stage still need physical verification.

The serial/OLED baseline compiled locally on 14 November 2026 using PlatformIO Core 6.2.0 and the pinned dependencies below. The earlier IRK stage compiled on that date. The new network stage is compiled separately with private local configuration and with an enabled-network fixture. Static build usage does not measure TLS handshake heap or prove radio reception/runtime stability.

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

The project pins Espressif32 platform **8.0.0** (Arduino core 3.3.12 / ESP-IDF 5.5.5), Adafruit SSD1306 **2.5.17**, Adafruit GFX **1.13.0** and Adafruit BusIO **1.17.4**. The network stage upgrades the earlier Arduino 2.0.9 baseline; BLE backend and network hardware verification must be repeated. The first build may need internet access to download dependencies.

For an isolated PlatformIO tool cache inside this ignored project directory, prefix commands with `PLATFORMIO_CORE_DIR="$PWD/.pio-core"`.

The following commands are for a hardware test performed by the user or separately agreed in chat. Uploading replaces the current firmware. The user flashed the earlier serial/OLED stage; the BLE/IRK stages have not been uploaded by the assistant. Connect a USB data cable, list ports, then replace `YOUR_PORT` with the board's port:

Use a known data-capable USB-A to USB-C cable for bring-up: USB-C to USB-C provided neither power nor data in the user's tested setup. See [USB cable learnings](docs/learnings.md#usb-cable-choice) for the observation and its limits.

```sh
pio device list
pio run -e heltec_v3 -t upload --upload-port YOUR_PORT
pio device monitor --port YOUR_PORT --baud 115200
```

The firmware uses the board's UART serial connection at 115200 baud. Expect a repeating `Heltec V3` status line and an OLED showing BLE status, completed scans (`S`), report count (`R`), iPhone status, the latest matching rotating MAC, last-seen age and last matching RSSI. Scanner uptime and the full phone statistics are available on serial. Exit the monitor with Ctrl+C. If automatic upload fails, hold PRG/BOOT, tap RESET, release PRG/BOOT and retry. Close any serial monitor before uploading.

The OLED uses I2C SDA **17**, SCL **18**, reset **21**, address **0x3C**, and active-low Vext power **36**. These come from [Heltec's V3 pin definitions](https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series/blob/master/variants/heltec_wifi_lora_32_V3/pins_arduino.h) and [display initialization](https://github.com/HelTecAutomation/Heltec_ESP32/blob/master/src/heltec.cpp). Generic `SDA`/`SCL` are different pins on this board. Startup enables display power, resets the controller and checks its I2C response before allocating the display buffer. A missing display leaves serial heartbeat output running with `OLED unavailable`; initialization does not prove visible screen output. After resolving a display connection issue, reset the board to retry initialization.

`setup()` initializes the hardware once. `loop()` polls the asynchronous scanner and refreshes the status once per second without waiting for a serial monitor. The display driver handles pixels and I2C commands; the application owns power/reset sequencing and the status content.

## BLE scanning experiment

The scanner uses the BLE library bundled with the pinned Arduino core; no additional BLE dependency is needed. See [Espressif’s BLE implementation for core 3.3.12](https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/BLE/src/BLEScan.cpp).

- Passive five-second rounds listen for legacy BLE advertisements without sending scan requests or making connections. Scan interval is 100 ms; listening window is 80 ms.
- The callback counts reports and retains at most **32 address/type pairs per round**. Repeat reports update RSSI and any advertised name. A fixed table is reused each round; native NimBLE GAP callbacks avoid the C++ wrapper’s internal growing result map. Native address byte order is converted before IRK matching.
- Completion hands the table back to the main loop, which prints one entry per loop iteration before starting the next round. The OLED continues updating while scans run. There is a short listening gap while the previous round is printed.
- Serial entries show address, address type, most recently observed RSSI in dBm and any name observed during that round. Names are limited to 32 bytes and non-ASCII/control characters are replaced with `?`. Raw manufacturer/service payloads are not logged.
- `Reports` counts received callback reports in the current or just-completed round, including repeats. The serial summary’s retained address/type count is capped at 32; neither is a count of people or stable physical devices. Reports from addresses outside a full table are counted separately in serial output.
- A scan start failure retries after two seconds. If a scan fails to complete after ten seconds, the OLED shows `stalled`; reset the board if it persists. The firmware avoids attempting a potentially blocking restart while the stack still owns a scan.

Many advertisers have no name; passive scanning also misses names available only in a scan response. Phones may rotate addresses or stop advertising. The generic scan list alone does not identify a phone across address rotation; IRK matching below adds a selected-device last-seen estimate. RSSI is not a distance estimate.

To check on hardware after uploading this stage: keep only the V3 connected, open the 115200-baud monitor, and watch several rounds. Confirm that the completed-scan count and uptime advance, serial summaries arrive about every five seconds, and a known advertiser can be identified when nearby. More than 32 address/type pairs must not increase the retained table size. Leave it running for several minutes and check for resets or stalls. An empty scan is a valid result, not proof that no Bluetooth devices are nearby. Raw serial output contains nearby device identifiers: keep it local and out of Git.

## iPhone presence with a Remote IRK

`include/config.local.h` holds this installation’s key and timeout; it is ignored by Git. The supplied key has been placed there locally. For a fresh checkout, copy [the placeholder example](include/config.example.h) to that filename and set `IPHONE_IRK_BASE64` to the macOS plist **Remote IRK** base64 data only, without XML or whitespace. `IPHONE_TIMEOUT_MS` defaults to `120000` (120 seconds). Rebuild and upload after editing it. Without a valid 16-byte key the scanner still works, but phone status stays unknown and serial startup reports tracking disabled.

The key is decoded from base64 and its bytes reversed to standard AES order, matching [Home Assistant’s macOS/base64 conversion](https://github.com/home-assistant/core/blob/dev/homeassistant/components/private_ble_device/config_flow.py). This format is specific to the macOS Remote IRK representation; do not paste a hex key or automatically try both key orders.

For each unresolved random address with RPA bits `01`, the resolver encrypts thirteen zero bytes plus the address’s three-byte `prand` using AES-128, then compares the last three ciphertext bytes with the address hash. See [Bluetooth Core v5.4, Vol 3 Part H, sections 2.2.2 and D.7](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host/security-manager-specification.html). The firmware uses the bundled mbedTLS AES backend and checks the published D.7 vector at startup before enabling tracking. Matching runs before the diagnostic table limit, so a full 32-entry list does not exclude the target. A fixed-size protected statistics snapshot crosses into the main loop: latest matching MAC/timestamp/RSSI plus cumulative match and address-transition counters. Every verified callback updates the counters before snapshotting, even when several arrive between loop iterations.

Phone status appears on both serial and the OLED:

| State | Meaning |
| --- | --- |
| `unknown` | No match since startup, missing/invalid key, failed crypto self-test, or scanner failure/stall. |
| `present` | A matching RPA was received less than the configured timeout ago. |
| `not seen recently` | A previous match has aged past the timeout while scanning is healthy. The OLED abbreviates this as `not seen`. |

The OLED uses eight text rows: board, BLE status, scan/report counts, phone state, `Last MAC (rotating):`, the address, last-seen age, and `Last RSSI`. Before any match the MAC/RSSI show `--` and last-seen shows `never`. The MAC is the latest address actually matched by the IRK; it is not a prediction of the next address or a permanent hardware address.

Serial phone status additionally reports **uptime at last detection**, **matching advertisement reports**, and **observed address changes**. The first address does not count as a change; a transition to any different matching address increments it, including a return to an earlier address. Repeat reports on the same address still increment the match count and refresh RSSI/time. These are reports delivered to the scanner callback, not an estimate of every packet sent by the phone. Counts reset on reboot and do not retain address history. Detection uptime uses the ESP32’s 64-bit monotonic timer, so it survives the normal 32-bit `millis()` rollover; it is not wall-clock time.

The latest matching MAC, last-seen age and **last matching** RSSI remain historical values after a timeout or scanner failure. A fresh match resets the age. Scanner recovery does not refresh an old match. The age uses accumulated 64-bit elapsed time; normal `millis()` rollover does not make an expired phone present again. State is not persisted over reset: startup returns to unknown until a match arrives.

This recognizes suitable advertisements when received; it cannot prove that Bluetooth is off, that the phone has left, or which room it occupies. iOS may stop advertising or use addresses that cannot be resolved by this key. The hash is only 24 bits, so a match is not cryptographic authentication or an absolute identity guarantee. Address-rotation timing is not used in the logic. No pairing or active GATT connection is introduced. Wi-Fi serves the observation dashboard and protected settings editor below; MQTT remains unimplemented.

The IRK is never printed. It is compiled into the firmware, so treat `.pio/` binaries and the local config as private; ignore rules are not firmware encryption. Generic scan diagnostics still print nearby addresses/names as before.

### Validation and hardware trial

On the development Mac, run:

```sh
python3 tests/run_tests.py
PLATFORMIO_CORE_DIR="$PWD/.pio-core" /Users/finnmainstone/.platformio/penv/bin/platformio run -e heltec_v3
```

The macOS tests use Apple’s CommonCrypto AES backend with the same pure resolver and timing code. They cover the published vector, macOS key byte order, invalid configuration, wrong keys/hashes/address types, exact timeout boundaries, scanner failure/recovery, timer rollover, match counts, address transitions, coherent snapshots and 64-bit detection uptime, with address/undefined-behavior sanitizers. The firmware backend is compiled here; its startup self-test executes on the ESP32 after upload. Passing host tests does not validate the supplied key against this iPhone.

After the user uploads: check serial startup for `iPhone tracker configured`, keep the phone nearby with Bluetooth enabled, and look for `present`, a recent last-seen age and RSSI. If it stays unknown, check advertising/phone state and key provenance before changing the resolver. Then take the phone out of range or turn Bluetooth off in iOS Settings, wait at least 120 seconds after the last match and check `not seen`. Return it and confirm a new match restores `present`. Confirm the latest matching MAC and RSSI appear on both OLED and serial, repeated matches increase the serial counter, and last-detection uptime agrees with last-seen age. Observe across actual address rotation to establish that recognition survives it and that the transition counter increases. When observations stop, confirm that the MAC and last RSSI remain historical values while last-seen age increases. Keep diagnostic captures local. These hardware checks remain pending.

## Wi-Fi, local dashboard and saved settings

The networking stage serves observations over authenticated HTTPS and provides explicit protected settings saves. It shows Wi-Fi RSSI separately from BLE RSSI, a strongest-first observation list, phone statistics and scanner/memory health. It uses private credentials and a unique local certificate; there is no open setup hotspot or HTTP fallback.

See [network setup, certificate trust and validation](docs/network-dashboard.md). The private SSID is configured locally; fill the Wi-Fi password and keep the dashboard password at least 24 characters in ignored `include/config.local.network.h`. Rebuild/upload after editing. Networking stays disabled if settings are incomplete or invalid.

Name observed devices, assign manual room labels, edit the scanner name/room and phone timeout, and download/import a versioned settings JSON file. These settings are saved in ESP32 NVS and normally survive firmware uploads. Full erase can remove them. Exports exclude credentials, TLS material and IRKs; one scanner cannot infer the correct room from RSSI alone.

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

An MQTT broker and Home Assistant's MQTT integration will be needed for the later MQTT stage. Their setup is not implemented here yet. See [Home Assistant MQTT](https://www.home-assistant.io/integrations/mqtt/) and [MQTT device tracking](https://www.home-assistant.io/integrations/device_tracker.mqtt/).

## What “tracking” means initially

The first goal is whether a selected advertiser has been seen recently by one scanner. It is not precise positioning. RSSI varies with orientation, walls and interference; it should not be presented as an accurate distance.

The first selected target is the user’s iPhone, recognized through its Remote IRK rather than a fixed BLE address. Other devices may require different matching rules.

## Configuration and privacy

Credentials, real tracked-device identifiers and raw scan captures belong in local ignored files. Configuration examples use placeholders. MQTT output should be limited to deliberately selected devices.

## Project documents

- [Working agreement](AGENTS.md): discuss changes before implementation and validate each agreed stage.
- [Learning roadmap](docs/roadmap.md): proposed experiments and their acceptance checks.
- [Network dashboard](docs/network-dashboard.md): private setup, certificate trust, security boundaries and hardware checks.
- [Project learnings](docs/learnings.md): practical observations and troubleshooting notes.
- [Changelog](CHANGELOG.md): completed notable changes.

The repository has an initial Git commit. The user manages subsequent commits and publication.

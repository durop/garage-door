# Garage Door Controller – ESP32-C3 + MQTT + Home Assistant

A DIY garage door opener/closer using an **ESP32-C3 Mini**, a **reed switch**, and a
**relay module**, connected to **Home Assistant** via MQTT auto-discovery.

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20me%20a%20coffee-support-yellow?logo=buymeacoffee&logoColor=black)](https://buymeacoffee.com/durop)

If you open the Buy Me a Coffee page on your phone, you can pay with **Google Pay** or **Apple Pay**.

---

## Features

| Feature | Details |
|---|---|
| **Door state** | Reed switch (NO) detects open / closed |
| **Open / Close** | Relay pulses the garage door button |
| **MQTT auto-discovery** | Home Assistant automatically creates the entity |
| **Availability** | HA shows if the device goes offline (last-will) |
| **Attributes** | IP, RSSI, uptime exposed in HA |
| **Debounce** | Reed switch is software-debounced (100 ms) |
| **Periodic re-publish** | State re-sent every 30 s in case HA restarts |
| **Status LED** | Non-blocking diagnostic patterns — see [LED Diagnostics](#led-diagnostics) |
| **OTA Updates** | Upload new firmware over WiFi via ElegantOTA (`http://<device-ip>/update`) |

---

## Hardware

| Component | Purpose |
|---|---|
| ESP32-C3 Mini | Microcontroller with WiFi |
| Reed Switch (NO) | Detects if the garage door is closed |
| 3V 1-Channel Relay (High-Level Trigger) | Simulates pressing the wall button |

---

## Wiring Diagram

```
                        ESP32-C3 Mini
                     ┌───────────────────┐
                     │                   │
    Reed Switch ─────┤ GPIO 4       3V3  ├──── Relay VCC
    (other leg GND)  │                   │
                     │ GND ──────── GND  ├──── Relay GND
                     │                   │
                     │ GPIO 5 ────────── ├──── Relay IN
                     │                   │
                     └───────────────────┘

    Relay COM ──── Garage door wall-button terminal 1
    Relay NO  ──── Garage door wall-button terminal 2
```

### Reed Switch Wiring
```
    Reed Switch (NO = Normally Open)
    ┌─────────┐
    │         │
    ├── Leg 1 ├──── GPIO 4  (uses internal pull-up)
    │         │
    ├── Leg 2 ├──── GND
    │         │
    └─────────┘

    • Mount the MAGNET on the garage door itself.
    • Mount the REED SWITCH on the door frame, aligned with the magnet
      when the door is fully CLOSED.
    • Door CLOSED → magnet near → reed closes → GPIO reads LOW  → "closed"
    • Door OPEN   → magnet away → reed open   → GPIO reads HIGH → "open"
```

### Reed Switch Physical Placement

For a standard overhead sectional garage door, place the sensor at the **bottom**:

```
    Door frame (fixed)        Garage door (moving)
    │                         │
    │  [REED SWITCH] ◄────────► [MAGNET]   ← bottom panel, door CLOSED
    │
    └──── floor
```

| Part | Where to mount |
|---|---|
| **Reed switch** | Fixed **door frame**, near the **bottom corner** |
| **Magnet** | **Bottom door panel**, aligned with the reed switch |

**Magnets together when door is close at the bottom (depends on Door type)**

When the door is fully **closed** the bottom panel sits flush against the frame — that
is the only moment the magnet and reed switch are close enough to trigger.
When the door is **open** (overhead), the bottom panel travels all the way up and the
magnet moves far away from the reed, which is exactly what the firmware expects:

```
Door CLOSED (down)  → bottom panel at frame → magnet near reed → GPIO LOW  → "closed" ✓
Door OPEN (overhead)→ bottom panel up high  → magnet far away  → GPIO HIGH → "open"   ✓
```

### Relay → Garage Door Motor
```
    Your garage door motor has a wall-mount button. That button simply
    shorts two wires together. The relay does the same thing:

    Relay COM ─────── Wire 1 (from wall-button terminal)
    Relay NO  ─────── Wire 2 (from wall-button terminal)

    ⚠️  Do NOT connect the relay to mains/AC power.
        The two wall-button wires are low-voltage (typically 12-24V DC).
        You're wiring the relay IN PARALLEL with your existing wall button.
        The wall button will continue to work normally.
```

---

## LED Diagnostics

The built-in LED (GPIO 8) shows the current system status at a glance using four
non-blocking patterns.  No serial monitor needed — just look at the light:

| LED Pattern | Visual | Meaning |
|---|---|---|
| ♥ **Heartbeat** | `● ● · · · · · · · · ● ● · · · · · · · ·` | **All good** — WiFi ✓ + MQTT ✓ |
| 🔵 **Slow blink** | `●●●●●○○○○○●●●●●○○○○○` (1 s on / 1 s off) | WiFi ✓, **MQTT disconnected** |
| ⚡ **Fast blink** | `●●○○●●○○●●○○●●○○` (150 ms on / 150 ms off) | **WiFi disconnected** |
| ⚫ **Off** | `○○○○○○○○○○○○○○○○○○` | Boot / not initialised yet |

### How to read it

```
Is the LED doing a calm double-pulse every ~3 seconds?
  └─ YES → All good, WiFi + MQTT connected ✓

Is the LED blinking slowly (about once per second)?
  └─ YES → WiFi is connected, but MQTT broker is unreachable
           Check: broker running? correct IP/port/credentials in config.h?

Is the LED blinking very fast?
  └─ YES → WiFi is disconnected
           Check: correct SSID/password? 2.4 GHz network? router reachable?

Is the LED completely off?
  └─ Device is still booting, or something crashed (check serial log)
```

> **Tip:** The heartbeat pattern is deliberately subtle — two quick flashes then a
> long pause.  This makes it instantly distinguishable from the blink patterns and
> also tells you the main loop is still running (not frozen).

---

## Software Setup

### 1. Install PlatformIO

Install the [PlatformIO IDE](https://platformio.org/install/ide?install=vscode)
extension for VS Code, or use the CLI:

```bash
pip install platformio
```

### 2. Configure

Edit **`include/config.h`** with your network details:

```cpp
#define WIFI_SSID         "YourWiFi"
#define WIFI_PASSWORD     "YourPassword"
#define MQTT_HOST         "192.168.1.100"
#define MQTT_PORT         1883
#define MQTT_USER         "mqtt_user"
#define MQTT_PASSWORD     "mqtt_pass"
```

### 3. Build & Upload

```bash
# Build
pio run

# Upload (connect ESP32-C3 via USB)
pio run --target upload

# Monitor serial output
pio device monitor
```

Or in one command:
```bash
pio run --target upload && pio device monitor
```

---

## Home Assistant

**No configuration needed!** 🎉

The device uses MQTT auto-discovery. Once it connects to your MQTT broker,
Home Assistant will automatically create:

- A **Cover** entity named **"Garage Door"** with:
  - Open / Close / Stop buttons
  - State: open or closed
  - Attributes: IP, RSSI, uptime, firmware version

You'll find it under **Settings → Devices & Services → MQTT → Devices**.

### Dashboard Card Example

```yaml
type: entities
title: Garage
entities:
  - entity: cover.garage_door
    name: Garage Door
```

Or use the **Tile card** for a nice big toggle:
```yaml
type: tile
entity: cover.garage_door
name: Garage Door
icon: mdi:garage
```

---

## MQTT Topics

| Topic | Direction | Payload | Retained |
|---|---|---|---|
| `garage_door/state` | Device → HA | `open` / `closed` | ✅ |
| `garage_door/set` | HA → Device | `OPEN` / `CLOSE` / `STOP` | ❌ |
| `garage_door/availability` | Device → HA | `online` / `offline` | ✅ |
| `garage_door/attributes` | Device → HA | JSON | ✅ |
| `homeassistant/cover/garage_door/config` | Device → HA | JSON (discovery) | ✅ |

---

## OTA (Over-the-Air) Updates

After the first USB flash, you can update the firmware wirelessly using **ElegantOTA**.

1. Make sure the ESP32-C3 is powered on and connected to your WiFi.
2. Open a browser and go to: `http://<device-ip>/update`
   (the IP is printed on the serial monitor at boot, and shown in HA device attributes).
3. Select **Firmware**, pick the `.bin` file, and click **Update**.
4. The device will reboot automatically with the new firmware.

To build the `.bin` file without uploading via USB:
```bash
pio run
# Output: .pio/build/esp32c3/firmware.bin
```

You can optionally protect the OTA page with Basic-Auth by setting `OTA_USERNAME` and
`OTA_PASSWORD` in `config.h`.

A simple status page is also available at `http://<device-ip>/` showing the device name
and current firmware version.

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| LED blinks fast | WiFi is down — check SSID/password in `config.h`. Make sure 2.4 GHz (ESP32-C3 doesn't support 5 GHz). |
| LED blinks slowly | WiFi OK but MQTT is down — check broker is running, IP/port/credentials in `config.h`. |
| LED heartbeats but HA has no entity | MQTT is connected but discovery failed — check HA → Settings → Integrations → MQTT. Restart HA. |
| Device won't connect to WiFi | Double-check SSID/password in `config.h`. Make sure 2.4 GHz (ESP32-C3 doesn't support 5 GHz). |
| HA doesn't show the entity | Check MQTT broker is running. Check HA → Settings → Integrations → MQTT is configured. Restart HA. |
| Door state is inverted | If your reed switch is NC (normally closed) instead of NO, flip the logic in `readDoorClosed()`: change `== LOW` to `== HIGH`. |
| Relay clicks but door doesn't move | Verify you wired to the wall-button terminals on the garage motor (low voltage), not the motor power. |
| Relay doesn't click | Confirm the relay is a 3.3V high-level trigger. Check wiring: VCC→3V3, GND→GND, IN→GPIO 5. |

---

## Pin Reference (ESP32-C3 Mini)

| GPIO | Usage | Notes |
|---|---|---|
| 4 | Reed switch input | Internal pull-up enabled |
| 5 | Relay output | HIGH = relay ON |
| 8 | Status LED | Active-low on most C3 Mini boards |

---

## Tests

The project includes native (desktop) unit tests that verify core logic without
requiring hardware.  Tests are written with the
[Unity](https://github.com/ThrowTheSwitch/Unity) test framework and run on your
host machine.

### Running tests with PlatformIO

```bash
pio test -e native
```

### PlatformIO VS Code Test button note

The global **Test** button in the PlatformIO extension may not detect these tests
in this project layout. If that happens, run tests with:

```bash
pio test -e native
```

Or run tests from the **native** environment entry in the PlatformIO panel.

### Running tests without PlatformIO

If you prefer plain g++:

```bash
# Clone Unity (one-time)
git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git /tmp/Unity

# Garage hardware tests
g++ -std=c++17 -Itest/mocks -Iinclude -I/tmp/Unity/src -DUNIT_TEST \
    -o /tmp/test_garage_hardware \
    test/test_garage_hardware/test_main.cpp /tmp/Unity/src/unity.c && \
    /tmp/test_garage_hardware

# State / debounce logic tests
g++ -std=c++17 -I/tmp/Unity/src -DUNIT_TEST \
    -o /tmp/test_state_logic \
    test/test_state_logic/test_main.cpp /tmp/Unity/src/unity.c && \
    /tmp/test_state_logic
```

### Test overview

| Suite | Tests | What it covers |
|---|---|---|
| `test_garage_hardware` | 24 | Pin init, reed-switch reading, relay trigger/cooldown/force, relay pulse management, LED pattern engine (off/solid/fast/slow/heartbeat), blocking blink helper |
| `test_state_logic` | 15 | Door state determination (open/closed/opening/closing), transit arrival detection, debounce algorithm (stable, bounce rejection, timer reset, multi-transition) |

---

## License

MIT — do whatever you want with it. 🏠🚗


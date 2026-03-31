// ============================================================
//  garage_hardware.h — Reed switch, relay & status LED
// ============================================================
#pragma once

#include <Arduino.h>
#include "config.h"

namespace GarageHardware {

    /// LED blink patterns for visual diagnostics
    enum class LedPattern : uint8_t {
        OFF,          // LED always off
        SOLID,        // LED always on  (not used by default; available if needed)
        FAST_BLINK,   // WiFi disconnected        — 150 ms on / 150 ms off
        SLOW_BLINK,   // WiFi OK, MQTT disconnect  — 1 s on / 1 s off
        HEARTBEAT     // All good (WiFi + MQTT)    — two quick flashes, long pause
    };

    void init();

    /// Must be called every loop() — manages non-blocking relay pulse + LED pattern
    void loop();

    /// Set the current LED blink pattern (non-blocking, driven by loop())
    void setLedPattern(LedPattern pattern);

    /// Returns the currently active LED pattern
    LedPattern getLedPattern();

    /// Returns true if the door is closed (magnet near reed switch)
    bool readDoorClosed();

    /// Start a non-blocking relay pulse (returns false if cooldown active).
    /// Pass force=true to bypass the cooldown (used by the STOP command).
    bool triggerRelay(bool force = false);

    /// Returns true if the relay is currently mid-pulse
    bool isRelayBusy();

    /// Quick LED blink helper (blocking — only use during setup,
    /// before the non-blocking pattern engine takes over in loop())
    void blinkLed(int times, int intervalMs);


}  // namespace GarageHardware


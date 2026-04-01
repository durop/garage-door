// ============================================================
//  garage_hardware.cpp — Reed switch, relay & status LED
// ============================================================

#include "garage_hardware.h"

namespace GarageHardware {

// ─── Non-blocking relay state ───────────────────────────────
static bool     _relayActive    = false;
static uint32_t _relayStartTime = 0;
static uint32_t _lastTriggerTime = 0;   // for cooldown

// ─── Non-blocking LED pattern state ─────────────────────────
static LedPattern _ledPattern      = LedPattern::OFF;
static uint32_t   _ledPhaseStart   = 0;   // when the current phase began
static uint8_t    _ledPhaseIndex   = 0;   // which phase of the pattern we're in
static bool       _ledOn           = false;

// Heartbeat timing: ON(100) → OFF(150) → ON(100) → OFF(2500)  ≈ 2.85 s cycle
// Stored as {duration, ledState} pairs.  Phase index wraps around.
struct LedPhase { uint16_t durationMs; bool on; };

static const LedPhase HEARTBEAT_PHASES[] = {
    { LED_HEARTBEAT_PULSE_MS, true  },  // first flash
    { LED_HEARTBEAT_GAP_MS,   false },  // short gap
    { LED_HEARTBEAT_PULSE_MS, true  },  // second flash
    { LED_HEARTBEAT_PAUSE_MS, false },  // long pause
};
static const uint8_t HEARTBEAT_PHASE_COUNT = sizeof(HEARTBEAT_PHASES) / sizeof(HEARTBEAT_PHASES[0]);

static void setLedRaw(bool on) {
    if (on != _ledOn) {
        _ledOn = on;
        digitalWrite(PIN_STATUS_LED, on ? LOW : HIGH);   // active-low
    }
}

void init() {
    // Reset internal state so init() is deterministic across boots and tests.
    _relayActive     = false;
    _relayStartTime  = 0;
    _lastTriggerTime = 0;

    _ledPattern    = LedPattern::OFF;
    _ledPhaseStart = millis();
    _ledPhaseIndex = 0;
    _ledOn         = false;

    pinMode(PIN_REED_SWITCH, INPUT_PULLUP);

    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);        // relay OFF at boot

    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);  // LED off (active-low)
}

void loop() {
    // ── Relay pulse management ──
    if (_relayActive && (millis() - _relayStartTime >= RELAY_PULSE_MS)) {
        digitalWrite(PIN_RELAY, LOW);
        _relayActive = false;
        DEBUG_PRINTLN("Relay OFF (pulse complete)");
    }

    // ── Non-blocking LED pattern engine ──
    uint32_t now = millis();
    uint32_t elapsed = now - _ledPhaseStart;

    switch (_ledPattern) {
        case LedPattern::OFF:
            setLedRaw(false);
            break;

        case LedPattern::SOLID:
            setLedRaw(true);
            break;

        case LedPattern::FAST_BLINK:
            if (elapsed >= LED_FAST_BLINK_MS) {
                _ledPhaseStart = now;
                setLedRaw(!_ledOn);
            }
            break;

        case LedPattern::SLOW_BLINK:
            if (elapsed >= LED_SLOW_BLINK_MS) {
                _ledPhaseStart = now;
                setLedRaw(!_ledOn);
            }
            break;

        case LedPattern::HEARTBEAT: {
            const LedPhase& phase = HEARTBEAT_PHASES[_ledPhaseIndex];
            setLedRaw(phase.on);
            if (elapsed >= phase.durationMs) {
                _ledPhaseIndex = (_ledPhaseIndex + 1) % HEARTBEAT_PHASE_COUNT;
                _ledPhaseStart = now;
            }
            break;
        }
    }
}

void setLedPattern(LedPattern pattern) {
    if (pattern != _ledPattern) {
        _ledPattern    = pattern;
        _ledPhaseStart = millis();
        _ledPhaseIndex = 0;
        DEBUG_PRINTF("LED pattern → %s\n",
            pattern == LedPattern::OFF        ? "OFF" :
            pattern == LedPattern::SOLID      ? "SOLID" :
            pattern == LedPattern::FAST_BLINK ? "FAST_BLINK" :
            pattern == LedPattern::SLOW_BLINK ? "SLOW_BLINK" :
                                                "HEARTBEAT");
    }
}

LedPattern getLedPattern() {
    return _ledPattern;
}

bool readDoorClosed() {
    // NO reed switch + internal pull-up:
    //   magnet present (door closed) → switch closes → pin LOW
    //   magnet absent  (door open)   → switch open   → pin HIGH
    return digitalRead(PIN_REED_SWITCH) == LOW;
}

bool triggerRelay(bool force) {
    // Reject if relay is already mid-pulse
    if (_relayActive) {
        DEBUG_PRINTLN("Relay: already active, ignoring");
        return false;
    }

    // Reject if cooldown hasn't expired (prevents double-taps that
    // would stop the door mid-travel) — skipped when force=true (STOP command)
    if (!force && _lastTriggerTime > 0 && (millis() - _lastTriggerTime < RELAY_COOLDOWN_MS)) {
        DEBUG_PRINTF("Relay: cooldown active (%lu ms remaining), ignoring\n",
                      RELAY_COOLDOWN_MS - (millis() - _lastTriggerTime));
        return false;
    }

    DEBUG_PRINTLN("Relay ON (pulse started)");
    digitalWrite(PIN_RELAY, HIGH);
    _relayActive     = true;
    _relayStartTime  = millis();
    _lastTriggerTime = _relayStartTime;
    return true;
}

bool isRelayBusy() {
    return _relayActive;
}

void blinkLed(int times, int intervalMs) {
    for (int i = 0; i < times; i++) {
        setLedRaw(true);
        delay(intervalMs);
        setLedRaw(false);
        delay(intervalMs);
    }
}


}  // namespace GarageHardware


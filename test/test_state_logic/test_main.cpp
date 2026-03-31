// ============================================================
//  Unit tests for door state logic
//  ─ Tests the state-determination and debounce algorithms
//    extracted from mqtt_manager.cpp and main.cpp
// ============================================================
//
//  These tests exercise the pure logic without needing MQTT,
//  WiFi, or hardware mocks.  The algorithms are reproduced here
//  exactly as they appear in the firmware so regressions in the
//  logic can be caught without complex integration mocking.
// ============================================================

#include <cstdint>
#include <cstring>
#include <unity.h>

void setUp(void)    {}
void tearDown(void) {}

// ─── Debounce parameters (mirrored from config.h) ───────────
static const uint32_t DEBOUNCE_MS = 200;

// ═════════════════════════════════════════════════════════════
//  Door state determination (from mqtt_manager.cpp publishState)
// ═════════════════════════════════════════════════════════════

/// Pure function equivalent of the state-determination block
/// inside MqttManager::publishState().
/// Returns "open", "closed", "opening", or "closing".
/// Sets `transitCompleted` to true when the door has arrived.
static const char* determineDoorState(
    bool doorClosed,
    bool inTransit,
    bool transitOpening,
    bool &transitCompleted)
{
    transitCompleted = false;

    if (inTransit) {
        bool arrived = transitOpening ? !doorClosed : doorClosed;
        if (arrived) {
            transitCompleted = true;
            return doorClosed ? "closed" : "open";
        }
        return transitOpening ? "opening" : "closing";
    }
    return doorClosed ? "closed" : "open";
}

// ─── Basic states (no transit) ──────────────────────────────

void test_state_closed_when_door_closed(void) {
    bool tc;
    TEST_ASSERT_EQUAL_STRING("closed", determineDoorState(true, false, false, tc));
}

void test_state_open_when_door_open(void) {
    bool tc;
    TEST_ASSERT_EQUAL_STRING("open", determineDoorState(false, false, false, tc));
}

// ─── Transit states ─────────────────────────────────────────

void test_state_opening_during_transit(void) {
    bool tc;
    // Door was closed, command sent → transit opening, door still closed
    const char* s = determineDoorState(true, true, true, tc);
    TEST_ASSERT_EQUAL_STRING("opening", s);
    TEST_ASSERT_FALSE(tc);
}

void test_state_closing_during_transit(void) {
    bool tc;
    // Door was open, command sent → transit closing, door still open
    const char* s = determineDoorState(false, true, false, tc);
    TEST_ASSERT_EQUAL_STRING("closing", s);
    TEST_ASSERT_FALSE(tc);
}

// ─── Transit arrival ────────────────────────────────────────

void test_state_open_after_opening_completes(void) {
    bool tc;
    // Transit opening, and door is now open (not closed)
    const char* s = determineDoorState(false, true, true, tc);
    TEST_ASSERT_EQUAL_STRING("open", s);
    TEST_ASSERT_TRUE(tc);
}

void test_state_closed_after_closing_completes(void) {
    bool tc;
    // Transit closing, and door is now closed
    const char* s = determineDoorState(true, true, false, tc);
    TEST_ASSERT_EQUAL_STRING("closed", s);
    TEST_ASSERT_TRUE(tc);
}

// ─── Edge: transit flag set but door somehow already in target state ──

void test_opening_transit_still_reports_opening_while_closed(void) {
    bool tc;
    // Door is closed, transit says opening → not arrived yet
    const char* s = determineDoorState(true, true, true, tc);
    TEST_ASSERT_EQUAL_STRING("opening", s);
    TEST_ASSERT_FALSE(tc);
}

void test_closing_transit_still_reports_closing_while_open(void) {
    bool tc;
    // Door is open, transit says closing → not arrived yet
    const char* s = determineDoorState(false, true, false, tc);
    TEST_ASSERT_EQUAL_STRING("closing", s);
    TEST_ASSERT_FALSE(tc);
}

// ═════════════════════════════════════════════════════════════
//  Debounce algorithm (from main.cpp loop)
// ═════════════════════════════════════════════════════════════

/// Encapsulates the debounce state exactly as maintained in main.cpp.
struct DebounceState {
    bool     lastRawReading;
    bool     debouncedState;
    bool     currentDoorClosed;
    uint32_t lastDebounceTime;
};

/// Run one iteration of the debounce algorithm (mirrors main.cpp).
/// Returns true if the door state changed this tick.
static bool debounceUpdate(DebounceState &s, bool rawReading, uint32_t now) {
    bool changed = false;

    if (rawReading != s.lastRawReading) {
        s.lastDebounceTime = now;
        s.lastRawReading   = rawReading;
    }

    if ((now - s.lastDebounceTime) > DEBOUNCE_MS) {
        if (rawReading != s.debouncedState) {
            s.debouncedState    = rawReading;
            s.currentDoorClosed = s.debouncedState;
            changed = true;
        }
    }

    return changed;
}

// ─── Stable reading keeps state ─────────────────────────────

void test_debounce_stable_closed_stays_closed(void) {
    DebounceState s = { true, true, true, 0 };
    bool changed = debounceUpdate(s, true, 1000);
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_TRUE(s.currentDoorClosed);
}

void test_debounce_stable_open_stays_open(void) {
    DebounceState s = { false, false, false, 0 };
    bool changed = debounceUpdate(s, false, 1000);
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_FALSE(s.currentDoorClosed);
}

// ─── Bouncing during debounce window ────────────────────────

void test_debounce_rejects_change_during_window(void) {
    // Start: door closed, stable
    DebounceState s = { true, true, true, 0 };

    // Sensor flickers to open at t=1000
    bool changed = debounceUpdate(s, false, 1000);
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_TRUE(s.currentDoorClosed); // state hasn't changed yet

    // Bounces back to closed at t=1050 (within debounce window)
    changed = debounceUpdate(s, true, 1050);
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_TRUE(s.currentDoorClosed);

    // Time passes well beyond debounce — but reading is closed again
    changed = debounceUpdate(s, true, 2000);
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_TRUE(s.currentDoorClosed);
}

// ─── Legitimate state change after debounce period ──────────

void test_debounce_accepts_change_after_window(void) {
    DebounceState s = { true, true, true, 0 };

    // Sensor reads open at t=1000
    debounceUpdate(s, false, 1000);
    TEST_ASSERT_TRUE(s.currentDoorClosed); // not yet

    // Sensor still reads open past debounce window
    bool changed = debounceUpdate(s, false, 1000 + DEBOUNCE_MS + 1);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_FALSE(s.currentDoorClosed); // now open
}

void test_debounce_open_to_closed(void) {
    DebounceState s = { false, false, false, 0 };

    // Door closes at t=500
    debounceUpdate(s, true, 500);
    TEST_ASSERT_FALSE(s.currentDoorClosed);

    // Still closed past debounce
    bool changed = debounceUpdate(s, true, 500 + DEBOUNCE_MS + 1);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_TRUE(s.currentDoorClosed);
}

// ─── Multiple transitions ───────────────────────────────────

void test_debounce_multiple_transitions(void) {
    DebounceState s = { true, true, true, 0 };

    // Open
    debounceUpdate(s, false, 1000);
    debounceUpdate(s, false, 1000 + DEBOUNCE_MS + 1);
    TEST_ASSERT_FALSE(s.currentDoorClosed);

    // Close again
    debounceUpdate(s, true, 3000);
    bool changed = debounceUpdate(s, true, 3000 + DEBOUNCE_MS + 1);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_TRUE(s.currentDoorClosed);
}

// ─── Rapid bounce resets debounce timer ─────────────────────

void test_debounce_bounce_resets_timer(void) {
    DebounceState s = { true, true, true, 0 };

    // Sensor reads open at t=1000
    debounceUpdate(s, false, 1000);

    // Bounces back at t=1100 (within window)
    debounceUpdate(s, true, 1100);

    // Goes open again at t=1150
    debounceUpdate(s, false, 1150);

    // Check just before new debounce window expires
    bool changed = debounceUpdate(s, false, 1150 + DEBOUNCE_MS - 1);
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_TRUE(s.currentDoorClosed); // still closed

    // Now past new debounce window → state changes
    changed = debounceUpdate(s, false, 1150 + DEBOUNCE_MS + 1);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_FALSE(s.currentDoorClosed);
}

// ═════════════════════════════════════════════════════════════
//  Test runner
// ═════════════════════════════════════════════════════════════

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Door state determination
    RUN_TEST(test_state_closed_when_door_closed);
    RUN_TEST(test_state_open_when_door_open);
    RUN_TEST(test_state_opening_during_transit);
    RUN_TEST(test_state_closing_during_transit);
    RUN_TEST(test_state_open_after_opening_completes);
    RUN_TEST(test_state_closed_after_closing_completes);
    RUN_TEST(test_opening_transit_still_reports_opening_while_closed);
    RUN_TEST(test_closing_transit_still_reports_closing_while_open);

    // Debounce algorithm
    RUN_TEST(test_debounce_stable_closed_stays_closed);
    RUN_TEST(test_debounce_stable_open_stays_open);
    RUN_TEST(test_debounce_rejects_change_during_window);
    RUN_TEST(test_debounce_accepts_change_after_window);
    RUN_TEST(test_debounce_open_to_closed);
    RUN_TEST(test_debounce_multiple_transitions);
    RUN_TEST(test_debounce_bounce_resets_timer);

    return UNITY_END();
}

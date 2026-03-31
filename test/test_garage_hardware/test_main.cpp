// ============================================================
//  Unit tests for garage_hardware module
//  ─ Tests reed switch, relay control & LED pattern engine
// ============================================================

// Include the source under test (single translation-unit approach:
// the mock Arduino.h is found via -Itest/mocks before any other).
#include "../../src/garage_hardware.cpp"

#include <unity.h>

// ─── Helpers ────────────────────────────────────────────────
static void resetAll() {
    mock_reset();
    // Re-initialise the module (sets pin modes, clears relay/LED)
    GarageHardware::init();
    // Reset internal statics that init() doesn't touch
    // (they live inside the GarageHardware namespace in this TU)
    GarageHardware::_relayActive     = false;
    GarageHardware::_relayStartTime  = 0;
    GarageHardware::_lastTriggerTime = 0;
    GarageHardware::_ledPattern      = GarageHardware::LedPattern::OFF;
    GarageHardware::_ledPhaseStart   = 0;
    GarageHardware::_ledPhaseIndex   = 0;
    GarageHardware::_ledOn           = false;
}

void setUp(void)    { resetAll(); }
void tearDown(void) {}

// ═════════════════════════════════════════════════════════════
//  init()
// ═════════════════════════════════════════════════════════════

void test_init_sets_reed_switch_as_input_pullup(void) {
    GarageHardware::init();
    TEST_ASSERT_EQUAL(INPUT_PULLUP, mock_get_pin_mode(PIN_REED_SWITCH));
}

void test_init_sets_relay_as_output_low(void) {
    GarageHardware::init();
    TEST_ASSERT_EQUAL(OUTPUT, mock_get_pin_mode(PIN_RELAY));
    TEST_ASSERT_EQUAL(LOW,    mock_get_pin_write(PIN_RELAY));
}

void test_init_sets_led_as_output_off(void) {
    GarageHardware::init();
    TEST_ASSERT_EQUAL(OUTPUT, mock_get_pin_mode(PIN_STATUS_LED));
    TEST_ASSERT_EQUAL(HIGH,   mock_get_pin_write(PIN_STATUS_LED));  // active-low: HIGH = off
}

// ═════════════════════════════════════════════════════════════
//  readDoorClosed()
// ═════════════════════════════════════════════════════════════

void test_read_door_closed_when_magnet_present(void) {
    // Reed switch closed (magnet near) → pin reads LOW → door is closed
    mock_set_pin_value(PIN_REED_SWITCH, LOW);
    TEST_ASSERT_TRUE(GarageHardware::readDoorClosed());
}

void test_read_door_open_when_no_magnet(void) {
    // Reed switch open (no magnet) → pin reads HIGH → door is open
    mock_set_pin_value(PIN_REED_SWITCH, HIGH);
    TEST_ASSERT_FALSE(GarageHardware::readDoorClosed());
}

// ═════════════════════════════════════════════════════════════
//  triggerRelay()
// ═════════════════════════════════════════════════════════════

void test_trigger_relay_success(void) {
    mock_set_millis(10000);
    bool result = GarageHardware::triggerRelay();
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(HIGH, mock_get_pin_write(PIN_RELAY));
}

void test_trigger_relay_rejected_when_already_active(void) {
    mock_set_millis(10000);
    GarageHardware::triggerRelay();        // first trigger
    bool result = GarageHardware::triggerRelay();  // second while active
    TEST_ASSERT_FALSE(result);
}

void test_trigger_relay_rejected_during_cooldown(void) {
    mock_set_millis(10000);
    GarageHardware::triggerRelay();

    // Advance past the pulse duration so relay completes
    mock_advance_millis(RELAY_PULSE_MS + 1);
    GarageHardware::loop();

    // Still within cooldown
    mock_advance_millis(100);
    bool result = GarageHardware::triggerRelay();
    TEST_ASSERT_FALSE(result);
}

void test_trigger_relay_allowed_after_cooldown(void) {
    mock_set_millis(10000);
    GarageHardware::triggerRelay();

    // Advance past pulse + full cooldown
    mock_advance_millis(RELAY_PULSE_MS + 1);
    GarageHardware::loop();
    mock_advance_millis(RELAY_COOLDOWN_MS + 1);

    bool result = GarageHardware::triggerRelay();
    TEST_ASSERT_TRUE(result);
}

void test_trigger_relay_force_bypasses_cooldown(void) {
    mock_set_millis(10000);
    GarageHardware::triggerRelay();

    // Advance past pulse so relay is no longer active
    mock_advance_millis(RELAY_PULSE_MS + 1);
    GarageHardware::loop();

    // Still in cooldown, but force=true should bypass
    mock_advance_millis(100);
    bool result = GarageHardware::triggerRelay(true);
    TEST_ASSERT_TRUE(result);
}

void test_trigger_relay_force_rejected_when_active(void) {
    mock_set_millis(10000);
    GarageHardware::triggerRelay();

    // Even with force=true, reject if relay is mid-pulse
    bool result = GarageHardware::triggerRelay(true);
    TEST_ASSERT_FALSE(result);
}

// ═════════════════════════════════════════════════════════════
//  isRelayBusy()
// ═════════════════════════════════════════════════════════════

void test_relay_not_busy_initially(void) {
    TEST_ASSERT_FALSE(GarageHardware::isRelayBusy());
}

void test_relay_busy_after_trigger(void) {
    mock_set_millis(10000);
    GarageHardware::triggerRelay();
    TEST_ASSERT_TRUE(GarageHardware::isRelayBusy());
}

void test_relay_not_busy_after_pulse_completes(void) {
    mock_set_millis(10000);
    GarageHardware::triggerRelay();

    mock_advance_millis(RELAY_PULSE_MS + 1);
    GarageHardware::loop();

    TEST_ASSERT_FALSE(GarageHardware::isRelayBusy());
}

// ═════════════════════════════════════════════════════════════
//  Relay pulse management in loop()
// ═════════════════════════════════════════════════════════════

void test_relay_turns_off_after_pulse_duration(void) {
    mock_set_millis(10000);
    GarageHardware::triggerRelay();
    TEST_ASSERT_EQUAL(HIGH, mock_get_pin_write(PIN_RELAY));

    // Just before pulse ends — relay should still be on
    mock_advance_millis(RELAY_PULSE_MS - 1);
    GarageHardware::loop();
    TEST_ASSERT_EQUAL(HIGH, mock_get_pin_write(PIN_RELAY));

    // At/after pulse duration — relay should turn off
    mock_advance_millis(2);
    GarageHardware::loop();
    TEST_ASSERT_EQUAL(LOW, mock_get_pin_write(PIN_RELAY));
}

// ═════════════════════════════════════════════════════════════
//  setLedPattern() / getLedPattern()
// ═════════════════════════════════════════════════════════════

void test_get_led_pattern_default_off(void) {
    TEST_ASSERT_EQUAL(GarageHardware::LedPattern::OFF, GarageHardware::getLedPattern());
}

void test_set_get_led_pattern(void) {
    GarageHardware::setLedPattern(GarageHardware::LedPattern::HEARTBEAT);
    TEST_ASSERT_EQUAL(GarageHardware::LedPattern::HEARTBEAT, GarageHardware::getLedPattern());

    GarageHardware::setLedPattern(GarageHardware::LedPattern::FAST_BLINK);
    TEST_ASSERT_EQUAL(GarageHardware::LedPattern::FAST_BLINK, GarageHardware::getLedPattern());

    GarageHardware::setLedPattern(GarageHardware::LedPattern::SLOW_BLINK);
    TEST_ASSERT_EQUAL(GarageHardware::LedPattern::SLOW_BLINK, GarageHardware::getLedPattern());

    GarageHardware::setLedPattern(GarageHardware::LedPattern::SOLID);
    TEST_ASSERT_EQUAL(GarageHardware::LedPattern::SOLID, GarageHardware::getLedPattern());
}

// ═════════════════════════════════════════════════════════════
//  LED pattern engine in loop()
// ═════════════════════════════════════════════════════════════

void test_led_off_keeps_led_off(void) {
    GarageHardware::setLedPattern(GarageHardware::LedPattern::OFF);
    GarageHardware::loop();
    // Active-low: HIGH = LED off
    TEST_ASSERT_EQUAL(HIGH, mock_get_pin_write(PIN_STATUS_LED));
}

void test_led_solid_keeps_led_on(void) {
    GarageHardware::setLedPattern(GarageHardware::LedPattern::SOLID);
    GarageHardware::loop();
    // Active-low: LOW = LED on
    TEST_ASSERT_EQUAL(LOW, mock_get_pin_write(PIN_STATUS_LED));
}

void test_led_fast_blink_toggles(void) {
    mock_set_millis(1000);
    GarageHardware::setLedPattern(GarageHardware::LedPattern::FAST_BLINK);
    GarageHardware::loop();
    uint8_t first = mock_get_pin_write(PIN_STATUS_LED);

    // Advance past fast-blink half-period
    mock_advance_millis(LED_FAST_BLINK_MS + 1);
    GarageHardware::loop();
    uint8_t second = mock_get_pin_write(PIN_STATUS_LED);

    TEST_ASSERT_NOT_EQUAL(first, second);

    // Advance again — should toggle back
    mock_advance_millis(LED_FAST_BLINK_MS + 1);
    GarageHardware::loop();
    uint8_t third = mock_get_pin_write(PIN_STATUS_LED);

    TEST_ASSERT_EQUAL(first, third);
}

void test_led_slow_blink_toggles(void) {
    mock_set_millis(1000);
    GarageHardware::setLedPattern(GarageHardware::LedPattern::SLOW_BLINK);
    GarageHardware::loop();
    uint8_t first = mock_get_pin_write(PIN_STATUS_LED);

    mock_advance_millis(LED_SLOW_BLINK_MS + 1);
    GarageHardware::loop();
    uint8_t second = mock_get_pin_write(PIN_STATUS_LED);

    TEST_ASSERT_NOT_EQUAL(first, second);
}

void test_led_fast_blink_no_toggle_before_period(void) {
    mock_set_millis(1000);
    GarageHardware::setLedPattern(GarageHardware::LedPattern::FAST_BLINK);
    GarageHardware::loop();
    uint8_t first = mock_get_pin_write(PIN_STATUS_LED);

    // Advance less than the blink period
    mock_advance_millis(LED_FAST_BLINK_MS - 10);
    GarageHardware::loop();
    uint8_t second = mock_get_pin_write(PIN_STATUS_LED);

    TEST_ASSERT_EQUAL(first, second);
}

void test_led_heartbeat_pattern_sequence(void) {
    mock_set_millis(1000);
    GarageHardware::setLedPattern(GarageHardware::LedPattern::HEARTBEAT);

    // Phase 0: first flash (ON for LED_HEARTBEAT_PULSE_MS)
    GarageHardware::loop();
    TEST_ASSERT_EQUAL(LOW, mock_get_pin_write(PIN_STATUS_LED));  // active-low ON

    // Advance past phase 0 → loop() transitions to phase 1, but still
    // outputs phase 0's state; a second loop() applies phase 1's state.
    mock_advance_millis(LED_HEARTBEAT_PULSE_MS + 1);
    GarageHardware::loop();   // transition
    GarageHardware::loop();   // phase 1: short gap (OFF)
    TEST_ASSERT_EQUAL(HIGH, mock_get_pin_write(PIN_STATUS_LED)); // active-low OFF

    // Advance past phase 1 → phase 2: second flash (ON)
    mock_advance_millis(LED_HEARTBEAT_GAP_MS + 1);
    GarageHardware::loop();
    GarageHardware::loop();
    TEST_ASSERT_EQUAL(LOW, mock_get_pin_write(PIN_STATUS_LED));

    // Advance past phase 2 → phase 3: long pause (OFF)
    mock_advance_millis(LED_HEARTBEAT_PULSE_MS + 1);
    GarageHardware::loop();
    GarageHardware::loop();
    TEST_ASSERT_EQUAL(HIGH, mock_get_pin_write(PIN_STATUS_LED));
}

// ═════════════════════════════════════════════════════════════
//  blinkLed() — blocking helper
// ═════════════════════════════════════════════════════════════

void test_blink_led_advances_time(void) {
    mock_set_millis(0);
    GarageHardware::blinkLed(3, 100);
    // 3 blinks × (100 ms on + 100 ms off) = 600 ms
    TEST_ASSERT_EQUAL_UINT32(600, millis());
}

// ═════════════════════════════════════════════════════════════
//  Test runner
// ═════════════════════════════════════════════════════════════

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // init
    RUN_TEST(test_init_sets_reed_switch_as_input_pullup);
    RUN_TEST(test_init_sets_relay_as_output_low);
    RUN_TEST(test_init_sets_led_as_output_off);

    // readDoorClosed
    RUN_TEST(test_read_door_closed_when_magnet_present);
    RUN_TEST(test_read_door_open_when_no_magnet);

    // triggerRelay
    RUN_TEST(test_trigger_relay_success);
    RUN_TEST(test_trigger_relay_rejected_when_already_active);
    RUN_TEST(test_trigger_relay_rejected_during_cooldown);
    RUN_TEST(test_trigger_relay_allowed_after_cooldown);
    RUN_TEST(test_trigger_relay_force_bypasses_cooldown);
    RUN_TEST(test_trigger_relay_force_rejected_when_active);

    // isRelayBusy
    RUN_TEST(test_relay_not_busy_initially);
    RUN_TEST(test_relay_busy_after_trigger);
    RUN_TEST(test_relay_not_busy_after_pulse_completes);

    // Relay pulse management
    RUN_TEST(test_relay_turns_off_after_pulse_duration);

    // LED pattern get/set
    RUN_TEST(test_get_led_pattern_default_off);
    RUN_TEST(test_set_get_led_pattern);

    // LED pattern engine
    RUN_TEST(test_led_off_keeps_led_off);
    RUN_TEST(test_led_solid_keeps_led_on);
    RUN_TEST(test_led_fast_blink_toggles);
    RUN_TEST(test_led_slow_blink_toggles);
    RUN_TEST(test_led_fast_blink_no_toggle_before_period);
    RUN_TEST(test_led_heartbeat_pattern_sequence);

    // blinkLed
    RUN_TEST(test_blink_led_advances_time);

    return UNITY_END();
}

// ============================================================
//  main.cpp — ESP32-C3 Garage Door Controller
//  ─ Entry point: setup() + loop()
// ============================================================

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "garage_hardware.h"
#include "debounce.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "ota_manager.h"

// ─── Watchdog ───────────────────────────────────────────────
#define WDT_TIMEOUT_SEC  30   // reset if loop doesn't run for 30 s

// ─── Door state tracking ────────────────────────────────────
static DebounceState doorDebounce = { true, true, true, 0 };
static uint32_t lastStatePublish  = 0;
static uint32_t bootTime          = 0;

// ════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════
void setup() {
    #ifdef SERIAL_DEBUG
    Serial.begin(115200);
    delay(500);
    #endif
    DEBUG_PRINTLN("\n\n===== Garage Door Controller =====");

    // Hardware watchdog — resets the ESP if loop() ever hangs
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);

    GarageHardware::init();

    // Read initial door state
    bool initState = GarageHardware::readDoorClosed();
    doorDebounce = { initState, initState, initState, 0 };
    DEBUG_PRINTF("Initial door state: %s\n", initState ? "CLOSED" : "OPEN");

    WifiManager::connectBlocking();   // blocking OK here — we're in setup
    MqttManager::init();
    OtaManager::init();                // start ElegantOTA web server

    bootTime = millis();
    DEBUG_PRINTLN("Setup complete ✓");
}

// ════════════════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════════════════
void loop() {
    esp_task_wdt_reset();   // pet the watchdog

    // ── WiFi (non-blocking) ──
    WifiManager::ensureConnected();

    // ── MQTT keep-alive, reconnect & transit timeout ──
    MqttManager::loop(doorDebounce.currentDoorClosed);

    // ── Status LED — single place that decides the pattern ──
    //    (must be set before GarageHardware::loop() which drives the LED)
    if (!WifiManager::isConnected()) {
        GarageHardware::setLedPattern(GarageHardware::LedPattern::FAST_BLINK);
    } else if (!MqttManager::isConnected()) {
        GarageHardware::setLedPattern(GarageHardware::LedPattern::SLOW_BLINK);
    } else {
        GarageHardware::setLedPattern(GarageHardware::LedPattern::HEARTBEAT);
    }

    // ── Relay pulse manager + LED pattern engine (non-blocking) ──
    GarageHardware::loop();

    // ── OTA update handler ──
    OtaManager::loop();

    // ── Reed-switch debounce ──
    bool raw = GarageHardware::readDoorClosed();
    if (debounceUpdate(doorDebounce, raw, millis(), DEBOUNCE_MS)) {
        DEBUG_PRINTF("Door state changed → %s\n",
                       doorDebounce.currentDoorClosed ? "CLOSED" : "OPEN");
        MqttManager::publishState(doorDebounce.currentDoorClosed, true);
    }

    // ── Periodic re-publish (in case HA restarted) ──
    if (millis() - lastStatePublish > STATE_PUBLISH_INTERVAL_MS) {
        lastStatePublish = millis();
        MqttManager::publishState(doorDebounce.currentDoorClosed, true);
        MqttManager::publishAttributes(bootTime);
    }
}

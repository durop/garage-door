// ============================================================
// config.h — WiFi, MQTT & Hardware Configuration
// ============================================================
// Fill in your credentials below.
// This file is git-ignored so your secrets stay safe.
// ============================================================
#pragma once

// ----- Debug (comment out for production to silence serial) --
#define SERIAL_DEBUG

#ifdef SERIAL_DEBUG
  #define DEBUG_PRINT(...)   Serial.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
  #define DEBUG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)   ((void)0)
  #define DEBUG_PRINTLN(...) ((void)0)
  #define DEBUG_PRINTF(...)  ((void)0)
#endif

// ----- WiFi -----
#define WIFI_SSID         "your_wifi_ssid"   // your WiFi SSID
#define WIFI_PASSWORD     "your_wifi_password"

// ----- MQTT Broker -----
#define MQTT_HOST         "192.168.80.40"   // IP of your HA / MQTT broker

//  ┌─────────────────────────────────────────────────────────┐
//  │  TLS — STRONGLY RECOMMENDED                            │
//  │  Your MQTT credentials and door commands travel over    │
//  │  the network. Without TLS anyone on your WiFi can       │
//  │  sniff them or send OPEN commands.                      │
//  │                                                         │
//  │  1. Enable TLS in your Mosquitto config (port 8883).    │
//  │  2. Set MQTT_USE_TLS to true below.                     │
//  │  3. Paste your broker's CA certificate in MQTT_CA_CERT. │
//  │     (or set it to nullptr to skip server verification — │
//  │      still encrypts traffic, but no MITM protection.)   │
//  └─────────────────────────────────────────────────────────┘
#define MQTT_USE_TLS      true
#define MQTT_PORT         8883              // 8883 = TLS, 1883 = plaintext

// CA certificate of your MQTT broker (PEM format).
// Generate with:  openssl s_client -connect YOUR_BROKER:8883 < /dev/null 2>/dev/null | openssl x509
// Paste the full "-----BEGIN CERTIFICATE----- ... -----END CERTIFICATE-----" block.
// Set to nullptr to encrypt without verifying the server (still much better than plaintext).
static const char* MQTT_CA_CERT = nullptr;
//
// Example (replace with your own):
// static const char* MQTT_CA_CERT = R"EOF(
// -----BEGIN CERTIFICATE-----
// MIIBxTCCAWugAwIBAgIUd...
// -----END CERTIFICATE-----
// )EOF";

#define MQTT_USER         "homeassistant"        // leave "" if no auth
#define MQTT_PASSWORD     "your_mqtt_password"    // leave "" if no auth

// ----- Device Identity -----
#define DEVICE_NAME       "garage_door"      // used in MQTT topics & HA entity IDs
#define FRIENDLY_NAME     "Garage Door"      // shown in Home Assistant UI

// ----- Hardware Pins (ESP32-C3 Mini) -----
//  Reed switch → GPIO 4  (NO reed switch, other leg to GND)
//  Relay IN    → GPIO 5  (high-level trigger)
//  Built-in LED on most C3-Mini boards is GPIO 8
#define PIN_REED_SWITCH   4
#define PIN_RELAY         5
#define PIN_STATUS_LED    8

// ----- Behaviour -----
#define RELAY_PULSE_MS        750    // how long to "press the button" (ms)
#define RELAY_COOLDOWN_MS     3000   // ignore commands for this long after a trigger
#define DEBOUNCE_MS           200    // reed-switch debounce time (ms)
#define DOOR_TRAVEL_TIMEOUT_MS 20000 // max time a door takes to fully open/close
#define MQTT_RECONNECT_MS     5000   // ms between MQTT reconnect attempts
#define MQTT_KEEPALIVE_SEC    120    // broker waits 1.5× before last-will (default 15 is too short for weak WiFi)
#define STATE_PUBLISH_INTERVAL_MS 30000  // republish state every 30 s
#define WIFI_CONNECT_TIMEOUT_MS   15000  // max time to wait for WiFi (initial only)
#define MQTT_MAX_MESSAGE_LEN  64     // max inbound MQTT payload we'll handle
#define CMD_RATE_LIMIT_MS     1000   // ignore commands arriving faster than this
#define CMD_ARM_DELAY_MS      3000   // ignore commands for this long after MQTT (re)connect

// ----- OTA (ElegantOTA) -----
#define OTA_WEB_PORT          80       // HTTP port for the OTA update page
#define OTA_USERNAME          "ota_garage"          // leave "" for no auth (LAN-only is OK)
#define OTA_PASSWORD          "your_ota_password"  // set both to enable Basic-Auth on the OTA page
#define FIRMWARE_VERSION      "1.2.4"

// ----- NTP -----
#define NTP_SERVER        "pool.ntp.org"
#define NTP_GMT_OFFSET    3600        // UTC+1 (CET) — adjust for your timezone
#define NTP_DST_OFFSET    3600        // +1 h for CEST daylight saving

// ----- Status LED patterns -----
//  HEARTBEAT  = WiFi ✓ + MQTT ✓  (two quick flashes, long pause)
//  SLOW_BLINK = WiFi ✓ + MQTT ✗  (1 s on / 1 s off)
//  FAST_BLINK = WiFi ✗           (150 ms on / 150 ms off)
#define LED_FAST_BLINK_MS         150   // half-period for fast blink
#define LED_SLOW_BLINK_MS         1000  // half-period for slow blink
#define LED_HEARTBEAT_PULSE_MS    100   // each flash of the double-pulse
#define LED_HEARTBEAT_GAP_MS      150   // gap between the two flashes
#define LED_HEARTBEAT_PAUSE_MS    2500  // pause after the double-pulse


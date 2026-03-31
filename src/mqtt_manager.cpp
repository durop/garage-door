// ============================================================
//  mqtt_manager.cpp — MQTT connection, discovery & messaging
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"
#include "mqtt_manager.h"
#include "wifi_manager.h"
#include "garage_hardware.h"

// ─── Topics ─────────────────────────────────────────────────
static const char* TOPIC_STATE      = DEVICE_NAME "/state";
static const char* TOPIC_COMMAND    = DEVICE_NAME "/set";
static const char* TOPIC_AVAIL      = DEVICE_NAME "/availability";
static const char* TOPIC_ATTRIBUTES = DEVICE_NAME "/attributes";

// ─── TLS / plaintext client (chosen at compile time) ────────
#if MQTT_USE_TLS
  static WiFiClientSecure _netClient;
#else
  static WiFiClient       _netClient;
#endif
static PubSubClient _mqtt(_netClient);

// ─── Unique client ID (DEVICE_NAME + MAC suffix) ────────────
static char _clientId[40];

static bool     _lastPublishedClosed = true;
static uint32_t _lastReconnectAttempt = 0;

// Dedup: remember the last state string we successfully published.
// Only re-publish on actual change, transit events, or first boot.
static char _lastPublishedState[10] = "";
static bool _bootPublishDone = false;

// Current door state — kept in sync by main.cpp via publishState()
static bool _currentDoorClosed = true;

// Transitional state tracking ("opening" / "closing")
static bool     _inTransit       = false;
static bool     _transitOpening  = false;   // true = opening, false = closing
static uint32_t _transitStart    = 0;
static bool     _transitStartDoorClosed = true;  // reed-switch state when transit began

// Stopped mid-travel tracking (single-button door reverses direction on next press)
static bool     _stopped            = false;
static bool     _stoppedWasOpening  = false;  // true = was opening before stop

// Publish-failure retry
static bool _pendingStatePublish = false;

// Command rate-limiting
static uint32_t _lastCommandTime = 0;

// Post-connect grace period — ignore commands right after (re)connecting
// to prevent retained messages or eager HA commands from firing the relay.
static bool     _commandsArmed   = false;
static uint32_t _connectTime     = 0;

// Track when we last had a live MQTT connection, so we can tell
// brief reconnects from cold boots / long outages.
static uint32_t _lastConnectedTime = 0;

// ─── Forward declarations (internal) ────────────────────────
static void onMessage(char* topic, byte* payload, unsigned int length);
static void publishDiscovery();
static bool mqttPublishState(const char* state);
static void buildClientId();

// ═════════════════════════════════════════════════════════════
//  Public API
// ═════════════════════════════════════════════════════════════

void MqttManager::init() {
    buildClientId();

    #if MQTT_USE_TLS
      _netClient.setTimeout(5);    // 5 s cap on TLS handshake — prevents blocking the loop
      if (MQTT_CA_CERT != nullptr) {
          _netClient.setCACert(MQTT_CA_CERT);
          DEBUG_PRINTLN("MQTT TLS: server certificate verification ON");
      } else {
          _netClient.setInsecure();   // encrypt, but skip server verify
          DEBUG_PRINTLN("MQTT TLS: encryption ON, server verify OFF (setInsecure)");
      }
    #else
      #warning "MQTT_USE_TLS is false — credentials and commands sent in PLAINTEXT!"
      DEBUG_PRINTLN("⚠ MQTT: TLS disabled — traffic is NOT encrypted!");
    #endif

    _mqtt.setServer(MQTT_HOST, MQTT_PORT);
    _mqtt.setKeepAlive(MQTT_KEEPALIVE_SEC);
    _mqtt.setCallback(onMessage);
    _mqtt.setBufferSize(1024);   // discovery payloads can be large
}

void MqttManager::loop(bool doorClosed) {
    _currentDoorClosed = doorClosed;

    // ── MQTT reconnect ──
    if (!_mqtt.connected()) {
        if (!WifiManager::isConnected()) {
            return;  // no point attempting TLS without a network path
        }
        uint32_t now = millis();
        if (now - _lastReconnectAttempt > MQTT_RECONNECT_MS) {
            _lastReconnectAttempt = now;
            _netClient.stop();      // flush stale TLS state from previous session

            DEBUG_PRINT("MQTT connecting… ");
            bool ok = _mqtt.connect(
                _clientId,
                MQTT_USER, MQTT_PASSWORD
            );

            if (ok) {
                DEBUG_PRINTLN("connected ✓");
                _commandsArmed = false;
                _connectTime   = millis();
                _mqtt.subscribe(TOPIC_COMMAND);
                publishDiscovery();
                publishState(doorClosed, true);
                _lastConnectedTime = millis();
            } else {
                DEBUG_PRINTF("failed (rc=%d) – retry in %d s\n",
                              _mqtt.state(), MQTT_RECONNECT_MS / 1000);
            }
        }
    }
    _mqtt.loop();

    // ── Track last known good connection time ──
    if (_mqtt.connected()) {
        _lastConnectedTime = millis();
    }

    // ── Arm commands after grace period ──
    if (!_commandsArmed && _mqtt.connected()
        && (millis() - _connectTime >= CMD_ARM_DELAY_MS)) {
        _commandsArmed = true;
        DEBUG_PRINTLN("Commands armed ✓");
    }

    // ── Retry failed publish ──
    if (_pendingStatePublish && _mqtt.connected()) {
        publishState(_currentDoorClosed, true);
    }

    // ── Transit timeout: if the door has been "opening"/"closing"
    //    for too long, fall back to the reed-switch truth ──
    if (_inTransit && (millis() - _transitStart > DOOR_TRAVEL_TIMEOUT_MS)) {
        DEBUG_PRINTLN("Transit timeout — falling back to reed-switch state");
        _inTransit = false;
        publishState(_currentDoorClosed, true);
    }
}

void MqttManager::publishState(bool doorClosed, bool force) {
    _currentDoorClosed = doorClosed;

    const char* state;
    if (_inTransit) {
        bool stateChanged = (doorClosed != _transitStartDoorClosed);
        bool arrived = (_transitOpening ? !doorClosed : doorClosed) && stateChanged;
        if (arrived) {
            _inTransit = false;
            state = doorClosed ? "closed" : "open";
            DEBUG_PRINTF("Door arrived → %s\n", state);
        } else {
            state = _transitOpening ? "opening" : "closing";
        }
    } else if (_stopped) {
        if (doorClosed) {
            // Reed switch says closed — door was closed (e.g. wall button)
            _stopped = false;
            state = "closed";
            DEBUG_PRINTLN("Door closed while stopped → clearing stopped state");
        } else {
            state = "stopped";
        }
    } else {
        state = doorClosed ? "closed" : "open";
    }

    // Dedup: skip if the state string hasn't changed since last successful
    // publish — unless this is the very first publish after boot.
    if (_bootPublishDone && strcmp(state, _lastPublishedState) == 0) {
        return;
    }

    if (force || (doorClosed != _lastPublishedClosed) || _pendingStatePublish) {
        bool ok = mqttPublishState(state);
        if (ok) {
            _lastPublishedClosed = doorClosed;
            _pendingStatePublish = false;
            strncpy(_lastPublishedState, state, sizeof(_lastPublishedState) - 1);
            _lastPublishedState[sizeof(_lastPublishedState) - 1] = '\0';
            _bootPublishDone = true;
        } else {
            _pendingStatePublish = true;
            DEBUG_PRINTLN("⚠ State publish failed — will retry");
        }
    }
}

void MqttManager::setTransitState(bool wasClosedBeforeCommand) {
    _inTransit      = true;
    _transitOpening = wasClosedBeforeCommand;
    _transitStart   = millis();
    _transitStartDoorClosed = _currentDoorClosed;
    _stopped        = false;

    const char* state = _transitOpening ? "opening" : "closing";
    DEBUG_PRINTF("Transit state → %s\n", state);
    mqttPublishState(state);
}

void MqttManager::publishAttributes(uint32_t bootTime) {
    JsonDocument doc;
    doc["ip"]         = WiFi.localIP().toString();
    doc["rssi"]       = WiFi.RSSI();
    doc["uptime_sec"] = (millis() - bootTime) / 1000;
    doc["firmware"]   = FIRMWARE_VERSION;

    // NTP wall-clock timestamp (empty string if NTP hasn't synced yet)
    time_t now = time(nullptr);
    if (now > 1700000000) {
        char timeBuf[25];
        strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", localtime(&now));
        doc["timestamp"] = timeBuf;
    }

    char buf[192];
    serializeJson(doc, buf, sizeof(buf));
    _mqtt.publish(TOPIC_ATTRIBUTES, buf, true);
}

bool MqttManager::isConnected() {
    return _mqtt.connected();
}

// ═════════════════════════════════════════════════════════════
//  Internal helpers
// ═════════════════════════════════════════════════════════════

static void buildClientId() {
    // Use MAC address to make client ID unique — prevents another
    // client from impersonating us and kicking us off the broker.
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(_clientId, sizeof(_clientId), "%s_%02X%02X%02X",
             DEVICE_NAME, mac[3], mac[4], mac[5]);
    DEBUG_PRINTF("MQTT client ID: %s\n", _clientId);
}

static bool mqttPublishState(const char* state) {
    bool ok = _mqtt.publish(TOPIC_STATE, state, true);
    if (ok) {
        DEBUG_PRINTF("Published state: %s\n", state);
    }
    return ok;
}

// ═════════════════════════════════════════════════════════════
//  Discovery (internal)
// ═════════════════════════════════════════════════════════════
static void publishDiscovery() {
    JsonDocument doc;
    doc["name"]                   = FRIENDLY_NAME;
    doc["unique_id"]              = DEVICE_NAME "_cover";
    doc["device_class"]           = "garage";
    doc["command_topic"]          = TOPIC_COMMAND;
    doc["state_topic"]            = TOPIC_STATE;
    doc["payload_open"]           = "OPEN";
    doc["payload_close"]          = "CLOSE";
    doc["payload_stop"]           = "STOP";
    doc["state_open"]             = "open";
    doc["state_closed"]           = "closed";
    doc["state_opening"]          = "opening";
    doc["state_closing"]          = "closing";
    doc["state_stopped"]          = "stopped";
    doc["json_attributes_topic"]  = TOPIC_ATTRIBUTES;
    doc["retain"]                 = true;

    JsonObject dev        = doc["device"].to<JsonObject>();
    dev["identifiers"][0] = DEVICE_NAME;
    dev["name"]           = FRIENDLY_NAME;
    dev["manufacturer"]   = "DIY";
    dev["model"]          = "ESP32-C3 Garage Controller";
    dev["sw_version"]     = FIRMWARE_VERSION;

    char buf[768];
    size_t n = serializeJson(doc, buf, sizeof(buf));

    String topic = String("homeassistant/cover/") + DEVICE_NAME + "/config";
    _mqtt.publish(topic.c_str(), buf, true);
    DEBUG_PRINTF("Discovery published → %s (%d bytes)\n", topic.c_str(), n);
}

// ═════════════════════════════════════════════════════════════
//  Incoming commands (internal)
// ═════════════════════════════════════════════════════════════
static void onMessage(char* topic, byte* payload, unsigned int length) {
    // ── Guard: oversized payloads ──
    if (length > MQTT_MAX_MESSAGE_LEN) {
        DEBUG_PRINTF("MQTT ← payload too large (%u bytes), dropping\n", length);
        return;
    }

    // ── Guard: post-connect grace period (ignore retained / eager commands) ──
    if (!_commandsArmed) {
        DEBUG_PRINTLN("MQTT ← command ignored (post-connect grace period)");
        return;
    }

    // ── Guard: command rate-limit (flood protection) ──
    uint32_t now = millis();
    if (now - _lastCommandTime < CMD_RATE_LIMIT_MS) {
        DEBUG_PRINTLN("MQTT ← rate-limited, dropping");
        return;
    }
    _lastCommandTime = now;

    char msg[MQTT_MAX_MESSAGE_LEN + 1];
    memcpy(msg, payload, length);
    msg[length] = '\0';

    DEBUG_PRINTF("MQTT ← [%s] %s\n", topic, msg);

    if (strcmp(topic, TOPIC_COMMAND) != 0) return;

    if (strcmp(msg, "OPEN") == 0) {
        if (_currentDoorClosed && !_inTransit) {
            DEBUG_PRINTLN("Command: OPEN → triggering relay");
            if (GarageHardware::triggerRelay()) {
                MqttManager::setTransitState(true);
            }
        } else if (_stopped && !_stoppedWasOpening) {
            // Stopped while closing → single-button reverses → opens
            DEBUG_PRINTLN("Command: OPEN (stopped, was closing) → triggering relay");
            if (GarageHardware::triggerRelay()) {
                MqttManager::setTransitState(true);
            }
        } else {
            DEBUG_PRINTLN("Command: OPEN → ignoring (already open/opening or can't open)");
        }
    }
    else if (strcmp(msg, "CLOSE") == 0) {
        if (!_currentDoorClosed && !_inTransit && !_stopped) {
            DEBUG_PRINTLN("Command: CLOSE → triggering relay");
            if (GarageHardware::triggerRelay()) {
                MqttManager::setTransitState(false);
            }
        } else if (_stopped && _stoppedWasOpening) {
            // Stopped while opening → single-button reverses → closes
            DEBUG_PRINTLN("Command: CLOSE (stopped, was opening) → triggering relay");
            if (GarageHardware::triggerRelay()) {
                MqttManager::setTransitState(false);
            }
        } else {
            DEBUG_PRINTLN("Command: CLOSE → ignoring (already closed/closing or can't close)");
        }
    }
    else if (strcmp(msg, "STOP") == 0) {
        if (_inTransit) {
            DEBUG_PRINTLN("Command: STOP → triggering relay");
            if (GarageHardware::triggerRelay(true)) {   // force=true bypasses cooldown
                _stoppedWasOpening = _transitOpening;
                _inTransit = false;
                _stopped = true;
                MqttManager::publishState(_currentDoorClosed, true);
            }
        } else {
            DEBUG_PRINTLN("Command: STOP → not in transit, ignoring");
        }
    }
}

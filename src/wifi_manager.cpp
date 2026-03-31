// ============================================================
//  wifi_manager.cpp — WiFi connection handling
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include "config.h"
#include "wifi_manager.h"
#include "garage_hardware.h"

namespace WifiManager {

static bool     _wasConnected = false;
static uint32_t _lastReconnectAttempt = 0;
static const uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000;

static void startMdns() {
    if (MDNS.begin(DEVICE_NAME)) {
        MDNS.addService("http", "tcp", OTA_WEB_PORT);
        DEBUG_PRINTF("mDNS: %s.local\n", DEVICE_NAME);
    } else {
        DEBUG_PRINTLN("mDNS: failed to start");
    }
}

void connectBlocking() {
    DEBUG_PRINTF("Connecting to WiFi \"%s\"", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(DEVICE_NAME);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        DEBUG_PRINT(".");
        GarageHardware::blinkLed(1, 250);
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            DEBUG_PRINTLN("\nWiFi timeout – will retry in loop");
            return;
        }
    }

    DEBUG_PRINTF("\nWiFi connected — IP: %s\n",
                  WiFi.localIP().toString().c_str());
    _wasConnected = true;

    startMdns();
    configTime(NTP_GMT_OFFSET, NTP_DST_OFFSET, NTP_SERVER);
    DEBUG_PRINTLN("NTP sync requested");
}

void ensureConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!_wasConnected) {
            DEBUG_PRINTF("WiFi reconnected — IP: %s\n",
                          WiFi.localIP().toString().c_str());
            _wasConnected = true;
            startMdns();
        }
        return;
    }

    // WiFi dropped
    if (_wasConnected) {
        DEBUG_PRINTLN("WiFi lost");
        _wasConnected = false;
    }

    // Non-blocking: only kick off a reconnect attempt periodically.
    // WiFi.setAutoReconnect(true) does background work, but we poke
    // it here in case the auto-reconnect stalls.
    uint32_t now = millis();
    if (now - _lastReconnectAttempt > WIFI_RECONNECT_INTERVAL_MS) {
        _lastReconnectAttempt = now;
        DEBUG_PRINTLN("WiFi: kicking reconnect…");
        WiFi.disconnect(false);   // disconnect but keep WiFi module on
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        WiFi.setAutoReconnect(true);  // re-arm — disconnect() clears this flag
    }
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

}  // namespace WifiManager


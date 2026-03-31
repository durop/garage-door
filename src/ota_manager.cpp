// ============================================================
//  ota_manager.cpp — ElegantOTA over-the-air update support
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include "config.h"
#include "ota_manager.h"

static AsyncWebServer _server(OTA_WEB_PORT);

// ── Callbacks for progress logging ──────────────────────────
static unsigned long _otaStart = 0;

static void onOTAStart() {
    DEBUG_PRINTLN("OTA update started");
    _otaStart = millis();
}

static void onOTAProgress(size_t current, size_t final_size) {
    static uint8_t lastPct = 255;
    uint8_t pct = (uint8_t)((current * 100) / final_size);
    if (pct != lastPct && pct % 10 == 0) {
        DEBUG_PRINTF("OTA progress: %u%%\n", pct);
        lastPct = pct;
    }
}

static void onOTAEnd(bool success) {
    if (success) {
        DEBUG_PRINTF("OTA update finished OK in %lu ms — rebooting\n",
                      millis() - _otaStart);
    } else {
        DEBUG_PRINTLN("OTA update FAILED");
    }
}

// ═════════════════════════════════════════════════════════════
//  Public API
// ═════════════════════════════════════════════════════════════

void OtaManager::init() {
    // Simple root page — useful to verify the device is reachable
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html = "<h1>" FRIENDLY_NAME "</h1>"
                      "<p>Firmware: " FIRMWARE_VERSION "</p>"
                      "<p><a href='/update'>OTA Update</a></p>";
        request->send(200, "text/html", html);
    });

    ElegantOTA.begin(&_server);

    // Basic-Auth — must be set AFTER begin()
    #if defined(OTA_USERNAME) && defined(OTA_PASSWORD)
      if (strlen(OTA_USERNAME) > 0 && strlen(OTA_PASSWORD) > 0) {
          ElegantOTA.setAuth(OTA_USERNAME, OTA_PASSWORD);
          DEBUG_PRINTLN("OTA: Basic-Auth enabled");
      }
    #endif

    // Register event callbacks
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);

    _server.begin();

    DEBUG_PRINTF("OTA ready — http://%s:%d/update\n",
                  WiFi.localIP().toString().c_str(), OTA_WEB_PORT);
}

void OtaManager::loop() {
    ElegantOTA.loop();
}


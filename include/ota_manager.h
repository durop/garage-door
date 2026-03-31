// ============================================================
//  ota_manager.h — ElegantOTA over-the-air update support
// ============================================================
#pragma once

namespace OtaManager {

    /// Start the async web server + ElegantOTA.
    /// Call once in setup(), AFTER WiFi is connected.
    void init();

    /// Must be called every loop() iteration.
    void loop();

}  // namespace OtaManager


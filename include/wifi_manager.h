// ============================================================
//  wifi_manager.h — WiFi connection handling
// ============================================================
#pragma once

namespace WifiManager {

    /// Blocking connect — use only during setup().
    void connectBlocking();

    /// Non-blocking reconnect — call from loop().
    /// Returns immediately; WiFi reconnects in the background.
    void ensureConnected();

    /// Returns true if WiFi is currently connected.
    bool isConnected();

}  // namespace WifiManager


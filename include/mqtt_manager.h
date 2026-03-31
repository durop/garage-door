// ============================================================
//  mqtt_manager.h — MQTT connection, discovery & messaging
// ============================================================
#pragma once

#include <cstdint>

namespace MqttManager {

    /// One-time setup: configure broker, callback, buffer size.
    void init();

    /// Call every loop iteration — handles keepalive, reconnect & transit timeout.
    void loop(bool doorClosed);

    /// Publish the door state ("open" / "closed" / "opening" / "closing").
    void publishState(bool doorClosed, bool force = false);

    /// Notify that a command was accepted (sets transitional state).
    void setTransitState(bool wasClosedBeforeCommand);

    /// Publish JSON attributes (IP, RSSI, uptime, etc.).
    void publishAttributes(uint32_t bootTime);

    /// Returns true if MQTT is currently connected.
    bool isConnected();

}  // namespace MqttManager


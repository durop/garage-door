// ============================================================
//  door_state.h — Pure door-state determination logic
//  Shared between firmware (mqtt_manager.cpp) and unit tests.
// ============================================================
#pragma once

/// Determine the door state string from current inputs.
/// Returns "open", "closed", "opening", or "closing".
/// Sets `transitCompleted` to true when the door has arrived at its target.
inline const char* determineDoorState(
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

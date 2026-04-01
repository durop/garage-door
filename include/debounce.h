// ============================================================
//  debounce.h — Reed-switch debounce algorithm
//  Shared between firmware (main.cpp) and unit tests.
// ============================================================
#pragma once

#include <cstdint>

/// Encapsulates the state needed by the debounce algorithm.
struct DebounceState {
    bool     lastRawReading;
    bool     debouncedState;
    bool     currentDoorClosed;
    uint32_t lastDebounceTime;
};

/// Run one iteration of the debounce algorithm.
/// Returns true if the door state changed this tick.
inline bool debounceUpdate(DebounceState &s, bool rawReading, uint32_t now,
                           uint32_t debounceMs) {
    bool changed = false;

    if (rawReading != s.lastRawReading) {
        s.lastDebounceTime = now;
        s.lastRawReading   = rawReading;
    }

    if ((now - s.lastDebounceTime) > debounceMs) {
        if (rawReading != s.debouncedState) {
            s.debouncedState    = rawReading;
            s.currentDoorClosed = s.debouncedState;
            changed = true;
        }
    }

    return changed;
}

#pragma once
#include <stdint.h>

// One event per press, with a five-second lockout after each accepted press.
class ButtonPressFilter {
    bool wasPressed = false;
    bool hasAcceptedPress = false;
    uint32_t lastAcceptedMs = 0;

public:
    bool Update(bool pressed, uint32_t nowMs) {
        const bool risingEdge = pressed && !wasPressed;
        wasPressed = pressed;
        if (!risingEdge ||
            (hasAcceptedPress && (uint32_t)(nowMs - lastAcceptedMs) < 5000u))
            return false;
        lastAcceptedMs = nowMs;
        hasAcceptedPress = true;
        return true;
    }
};

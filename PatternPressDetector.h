#pragma once
#include "jcpm-hsm-mattmc-signals.h"

class PatternPressDetector {
public:
    PatternPressDetector(hsm_state_t* state, const bool* pattern, uint8_t patternLen,
                        uint32_t shortPressMaxMs = 400, uint32_t patternTimeoutMs = 2000)
        : state(state), pattern(pattern), patternLen(patternLen),
          shortPressMaxMs(shortPressMaxMs), patternTimeoutMs(patternTimeoutMs)
    {
        reset();
        for (uint8_t i = 0; i < MAX_KEYS; ++i) {
            lastDownTime[i] = 0;
        }
    }

    // Call this when a button is pressed
    void onButtonDown(uint8_t switch_id) {
        lastDownTime[switch_id] = millis();
    }

    // Call this when a button is released
    void onButtonUp(uint8_t switch_id) {
        uint32_t now = millis();
        uint32_t duration = now - lastDownTime[switch_id];
        lastDownTime[switch_id] = 0;

        if (count == 0) {
            sequenceStart = now;
        } else if (now - sequenceStart > patternTimeoutMs) {
            reset();
            sequenceStart = now;
        }

        bool isLong = (duration > shortPressMaxMs);
        if (pattern[count] == isLong) {
            count++;
            if (count == patternLen) {
                state->EventQueuePush(SIG_PATTERN_PRESS);
                reset();
            }
        } else {
            reset();
        }
    }

    void reset() {
        count = 0;
        sequenceStart = 0;
    }

private:
    hsm_state_t* state;
    const bool* pattern;
    uint8_t patternLen;
    uint8_t count = 0;
    uint32_t sequenceStart = 0;
    uint32_t shortPressMaxMs;
    uint32_t patternTimeoutMs;
    static const uint8_t MAX_KEYS = 10;
    uint32_t lastDownTime[MAX_KEYS];
};
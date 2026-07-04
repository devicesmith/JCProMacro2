#pragma once
//#include "hsm-signals.h"

class PatternPressDetector {
public:
    PatternPressDetector(hsm_state_t* state, const bool* pattern, uint8_t patternLen,
                        uint32_t shortPressMaxMs = 400, uint32_t patternTimeoutMs = 3000)
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
        if (switch_id >= MAX_KEYS) {
            Serial.print("Pattern OOB down id=");
            Serial.println(switch_id);
            return;
        }
        Serial.print("Pattern down id=");
        Serial.println(switch_id);
        lastDownTime[switch_id] = millis();
    }

    // Call this when a button is released
    void onButtonUp(uint8_t switch_id) {
        if (switch_id >= MAX_KEYS) {
            Serial.print("Pattern OOB up id=");
            Serial.println(switch_id);
            return;
        }
        uint32_t now = millis();
        if (lastDownTime[switch_id] == 0) {
            Serial.print("Pattern up without down id=");
            Serial.println(switch_id);
            return;
        }
        uint32_t duration = now - lastDownTime[switch_id];
        lastDownTime[switch_id] = 0;

        if (count == 0) {
            sequenceStart = now;
        } else if (now - sequenceStart > patternTimeoutMs) {
            Serial.println("Timeout");
            reset();
            sequenceStart = now;
        }

        bool isLong = (duration > shortPressMaxMs);
        Serial.print("Pattern up id=");
        Serial.print(switch_id);
        Serial.print(" duration=");
        Serial.print(duration);
        Serial.print("ms expect=");
        Serial.println(pattern[count] ? "LONG" : "SHORT");
        if (isLong) {
            Serial.println("LONG");
        } else {
            Serial.println("SHORT");
        }
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
        Serial.println("RESET");
    }

private:
    hsm_state_t* state;
    const bool* pattern;
    uint8_t patternLen;
    uint8_t count = 0;
    uint32_t sequenceStart = 0;
    uint32_t shortPressMaxMs;
    uint32_t patternTimeoutMs;
    static const uint8_t MAX_KEYS = 12;
    uint32_t lastDownTime[MAX_KEYS];
};
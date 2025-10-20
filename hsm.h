#pragma once
#include <Arduino.h>
#include "hsm-signals.h"
#include "macropad-signals.h"

// Forward declaration for signal names array
extern char const * macropad_signal_names[];

#define ARRAY_LENGTH(array) (sizeof(array)/sizeof(*(array)))
#define STATE_DEPTH_MAX 10
#define HSM_STATE_CAST(x) ((hsm_state_t*)(x))

typedef enum {
    HSM_STATE_IGNORED,      // 0
    HSM_STATE_HANDLED,      // 1
    HSM_STATE_CHANGED,      // 2
    HSM_STATE_DO_SUPERSTATE // 3
} hsm_state_result_t;

typedef uint8_t hsm_signal_t;

// Base structure for events in the HSM.
// State machines will derive from this structure to create specific event types.
struct hsm_event {
    hsm_signal_t signal;
    hsm_event(hsm_signal_t sig = HSM_SIG_NONE)
        : signal(sig) {};
};
typedef hsm_event hsm_event_t;

class hsm_state_t;

typedef hsm_state_result_t (*state_handler_t)(hsm_state_t * state,
                                              hsm_event_t const * hsmEvent);

const int MAX_EVENTS = 16;

class EventQueue {
private:
    hsm_event_t events[MAX_EVENTS];
    uint8_t head = 0;
    uint8_t tail = 0;
    uint8_t size = 0;

public:
    EventQueue() = default;
    ~EventQueue() = default;

    bool registerEvent(hsm_signal_t signal) {
        if (size == MAX_EVENTS) {
            return false;
        }
        events[tail] = hsm_event_t(signal);
        tail = (tail + 1) % MAX_EVENTS;
        size++;
        return true;
    }

    bool retrieveEvent(hsm_event_t** event) {
        if (size == 0) {
            *event = nullptr;
            return false;
        }
        *event = &events[head];
        head = (head + 1) % MAX_EVENTS;
        size--;
        return true;
    }

    inline bool isEmpty() const {
        return size == 0;
    }

    inline bool isFull() const {
        return size == MAX_EVENTS;
    }

    inline int getSize() const {
        return size;
    }
};

#define HSM_EVENT_QUEUE_SIZE 8

class hsm_state_t {
public:
    virtual void EventQueuePush(hsm_signal_t signal);
    hsm_event_t* EventQueuePop();
    int EventQueueGetSize();

    state_handler_t GetStateHandler();
    void SetStateHandler(state_handler_t st);
    hsm_state_result_t StateHandler(hsm_event_t const * e);
    ~hsm_state_t();

private:
    state_handler_t stateHandler;
    EventQueue Events;
};

#define HSM_DEBUG_LOGGING
#define HSM_DEBUG_EXTENDED
//#define HSME_DEBUG_LOG_STATE

#ifdef HSM_DEBUG_LOGGING
    #define HSM_DEBUG_LOG(x) (Serial.print(__func__),Serial.print(":"), Serial.print(__LINE__),Serial.print(":"),Serial.println(x))
#else
    #define HSM_DEBUG_LOG(x) (void(x))
#endif

#ifdef HSM_DEBUG_LOG_STATE
    #define HSM_DEBUG_LOG_STATE(x) (Serial.print(__func__),Serial.print(":"), Serial.print(__LINE__),Serial.print(":"),Serial.println(x))
#else
    #define HSM_DEBUG_LOG_STATE(x) (void(x))
#endif


//#define HSM_DEBUG_LOGGGING_EXTENDED
#ifdef HSM_DEBUG_LOGGGING_EXTENDED
    #define HSM_DEBUG_LOG_STATE_EVENT(stateData, e) { \
        (void)(stateData); \
        if (true) { \
            Serial.print(__func__); \
            Serial.print("->"); \
            if ((e)->signal < SIG_LAST) { \
                Serial.println(macropad_signal_names[(e)->signal]); \
            } else { \
                Serial.print("UNKNOWN_SIGNAL("); \
                Serial.print((e)->signal); \
                Serial.println(")"); \
            } \
        } \
    } 
#else
    #define HSM_DEBUG_LOG_STATE_EVENT(stateData, e) { \
        (void)(stateData); \
        uint8_t ignore_signal[] = {HSM_SIG_SILENT, HSM_SIG_INITIAL_TRANS, HSM_STATE_IGNORED, SIG_TICK}; \
        bool print_signal = true; \
        for (unsigned int i = 0; i < sizeof(ignore_signal)/sizeof(ignore_signal[0]); ++i) { \
            if ((e)->signal == ignore_signal[i]) { \
                print_signal = false; \
                break; \
            } \
        } \
        if (print_signal) { \
            Serial.print(__func__); \
            Serial.print("->"); \
            Serial.println(macropad_signal_names[(e)->signal]); \
        } \
    } 
#endif

#define CHANGE_STATE(current_state_data, new_state) (current_state_data->SetStateHandler((new_state)), \
                    HSM_DEBUG_LOG("CHANGE_STATE"), HSM_STATE_CHANGED)

#define HANDLE_STATE() (HSM_DEBUG_LOG_STATE("HANDLE_STATE"), HSM_STATE_HANDLED)

#define IGNORE_STATE(x) (void(x), HSM_DEBUG_LOG_STATE("IGNORE_STATE"), HSM_STATE_IGNORED)

#define HANDLE_SUPER_STATE(state_data, super_state) (state_data->SetStateHandler((super_state)),\
                                                     HSM_DEBUG_LOG_STATE("HANDLE_SUPER_STATE"), \
                                                     HSM_STATE_DO_SUPERSTATE)

#define STATE_SEARCH_PATH_DEPTH 10

class HSM {
private:
    hsm_event_t silentEvent;
    hsm_event_t entryEvent;
    hsm_event_t exitEvent;
    hsm_event_t initEvent;
    EventQueue inQueue;

public:
    HSM();
    void SetInitialState(state_handler_t initialState);
    void ProcessInQueue(hsm_state_t * state);
    void Process();
    virtual hsm_state_t * GetStateData() = 0;

    static hsm_state_result_t rootState(hsm_state_t * state,
                                        hsm_event_t const * hsmEvent);

    void EventQueuePush(hsm_signal_t signal);
    hsm_event_t* EventQueuePop();
    int EventQueueGetSize();

private:
    int CheckForHandlerInPath(hsm_state_t * state,
                              hsm_state_t pathToStateArray[],
                              int pathToStateArrayDepth);
    hsm_state_result_t callStateHandler(hsm_state_t * state,
                                        hsm_event_t const * e,
                                        bool log);
    int DiscoverHierarchyToRootState(hsm_state_t * targetState,
                                     hsm_state_t pathToTargetArray[],
                                     int pathToTargetMaxDepth);
    int DiscoverHierarchy(hsm_state_t * topState,
                          hsm_state_t * bottomState,
                          hsm_state_t pathToTargetArray[],
                          int pathToTargetMaxDepth);
};
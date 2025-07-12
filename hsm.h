#pragma once  
#include <Arduino.h>

#define ARRAY_LENGTH(array) (sizeof(array)/sizeof(*(array)))

#define STATE_DEPTH_MAX 10

#define HSM_STATE_CAST(x) ((hsm_state_t*)(x))

typedef enum
{
    HSM_STATE_IGNORED, 		// 0
    HSM_STATE_HANDLED, 		// 1
    HSM_STATE_CHANGED, 		// 2
    HSM_STATE_DO_SUPERSTATE // 3
} hsm_state_result_t;

enum hsm_signal
{
   HSM_SIG_NONE = 0,
   HSM_SIG_SILENT,        // 1  : Falls through to superstate handler
   HSM_SIG_ENTRY,         // 2
   HSM_SIG_EXIT,          // 3
   HSM_SIG_INITIAL_TRANS, // 4
   HSM_SIG_USER           // 5
};

typedef unsigned int hsm_signal_t;

// Base structure for events in the HSM.
// State machines will derive from this structure to create specific event types.
struct hsm_event
{
   hsm_signal_t signal;

   hsm_event(hsm_signal_t sig = HSM_SIG_NONE)
     : signal(sig) {};
};
typedef hsm_event hsm_event_t;

class hsm_state_t;

typedef hsm_state_result_t (*state_handler_t)(hsm_state_t * state,
                                              hsm_event_t const * hsmEvent);

const int MAX_EVENTS = 16;


class EventQueue
{
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

class hsm_state_t
{
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

#ifdef HSM_DEBUG_LOGGING

extern int signal_filter[10];
extern bool print_signal;

#define HSM_DEBUG_LOG_STATE_EVENT(stateData, e) { \
    int *f = std::find(std::begin(signal_filter), std::end(signal_filter), e->signal); \
    print_signal = (f == std::end(signal_filter)); \
    if (print_signal) printf("%s(e:%d) -> ", __func__, (e)->signal); }

#define HSM_DEBUG_PRINT(x) (print_signal ? printf("%s [%s]\n", __func__, (x)) : HSM_STATE_HANDLED)

#else
#define HSM_DEBUG_LOG_STATE_EVENT(stateData, e) (void(stateData), void(e))
#define HSM_DEBUG_PRINT(x) (void(x))
#endif

#define CHANGE_STATE(current_state_data, new_state) (current_state_data->SetStateHandler((new_state)), \
                    HSM_DEBUG_PRINT("CHANGE_STATE"), HSM_STATE_CHANGED)

#define HANDLE_STATE() (HSM_DEBUG_PRINT("HANDLE_STATE"), HSM_STATE_HANDLED)

#define IGNORE_STATE(x) (void(x), HSM_DEBUG_PRINT("IGNORE_STATE"), HSM_STATE_IGNORED)

#define HANDLE_SUPER_STATE(state_data, super_state) (state_data->SetStateHandler((super_state)),\
                                                     HSM_DEBUG_PRINT("HANDLE_SUPER_STATE"), \
                                                     HSM_STATE_DO_SUPERSTATE)

#define STATE_SEARCH_PATH_DEPTH 10

class HSM
{
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
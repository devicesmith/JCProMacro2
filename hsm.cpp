#include "hsm.h"

int signal_filter[10] = {5};
bool print_signal = false;

void hsm_state_t::EventQueuePush(hsm_signal_t signal) {
    Events.registerEvent(signal);
}

hsm_event_t* hsm_state_t::EventQueuePop() {
    hsm_event_t* event = nullptr;
    if (Events.retrieveEvent(&event)) {
        return event;
    }
    return nullptr;
}

int hsm_state_t::EventQueueGetSize() {
    return Events.getSize();
}

state_handler_t hsm_state_t::GetStateHandler() {
    return stateHandler;
}

void hsm_state_t::SetStateHandler(state_handler_t st) {
    stateHandler = st;
}

hsm_state_result_t hsm_state_t::StateHandler(hsm_event_t const * e) {
    return stateHandler(this, e);
}

hsm_state_t::~hsm_state_t() {
}

HSM::HSM() {
    silentEvent.signal = HSM_SIG_SILENT;
    entryEvent.signal = HSM_SIG_ENTRY;
    exitEvent.signal = HSM_SIG_EXIT;
    initEvent.signal = HSM_SIG_INITIAL_TRANS;
}

hsm_state_result_t HSM::callStateHandler(hsm_state_t * state,
                                         hsm_event_t const * e,
                                         bool log) {
    hsm_state_result_t result;
    result = state->StateHandler(e);
    return result;
}

hsm_state_result_t HSM::rootState(hsm_state_t * me,
                                  hsm_event_t const *hsmEvent) {
    HSM_DEBUG_LOG_STATE_EVENT(me, hsmEvent);
    return IGNORE_STATE();
}

int HSM::CheckForHandlerInPath(hsm_state_t * state,
                               hsm_state_t pathToStateArray[],
                               int pathToStateArrayDepth) {
    for (int i = 0; i < pathToStateArrayDepth; ++i) {
        if (state->GetStateHandler() == pathToStateArray[i].GetStateHandler()) {
            return i;
        }
    }
    return -1;
}

int HSM::DiscoverHierarchyToRootState(hsm_state_t * state,
                                      hsm_state_t pathToTargetStateArray[],
                                      int pathToTargetMaxDepth) {
    if (pathToTargetStateArray == nullptr || pathToTargetMaxDepth < 1) {
        return 0;
    }

    hsm_state_result_t stateResult;
    int index = 0;
    hsm_state_t originalState;
    originalState.SetStateHandler(state->GetStateHandler());

    do {
        pathToTargetStateArray[index++] = *state;
        stateResult = callStateHandler(state, &silentEvent, false);
    } while (stateResult == HSM_STATE_DO_SUPERSTATE && index < pathToTargetMaxDepth);

    state->SetStateHandler(originalState.GetStateHandler());
    return index;
}

int HSM::DiscoverHierarchy(hsm_state_t * topState,
                           hsm_state_t * bottomState,
                           hsm_state_t pathToTargetStateArray[],
                           int pathToTargetMaxDepth) {
    if (pathToTargetStateArray == nullptr || pathToTargetMaxDepth < 1) {
        return 0;
    }

    int index = 0;
    hsm_state_t originalState;
    originalState.SetStateHandler(bottomState->GetStateHandler());

    while (bottomState->GetStateHandler() != topState->GetStateHandler() && index < pathToTargetMaxDepth) {
        pathToTargetStateArray[index++] = *bottomState;
        callStateHandler(bottomState, &silentEvent, false);
    }

    bottomState->SetStateHandler(originalState.GetStateHandler());
    return index;
}

void HSM::SetInitialState(state_handler_t initialState) {
    hsm_state_t topState;
    topState.SetStateHandler(rootState);
    callStateHandler(&topState, &initEvent, true);

    hsm_state_t pathToTargetState[STATE_DEPTH_MAX];
    hsm_state_t destinationState;
    destinationState.SetStateHandler(initialState);
    GetStateData()->SetStateHandler(initialState);

    do {
        int depth = DiscoverHierarchy(&topState, &destinationState, pathToTargetState, ARRAY_LENGTH(pathToTargetState));
        int index = depth - 1;
        do {
            callStateHandler(&pathToTargetState[index], &entryEvent, true);
        } while (pathToTargetState[index--].GetStateHandler() != GetStateData()->GetStateHandler());

        topState.SetStateHandler(pathToTargetState[0].GetStateHandler());
        destinationState.SetStateHandler(pathToTargetState[0].GetStateHandler());
        GetStateData()->SetStateHandler(pathToTargetState[0].GetStateHandler());
    } while (callStateHandler(GetStateData(), &initEvent, true) == HSM_STATE_CHANGED);
}

void HSM::Process() {
    ProcessInQueue(GetStateData());
}

void HSM::ProcessInQueue(hsm_state_t * st) {
    hsm_state_t initialState;
    
    while (st->EventQueueGetSize() > 0) {
        initialState.SetStateHandler(st->GetStateHandler());
        hsm_event_t* e = st->EventQueuePop();
        hsm_state_result_t currentResult;
        bool selfTrans = false;
        bool processing = false;
        bool backToSelfTop = false;
        bool backToSelfBottom = false;

        hsm_state_t lastState;
        hsm_state_t stateHandlingEvent;

        lastState.SetStateHandler(st->GetStateHandler());
        do {
            stateHandlingEvent.SetStateHandler(st->GetStateHandler());
            currentResult = callStateHandler(st, e, true);
            selfTrans = (st->GetStateHandler() == lastState.GetStateHandler());
            lastState.SetStateHandler(st->GetStateHandler());
        } while (currentResult == HSM_STATE_DO_SUPERSTATE);

        backToSelfTop = ((st->GetStateHandler() == initialState.GetStateHandler()) &&
                         (st->GetStateHandler() != stateHandlingEvent.GetStateHandler()));

        if (currentResult == HSM_STATE_HANDLED || currentResult == HSM_STATE_IGNORED) {
            st->SetStateHandler(initialState.GetStateHandler());
        } else if (currentResult == HSM_STATE_CHANGED) {
            hsm_state_t destinationState;
            hsm_state_t pathToTargetState[STATE_DEPTH_MAX];
            destinationState.SetStateHandler(st->GetStateHandler());
            st->SetStateHandler(initialState.GetStateHandler());

            processing = true;
            while (processing) {
                DiscoverHierarchyToRootState(&destinationState, pathToTargetState, ARRAY_LENGTH(pathToTargetState));
                do {
                    int index = CheckForHandlerInPath(st, pathToTargetState, ARRAY_LENGTH(pathToTargetState));
                    if (index == 0) {
                        if (selfTrans) {
                            callStateHandler(st, &exitEvent, true);
                        } else if (backToSelfTop) {
                            destinationState.SetStateHandler(stateHandlingEvent.GetStateHandler());
                            backToSelfTop = false;
                            backToSelfBottom = true;
                            break;
                        } else if (backToSelfBottom) {
                            destinationState.SetStateHandler(lastState.GetStateHandler());
                            backToSelfBottom = false;
                            break;
                        } else {
                            if (HSM_STATE_CHANGED != callStateHandler(&destinationState, &initEvent, true)) {
                                st->SetStateHandler(pathToTargetState[0].GetStateHandler());
                                processing = false;
                            }
                            break;
                        }
                    } else if (index > 0) {
                        int entryIndex;
                        for (entryIndex = index - 1; entryIndex >= 0; entryIndex--) {
                            st->SetStateHandler(pathToTargetState[entryIndex].GetStateHandler());
                            callStateHandler(st, &entryEvent, true);
                        }
                        st->SetStateHandler(pathToTargetState[0].GetStateHandler());
                        if (HSM_STATE_CHANGED == callStateHandler(st, &initEvent, true)) {
                            destinationState.SetStateHandler(st->GetStateHandler());
                            stateHandlingEvent.SetStateHandler(pathToTargetState[0].GetStateHandler());
                        } else {
                            processing = false;
                        }
                        st->SetStateHandler(pathToTargetState[0].GetStateHandler());
                        break;
                    } else {
                        callStateHandler(st, &exitEvent, true);
                    }
                } while (HSM_STATE_DO_SUPERSTATE == callStateHandler(st, &silentEvent, false));
            }
        }
    }
}
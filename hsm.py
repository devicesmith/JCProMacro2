"""Reusable hierarchical state machine core for CircuitPython projects."""

# Result codes match the original C++ values.
HSM_STATE_IGNORED = 0
HSM_STATE_HANDLED = 1
HSM_STATE_CHANGED = 2
HSM_STATE_DO_SUPERSTATE = 3

# Reserved framework signals.
HSM_SIG_NONE = 0
HSM_SIG_SILENT = 1
HSM_SIG_ENTRY = 2
HSM_SIG_EXIT = 3
HSM_SIG_INITIAL_TRANS = 4
HSM_SIG_USER = 5

STATE_DEPTH_MAX = 10

# Set to True to enable trace output from handle_state/change_state/etc.
HSM_DEBUG = False

TRACE_SILENT_SIGNALS = set()

# Replace with a signal-name lookup, e.g. macropad_signals.signal_name
_signal_name_fn = str

# Internal: populated by _call_state_handler before each handler call.
_dispatch_ctx = {"handler": "?", "signal": 0}


def trace_silence(*signals):
    TRACE_SILENT_SIGNALS.update(signals)


def trace_unsilence(*signals):
    for signal in signals:
        TRACE_SILENT_SIGNALS.discard(signal)


def _trace(action, extra=""):
    sig = _dispatch_ctx["signal"]
    if sig in TRACE_SILENT_SIGNALS:
        return
    handler = _dispatch_ctx["handler"]
    msg = "[HSM] {}: {}({}) in {}".format(action, _signal_name_fn(sig), sig, handler)
    if extra:
        msg += " " + extra
    print(msg)


class Event:
    __slots__ = ("signal", "payload")

    def __init__(self, signal=HSM_SIG_NONE, payload=None):
        self.signal = signal
        self.payload = payload


class EventQueue:
    def __init__(self, max_events=16):
        self._events = [None] * max_events
        self._head = 0
        self._tail = 0
        self._size = 0

    def register_event(self, signal, payload=None):
        if self._size == len(self._events):
            return False
        self._events[self._tail] = Event(signal, payload)
        self._tail = (self._tail + 1) % len(self._events)
        self._size += 1
        return True

    def retrieve_event(self):
        if self._size == 0:
            return None
        event = self._events[self._head]
        self._events[self._head] = None
        self._head = (self._head + 1) % len(self._events)
        self._size -= 1
        return event

    def is_empty(self):
        return self._size == 0

    def is_full(self):
        return self._size == len(self._events)

    def get_size(self):
        return self._size


class StateData:
    def __init__(self, max_events=16):
        self._state_handler = None
        self._events = EventQueue(max_events)

    def event_queue_push(self, signal, payload=None):
        return self._events.register_event(signal, payload)

    def event_queue_pop(self):
        return self._events.retrieve_event()

    def event_queue_get_size(self):
        return self._events.get_size()

    def get_state_handler(self):
        return self._state_handler

    def set_state_handler(self, state_handler):
        self._state_handler = state_handler

    def state_handler(self, event):
        return self._state_handler(self, event)


def change_state(state_data, new_state):
    state_data.set_state_handler(new_state)
    if HSM_DEBUG:
        _trace("CHANGE", "-> {}".format(getattr(new_state, "__name__", str(new_state))))
    return HSM_STATE_CHANGED


def handle_state():
    if HSM_DEBUG:
        _trace("HANDLED")
    return HSM_STATE_HANDLED


def ignore_state():
    if HSM_DEBUG:
        _trace("IGNORED")
    return HSM_STATE_IGNORED


def handle_super_state(state_data, super_state):
    state_data.set_state_handler(super_state)
    if HSM_DEBUG:
        _trace("SUPER", "-> {}".format(getattr(super_state, "__name__", str(super_state))))
    return HSM_STATE_DO_SUPERSTATE


class HSM:
    def __init__(self):
        self._silent_event = Event(HSM_SIG_SILENT)
        self._entry_event = Event(HSM_SIG_ENTRY)
        self._exit_event = Event(HSM_SIG_EXIT)
        self._init_event = Event(HSM_SIG_INITIAL_TRANS)
        self._in_queue = EventQueue()

    def get_state_data(self):
        raise NotImplementedError

    @staticmethod
    def root_state(_state_data, _event):
        return ignore_state()

    def event_queue_push(self, signal, payload=None):
        #print(f"Event pushed: signal={signal}, payload={payload}")
        return self._in_queue.register_event(signal, payload)

    def _call_state_handler(self, state_data, event):
        if HSM_DEBUG:
            _dispatch_ctx["handler"] = getattr(
                state_data.get_state_handler(), "__name__", str(state_data.get_state_handler())
            )
            _dispatch_ctx["signal"] = event.signal
        return state_data.state_handler(event)

    def _get_parent_handler(self, handler):
        state_data = self.get_state_data()
        original_handler = state_data.get_state_handler()
        state_data.set_state_handler(handler)
        result = self._call_state_handler(state_data, self._silent_event)
        parent = None
        if result == HSM_STATE_DO_SUPERSTATE:
            parent = state_data.get_state_handler()
        state_data.set_state_handler(original_handler)
        return parent

    def _build_path_to_root(self, handler):
        path = []
        current = handler
        depth = 0
        while current is not None and depth < STATE_DEPTH_MAX:
            path.append(current)
            parent = self._get_parent_handler(current)
            if parent is None or parent == current:
                break
            current = parent
            depth += 1
        return path

    def _find_lca(self, source_path, target_path):
        for source_handler in source_path:
            for target_handler in target_path:
                if source_handler == target_handler:
                    return source_handler
        return None

    def _run_entry_exit_transition(self, source_handler, target_handler):
        state_data = self.get_state_data()
        if source_handler == target_handler:
            state_data.set_state_handler(source_handler)
            self._call_state_handler(state_data, self._exit_event)
            state_data.set_state_handler(target_handler)
            self._call_state_handler(state_data, self._entry_event)
            return

        source_path = self._build_path_to_root(source_handler)
        target_path = self._build_path_to_root(target_handler)
        lca = self._find_lca(source_path, target_path)

        for handler in source_path:
            if handler == lca:
                break
            state_data.set_state_handler(handler)
            self._call_state_handler(state_data, self._exit_event)

        entry_path = []
        for handler in target_path:
            if handler == lca:
                break
            entry_path.append(handler)

        for handler in reversed(entry_path):
            state_data.set_state_handler(handler)
            self._call_state_handler(state_data, self._entry_event)

        state_data.set_state_handler(target_handler)

    def _run_initial_transitions(self):
        state_data = self.get_state_data()
        guard = 0
        while guard < STATE_DEPTH_MAX:
            current_handler = state_data.get_state_handler()
            result = self._call_state_handler(state_data, self._init_event)
            if result != HSM_STATE_CHANGED:
                state_data.set_state_handler(current_handler)
                return
            target_handler = state_data.get_state_handler()
            self._run_entry_exit_transition(current_handler, target_handler)
            guard += 1

    def set_initial_state(self, initial_state):
        state_data = self.get_state_data()
        state_data.set_state_handler(initial_state)

        target_path = self._build_path_to_root(initial_state)
        for handler in reversed(target_path):
            state_data.set_state_handler(handler)
            self._call_state_handler(state_data, self._entry_event)

        state_data.set_state_handler(initial_state)
        self._run_initial_transitions()

    def _dispatch_event(self, event):
        state_data = self.get_state_data()
        source_handler = state_data.get_state_handler()
        if source_handler is None:
            return

        dispatch_handler = source_handler
        while True:
            state_data.set_state_handler(dispatch_handler)
            result = self._call_state_handler(state_data, event)

            if result == HSM_STATE_DO_SUPERSTATE:
                parent = state_data.get_state_handler()
                if parent is None or parent == dispatch_handler:
                    state_data.set_state_handler(source_handler)
                    return
                dispatch_handler = parent
                continue

            if result in (HSM_STATE_HANDLED, HSM_STATE_IGNORED):
                state_data.set_state_handler(source_handler)
                return

            if result == HSM_STATE_CHANGED:
                target_handler = state_data.get_state_handler()
                self._run_entry_exit_transition(source_handler, target_handler)
                self._run_initial_transitions()
                return

            state_data.set_state_handler(source_handler)
            return

    def process(self):
        state_data = self.get_state_data()

        while self._in_queue.get_size() > 0:
            # print(f"Processing event queue, size: {self._in_queue.get_size()}")
            event = self._in_queue.retrieve_event()
            if event is not None:
                state_data.event_queue_push(event.signal, event.payload)

        while state_data.event_queue_get_size() > 0:
            event = state_data.event_queue_pop()
            if event is not None:
                #print(f"Dispatching event: signal={event.signal}, payload={event.payload}")
                self._dispatch_event(event)

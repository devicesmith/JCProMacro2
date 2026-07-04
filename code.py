"""CircuitPython entry point for the MacroPad HSM port."""

import time

try:
    from adafruit_macropad import MacroPad  # type: ignore[import-not-found]
except ImportError:
    MacroPad = None

import hsm as _hsm_module
from hsm import (
    HSM,
    HSM_SIG_ENTRY,
    HSM_SIG_EXIT,
    HSM_SIG_INITIAL_TRANS,
    StateData,
    change_state,
    handle_state,
    handle_super_state,
)
from macropad_signals import (
    SIG_ENC_DOWN,
    SIG_ENC_UP,
    SIG_K1_DOWN,
    SIG_K1_UP,
    SIG_K2_DOWN,
    SIG_K2_UP,
    SIG_K3_DOWN,
    SIG_K3_UP,
    SIG_K4_DOWN,
    SIG_K4_UP,
    SIG_K5_DOWN,
    SIG_K5_UP,
    SIG_K6_DOWN,
    SIG_K6_UP,
    SIG_K7_DOWN,
    SIG_K7_UP,
    SIG_K8_DOWN,
    SIG_K8_UP,
    SIG_K9_DOWN,
    SIG_K9_UP,
    SIG_K10_DOWN,
    SIG_K10_UP,
    SIG_K11_DOWN,
    SIG_K11_UP,
    SIG_K12_DOWN,
    SIG_K12_UP,
    SIG_TICK,
    SIG_VOL_DEC,
    SIG_VOL_INC,
)


KEYENC_BIT_POS = 0
DEBUG_ENCODER_BUTTON_SIGNALS = True
DEBUG_ENQUEUE = True
HSM_DEBUG = True

# Wire signal name lookup and debug flag into the hsm module.
from macropad_signals import signal_name as _sig_name_fn  # noqa: E402
_hsm_module._signal_name_fn = _sig_name_fn
_hsm_module.HSM_DEBUG = HSM_DEBUG
_hsm_module.trace_silence(SIG_TICK, _hsm_module.HSM_SIG_SILENT, HSM_SIG_ENTRY, HSM_SIG_EXIT)

BIT_POS_TO_DOWN_SIGNAL = {
    0: SIG_ENC_DOWN,
    1: SIG_K1_DOWN,
    2: SIG_K2_DOWN,
    3: SIG_K3_DOWN,
    4: SIG_K4_DOWN,
    5: SIG_K5_DOWN,
    6: SIG_K6_DOWN,
    7: SIG_K7_DOWN,
    8: SIG_K8_DOWN,
    9: SIG_K9_DOWN,
    10: SIG_K10_DOWN,
    11: SIG_K11_DOWN,
    12: SIG_K12_DOWN,
}

BIT_POS_TO_UP_SIGNAL = {
    0: SIG_ENC_UP,
    1: SIG_K1_UP,
    2: SIG_K2_UP,
    3: SIG_K3_UP,
    4: SIG_K4_UP,
    5: SIG_K5_UP,
    6: SIG_K6_UP,
    7: SIG_K7_UP,
    8: SIG_K8_UP,
    9: SIG_K9_UP,
    10: SIG_K10_UP,
    11: SIG_K11_UP,
    12: SIG_K12_UP,
}


def _debug_pressed_labels(bitmask):
    labels = []
    if bitmask & (1 << KEYENC_BIT_POS):
        labels.append("ENC")

    for key_number in range(12):
        if bitmask & (1 << (key_number + 1)):
            labels.append("K{}".format(key_number + 1))

    return labels


class MacroPadInputScanner:
    def __init__(self, macropad):
        self._macropad = macropad
        self._key_bits = 0
        self._last_reported_bits = None

    def get_keys(self):
        while True:
            key_event = self._macropad.keys.events.get()
            if key_event is None:
                break

            bit_pos = key_event.key_number + 1
            bit_mask = 1 << bit_pos

            if key_event.pressed:
                self._key_bits |= bit_mask
            elif key_event.released:
                self._key_bits &= ~bit_mask

        encoder_mask = 1 << KEYENC_BIT_POS
        # On this setup encoder_switch=True represents pressed.
        if self._macropad.encoder_switch:
            self._key_bits |= encoder_mask
        else:
            self._key_bits &= ~encoder_mask

        if self._key_bits != self._last_reported_bits:
            labels = _debug_pressed_labels(self._key_bits)
            if labels:
                print("keys=0x{:04X} pressed={}".format(self._key_bits, ", ".join(labels)))
            else:
                print("keys=0x{:04X} pressed=none".format(self._key_bits))
            self._last_reported_bits = self._key_bits

        return self._key_bits

    def get_encoder(self):
        return int(self._macropad.encoder)


def getKeys(scanner):
    return scanner.get_keys()


def getEncoder(scanner):
    return scanner.get_encoder()


class InputEventUpdater:
    def __init__(self):
        self._prev_keys = 0
        self._prev_encoder = 0
        self._accumulated_encoder_delta = 0
        self._encoder_threshold = 1

    def update_key_events(self, current_keys, state_data):
        for bit_pos in range(13):
            mask = 1 << bit_pos
            prev_pressed = (self._prev_keys & mask) != 0
            curr_pressed = (current_keys & mask) != 0

            if curr_pressed == prev_pressed:
                continue

            if curr_pressed:
                signal = BIT_POS_TO_DOWN_SIGNAL.get(bit_pos)
            else:
                signal = BIT_POS_TO_UP_SIGNAL.get(bit_pos)

            if signal is not None:
                state_data.event_queue_push(signal)

        self._prev_keys = current_keys

    def update_encoder_events(self, current_encoder, state_data):
        delta = current_encoder - self._prev_encoder
        self._accumulated_encoder_delta += delta

        while self._accumulated_encoder_delta >= self._encoder_threshold:
            state_data.event_queue_push(SIG_VOL_DEC)
            self._accumulated_encoder_delta -= self._encoder_threshold

        while self._accumulated_encoder_delta <= -self._encoder_threshold:
            state_data.event_queue_push(SIG_VOL_INC)
            self._accumulated_encoder_delta += self._encoder_threshold

        self._prev_encoder = current_encoder


class MacroPadStateData(StateData):
    def __init__(self):
        super().__init__(max_events=32)
        self.down_color = 0xFF0000
        self.up_color = 0x00FF00

    def event_queue_push(self, signal, payload=None):
        from macropad_signals import signal_name as _sig_name
        from hsm import HSM_SIG_ENTRY, HSM_SIG_EXIT, HSM_SIG_INITIAL_TRANS, HSM_SIG_SILENT
        _silent_signals = (SIG_TICK, HSM_SIG_ENTRY, HSM_SIG_EXIT, HSM_SIG_INITIAL_TRANS, HSM_SIG_SILENT)
        if DEBUG_ENQUEUE and signal not in _silent_signals:
            print("enqueue: {}({})".format(_sig_name(signal), signal))
        return super().event_queue_push(signal, payload)


class MacroPadMachine(HSM):
    def __init__(self):
        super().__init__()
        self._state_data = MacroPadStateData()

    def get_state_data(self):
        return self._state_data

    @staticmethod
    def top_state(state_data, event):
        if event.signal == HSM_SIG_ENTRY:
            return handle_state()
        if event.signal == HSM_SIG_EXIT:
            return handle_state()
        if event.signal == HSM_SIG_INITIAL_TRANS:
            return handle_state()
        return handle_super_state(state_data, HSM.root_state)

    @staticmethod
    def mode_ubuntu_state(state_data, event):
        if event.signal == HSM_SIG_ENTRY:
            print("enter mode_ubuntu_state")
            return handle_state()
        if event.signal == HSM_SIG_EXIT:
            print("exit mode_ubuntu_state")
            return handle_state()
        if event.signal == SIG_ENC_UP:
            print("transition -> mode_switch_apps_state")
            return change_state(state_data, MacroPadMachine.mode_switch_apps_state)
        if event.signal == SIG_TICK:
            print("mode_ubuntu_state tick")
            return handle_state()
        return handle_super_state(state_data, MacroPadMachine.top_state)

    @staticmethod
    def mode_switch_apps_state(state_data, event):
        if event.signal == HSM_SIG_ENTRY:
            print("enter mode_switch_apps_state")
            return handle_state()
        if event.signal == HSM_SIG_EXIT:
            print("exit mode_switch_apps_state")
            return handle_state()
        if event.signal == SIG_ENC_UP:
            print("transition -> mode_ubuntu_state")
            return change_state(state_data, MacroPadMachine.mode_ubuntu_state)
        if event.signal == SIG_TICK:
            print("mode_switch_apps_state tick")
            return handle_state()
        return handle_super_state(state_data, MacroPadMachine.top_state)


def main():
    if MacroPad is None:
        raise RuntimeError("Install adafruit_macropad on CIRCUITPY/lib before running code.py")

    print("MacroPad HSM port starting...")
    macropad = MacroPad()
    input_scanner = MacroPadInputScanner(macropad)
    machine = MacroPadMachine()
    input_event_updater = InputEventUpdater()
    machine.set_initial_state(MacroPadMachine.mode_ubuntu_state)

    last_tick = time.monotonic()
    while True:
        keys = getKeys(input_scanner)
        encoder = getEncoder(input_scanner)
        input_event_updater.update_key_events(keys, machine.get_state_data())
        input_event_updater.update_encoder_events(encoder, machine.get_state_data())
        now = time.monotonic()
        if now - last_tick >= 1.0:
            machine.event_queue_push(SIG_TICK)
            last_tick = now

        machine.process()


main()

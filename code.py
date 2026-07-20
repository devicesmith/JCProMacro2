"""CircuitPython entry point for the MacroPad HSM port."""

import gc
import os
import time

try:
    import displayio  # type: ignore[import-not-found]
except ImportError:
    displayio = None

try:
    import terminalio  # type: ignore[import-not-found]
    from adafruit_display_text import label as display_label  # type: ignore[import-not-found]
except ImportError:
    terminalio = None
    display_label = None


try:
    from adafruit_macropad import MacroPad  # type: ignore[import-not-found]
except ImportError:
    MacroPad = None

from macropad_machine import MACROPAD_MACHINE_VERSION, MacroPadMachine
from macropad_signals import (
    SIG_ENC_CCW,
    SIG_ENC_CW,
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
)


KEYENC_BIT_POS = 0
DEBUG_ENCODER_BUTTON_SIGNALS = True
OLED_UPDATE_INTERVAL = 0.2

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


def print_memory_usage(label):
    gc.collect()
    ram_used = gc.mem_alloc()
    ram_free = gc.mem_free()
    vfs = os.statvfs("/")
    block_size = vfs[0]
    fs_total = block_size * vfs[2]
    fs_free = block_size * vfs[3]
    fs_used = fs_total - fs_free
    print(
        "[{}]\n    RAM used={:,} free={:,}\n    FS used={:,} free={:,} total={:,}".format(
            label,
            ram_used,
            ram_free,
            fs_used,
            fs_free,
            fs_total,
        )
    )


class OledStatusDisplay:
    def __init__(self, macropad, max_lines=4, max_chars=21):
        self._enabled = (
            displayio is not None
            and terminalio is not None
            and display_label is not None
            and hasattr(macropad, "display")
        )
        self._max_lines = max_lines
        self._max_chars = max_chars
        self._labels = []
        self._lines = [""] * max_lines

        if not self._enabled:
            return

        root_group = displayio.Group()
        for line_index in range(max_lines):
            line_label = display_label.Label(
                terminalio.FONT,
                text="",
                color=0xFFFFFF,
                x=0,
                y=8 + (line_index * 12),
            )
            self._labels.append(line_label)
            root_group.append(line_label)

        macropad.display.root_group = root_group

    @property
    def enabled(self):
        return self._enabled

    def _fit(self, text):
        return str(text)[: self._max_chars]

    def set_line(self, line_index, text):
        if line_index < 0 or line_index >= self._max_lines:
            return

        fitted_text = self._fit(text)
        self._lines[line_index] = fitted_text

        if self._enabled:
            self._labels[line_index].text = fitted_text

    def show(self, *lines):
        for line_index in range(self._max_lines):
            text = lines[line_index] if line_index < len(lines) else ""
            self.set_line(line_index, text)

    def append(self, text):
        fitted_text = self._fit(text)
        self._lines = self._lines[1:] + [fitted_text]
        for line_index in range(self._max_lines):
            if self._enabled:
                self._labels[line_index].text = self._lines[line_index]


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
            state_data.event_queue_push(SIG_ENC_CW)
            self._accumulated_encoder_delta -= self._encoder_threshold

        while self._accumulated_encoder_delta <= -self._encoder_threshold:
            state_data.event_queue_push(SIG_ENC_CCW)
            self._accumulated_encoder_delta += self._encoder_threshold

        self._prev_encoder = current_encoder


def main():
    if MacroPad is None:
        raise RuntimeError("Install adafruit_macropad on CIRCUITPY/lib before running code.py")

    print("MacroPad HSM port starting...")
    print_memory_usage("after imports")
    macropad = MacroPad()
    oled = OledStatusDisplay(macropad)
    if oled.enabled:
        oled.show("HSM " + MACROPAD_MACHINE_VERSION, "booting...")
    else:
        print("OLED display skipped: display text modules not available")
    input_scanner = MacroPadInputScanner(macropad)
    machine = MacroPadMachine(macropad)
    input_event_updater = InputEventUpdater()
    machine.set_initial_state(MacroPadMachine.mode_ubuntu_state)
    print_memory_usage("after setup")
    if oled.enabled:
        oled.show("HSM " + MACROPAD_MACHINE_VERSION, "ready")

    last_tick = time.monotonic()
    last_oled_update = 0
    while True:
        keys = getKeys(input_scanner)
        encoder = getEncoder(input_scanner)
        state_data = machine.get_state_data()
        input_event_updater.update_key_events(keys, state_data)
        input_event_updater.update_encoder_events(encoder, state_data)
        now = time.monotonic()
        if now - last_tick >= machine.TICK_INTERVAL:
            machine.event_queue_push(SIG_TICK)
            last_tick = now

        machine.process()

        if oled.enabled and (now - last_oled_update) >= OLED_UPDATE_INTERVAL:
            active_handler = state_data.get_state_handler()
            mode_name = getattr(active_handler, "__name__", "unknown")
            mute_timer_seconds = state_data.mute_timer_ticks_remaining // machine.TICKS_PER_SECOND
            mute_status = "M:ON" if state_data.is_muted else "M:OFF"
            if state_data.mute_timer_ticks_remaining > 0:
                mute_status += " T:{}s".format(mute_timer_seconds)
            oled.show(
                "HSM " + MACROPAD_MACHINE_VERSION,
                "mode:" + mode_name,
                "enc:{} keys:{:04X}".format(encoder, keys),
                mute_status,
            )
            last_oled_update = now


main()

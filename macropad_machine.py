"""MacroPad-specific hierarchical state machine implementation."""

import hsm as _hsm_module
try:
    import usb_hid  # type: ignore[import-not-found]
    from adafruit_hid.consumer_control import ConsumerControl  # type: ignore[import-not-found]
    from adafruit_hid.consumer_control_code import ConsumerControlCode  # type: ignore[import-not-found]
except ImportError:
    usb_hid = None
    ConsumerControl = None
    ConsumerControlCode = None

from hsm import (
    HSM,
    HSM_SIG_ENTRY,
    HSM_SIG_EXIT,
    HSM_SIG_INITIAL_TRANS,
    HSM_SIG_SILENT,
    StateData,
    change_state,
    handle_state,
    handle_super_state,
)
from macropad_signals import (
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
    SIG_VOL_KNOB_DOWN,
    SIG_VOL_KNOB_UP,
    SIG_VOL_BTN_DOWN,
    SIG_VOL_BTN_UP,
    SIG_MUTE_BTN,
    SIG_ENC_DOWN,
    signal_name as _sig_name_fn,
)


DEBUG_ENQUEUE = True
HSM_DEBUG = True

REGULAR_BRIGHTNESS = 0.50
HIGH_BRIGHTNESS = 1.00

_hsm_module._signal_name_fn = _sig_name_fn
_hsm_module.HSM_DEBUG = HSM_DEBUG
_hsm_module.trace_silence(SIG_TICK, _hsm_module.HSM_SIG_SILENT, HSM_SIG_ENTRY, HSM_SIG_EXIT)


if ConsumerControl is not None and usb_hid is not None:
    _consumer_control = ConsumerControl(usb_hid.devices)
else:
    _consumer_control = None


def _send_volume_down():
    if _consumer_control is None:
        print("volume down skipped: adafruit_hid not available")
        return
    _consumer_control.send(ConsumerControlCode.VOLUME_DECREMENT)


def _send_volume_up():
    if _consumer_control is None:
        print("volume up skipped: adafruit_hid not available")
        return
    _consumer_control.send(ConsumerControlCode.VOLUME_INCREMENT)


def _send_mute_toggle():
    if _consumer_control is None:
        print("mute toggle skipped: adafruit_hid not available")
        return
    _consumer_control.send(ConsumerControlCode.MUTE)

class MacroPadStateData(StateData):
    def __init__(self):
        super().__init__(max_events=32)
        self.down_color = 0xFF0000
        self.up_color = 0x00FF00
        self.is_muted = False

    def event_queue_push(self, signal, payload=None):
        silent_signals = (
            SIG_TICK,
            HSM_SIG_ENTRY,
            HSM_SIG_EXIT,
            HSM_SIG_INITIAL_TRANS,
            HSM_SIG_SILENT,
        )
        if DEBUG_ENQUEUE and signal not in silent_signals:
            print("enqueue: {}({})".format(_sig_name_fn(signal), signal))
        return super().event_queue_push(signal, payload)


def _signal_to_order(signal):
    signal_to_order_map = {
        SIG_K1_DOWN: 0,
        SIG_K1_UP: 0,
        SIG_K2_DOWN: 1,
        SIG_K2_UP: 1,
        SIG_K3_DOWN: 2,
        SIG_K3_UP: 2,
        SIG_K4_DOWN: 3,
        SIG_K4_UP: 3,
        SIG_K5_DOWN: 4,
        SIG_K5_UP: 4,
        SIG_K6_DOWN: 5,
        SIG_K6_UP: 5,
        SIG_K7_DOWN: 6,
        SIG_K7_UP: 6,
        SIG_K8_DOWN: 7,
        SIG_K8_UP: 7,
        SIG_K9_DOWN: 8,
        SIG_K9_UP: 8,
        SIG_K10_DOWN: 9,
        SIG_K10_UP: 9,
        SIG_K11_DOWN: 10,
        SIG_K11_UP: 10,
        SIG_K12_DOWN: 11,
        SIG_K12_UP: 11,
    }
    return signal_to_order_map.get(signal, -1)


def _int_color_to_rgb(color):
    return ((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF)


class MacroPadMachine(HSM):
    def __init__(self, macropad=None):
        super().__init__()
        self._state_data = MacroPadStateData()
        self._state_data.machine = self
        self._pixels = None
        if macropad is not None and hasattr(macropad, "pixels"):
            self._pixels = macropad.pixels
            self._pixels.brightness = REGULAR_BRIGHTNESS

    def _clearMuteFlagAndIndicator(self):
        self._state_data.is_muted = False
        self.set_key_color(SIG_MUTE_BTN, self._state_data.up_color)

    def _apply_mute_indicator(self):
        mute_color = 0xFF0000 if self._state_data.is_muted else self._state_data.up_color
        self.set_key_color(SIG_MUTE_BTN, mute_color)

    def toggle_mute(self):
        _send_mute_toggle()
        self._state_data.is_muted = not self._state_data.is_muted
        self._apply_mute_indicator()

    def clear_mute(self):
        if self._state_data.is_muted:
            self.toggle_mute()

    def set_all_key_colors(self, color):
        if self._pixels is None:
            return
        self._pixels.fill(_int_color_to_rgb(color))
        self._pixels.show()

    def set_key_color(self, signal, color):
        if self._pixels is None:
            return
        order = _signal_to_order(signal)
        if order < 0 or order >= len(self._pixels):
            return
        self._pixels[order] = _int_color_to_rgb(color)
        self._pixels.show()

    def set_keys_brightness(self, brightness):
        if self._pixels is None:
            return
        print("brght:", brightness)
        self._pixels.brightness = brightness
        self._pixels.show()

    def get_state_data(self):
        return self._state_data

    @staticmethod
    def top_state(state_data, event):
        machine = state_data.machine
        if event.signal == HSM_SIG_ENTRY:
            return handle_state()
        if event.signal == HSM_SIG_EXIT:
            return handle_state()
        if event.signal == HSM_SIG_INITIAL_TRANS:
            return handle_state()
        if event.signal in (
            SIG_K1_DOWN,
            SIG_K2_DOWN,
            SIG_K3_DOWN,
            SIG_K4_DOWN,
            SIG_K5_DOWN,
            SIG_K6_DOWN,
            SIG_K7_DOWN,
            SIG_K8_DOWN,
            SIG_K9_DOWN,
            SIG_K10_DOWN,
            SIG_K11_DOWN,
            #SIG_K12_DOWN,
        ):
            machine.set_key_color(event.signal, state_data.down_color)
            return handle_state()
        if event.signal in (
            SIG_K1_UP,
            SIG_K2_UP,
            SIG_K3_UP,
            SIG_K4_UP,
            SIG_K5_UP,
            SIG_K6_UP,
            SIG_K7_UP,
            SIG_K8_UP,
            SIG_K9_UP,
            SIG_K10_UP,
            SIG_K11_UP,
            #SIG_K12_UP,
        ):
            machine.set_key_color(event.signal, state_data.up_color)
            return handle_state()
        if event.signal == SIG_ENC_DOWN:
            machine.set_keys_brightness(HIGH_BRIGHTNESS)
            machine.set_key_color(event.signal, state_data.down_color)
            return handle_state()
        return handle_super_state(state_data, HSM.root_state)

    @staticmethod
    def mode_ubuntu_state(state_data, event):
        machine = state_data.machine
        if event.signal == HSM_SIG_ENTRY:
            print("enter mode_ubuntu_state")
            state_data.down_color = 0xFF0000
            state_data.up_color = 0x00FF00
            machine.set_keys_brightness(REGULAR_BRIGHTNESS)
            machine.set_all_key_colors(state_data.up_color)
            machine._apply_mute_indicator()
            return handle_state()
        if event.signal == HSM_SIG_EXIT:
            print("exit mode_ubuntu_state")
            return handle_state()
        if event.signal == SIG_ENC_UP:
            print("transition -> mode_switch_apps_state")
            return change_state(state_data, MacroPadMachine.mode_switch_apps_state)
        if event.signal == SIG_VOL_KNOB_DOWN or event.signal == SIG_VOL_BTN_DOWN:
            machine.clear_mute()
            _send_volume_down()
            return handle_state()
        if event.signal == SIG_VOL_KNOB_UP or event.signal == SIG_VOL_BTN_UP:
            machine.clear_mute()
            _send_volume_up()
            return handle_state()
        if event.signal == SIG_MUTE_BTN:
            machine.toggle_mute()
            machine._apply_mute_indicator()
            return handle_state()
        if event.signal == SIG_TICK:
            return handle_state()
        return handle_super_state(state_data, MacroPadMachine.top_state)

    @staticmethod
    def mode_switch_apps_state(state_data, event):
        machine = state_data.machine
        if event.signal == HSM_SIG_ENTRY:
            print("enter mode_switch_apps_state")
            state_data.down_color = 0xFF0000
            state_data.up_color = 0x0000FF
            machine.set_keys_brightness(REGULAR_BRIGHTNESS)
            machine.set_all_key_colors(state_data.up_color)
            return handle_state()
        if event.signal == HSM_SIG_EXIT:
            print("exit mode_switch_apps_state")
            return handle_state()
        if event.signal == SIG_ENC_UP:
            print("transition -> mode_ubuntu_state")
            return change_state(state_data, MacroPadMachine.mode_ubuntu_state)
        if event.signal == SIG_TICK:
            return handle_state()
        return handle_super_state(state_data, MacroPadMachine.top_state)
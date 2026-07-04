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
from macropad_signals import SIG_ENC_UP, SIG_TICK, SIG_VOLUME_DOWN, SIG_VOLUME_UP, signal_name as _sig_name_fn


DEBUG_ENQUEUE = True
HSM_DEBUG = True

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


class MacroPadStateData(StateData):
    def __init__(self):
        super().__init__(max_events=32)
        self.down_color = 0xFF0000
        self.up_color = 0x00FF00

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
        if event.signal == SIG_VOLUME_DOWN:
            _send_volume_down()
            return handle_state()
        if event.signal == SIG_VOLUME_UP:
            _send_volume_up()
            return handle_state()
        if event.signal == SIG_TICK:
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
            return handle_state()
        return handle_super_state(state_data, MacroPadMachine.top_state)
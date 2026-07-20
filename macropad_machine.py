"""MacroPad-specific hierarchical state machine implementation."""

import hsm as _hsm_module
try:
    import usb_hid  # type: ignore[import-not-found]
    from adafruit_hid.consumer_control import ConsumerControl  # type: ignore[import-not-found]
    from adafruit_hid.consumer_control_code import ConsumerControlCode  # type: ignore[import-not-found]
    from adafruit_hid.keycode import Keycode  # type: ignore[import-not-found]
except ImportError:
    usb_hid = None
    ConsumerControl = None
    ConsumerControlCode = None
    Keycode = None

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

import macropad_signals as sig

DEBUG_ENQUEUE = True
HSM_DEBUG = True

REGULAR_BRIGHTNESS = 0.50
HIGH_BRIGHTNESS = 1.00
VOL_BTN_TICK_THRESHOLD = 4  # Number of ticks before sending repeated volume change events
MUTE_TIMER_SECONDS = 30

#_hsm_module._signal_name_fn = _sig_name_fn
_hsm_module._signal_name_fn = sig.signal_name
_hsm_module.HSM_DEBUG = HSM_DEBUG
_hsm_module.trace_silence(sig.SIG_TICK, _hsm_module.HSM_SIG_SILENT, HSM_SIG_ENTRY, HSM_SIG_EXIT)


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
        self.vol_dn_btn_is_dn = False
        self.vol_up_btn_is_dn = False
        self.vol_btn_tick_count = 0
        self.pw_tick_count = 0
        self.mute_timer_ticks_remaining = 0

    def event_queue_push(self, signal, payload=None):
        silent_signals = (
            sig.SIG_TICK,
            HSM_SIG_ENTRY,
            HSM_SIG_EXIT,
            HSM_SIG_INITIAL_TRANS,
            HSM_SIG_SILENT,
        )
        if DEBUG_ENQUEUE and signal not in silent_signals:
            print("enqueue: {}({})".format(sig.signal_name(signal), signal))
        return super().event_queue_push(signal, payload)


def _signal_to_order(signal):
    signal_to_order_map = {
        sig.SIG_K1_DOWN: 0,
        sig.SIG_K1_UP: 0,
        sig.SIG_K2_DOWN: 1,
        sig.SIG_K2_UP: 1,
        sig.SIG_K3_DOWN: 2,
        sig.SIG_K3_UP: 2,
        sig.SIG_K4_DOWN: 3,
        sig.SIG_K4_UP: 3,
        sig.SIG_K5_DOWN: 4,
        sig.SIG_K5_UP: 4,
        sig.SIG_K6_DOWN: 5,
        sig.SIG_K6_UP: 5,
        sig.SIG_K7_DOWN: 6,
        sig.SIG_K7_UP: 6,
        sig.SIG_K8_DOWN: 7,
        sig.SIG_K8_UP: 7,
        sig.SIG_K9_DOWN: 8,
        sig.SIG_K9_UP: 8,
        sig.SIG_K10_DOWN: 9,
        sig.SIG_K10_UP: 9,
        sig.SIG_K11_DOWN: 10,
        sig.SIG_K11_UP: 10,
        sig.SIG_K12_DOWN: 11,
        sig.SIG_K12_UP: 11,
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
        self._keyboard = None
        self._keyboard_layout = None
        if macropad is not None and hasattr(macropad, "pixels"):
            self._pixels = macropad.pixels
            self._pixels.brightness = REGULAR_BRIGHTNESS
        if macropad is not None and hasattr(macropad, "keyboard"):
            self._keyboard = macropad.keyboard
        if macropad is not None and hasattr(macropad, "keyboard_layout"):
            self._keyboard_layout = macropad.keyboard_layout
        self.TICK_INTERVAL = 0.1
        self.TICKS_PER_SECOND = int(1.0 / self.TICK_INTERVAL)

    def _send_keyboard_text(self, text, press_enter=False):
        if self._keyboard_layout is None:
            print("keyboard text skipped: keyboard_layout not available")
            return
        self._keyboard_layout.write(text)
        if press_enter and self._keyboard is not None and Keycode is not None:
            self._keyboard.send(Keycode.ENTER)

    def _clearMuteFlagAndIndicator(self):
        self._state_data.is_muted = False
        self.set_key_color(sig.SIG_MUTE_BTN, self._state_data.up_color)
        self.set_key_color(sig.SIG_MUTE_TMR_BTN, self._state_data.up_color)

    def _apply_mute_indicator(self):
        timer_running = self._state_data.mute_timer_ticks_remaining > 0
        mute_color = 0xFF0000 if self._state_data.is_muted and not timer_running else self._state_data.up_color
        timer_color = 0xFF0000 if timer_running else self._state_data.up_color
        self.set_key_color(sig.SIG_MUTE_BTN, mute_color)
        self.set_key_color(sig.SIG_MUTE_TMR_BTN, timer_color)

    def toggle_mute(self):
        _send_mute_toggle()
        self._state_data.is_muted = not self._state_data.is_muted
        self._apply_mute_indicator()

    def clear_mute(self):
        if self._state_data.is_muted:
            self.toggle_mute()

    def _cancel_mute_timer(self):
        if self._state_data.mute_timer_ticks_remaining > 0:
            self._state_data.mute_timer_ticks_remaining = 0
            self._apply_mute_indicator()

    def _start_or_extend_mute_timer(self):
        timer_ticks = MUTE_TIMER_SECONDS * self.TICKS_PER_SECOND
        if self._state_data.mute_timer_ticks_remaining <= 0:
            self._state_data.mute_timer_ticks_remaining = timer_ticks
            if not self._state_data.is_muted:
                self.toggle_mute()
            else:
                self._apply_mute_indicator()
            return
        self._state_data.mute_timer_ticks_remaining += timer_ticks
        self._apply_mute_indicator()

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
            sig.SIG_K1_DOWN,
            sig.SIG_K2_DOWN,
            sig.SIG_K3_DOWN,
            sig.SIG_K4_DOWN,
            sig.SIG_K5_DOWN,
            sig.SIG_K6_DOWN,
            sig.SIG_K7_DOWN,
            sig.SIG_K8_DOWN,
            sig.SIG_K9_DOWN,
            sig.SIG_VOL_DN_BTN_DN,
            sig.SIG_VOL_UP_BTN_DN,
        ):
            machine.set_key_color(event.signal, state_data.down_color)
            return handle_state()
        if event.signal in (
            sig.SIG_K1_UP,
            sig.SIG_K2_UP,
            sig.SIG_K3_UP,
            sig.SIG_K4_UP,
            sig.SIG_K5_UP,
            sig.SIG_K6_UP,
            sig.SIG_K7_UP,
            sig.SIG_K8_UP,
            sig.SIG_K9_UP,
            sig.SIG_VOL_DN_BTN_UP,
            sig.SIG_VOL_UP_BTN_UP,
        ):
            machine.set_key_color(event.signal, state_data.up_color)
            return handle_state()
        if event.signal == sig.SIG_ENC_DOWN:
            machine.set_keys_brightness(HIGH_BRIGHTNESS)
            machine.set_key_color(event.signal, state_data.down_color)
            return handle_state()

        if event.signal == sig.SIG_VOL_UP_BTN_DN:

            return handle_state()

        if event.signal == sig.SIG_PW1:
            machine._send_keyboard_text("PW1", press_enter=True)
            return handle_state()
        if event.signal == sig.SIG_PW2:
            machine._send_keyboard_text("PW2", press_enter=True)
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
        if event.signal == sig.SIG_ENC_UP:
            print("transition -> mode_switch_apps_state")
            return change_state(state_data, MacroPadMachine.mode_switch_apps_state)
        if event.signal == sig.SIG_VOL_KNOB_DOWN:
            machine._cancel_mute_timer()
            machine.clear_mute()
            _send_volume_down()
            return handle_state()
        if event.signal == sig.SIG_VOL_KNOB_UP:
            machine._cancel_mute_timer()
            machine.clear_mute()
            _send_volume_up()
            return handle_state()
        if event.signal == sig.SIG_VOL_DN_BTN_DN:
            machine._cancel_mute_timer()
            machine.clear_mute()
            machine._state_data.vol_dn_btn_is_dn = True
            _send_volume_down()
            #return handle_state()
            return handle_super_state(state_data, MacroPadMachine.top_state)
        if event.signal == sig.SIG_VOL_UP_BTN_DN:
            machine._cancel_mute_timer()
            machine.clear_mute()
            machine._state_data.vol_up_btn_is_dn = True
            _send_volume_up()
            return handle_super_state(state_data, MacroPadMachine.top_state)
        if event.signal == sig.SIG_VOL_DN_BTN_UP:
            machine._state_data.vol_dn_btn_is_dn = False
            machine._state_data.vol_btn_tick_count = 0
            return handle_super_state(state_data, MacroPadMachine.top_state)
        if event.signal == sig.SIG_VOL_UP_BTN_UP:
            machine._state_data.vol_up_btn_is_dn = False
            machine._state_data.vol_btn_tick_count = 0
            return handle_super_state(state_data, MacroPadMachine.top_state)
        if event.signal == sig.SIG_MUTE_BTN:
            if machine._state_data.mute_timer_ticks_remaining > 0 or machine._state_data.is_muted:
                machine._cancel_mute_timer()
                machine.clear_mute()
            else:
                machine.toggle_mute()
            return handle_state()
        if event.signal == sig.SIG_MUTE_TMR_BTN:
            machine._start_or_extend_mute_timer()
            return handle_state()
        if event.signal == sig.SIG_K9_UP:
            machine._apply_mute_indicator()
            return handle_state()
        if event.signal == sig.SIG_TICK:
            if machine._state_data.mute_timer_ticks_remaining > 0:
                machine._state_data.mute_timer_ticks_remaining -= 1
                if machine._state_data.mute_timer_ticks_remaining <= 0:
                    machine._state_data.mute_timer_ticks_remaining = 0
                    machine.clear_mute()
            if machine._state_data.vol_dn_btn_is_dn:
                machine._state_data.vol_btn_tick_count += 1
                if machine._state_data.vol_btn_tick_count > VOL_BTN_TICK_THRESHOLD:
                    _send_volume_down()
            if machine._state_data.vol_up_btn_is_dn:
                machine._state_data.vol_btn_tick_count += 1
                if machine._state_data.vol_btn_tick_count > VOL_BTN_TICK_THRESHOLD:
                    _send_volume_up()
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
        if event.signal == sig.SIG_ENC_UP:
            print("transition -> mode_ubuntu_state")
            return change_state(state_data, MacroPadMachine.mode_ubuntu_state)
        if event.signal == sig.SIG_TICK:
            return handle_state()
        if event.signal == sig.SIG_K1_DOWN:
            machine.event_queue_push(sig.SIG_PW1)
            return handle_state()
        if event.signal == sig.SIG_K2_DOWN:
            machine.event_queue_push(sig.SIG_PW2)
            return handle_state()
        if event.signal == sig.SIG_K3_DOWN:
            print("switch to log_ubuntu_in")
            return change_state(state_data, MacroPadMachine.log_ubuntu_in)
        return handle_super_state(state_data, MacroPadMachine.top_state)
    
    @staticmethod
    def log_ubuntu_in(state_data, event):
        machine = state_data.machine
        if event.signal == HSM_SIG_ENTRY:
            print("enter log_ubuntu_in")
            state_data.down_color = 0xFF0000
            state_data.up_color = 0xFFA500
            machine.set_keys_brightness(REGULAR_BRIGHTNESS)
            machine.set_all_key_colors(state_data.up_color)
            return handle_state()
        if event.signal == HSM_SIG_EXIT:
            state_data.pw_tick_count = 0
            print("exit log_ubuntu_in")
            return handle_state()
        if event.signal == sig.SIG_TICK:
            state_data.pw_tick_count += 1
            if state_data.pw_tick_count == 2 * machine.TICKS_PER_SECOND:
                machine.event_queue_push(sig.SIG_PW1)
            if state_data.pw_tick_count == 4 * machine.TICKS_PER_SECOND:
                machine.event_queue_push(sig.SIG_PW2)
            if state_data.pw_tick_count > 8 * machine.TICKS_PER_SECOND:
                return change_state(state_data, MacroPadMachine.mode_switch_apps_state)
            return handle_state()
        if event.signal == sig.SIG_K1_DOWN:
            return change_state(state_data, MacroPadMachine.mode_switch_apps_state)

        return handle_super_state(state_data, MacroPadMachine.top_state)
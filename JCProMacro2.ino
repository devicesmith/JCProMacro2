#include <Keyboard.h>

#include <Adafruit_SH110X.h>
#include <Adafruit_NeoPixel.h>
#include <RotaryEncoder.h>
#include <Wire.h>

#include "hsm.h"
#include "macropad-signals.h"
#include "PatternPressDetector.h"

#define BTN_DEC_VOL_KEYDOWN SIG_K10_DOWN
#define BTN_DEC_VOL_KEYUP SIG_K10_UP
#define BTN_INC_VOL_KEYDOWN SIG_K11_DOWN
#define BTN_INC_VOL_KEYUP SIG_K11_UP
#define BTN_MUTE_KEYDOWN SIG_K12_DOWN
#define BTN_MUTE_KEYUP SIG_K12_UP
#define BTN_MUTE_TIMER_KEYDOWN SIG_K9_DOWN
#define BTN_MUTE_TIMER_KEYUP SIG_K9_UP


// Create the neopixel strip with the built in definitions NUM_NEOPIXEL and PIN_NEOPIXEL
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_NEOPIXEL, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// Create the OLED display
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &SPI1, OLED_DC, OLED_RST, OLED_CS);

// Create the rotary encoder
RotaryEncoder encoder(PIN_ROTA, PIN_ROTB, RotaryEncoder::LatchMode::FOUR3);
void checkPosition() {  encoder.tick(); } // just call tick() to check the state.
// our encoder position state
int encoder_pos = 0;

#define USE_DEBUG_OUTPUT 0
#if USE_DEBUG_OUTPUT
#define DEBUG_LOG_STATE_EVENT(e) { \
    bool print_signal = true; \
    uint8_t ignore_signal[] = {HSM_SIG_SILENT, HSM_SIG_INITIAL_TRANS, HSM_STATE_IGNORED, SIG_TICK}; \
    for (unsigned int i = 0; i < sizeof(ignore_signal)/sizeof(ignore_signal[0]); ++i) { \
        if ((e)->signal == ignore_signal[i]) { \
            print_signal = false; \
            break; \
        } \
    } \
    if (print_signal) { \
        Serial.print(__func__); \
        Serial.print(":"); \
        Serial.print(__LINE__); \
        Serial.print("->"); \
        Serial.print(jcpm_signal_names[e->signal]); \
        Serial.print(" ("); Serial.print(e->signal); Serial.println(")"); \
    } \
} 
#else

#define DEBUG_LOG_STATE_EVENT(x) (void(x))

#endif

int signal_to_order(int signal) {
  switch (signal) {
    case SIG_K1_DOWN: case SIG_K1_UP: return 0;
    case SIG_K2_DOWN: case SIG_K2_UP: return 1;
    case SIG_K3_DOWN: case SIG_K3_UP: return 2;
    case SIG_K4_DOWN: case SIG_K4_UP: return 3;
    case SIG_K5_DOWN: case SIG_K5_UP: return 4;
    case SIG_K6_DOWN: case SIG_K6_UP: return 5;
    case SIG_K7_DOWN: case SIG_K7_UP: return 6;
    case SIG_K8_DOWN: case SIG_K8_UP: return 7;
    case SIG_K9_DOWN: case SIG_K9_UP: return 8;
    case SIG_K10_DOWN: case SIG_K10_UP: return 9;
    case SIG_K11_DOWN: case SIG_K11_UP: return 10;
    case SIG_K12_DOWN: case SIG_K12_UP: return 11;
    default: return -1;
  }
}

void KeyColorsSet(int r, int g, int b) {
  for (int i = 0; i < NUM_NEOPIXEL; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();  // Show results
}

void KeyColorSet(int signal, uint32_t c) {
  int order = signal_to_order(signal);
  
  if (order < 0 || order > NUM_NEOPIXEL) {
    return;
  }
  pixels.setPixelColor(order, c);
  pixels.show();  // Show results
}

void clearMuteFlag(bool *muted, bool *muted_timer) {
  *muted = false;
  *muted_timer = false;
  KeyColorSet(BTN_MUTE_KEYDOWN, 0x00FF00);
  KeyColorSet(BTN_MUTE_TIMER_KEYDOWN, 0x00FF00);
}

void sendMuteCommand() {
  Keyboard.consumerPress(KEY_MUTE);
  delay(100);
  Keyboard.consumerRelease();
}

struct state_data_t : hsm_state_t {
  uint32_t down_color = 0xFF0000; // Default color for keys
  uint32_t up_color   = 0x00FF00; // Default color for keys
};

class JCPMMachine : public HSM {
public:
  JCPMMachine();

  static hsm_state_result_t TopState(hsm_state_t *stateData, hsm_event_t const *e);
  static hsm_state_result_t ModeUbuntuState(hsm_state_t *stateData, hsm_event_t const *e);
  static hsm_state_result_t ModeUbuntuSwitchAppsState(hsm_state_t *stateData, hsm_event_t const *e);

  hsm_state_t * GetStateData();

private:
  // Declare the state data structure for the HSM
  state_data_t stateData;
};

// Constructor for the JCPMMachine class
JCPMMachine::JCPMMachine() {}

hsm_state_t * JCPMMachine::GetStateData() {
  return &stateData;
}

JCPMMachine jcpmHSM;


hsm_state_result_t JCPMMachine::TopState(hsm_state_t *stateData, hsm_event_t const *e) {
  state_data_t* derivedStateData = static_cast<state_data_t*>(stateData);
  const uint32_t down_color = derivedStateData->down_color;
  const uint32_t up_color = derivedStateData->up_color;

  HSM_DEBUG_LOG_STATE_EVENT(stateData, e);
  DEBUG_LOG_STATE_EVENT(e);

  switch (e->signal) {
    case HSM_SIG_ENTRY:
      return HANDLE_STATE();
    case HSM_SIG_EXIT:
      return HANDLE_STATE();
    case HSM_SIG_INITIAL_TRANS:
      return HANDLE_STATE();

    // Toggle colors here, for all keys, if not handled by a specific state
    case SIG_K1_DOWN: case SIG_K2_DOWN: case SIG_K3_DOWN: 
    case SIG_K4_DOWN: case SIG_K5_DOWN: case SIG_K6_DOWN: 
    case SIG_K7_DOWN: case SIG_K8_DOWN: case SIG_K9_DOWN:
    case SIG_K10_DOWN: case SIG_K11_DOWN: case SIG_K12_DOWN:
      KeyColorSet(e->signal, down_color);
      return HANDLE_STATE();
    case SIG_K1_UP: case SIG_K2_UP: case SIG_K3_UP:
    case SIG_K4_UP: case SIG_K5_UP: case SIG_K6_UP:
    case SIG_K7_UP: case SIG_K8_UP: case SIG_K9_UP:
    case SIG_K10_UP: case SIG_K11_UP: case SIG_K12_UP:
      KeyColorSet(e->signal, up_color);
      return HANDLE_STATE();

    case SIG_TICK:
      return HANDLE_STATE();

    default:
      return HANDLE_SUPER_STATE(stateData, &HSM::rootState);
  }
}

void showModeUbuntuScreen() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("               | VOL ");
  display.println("               v SCRN");
  display.println("------+-------+------");
  display.println("      |       |      ");
  display.println("------+-------+------");
  display.println("      |       |MT TMR ");
  display.println("------+-------+-------");
  display.println("VOL v | VOL ^ | MUTE  ");
  display.display();
}

const int TICKS_PER_SECOND = 10;
const int MUTE_DURATION_SECONDS = 30;

hsm_state_result_t JCPMMachine::ModeUbuntuState(hsm_state_t *stateData, hsm_event_t const *e) {
  state_data_t* derivedStateData = static_cast<state_data_t*>(stateData);
  static bool muted = false;
  static bool muted_timer = false;
  static int muted_timer_ticks = 0;
  static bool mic_muted = false;
  static bool vol_dec_btn_is_down = false;
  static int vol_dec_wait = 0;
  static bool vol_inc_btn_is_down = false;
  static int vol_inc_wait = 0;

  HSM_DEBUG_LOG_STATE_EVENT(stateData, e);
  DEBUG_LOG_STATE_EVENT(e);

  switch (e->signal) {
    case HSM_SIG_ENTRY:
      derivedStateData->down_color = 0xFF0000; // Red when down
      derivedStateData->up_color = 0x00FF00;   // Green when up
      KeyColorsSet(0, 0xFF, 0); // Set initial colors for keys
      showModeUbuntuScreen(); // Display the mode 1 screen

      // Indicate muted active on return to this keyboard "level"
      if (muted) {
         KeyColorSet(BTN_MUTE_KEYDOWN, 0xFF0000);
      }
      // Indicate mute timer active on return to this keyboard "level"
      if (muted_timer) {
        KeyColorSet(BTN_MUTE_TIMER_KEYDOWN, 0xFF0000);
      }
      // if (mic_muted) {
      //   KeyColorSet(SIG_K03_DOWN, 0x7F7F00);
      // }
      return HANDLE_STATE();

    case HSM_SIG_EXIT:
      return HANDLE_STATE();

    case HSM_SIG_INITIAL_TRANS:
      return HANDLE_STATE();

    case BTN_DEC_VOL_KEYDOWN:
      Keyboard.consumerPress(KEY_VOLUME_DECREMENT);
      Keyboard.consumerRelease();
      clearMuteFlag(&muted, &muted_timer);
      vol_dec_btn_is_down = true;
      break;

    case BTN_DEC_VOL_KEYUP:
      vol_dec_btn_is_down = false;
      vol_dec_wait = 0;
      break; // Let default handler handle it (and call superstate)

    case BTN_INC_VOL_KEYDOWN:
      Keyboard.consumerPress(KEY_VOLUME_INCREMENT);
      Keyboard.consumerRelease();
      clearMuteFlag(&muted, &muted_timer);
      vol_inc_btn_is_down = true;
      break;

    case BTN_INC_VOL_KEYUP:
      vol_inc_btn_is_down = false;
      vol_inc_wait = 0;
      break; // Let default handler handle it (and call superstate)

    case BTN_MUTE_KEYDOWN:
      if (muted || muted_timer) {
        clearMuteFlag(&muted, &muted_timer);
      } else {
        KeyColorSet(e->signal, 0xFF0000);
        muted = true;
      }
      sendMuteCommand();
      return HANDLE_STATE(); // don't call superstate

    case BTN_MUTE_KEYUP:
      return HANDLE_STATE();

    // // Mute Microphone toggle
    // case SIG_K03_DOWN:
    //   if (mic_muted) {
    //     mic_muted = false;
    //     KeyColorSet(e->signal, 0x00FF00); // Green when unmuted
    //   } else {
    //     mic_muted = true;
    //     KeyColorSet(e->signal, 0x7F7F00);
    //   }
    //   muteMicrophoneToggle();
    //   return HANDLE_STATE();

    // case SIG_K03_UP:
    //   return HANDLE_STATE();

    // case SIG_K12_DOWN:
    //   ConsumerKeyboard.press(KEY_PLAY_PAUSE);
    //   ConsumerKeyboard.release();
    //   break;

    case BTN_MUTE_TIMER_KEYDOWN:
      if (muted_timer) {
        muted_timer_ticks += (TICKS_PER_SECOND * MUTE_DURATION_SECONDS);
      } else {
        if (!muted) {
          sendMuteCommand();
          muted = true;
        } else {
          KeyColorSet(BTN_MUTE_TIMER_KEYDOWN, 0x00FF00);
        }
        muted_timer = true;
        muted_timer_ticks = TICKS_PER_SECOND * MUTE_DURATION_SECONDS;
      }
      break;

    case BTN_MUTE_TIMER_KEYUP:
      return HANDLE_STATE();

    // case SIG_K22_DOWN:
    //   patternPressDetector.onButtonDown(KEY22_ORDER);
    //   break;
    // case SIG_K22_UP:
    //   patternPressDetector.onButtonUp(KEY22_ORDER);
    //   break;
    // case SIG_PATTERN_PRESS:
    //   Keyboard.print(pattern_match_text);
    //   return HANDLE_STATE();

    case SIG_ENC_UP:
      return CHANGE_STATE(stateData, &JCPMMachine::ModeUbuntuSwitchAppsState);

    case SIG_VOL_DEC:
      Keyboard.consumerPress(KEY_VOLUME_DECREMENT);
      Keyboard.consumerRelease();
      clearMuteFlag(&muted, &muted_timer);
      break;

    case SIG_VOL_INC:
      Keyboard.consumerPress(KEY_VOLUME_INCREMENT);
      Keyboard.consumerRelease();
      clearMuteFlag(&muted, &muted_timer);
      break;

    case SIG_TICK:
      // button down and wait a few ticks
      if (vol_dec_btn_is_down && ++vol_dec_wait > 3) {
        derivedStateData->EventQueuePush(BTN_DEC_VOL_KEYDOWN);
      }
      if (vol_inc_btn_is_down && ++vol_inc_wait > 3) {
        derivedStateData->EventQueuePush(BTN_INC_VOL_KEYDOWN);
      }
      if (muted_timer) {
        if (--muted_timer_ticks < 1) {
          clearMuteFlag(&muted, &muted_timer);
          sendMuteCommand();
          KeyColorSet(BTN_MUTE_TIMER_KEYDOWN, 0x00FF00);
        }
      }
      break;

    default:
      return HANDLE_SUPER_STATE(stateData, &JCPMMachine::TopState);
  }
  return HANDLE_SUPER_STATE(stateData, &JCPMMachine::TopState);
}

// void showInfoScreen() {
//   oled.clear();
//   oled.println("   JC Pro Macro 2    ");
//   oled.println(" ------------------- ");
//   oled.print(" v");
//   oled.println(VERSION);
//   oled.println("                     ");
//   oled.println(" By DeviceSmith      ");
//   oled.println("                     ");
//   oled.println("                     ");
//   oled.println("                     ");
// }

void showModeUbuntuSwitchAppsScreen() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("          |     |Sett");
  display.println("  Volume  |     |ings");
  display.println("   ----   +-----+----");
  display.println(" |        | Out |Chro");
  display.println(" v Next   | look|me  ");
  display.println("-----+----+-----+----");
  display.println("Term | VS |Obsid|Team");
  display.println("inal |Code|ian  |    ");
  display.display();
}

hsm_state_result_t JCPMMachine::ModeUbuntuSwitchAppsState(hsm_state_t *stateData, hsm_event_t const *e) {
  state_data_t* derivedStateData = static_cast<state_data_t*>(stateData);

  HSM_DEBUG_LOG_STATE_EVENT(stateData, e);
  DEBUG_LOG_STATE_EVENT(e);
  
  switch (e->signal) {
    case HSM_SIG_ENTRY:
      derivedStateData->down_color = 0xFF0000; // Red when down
      derivedStateData->up_color = 0x0000FF;   // Blue when up
      KeyColorsSet(0, 0, 0xFF); // Set initial colors for keys
      showModeUbuntuSwitchAppsScreen(); // Display the mode 2 screen
      return HANDLE_STATE();

//     case SIG_K00_UP:
//       linuxSwitchToApp("terminal");
//       return CHANGE_STATE(stateData, &JCPMMachine::ModeUbuntuState);
//     case SIG_K01_UP:
//       linuxSwitchToApp("visual studio code");
//       return CHANGE_STATE(stateData, &JCPMMachine::ModeUbuntuState);
//     case SIG_K02_UP:
//       linuxSwitchToApp("Obsidian");
//       return CHANGE_STATE(stateData, &JCPMMachine::ModeUbuntuState);
//     case SIG_K03_UP:
//       linuxSwitchToApp("microsoft teams");
//       return CHANGE_STATE(stateData, &JCPMMachine::ModeUbuntuState);
//     case SIG_K12_UP:
//       linuxSwitchToApp("outlook");
//       return CHANGE_STATE(stateData, &JCPMMachine::ModeUbuntuState);
//     case SIG_K13_UP:
//       linuxSwitchToApp("chrome");
//       return CHANGE_STATE(stateData, &JCPMMachine::ModeUbuntuState);
//     case SIG_K22_UP:
//       showInfoScreen();
//       break;
//     case SIG_K23_UP:
//       linuxSwitchToApp("settings");
//       return CHANGE_STATE(stateData, &JCPMMachine::ModeUbuntuState);

    case SIG_ENC_UP:
      return CHANGE_STATE(stateData, &JCPMMachine::ModeUbuntuState);
  }
  return HANDLE_SUPER_STATE(stateData, &JCPMMachine::TopState);
}







uint16_t getKeys() {
  uint16_t bitValues = 0;

  for (int i = 0; i <= 12; i++) {
    bitValues |= (!digitalRead(i) << i);
  }

  return bitValues;
}

int32_t getEncoder() {
  return encoder.getPosition();
}

#define KEYENC_BIT_POS 0
#define KEY1_BIT_POS 1
#define KEY2_BIT_POS 2
#define KEY3_BIT_POS 3
#define KEY4_BIT_POS 4
#define KEY5_BIT_POS 5
#define KEY6_BIT_POS 6
#define KEY7_BIT_POS 7
#define KEY8_BIT_POS 8
#define KEY9_BIT_POS 9
#define KEY10_BIT_POS 10
#define KEY11_BIT_POS 11
#define KEY12_BIT_POS 12

uint8_t keyBitPosToDownSignal(uint8_t bitPos) {
  switch (bitPos) {
    case KEY1_BIT_POS: return SIG_K1_DOWN;
    case KEY2_BIT_POS: return SIG_K2_DOWN;
    case KEY3_BIT_POS: return SIG_K3_DOWN;
    case KEY4_BIT_POS: return SIG_K4_DOWN;
    case KEY5_BIT_POS: return SIG_K5_DOWN;
    case KEY6_BIT_POS: return SIG_K6_DOWN;
    case KEY7_BIT_POS: return SIG_K7_DOWN;
    case KEY8_BIT_POS: return SIG_K8_DOWN;
    case KEY9_BIT_POS: return SIG_K9_DOWN;
    case KEY10_BIT_POS: return SIG_K10_DOWN;
    case KEY11_BIT_POS: return SIG_K11_DOWN;
    case KEY12_BIT_POS: return SIG_K12_DOWN;
    case KEYENC_BIT_POS: return SIG_ENC_DOWN;
    default: return HSM_SIG_NONE; // Invalid bit position
  }
}

uint8_t keyBitPosToUpSignal(uint8_t bitPos) {
  switch (bitPos) {
    case KEY1_BIT_POS: return SIG_K1_UP;
    case KEY2_BIT_POS: return SIG_K2_UP;
    case KEY3_BIT_POS: return SIG_K3_UP;
    case KEY4_BIT_POS: return SIG_K4_UP;
    case KEY5_BIT_POS: return SIG_K5_UP;
    case KEY6_BIT_POS: return SIG_K6_UP;
    case KEY7_BIT_POS: return SIG_K7_UP;
    case KEY8_BIT_POS: return SIG_K8_UP;
    case KEY9_BIT_POS: return SIG_K9_UP;
    case KEY10_BIT_POS: return SIG_K10_UP;
    case KEY11_BIT_POS: return SIG_K11_UP;
    case KEY12_BIT_POS: return SIG_K12_UP;
    case KEYENC_BIT_POS: return SIG_ENC_UP;
    default: return HSM_SIG_NONE; // Invalid bit position
  }
}


void updateKeyEvents(uint16_t currentKeys) {
  static uint16_t prevKeys = 0;

  // Check each key for changes
  for (int i = 0; i <= 12; i++) {
    uint16_t mask = 1 << i;
    bool prevPressed = (prevKeys & mask) != 0;
    bool currPressed = (currentKeys & mask) != 0;

    // if(currPressed) {
    //   Serial.print("currentKeys:"); Serial.print(currentKeys, HEX);Serial.print(" i:");Serial.print(i);
    //   Serial.print(" prev:");Serial.print(prevPressed);Serial.print( " cur:");Serial.println(currPressed);
    // }

    if (currPressed != prevPressed) {
      // Key state changed
      if (currPressed) {
        
        // Key pressed - add corresponding DOWN event
        uint8_t downSignal = keyBitPosToDownSignal(i);
        if (downSignal != HSM_SIG_NONE) {
          jcpmHSM.GetStateData()->EventQueuePush(downSignal);
          //Serial.print("i:");Serial.print(i);Serial.print(" ");Serial.println(jcpm_signal_names[downSignal]);
        }
      } else {
        // Key released - add corresponding UP event
        uint8_t upSignal = keyBitPosToUpSignal(i);
        if (upSignal != HSM_SIG_NONE) {
          jcpmHSM.GetStateData()->EventQueuePush(upSignal);
          //Serial.print("i:");Serial.print(i);Serial.print(" ");Serial.println(jcpm_signal_names[upSignal]);
        }
      }
    }
  }

  prevKeys = currentKeys;
}

void updateEncoderEvents(int32_t currentEncoder) {
  static int32_t prevEncoder = 0;
  static int32_t accumulatedDelta = 0;
  const int32_t ENCODER_THRESHOLD = 1; // Adjust this value based on your encoder sensitivity

  // Calculate the change in encoder position
  int32_t delta = currentEncoder - prevEncoder;
  accumulatedDelta += delta;
#if 0
  if (currentEncoder != prevEncoder) {
    Serial.print("curr ("); Serial.print(currentEncoder); Serial.print(") ");
    Serial.print("prev ("); Serial.print(prevEncoder); Serial.print(") ");
    Serial.print("D:"); Serial.print(delta); Serial.print(" acc:"); Serial.println(accumulatedDelta);
  }
#endif
  // Check if we've accumulated enough change to trigger an event
  while (accumulatedDelta >= ENCODER_THRESHOLD) {
    jcpmHSM.GetStateData()->EventQueuePush(SIG_VOL_DEC);
    accumulatedDelta -= ENCODER_THRESHOLD;
  }

  while (accumulatedDelta <= -ENCODER_THRESHOLD) {
    jcpmHSM.GetStateData()->EventQueuePush(SIG_VOL_INC);
    accumulatedDelta += ENCODER_THRESHOLD;
  }

  prevEncoder = currentEncoder;
}


//
// Set Up
//
void setup() {

  //Serial.println("Attempting kb begin");
  Keyboard.begin();
  delay(5000);


  //Serial.begin(115200);
  while (!Serial) { delay(10); }     // wait till serial port is opened
  delay(100);  // RP2040 delay is not a bad idea

  //Serial.println("Adafruit Macropad with RP2040");


  // start pixels!
  pixels.begin();
  pixels.setBrightness(255);
  pixels.show(); // Initialize all pixels to 'off'

  // Start OLED
  display.begin(0, true); // we dont use the i2c address but we will reset!
  display.display();
  
  // set all mechanical keys to inputs
  for (uint8_t i=0; i<=12; i++) {
    pinMode(i, INPUT_PULLUP);
  }

  // set rotary encoder inputs and interrupts
  pinMode(PIN_ROTA, INPUT_PULLUP);
  pinMode(PIN_ROTB, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_ROTA), checkPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ROTB), checkPosition, CHANGE);  

  // We will use I2C for scanning the Stemma QT port
  //Wire.begin();

  // text display tests
  display.setTextSize(1);
  display.setTextWrap(false);
  display.setTextColor(SH110X_WHITE, SH110X_BLACK); // white text, black background

  //Serial.println("Starup tone");
  // Enable speaker
  pinMode(PIN_SPEAKER_ENABLE, OUTPUT);
  digitalWrite(PIN_SPEAKER_ENABLE, HIGH);
  // Play some tones
  pinMode(PIN_SPEAKER, OUTPUT);
  digitalWrite(PIN_SPEAKER, LOW);
  tone(PIN_SPEAKER, 988, 100);  // tone1 - B5
  delay(100);
  tone(PIN_SPEAKER, 1319, 200); // tone2 - E6
  delay(200);

  jcpmHSM.SetInitialState(JCPMMachine::ModeUbuntuState);

}

//uint8_t j = 0;
//bool i2c_found[128] = {false};

void loop() {
  
  encoder.tick();          // check the encoder

  if (!digitalRead(PIN_SWITCH)) {
    pixels.setBrightness(255);     // bright!
  } else {
    pixels.setBrightness(80);
  }

  // show neopixels, incredment swirl
  pixels.show();

  uint16_t keys = getKeys();
  int encoder = getEncoder();

  updateKeyEvents(keys);
  updateEncoderEvents(encoder);

  static unsigned long tick = 0;
  unsigned long currentTick = millis();
  if (currentTick - tick > 100) {
    tick = currentTick;
    jcpmHSM.GetStateData()->EventQueuePush(SIG_TICK);
  }

  jcpmHSM.Process();

}

#if 0
// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return pixels.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return pixels.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
   WheelPos -= 170;
   return pixels.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}
#endif

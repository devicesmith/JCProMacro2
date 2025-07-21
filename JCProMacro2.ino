#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
#include <Encoder.h>
#include <HID-Project.h>
#include <Adafruit_NeoPixel.h>
#include "hsm.h"
#include "jcpm-hsm-mattmc-signals.h"
#include "PatternPressDetector.h"

#define VERSION "0.0.11"  // Version of the project

#define NUMPIXELS 8  // Popular NeoPixel ring size and the number of keys
#define PIN 5        // On Trinket or Gemma, suggest changing this to 1

#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define OLED_RESET 4         // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

SSD1306AsciiWire oled;
Encoder encoder(1, 0);
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Key names are defined like a grid. Lower left is KEY00
// KEY10, KEY11, KEY20, KEY21 are not present
// These values map key to Arduino pins
#define KEYENC 4
#define KEYMOD 8
#define KEY00 15
#define KEY01 A0
#define KEY02 A1
#define KEY03 10
#define KEY12 A2
#define KEY13 16
#define KEY22 A3
#define KEY23 14

// Bit positions for each key, when reading the key state
#define KEY00_BIT_POS 0
#define KEY01_BIT_POS 1
#define KEY02_BIT_POS 2
#define KEY03_BIT_POS 3
#define KEY12_BIT_POS 4
#define KEY13_BIT_POS 5
#define KEY22_BIT_POS 6
#define KEY23_BIT_POS 7
#define KEYENC_BIT_POS 8
#define KEYMOD_BIT_POS 9

// Bit masks for each key, when reading the key state
constexpr uint16_t KEY00_BIT (1 << KEY00_BIT_POS);
constexpr uint16_t KEY01_BIT (1 << KEY01_BIT_POS);
constexpr uint16_t KEY02_BIT (1 << KEY02_BIT_POS);
constexpr uint16_t KEY03_BIT (1 << KEY03_BIT_POS);
constexpr uint16_t KEY12_BIT (1 << KEY12_BIT_POS);
constexpr uint16_t KEY13_BIT (1 << KEY13_BIT_POS);
constexpr uint16_t KEY22_BIT (1 << KEY22_BIT_POS);
constexpr uint16_t KEY23_BIT (1 << KEY23_BIT_POS);
constexpr uint16_t KEYENC_BIT (1 << KEYENC_BIT_POS);
constexpr uint16_t KEYMOD_BIT (1 << KEYMOD_BIT_POS);

// Order of keys in the NeoPixel ring
constexpr int KEY00_ORDER = 0;
constexpr int KEY01_ORDER = 1;
constexpr int KEY02_ORDER = 2;
constexpr int KEY03_ORDER = 7;
constexpr int KEY12_ORDER = 3;
constexpr int KEY13_ORDER = 6;
constexpr int KEY22_ORDER = 4;
constexpr int KEY23_ORDER = 5;

struct state_data_t : hsm_state_t {
  uint32_t down_color = 0xFF0000; // Default color for keys
  uint32_t up_color   = 0x00FF00; // Default color for keys
};

class JCPMMachine : public HSM {
public:
  JCPMMachine();

  static hsm_state_result_t TopState(hsm_state_t *stateData, hsm_event_t const *e);
  static hsm_state_result_t Mode1State(hsm_state_t *stateData, hsm_event_t const *e);
  static hsm_state_result_t Mode2State(hsm_state_t *stateData, hsm_event_t const *e);

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

// Define your pattern
const bool press_pattern[] = {true, false, false, true}; // long, short, short, long
PatternPressDetector patternPressDetector(jcpmHSM.GetStateData(), press_pattern, 4);

int signal_to_order(int signal) {
  switch (signal) {
    case SIG_K00_DOWN: case SIG_K00_UP: return KEY00_ORDER;
    case SIG_K01_DOWN: case SIG_K01_UP: return KEY01_ORDER;
    case SIG_K02_DOWN: case SIG_K02_UP: return KEY02_ORDER;
    case SIG_K03_DOWN: case SIG_K03_UP: return KEY03_ORDER;
    case SIG_K12_DOWN: case SIG_K12_UP: return KEY12_ORDER;
    case SIG_K13_DOWN: case SIG_K13_UP: return KEY13_ORDER;
    case SIG_K22_DOWN: case SIG_K22_UP: return KEY22_ORDER;
    case SIG_K23_DOWN: case SIG_K23_UP: return KEY23_ORDER;
    default: return -1;
  }
}

void KeyColorsSet(int r, int g, int b) {
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();  // Show results
}

void KeyColorSet(int signal, uint32_t c) {
  int order = signal_to_order(signal);
  if (order < 0 || order > NUMPIXELS) {
    return;
  }
  pixels.setPixelColor(order, c);
  pixels.show();  // Show results
}

void clearMute(bool *muted, bool *muted_timer) {
  *muted = false;
  *muted_timer = false;
  KeyColorSet(SIG_K02_DOWN, 0x00FF00);
  KeyColorSet(SIG_K13_DOWN, 0x00FF00);
}

void muteMicrophoneToggle() {
  // MS Teams Mute on Ubuntu
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.press(KEY_LEFT_SHIFT);
  Keyboard.press('m');
  delay(50);
  Keyboard.releaseAll();

#if 0
  // Microphone Mute (0x04F6) - not universally supported
  uint8_t report[3] = {0x01, 0xF6, 0x04};
  HID().SendReport(1, report, 3);

  uint8_t release[3] = {0x01, 0x00, 0x00};
  HID().SendReport(1, release, 3);

  //Consumer.write(HID_CONSUMER_MICROPHONE_CA); // Send microphone control (0x04)
  //Consumer.releaseAll();
#endif
}

#define DEBUG_LOG_STATE_EVENT(e) { \
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
        Serial.println(jcpm_signal_names[e->signal]); \
    } \
} 


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
    case SIG_TICK:
      return HANDLE_STATE();

    case SIG_K00_DOWN: case SIG_K01_DOWN: case SIG_K02_DOWN: case SIG_K03_DOWN:
    case SIG_K12_DOWN: case SIG_K13_DOWN: case SIG_K22_DOWN: case SIG_K23_DOWN:
      KeyColorSet(e->signal, down_color);
      return HANDLE_STATE();

    case SIG_K00_UP: case SIG_K01_UP: case SIG_K02_UP: case SIG_K03_UP:
    case SIG_K12_UP: case SIG_K13_UP: case SIG_K22_UP: case SIG_K23_UP:
      KeyColorSet(e->signal, up_color);
      return HANDLE_STATE();

    default:
      return HANDLE_SUPER_STATE(stateData, &HSM::rootState);
  }
}

void showMode1Screen() {
  oled.clear();
  oled.println("          | PW1 | KEY");
  oled.println("  Volume  |     | 23 ");
  oled.println("   ----   +-----+----");
  oled.println(" |        | PAS |Mute");
  oled.println(" v Next   | PLY |TMR ");
  oled.println("-----+----+-----+----");
  oled.println(" Vol | Vol|Mute |Mute");
  oled.println(" Dwn | Up |Vol  |Mic");
}

const int TICKS_PER_SECOND = 7;
const int MUTE_DURATION_SECONDS = 30;


hsm_state_result_t JCPMMachine::Mode1State(hsm_state_t *stateData, hsm_event_t const *e) {
  state_data_t* derivedStateData = static_cast<state_data_t*>(stateData);
  static bool muted = false;
  static bool muted_timer = false;
  static int muted_timer_ticks = 0;
  static bool mic_muted = false;

  HSM_DEBUG_LOG_STATE_EVENT(stateData, e);
  DEBUG_LOG_STATE_EVENT(e);

  switch (e->signal) {
    case HSM_SIG_ENTRY:
      derivedStateData->down_color = 0xFF0000; // Red when down
      derivedStateData->up_color = 0x00FF00;   // Green when up
      KeyColorsSet(0, 0xFF, 0); // Set initial colors for keys
      showMode1Screen(); // Display the mode 1 screen
      return HANDLE_STATE();

    case HSM_SIG_EXIT:
      return HANDLE_STATE();

    case HSM_SIG_INITIAL_TRANS:
      return HANDLE_STATE();

    case SIG_K00_DOWN:
      Consumer.write(MEDIA_VOLUME_DOWN);
      clearMute(&muted, &muted_timer);
      break;

    case SIG_K01_DOWN:
      Consumer.write(MEDIA_VOLUME_UP);
      clearMute(&muted, &muted_timer);
      break;

    case SIG_K02_DOWN:
      if (muted || muted_timer) {
        clearMute(&muted, &muted_timer);
      } else {
        KeyColorSet(e->signal, 0xFF0000);
        muted = true;
        muted_timer = false;
      }
      Consumer.write(MEDIA_VOLUME_MUTE);
      return HANDLE_STATE();

    case SIG_K02_UP:
      return HANDLE_STATE();

    case SIG_K03_DOWN:
      if (mic_muted) {
        mic_muted = false;
        KeyColorSet(e->signal, 0x00FF00); // Green when unmuted
      } else {
        mic_muted = true;
        KeyColorSet(e->signal, 0x7F7F00);
      }
      muteMicrophoneToggle();
      return HANDLE_STATE();

    case SIG_K03_UP:
      return HANDLE_STATE();

    case SIG_K12_DOWN:
      Consumer.write(MEDIA_PLAY_PAUSE);
      break;

    case SIG_K13_DOWN:
      if (muted_timer) {
        muted_timer_ticks += (TICKS_PER_SECOND * MUTE_DURATION_SECONDS);
      } else {
        if (!muted) {
          Consumer.write(MEDIA_VOLUME_MUTE);
        } else {
          KeyColorSet(SIG_K02_DOWN, 0x00FF00);
        }
        muted_timer = true;
        muted_timer_ticks = TICKS_PER_SECOND * MUTE_DURATION_SECONDS;
      }
      break;

    case SIG_K13_UP:
      return HANDLE_STATE();

    case SIG_ENC_UP:
      return CHANGE_STATE(stateData, &JCPMMachine::Mode2State);

    case SIG_VOL_DOWN:
      Consumer.write(MEDIA_VOLUME_DOWN);
      clearMute(&muted, &muted_timer);
      break;

    case SIG_VOL_UP:
      Consumer.write(MEDIA_VOLUME_UP);
      clearMute(&muted, &muted_timer);
      break;

    case SIG_TICK:
      if (muted_timer) {
        if (--muted_timer_ticks < 1) {
          clearMute(&muted, &muted_timer);
          Consumer.write(MEDIA_VOLUME_MUTE);
          KeyColorSet(SIG_K03_DOWN, 0x00FF00);
        }
      }
      break;

    default:
      return HANDLE_SUPER_STATE(stateData, &JCPMMachine::TopState);
  }
  return HANDLE_SUPER_STATE(stateData, &JCPMMachine::TopState);
}

void showMode2Screen() {
  oled.clear();
  oled.println("     Mode 2");
  oled.println("Not implemented");
  oled.println("");
  oled.print("   v");
  oled.println(VERSION);
}

hsm_state_result_t JCPMMachine::Mode2State(hsm_state_t *stateData, hsm_event_t const *e) {
  state_data_t* derivedStateData = static_cast<state_data_t*>(stateData);

  HSM_DEBUG_LOG_STATE_EVENT(stateData, e);
  DEBUG_LOG_STATE_EVENT(e);
  
  switch (e->signal) {
    case HSM_SIG_ENTRY:
      derivedStateData->down_color = 0xFF0000; // Red when down
      derivedStateData->up_color = 0x0000FF;   // Blue when up
      KeyColorsSet(0, 0, 0xFF); // Set initial colors for keys
      showMode2Screen(); // Display the mode 2 screen
      return HANDLE_STATE();

    case SIG_K22_DOWN:
      patternPressDetector.onButtonDown(KEY22_ORDER);
      break;
    case SIG_K22_UP:
      patternPressDetector.onButtonUp(KEY22_ORDER);
      break;

    case SIG_PATTERN_PRESS:
      Keyboard.print(pattern_match_text);
      return HANDLE_STATE();

    case SIG_ENC_UP:
      return CHANGE_STATE(stateData, &JCPMMachine::Mode1State);
  }
  return HANDLE_SUPER_STATE(stateData, &JCPMMachine::TopState);
}

uint16_t getKeys() {
  const int MAX = 1000;
  uint16_t bitvalues = 0;
  uint16_t prev_bits = -1;
  int32_t integrator = MAX;

  while (--integrator > 0) {
    bitvalues = (digitalRead(KEYENC) << KEYENC_BIT_POS);
    bitvalues |= (digitalRead(KEYMOD) << KEYMOD_BIT_POS);
    bitvalues |= (digitalRead(KEY00) << KEY00_BIT_POS);
    bitvalues |= (digitalRead(KEY01) << KEY01_BIT_POS);
    bitvalues |= (digitalRead(KEY02) << KEY02_BIT_POS);
    bitvalues |= (digitalRead(KEY03) << KEY03_BIT_POS);
    bitvalues |= (digitalRead(KEY12) << KEY12_BIT_POS);
    bitvalues |= (digitalRead(KEY13) << KEY13_BIT_POS);
    bitvalues |= (digitalRead(KEY22) << KEY22_BIT_POS);
    bitvalues |= (digitalRead(KEY23) << KEY23_BIT_POS);

    // Invert cause easier to think about
    bitvalues = ~bitvalues;

    if (prev_bits != bitvalues) {
      integrator = MAX;
      prev_bits = bitvalues;
    }
  }
  return bitvalues;
}

int32_t getEncoder() {
  return encoder.read();
}

void initHW() {
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(KEYENC, INPUT_PULLUP);  //SW1 pushbutton (encoder button)
  pinMode(KEY00, INPUT_PULLUP);   //SW2 pushbutton
  pinMode(KEY01, INPUT_PULLUP);   //SW3 pushbutton
  pinMode(KEY02, INPUT_PULLUP);   //SW4 pushbutton
  pinMode(KEY03, INPUT_PULLUP);   //SW5 pushbutton
  pinMode(KEY12, INPUT_PULLUP);   //SW6 pushbutton
  pinMode(KEY13, INPUT_PULLUP);   //SW7 pushbutton
  pinMode(KEY22, INPUT_PULLUP);   //SW8 pushbutton
  pinMode(KEY23, INPUT_PULLUP);   //SW9 pushbutton
  pinMode(KEYMOD, INPUT_PULLUP);  //SW10 pushbutton - acts as mode switch

  randomSeed(analogRead(A9));

  Wire.begin();
  Wire.setClock(400000L);

  oled.begin(&Adafruit128x64, SCREEN_ADDRESS, OLED_RESET);
  oled.displayRemap(true); // rotate display 180
  oled.setFont(System5x7);
  oled.clear();

  pixels.begin();
  pixels.clear();
  pixels.show();

  System.begin();
  Consumer.begin();
}

uint8_t keyBitPosToDownSignal(uint8_t bitPos) {
  switch (bitPos) {
    case KEY00_BIT_POS: return SIG_K00_DOWN;
    case KEY01_BIT_POS: return SIG_K01_DOWN;
    case KEY02_BIT_POS: return SIG_K02_DOWN;
    case KEY03_BIT_POS: return SIG_K03_DOWN;
    case KEY12_BIT_POS: return SIG_K12_DOWN;
    case KEY13_BIT_POS: return SIG_K13_DOWN;
    case KEY22_BIT_POS: return SIG_K22_DOWN;
    case KEY23_BIT_POS: return SIG_K23_DOWN;
    case KEYENC_BIT_POS: return SIG_ENC_DOWN;
    case KEYMOD_BIT_POS: return SIG_MODE_DOWN;
    default: return HSM_SIG_NONE; // Invalid bit position
  }
}

uint8_t keyBitPosToUpSignal(uint8_t bitPos) {
  switch (bitPos) {
    case KEY00_BIT_POS: return SIG_K00_UP;
    case KEY01_BIT_POS: return SIG_K01_UP;
    case KEY02_BIT_POS: return SIG_K02_UP;
    case KEY03_BIT_POS: return SIG_K03_UP;
    case KEY12_BIT_POS: return SIG_K12_UP;
    case KEY13_BIT_POS: return SIG_K13_UP;
    case KEY22_BIT_POS: return SIG_K22_UP;
    case KEY23_BIT_POS: return SIG_K23_UP;
    case KEYENC_BIT_POS: return SIG_ENC_UP;
    case KEYMOD_BIT_POS: return SIG_MODE_UP;
    default: return HSM_SIG_NONE; // Invalid bit position
  }
}

// Here's a simple key change detector:
void updateKeyEvents(uint16_t currentKeys) {
  static uint16_t prevKeys = 0;

  // Check each key for changes
  for (int i = 0; i < 10; i++) {
    uint16_t mask = 1 << i;
    bool prevPressed = (prevKeys & mask) != 0;
    bool currPressed = (currentKeys & mask) != 0;

    if (currPressed != prevPressed) {
      // Key state changed
      if (currPressed) {
        // Key pressed - add corresponding DOWN event
        uint8_t downSignal = keyBitPosToDownSignal(i);
        if (downSignal != HSM_SIG_NONE) {
          jcpmHSM.GetStateData()->EventQueuePush(downSignal);
        }
      } else {
        // Key released - add corresponding UP event
        uint8_t upSignal = keyBitPosToUpSignal(i);
        if (upSignal != HSM_SIG_NONE) {
          jcpmHSM.GetStateData()->EventQueuePush(upSignal);
        }
      }
    }
  }

  prevKeys = currentKeys;
}

void updateEncoderEvents(int32_t currentEncoder) {
  static int32_t prevEncoder = 0;
  static int32_t accumulatedDelta = 0;
  const int32_t ENCODER_THRESHOLD = 4; // Adjust this value based on your encoder sensitivity

  // Calculate the change in encoder position
  int32_t delta = currentEncoder - prevEncoder;
  accumulatedDelta += delta;

  // Check if we've accumulated enough change to trigger an event
  while (accumulatedDelta >= ENCODER_THRESHOLD) {
    jcpmHSM.GetStateData()->EventQueuePush(SIG_VOL_UP);
    accumulatedDelta -= ENCODER_THRESHOLD;
  }

  while (accumulatedDelta <= -ENCODER_THRESHOLD) {
    jcpmHSM.GetStateData()->EventQueuePush(SIG_VOL_DOWN);
    accumulatedDelta += ENCODER_THRESHOLD;
  }

  prevEncoder = currentEncoder;
}

void setup() {
  //Serial.begin(9600);
  Keyboard.begin();

  initHW();

  jcpmHSM.SetInitialState(JCPMMachine::Mode1State);

  // Serial.println("JCPM HSM started");
  // Serial.print("Version: ");
  // Serial.println(VERSION);
}

void loop() {
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


#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

#include <Encoder.h>
#include <HID-Project.h>
#include <Adafruit_NeoPixel.h>
#include "hsm.h"

#define NUMPIXELS 8  // Popular NeoPixel ring size and the number of keys
#define PIN 5        // On Trinket or Gemma, suggest changing this to 1


// Define events of the system
const HSM_EVENT JCPM_EVT_START    = (HSME_START);
const HSM_EVENT JCPM_EVT_NONE     = (HSME_START + 1);
const HSM_EVENT JCPM_EVT_TICK     = (HSME_START + 2);
const HSM_EVENT JCPM_EVT_K00_DOWN = (HSME_START + 3);
const HSM_EVENT JCPM_EVT_K00_UP   = (HSME_START + 4);
const HSM_EVENT JCPM_EVT_K01_DOWN = (HSME_START + 5);
const HSM_EVENT JCPM_EVT_K01_UP   = (HSME_START + 6);
const HSM_EVENT JCPM_EVT_K02_DOWN = (HSME_START + 7);
const HSM_EVENT JCPM_EVT_K02_UP   = (HSME_START + 8);
const HSM_EVENT JCPM_EVT_K03_DOWN = (HSME_START + 9);
const HSM_EVENT JCPM_EVT_K03_UP   = (HSME_START + 10);
const HSM_EVENT JCPM_EVT_K12_DOWN = (HSME_START + 11);
const HSM_EVENT JCPM_EVT_K12_UP   = (HSME_START + 12);
const HSM_EVENT JCPM_EVT_K13_DOWN = (HSME_START + 13);
const HSM_EVENT JCPM_EVT_K13_UP   = (HSME_START + 14);
const HSM_EVENT JCPM_EVT_K22_DOWN = (HSME_START + 15);
const HSM_EVENT JCPM_EVT_K22_UP   = (HSME_START + 16);
const HSM_EVENT JCPM_EVT_K23_DOWN = (HSME_START + 17);
const HSM_EVENT JCPM_EVT_K23_UP   = (HSME_START + 18);
const HSM_EVENT JCPM_EVT_ENC_DOWN = (HSME_START + 19);
const HSM_EVENT JCPM_EVT_ENC_UP   = (HSME_START + 20);
const HSM_EVENT JCPM_EVT_VOL_DOWN = (HSME_START + 21);
const HSM_EVENT JCPM_EVT_VOL_UP   = (HSME_START + 22);
const HSM_EVENT JCPM_EVT_PATTERN_PRESS = (HSME_START + 23);

const int MAX_EVENTS = 16;
const int MAX_KEYS = 11;


// Key names are defined like a grid. Lower left is KEY00
// KEY10, KEY11, KEY20, KEY21 are not present
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

// change these to constexpr using _BIT_POS below
#define KEY00_BIT 0x0001
#define KEY01_BIT 0x0002
#define KEY02_BIT 0x0004
#define KEY03_BIT 0x0008
#define KEY12_BIT 0x0010
#define KEY13_BIT 0x0020
#define KEY22_BIT 0x0040
#define KEY23_BIT 0x0080
#define KEYENC_BIT 0x0100
#define KEYMOD_BIT 0x0200

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

constexpr int KEY00_ORDER = 0;
constexpr int KEY01_ORDER = 1;
constexpr int KEY02_ORDER = 2;
constexpr int KEY03_ORDER = 7;
constexpr int KEY12_ORDER = 3;
constexpr int KEY13_ORDER = 6;
constexpr int KEY22_ORDER = 4;
constexpr int KEY23_ORDER = 5;

const int32_t ENCODER_THRESHOLD = 3;  // Minimum encoder change to trigger an event

#define DELAYVAL 500  // Time (in milliseconds) to pause between pixels

int32_t getEncoder();


// Define a simple event queue
// Event structure
struct Event {
  HSM_EVENT type;
  int event_id;

  Event(HSM_EVENT t = JCPM_EVT_NONE, int id = 0)
    : type(t), event_id(id) {}
};


// Event Queue class
class EventQueue {
private:
  Event events[MAX_EVENTS];
  size_t head = 0;
  size_t tail = 0;
  size_t size = 0;

public:
  EventQueue() = default;
  ~EventQueue() = default;

  // Register an event
  bool registerEvent(HSM_EVENT type, unsigned event_id) {
    if (size == MAX_EVENTS) {
      //std::cerr << "Error: Event queue is full" << std::endl;
      return false;
    }

    events[tail] = Event(type, event_id);
    tail = (tail + 1) % MAX_EVENTS;
    size++;
    return true;
  }

  // Retrieve an event
  bool retrieveEvent(Event& event) {
    if (size == 0) {
      //std::cerr << "Error: Event queue is empty" << std::endl;
      return false;
    }

    event = events[head];
    head = (head + 1) % MAX_EVENTS;
    size--;
    return true;
  }

  // Check if queue is empty
  bool isEmpty() const {
    return size == 0;
  }

  // Check if queue is full
  bool isFull() const {
    return size == MAX_EVENTS;
  }
};


// SwitchMonitor class to track switch state changes
class SwitchMonitor {
private:
  uint16_t prev_state;   // Previous switch states
  int32_t prev_encoder;  // Previous encoder value
  int32_t accumulated_delta; // Accumulated encoder change

  EventQueue& queue;     // Reference to the event queue

public:
  // Array of event types for each switch (down and up)
  static const HSM_EVENT key_events[MAX_KEYS][2];

// Constructor initializes with all switches off and a reference to the event queue
  SwitchMonitor(EventQueue& event_queue)
    : prev_state(0), prev_encoder(0), queue(event_queue) {}

  // Update switch states and generate events for changes
  void update(uint16_t current_state, int current_encoder) {
    // Compare current state with previous state
    for (unsigned i = 0; i < MAX_KEYS; ++i) {  // Monitor 11 switches
      uint16_t mask = 1 << i;
      bool prev_bit = (prev_state & mask) != 0;
      bool curr_bit = (current_state & mask) != 0;

      if (curr_bit != prev_bit) {  // State change detected
        // Index 0 for KEY_DOWN, 1 for KEY_UP
        HSM_EVENT event_type = curr_bit ? key_events[i][0] : key_events[i][1];

        // Use switch index (0-MAX_KEYS) as event_id
        if (!queue.registerEvent(event_type, i)) {
          //std::cerr << "Failed to register event for switch " << i << std::endl;
          Serial.println("Failed to register event for switch");
        }
      }
    }
    prev_state = current_state;  // Update previous state

    int32_t delta = current_encoder - prev_encoder;
    accumulated_delta += delta;

    // Update encoder
    while (accumulated_delta > ENCODER_THRESHOLD) {
      if (!queue.registerEvent(JCPM_EVT_VOL_UP, MAX_KEYS + 1)) {
        Serial.println("Failed to register ENCODER_UP event");
      }
      accumulated_delta -= ENCODER_THRESHOLD; // Reduce by threshold

    }
    while (accumulated_delta < -ENCODER_THRESHOLD) {
      if (!queue.registerEvent(JCPM_EVT_VOL_DOWN, MAX_KEYS + 1)) {
        Serial.println("Failed to register ENCODER_DOWN event");
      }
      accumulated_delta += ENCODER_THRESHOLD; // Reduce by threshold
    }
    prev_encoder = current_encoder;  // Update previous encoder value
  }

  // Get the previous switch state
  uint16_t getPreviousState() const {
    return prev_state;
  }
};

// Define the static key_events array outside of class (Arduino issue)
const HSM_EVENT SwitchMonitor::key_events[MAX_KEYS][2] = {
  { JCPM_EVT_K00_DOWN, JCPM_EVT_K00_UP },
  { JCPM_EVT_K01_DOWN, JCPM_EVT_K01_UP },
  { JCPM_EVT_K02_DOWN, JCPM_EVT_K02_UP },
  { JCPM_EVT_K03_DOWN, JCPM_EVT_K03_UP },
  { JCPM_EVT_K12_DOWN, JCPM_EVT_K12_UP },
  { JCPM_EVT_K13_DOWN, JCPM_EVT_K13_UP },
  { JCPM_EVT_K22_DOWN, JCPM_EVT_K22_UP },
  { JCPM_EVT_K23_DOWN, JCPM_EVT_K23_UP },
  { JCPM_EVT_ENC_DOWN, JCPM_EVT_ENC_UP },
  { JCPM_EVT_VOL_DOWN, JCPM_EVT_VOL_UP }
};

const uint32_t SHORT_PRESS_MAX_MS = 400; // Max duration for a short press
const uint32_t LONG_PRESS_MIN_MS = 400;  // Min duration for a long press
const uint32_t PATTERN_PRESS_TIMEOUT_MS = 2000; // Max time between presses in sequence

// PatternPressDetector class to detect a user-specified press sequence
class PatternPressDetector {
private:
    struct SwitchState {
        uint32_t last_down_time; // Time of last KEY_DOWN
        uint32_t last_press_duration; // Duration of last press
        int press_count; // Number of presses in current sequence
        uint32_t sequence_start_time; // Time of first press in sequence
    };
    SwitchState switch_states[MAX_KEYS]; // State for each switch
    EventQueue& queue; // Reference to the event queue
    const bool* pattern; // Pattern of short (false) and long (true) presses
    int pattern_length; // Length of the pattern

public:
    PatternPressDetector(EventQueue& event_queue, const bool* press_pattern, size_t length)
        : queue(event_queue), pattern(press_pattern), pattern_length(length) {
        // Initialize state for all switches
        for (int i = 0; i < MAX_KEYS; ++i) {
            switch_states[i] = {0, 0, 0, 0};
        }
        // Validate pattern
        if (length == 0 || press_pattern == nullptr) {
            pattern_length = 0; // Disable detector
        }
    }

    // Process an event and detect the specified press pattern
    void processEvent(const Event& event) {
        if (pattern_length == 0) {
          return; // Disabled if pattern is invalid
        }

        int switch_id = event.event_id;
        if (switch_id < 0 || switch_id > 10) {
          return; // Ignore non-switch events
        }

        uint32_t current_time = millis();
        SwitchState& state = switch_states[switch_id];

        if (event.type == SwitchMonitor::key_events[switch_id][0]) { // KEY_DOWN
            state.last_down_time = current_time;
            //Serial.print("D");
        } else if (event.type == SwitchMonitor::key_events[switch_id][1]) { // KEY_UP
            //Serial.print("U");;
            if (state.last_down_time == 0) return; // Ignore if no matching KEY_DOWN

            // Calculate press duration
            uint32_t duration = current_time - state.last_down_time;
            state.last_press_duration = duration;

            // Check if sequence has timed out
            if (state.press_count > 0 && 
                (current_time - state.sequence_start_time > PATTERN_PRESS_TIMEOUT_MS)) {
                state.press_count = 0; // Reset sequence
            }

            // Check if current press matches the expected pattern
            bool is_long_press = duration > LONG_PRESS_MIN_MS;
            bool expected_long = (state.press_count < pattern_length) ? pattern[state.press_count] : false;

            if ((is_long_press && expected_long) || (!is_long_press && !expected_long)) {
                if (state.press_count == 0) {
                    state.sequence_start_time = current_time;
                }
                state.press_count++;
                // Check if pattern is complete
                if (state.press_count == pattern_length) {
                    if (!queue.registerEvent(JCPM_EVT_PATTERN_PRESS, switch_id)) {
                        Serial.print("Failed to register PATTERN_PRESS event for switch ");
                        Serial.println(switch_id);
                    }
                    state.press_count = 0; // Reset sequence
                }
            } else {
                state.press_count = 0; // Reset if pattern doesn't match
            }
            state.last_down_time = 0; // Reset down time
        }
    }
};




typedef struct JCPM_T {
  /*
    Class that implements the Camera HSM and inherits from the HSM class
    Creating a HSM requires the following steps:
        1) Initialize the base HSM class
        2) Define the HSM states hierarchy
        3) Set the starting state
        4) Define the state handlers
            a) State handler must return "None" if the event IS handled
            b) State handler must return "event" if the event IS NOT handled
            c) State handler may handle the ENTRY event for state setup
            d) State handler may handle the EXIT event for state teardown/cleanup
            e) State handler may handle the INIT for self transition to child state
            f) Self transition to child state MUST NOT be handled by ENTRY or EXIT event
            g) Events ENTRY, EXIT and INIT do no need to return None for brevity
    */
  // Parent
  HSM parent;
  // Child members
  char param1;
  char param2;
} JCPM;


//
// File Globals
//


#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define OLED_RESET 4         // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
SSD1306AsciiWire oled;
Encoder encoder(1, 0);
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

JCPM basic;
HSM_STATE JCPM_StateMode1;
HSM_STATE JCPM_StateMode2;
HSM_STATE JCPM_StateTop;


int32_t getEncoder() {
  return encoder.read();
}

void KeyColorsSet(int r, int g, int b) {
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();  // Show results
}

void KeyColorSet(int key, uint32_t c) {

  if (key < 0 || key > NUMPIXELS) {
    return;
  }
  pixels.setPixelColor(key, c);
  pixels.show();  // Show results
}

HSM_EVENT JCPM_StateTopHandler(HSM* This, HSM_EVENT event, void* param) {
  uint32_t down_color = 0xFF0000; // Default color for keys
  uint32_t up_color   = 0x00FF00; // Default color for keys

  char sub_state = reinterpret_cast<JCPM*>(This)->param1;
  if (sub_state == 2) {
    down_color = 0xFF0000;
    up_color   = 0x0000FF;
  }
  
  switch (event) {
    case HSME_INIT:
      return 0;
    case HSME_ENTRY:
      return 0;
    case HSME_EXIT:
      return 0;

    case JCPM_EVT_K00_DOWN:
      KeyColorSet(KEY00_ORDER, down_color);
      return 0;
    case JCPM_EVT_K00_UP:
      KeyColorSet(KEY00_ORDER, up_color);
      return 0;
    case JCPM_EVT_K01_DOWN:
      KeyColorSet(KEY01_ORDER, down_color);
      return 0;
    case JCPM_EVT_K01_UP:
      KeyColorSet(KEY01_ORDER, up_color);
      return 0;
    case JCPM_EVT_K02_DOWN:
      KeyColorSet(KEY02_ORDER, down_color);
      return 0;
    case JCPM_EVT_K02_UP:
      if(sub_state != 1) {
        KeyColorSet(KEY02_ORDER, up_color);
      }    
      return 0;
    case JCPM_EVT_K03_DOWN:
      KeyColorSet(KEY03_ORDER, down_color);
      return 0;
    case JCPM_EVT_K03_UP:
      if (sub_state != 1) {
        KeyColorSet(KEY03_ORDER, up_color);
      }
      return 0;
    case JCPM_EVT_K12_DOWN:
      KeyColorSet(KEY12_ORDER, down_color);
      return 0;
    case JCPM_EVT_K12_UP:
      KeyColorSet(KEY12_ORDER, up_color);
      return 0;
    case JCPM_EVT_K13_DOWN:
      KeyColorSet(KEY13_ORDER, down_color);
      return 0;
    case JCPM_EVT_K13_UP:
      KeyColorSet(KEY13_ORDER, up_color);
      return 0;

    case JCPM_EVT_K22_DOWN:
      KeyColorSet(KEY22_ORDER, down_color);
      return 0;
    case JCPM_EVT_K22_UP:
      KeyColorSet(KEY22_ORDER, up_color);
      return 0;
    case JCPM_EVT_K23_DOWN:
      KeyColorSet(KEY23_ORDER, down_color);
      return 0;
    case JCPM_EVT_K23_UP:
      KeyColorSet(KEY23_ORDER, up_color);
      return 0;

    case JCPM_EVT_ENC_UP:
      HSM_Tran(This, &JCPM_StateMode1, 0, 0);
      return 0;

    case JCPM_EVT_TICK:
      return 0;

    case JCPM_EVT_PATTERN_PRESS:
      // Handle the pattern press event
      if (reinterpret_cast<int>(param) == 6) {
        Keyboard.print("SanClemente23224");
      }
     
      //Serial.print("PATTERN on key: ");
      //Serial.println(reinterpret_cast<int>(param));
      return 0;

    default:
      return 0; 
  }
  return 0;
}


HSM_EVENT JCPM_StateMode2Handler(HSM* This, HSM_EVENT event, void* param) {
  (void)param;

  switch (event) {
    case HSME_ENTRY:
      reinterpret_cast<JCPM*>(This)->param1 = 2;
      KeyColorsSet(0, 0, 255);
      oled.clear();
      oled.println("     Mode 2");
      oled.println("Not implemented");
      break;
    case HSME_EXIT:
      break;
    case JCPM_EVT_ENC_UP:
      HSM_Tran(This, &JCPM_StateMode1, 0, 0);
      return 0;

    case JCPM_EVT_TICK:
      return 0;

    default:
      //Serial.print("Unhandled event in Mode2: ");
      //Serial.println(event);
      return event; // Pass the event up
  }
  return event;
}

void showModeOneScreen() 
{
  oled.clear();
  oled.println("          | PW1 | KEY");
  oled.println("  Volume  |     | 23 ");
  oled.println("   ----   +-----+----");
  oled.println(" |        | PAS | NXT");
  oled.println(" v Next   | PLY |    ");
  oled.println("-----+----+-----+----");
  oled.println(" Vol | Vol|Mute |Mute");
  oled.println(" Dwn | Up |     |TMR ");
}

void clearMute(bool *muted, bool *muted_timer)
{
  *muted = false;
  *muted_timer = false;
  KeyColorSet(KEY02_ORDER, 0x00FF00);
  KeyColorSet(KEY03_ORDER, 0x00FF00);
}
const int TICKS_PER_SECOND = 7;


HSM_EVENT JCPM_StateMode1Handler(HSM* This, HSM_EVENT event, void* param) {
  (void)param;
  static bool muted = false;
  static bool muted_timer = false;
  static int muted_timer_ticks = 0;
//  static unsigned long key_down_timer;
//  static int key_down_count;

  switch (event) {
    case HSME_INIT:
      break;
    case HSME_ENTRY:
      KeyColorsSet(0, 255, 0);
      showModeOneScreen();
      reinterpret_cast<JCPM*>(This)->param1 = 1;
      break;
    case HSME_EXIT:
      break;
    case JCPM_EVT_K00_DOWN:
      Consumer.write(MEDIA_VOLUME_DOWN);
      clearMute(&muted, &muted_timer);
      break;
    case JCPM_EVT_K00_UP:
      //KeyColorSet(KEY00_ORDER, 0x00FF00);
      break; //return 0;

    case JCPM_EVT_K01_DOWN:
      Consumer.write(MEDIA_VOLUME_UP);
      clearMute(&muted, &muted_timer);      
      break;
    case JCPM_EVT_K01_UP:
      //KeyColorSet(KEY01_ORDER, 0x00FF00);
      break;

    // Mute toggle, disable Mute Timer
    case JCPM_EVT_K02_DOWN:
      if (muted || muted_timer)
      {
        clearMute(&muted, &muted_timer);
      }
      else
      {
        KeyColorSet(KEY02_ORDER, 0xFF0000);
        muted = true;
        muted_timer = false;
      }
      Consumer.write(MEDIA_VOLUME_MUTE);
      return 0;
    case JCPM_EVT_K02_UP:
      break;

    // Mute timer
    case JCPM_EVT_K03_DOWN:
      if (muted_timer)
      {
        muted_timer_ticks += (TICKS_PER_SECOND * 30);
      }
      else
      {
        if (!muted)
        {
          Consumer.write(MEDIA_VOLUME_MUTE);
        }
        muted_timer = true;
        muted_timer_ticks = TICKS_PER_SECOND * 30;
      }
      break; //return 0;
    case JCPM_EVT_K03_UP:
      //KeyColorSet(KEY03_ORDER, 0x00FF00);
      break;

    // Play/Pause
    case JCPM_EVT_K12_DOWN:
      Consumer.write(MEDIA_PLAY_PAUSE);
      break;
    case JCPM_EVT_K12_UP:
      //KeyColorSet(KEY12_ORDER, 0x00FF00);
      break;

    // Next
    case JCPM_EVT_K13_DOWN:
      Consumer.write(MEDIA_NEXT);
      break;
    case JCPM_EVT_K13_UP:
      //KeyColorSet(KEY13_ORDER, 0x00FF00);
      break;

    case JCPM_EVT_K22_DOWN:
      break;
    case JCPM_EVT_K22_UP:
      //KeyColorSet(KEY22_ORDER, 0x00FF00);
      break;
    case JCPM_EVT_K23_DOWN:
      break;
    case JCPM_EVT_K23_UP:
      //KeyColorSet(KEY23_ORDER, 0x00FF00);
      break;

    // Encoder events
    case JCPM_EVT_ENC_DOWN:
      break;
    case JCPM_EVT_ENC_UP:
      HSM_Tran(This, &JCPM_StateMode2, 0, 0);
      return 0;

    case JCPM_EVT_VOL_DOWN:
      Consumer.write(MEDIA_VOLUME_DOWN);
      clearMute(&muted, &muted_timer);
      return 0;
    case JCPM_EVT_VOL_UP:
      Consumer.write(MEDIA_VOLUME_UP);
      clearMute(&muted, &muted_timer);
      return 0;

    case JCPM_EVT_TICK:
      if (muted_timer) 
      {
        if (--muted_timer_ticks < 1)
        {
          clearMute(&muted, &muted_timer);
          Consumer.write(MEDIA_VOLUME_MUTE);          
          KeyColorSet(KEY03_ORDER, 0x00FF00);
        }        
      }
      break;

    //default:
    //  Serial.print("Unhandled event Mode1: ");
    //  Serial.println(event);
    //  return 0;
  }
  return event;
}

void initHW() {
  pinMode(LED_BUILTIN, OUTPUT);

  //Serial.begin(9600);
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

  pixels.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.clear();
  pixels.show();  // Show results
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
};


void setup() {
  Serial.begin(9600);
  Keyboard.begin();

  initHW();

  KeyColorsSet(0, 100, 100);

  HSM_STATE_Create(&JCPM_StateTop, "Top", JCPM_StateTopHandler, NULL);
  HSM_STATE_Create(&JCPM_StateMode1, "Mode1", JCPM_StateMode1Handler, &JCPM_StateTop);
  HSM_STATE_Create(&JCPM_StateMode2, "Mode2", JCPM_StateMode2Handler, &JCPM_StateTop);

  HSM_Create((HSM*)&basic, "JCPM", &JCPM_StateMode1);
}

EventQueue queue;
SwitchMonitor monitor(queue);
// Define pattern: false = short, true = long (e.g., short, short, long, short)
const bool long_press = true;
const bool short_press = false;
const bool press_pattern[] = {long_press, short_press, short_press, long_press};
PatternPressDetector pattern_detector(queue, press_pattern, 4);
void loop() {
  uint16_t keys = getKeys();
  int encoder = getEncoder();
  static unsigned long tick = 0;
  unsigned long currentTick = millis();

  monitor.update(keys, encoder);
  if (currentTick - tick > 100)
  {
    tick = currentTick;
    queue.registerEvent(JCPM_EVT_TICK, 0);
  }

  Event event;
  while (queue.retrieveEvent(event)) {
    pattern_detector.processEvent(event); // Check for pattern press

    HSM_Run((HSM*)&basic, event.type, (void*)event.event_id);
  }
}

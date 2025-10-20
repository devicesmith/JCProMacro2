#pragma once
#include "hsm-signals.h"

// Define all the signal events for the HSM
enum macropad_signal : uint8_t
{
  // Events to all
  SIG_TICK = HSM_SIG_USER,		// 5  : system clock tick
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
  SIG_ENC_DOWN,
  SIG_ENC_UP,
  SIG_VOL_DEC,
  SIG_VOL_INC,
  SIG_PATTERN_PRESS,
  SIG_LAST
};

typedef macropad_signal macropad_signal_t;

extern char const * macropad_signal_names[];

extern char const * pattern_match_text;

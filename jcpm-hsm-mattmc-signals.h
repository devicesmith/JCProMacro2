#pragma once
#include "hsm.h"

// Define all the signal events for the HSM
enum jcpm_signal : uint8_t
{
  // Events to all
  SIG_TICK = HSM_SIG_USER,		// 5  : system clock tick
  SIG_K00_DOWN,
  SIG_K00_UP,
  SIG_K01_DOWN,
  SIG_K01_UP,
  SIG_K02_DOWN,
  SIG_K02_UP,
  SIG_K03_DOWN,
  SIG_K03_UP,
  SIG_K12_DOWN,
  SIG_K12_UP,
  SIG_K13_DOWN,
  SIG_K13_UP,
  SIG_K22_DOWN,
  SIG_K22_UP,
  SIG_K23_DOWN,
  SIG_K23_UP,
  SIG_ENC_DOWN,
  SIG_ENC_UP,
  SIG_VOL_DOWN,
  SIG_VOL_UP,
  SIG_PATTERN_PRESS,
  SIG_LAST
} jcpm_signal_t;

char const * jcpm_signal_names[] = {
  "HSM_SIG_NONE",
  "HSM_SIG_SILENT",        // 1  : Falls through to superstate handler
  "HSM_SIG_ENTRY",         // 2
  "HSM_SIG_EXIT",          // 3
  "HSM_SIG_INITIAL_TRANS", // 4
  "SIG_TICK",
  "SIG_K00_DOWN",
  "SIG_K00_UP",
  "SIG_K01_DOWN",
  "SIG_K01_UP",
  "SIG_K02_DOWN",
  "SIG_K02_UP",
  "SIG_K03_DOWN",
  "SIG_K03_UP",
  "SIG_K12_DOWN",
  "SIG_K12_UP",
  "SIG_K13_DOWN",
  "SIG_K13_UP",
  "SIG_K22_DOWN",
  "SIG_K22_UP",
  "SIG_K23_DOWN",
  "SIG_K23_UP",
  "SIG_ENC_DOWN",
  "SIG_ENC_UP",
  "SIG_VOL_DOWN",
  "SIG_VOL_UP",
  "SIG_PATTERN_PRESS"
};
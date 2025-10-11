//#include "jcpm-hsm-mattmc-signals.h"

char const * jcpm_signal_names[] = {
  "HSM_SIG_NONE",
  "HSM_SIG_SILENT",        // 1  : Falls through to superstate handler
  "HSM_SIG_ENTRY",         // 2
  "HSM_SIG_EXIT",          // 3
  "HSM_SIG_INITIAL_TRANS", // 4
  "SIG_TICK",
  "SIG_K1_DOWN",
  "SIG_K1_UP",
  "SIG_K2_DOWN",
  "SIG_K2_UP",
  "SIG_K3_DOWN",
  "SIG_K3_UP",
  "SIG_K4_DOWN",
  "SIG_K4_UP",
  "SIG_K5_DOWN",
  "SIG_K5_UP",
  "SIG_K6_DOWN",
  "SIG_K6_UP",
  "SIG_K7_DOWN",
  "SIG_K7_UP",
  "SIG_K8_DOWN",
  "SIG_K8_UP",
  "SIG_K9_DOWN",
  "SIG_K9_UP",
  "SIG_K10_DOWN",
  "SIG_K10_UP",
  "SIG_K11_DOWN",
  "SIG_K11_UP",
  "SIG_K12_DOWN",
  "SIG_K12_UP",
  "SIG_ENC_DOWN",
  "SIG_ENC_UP",
  "SIG_VOL_DEC",
  "SIG_VOL_INC",
  "SIG_PATTERN_PRESS"
};

const char* pattern_match_text = "Pattern Matched!";

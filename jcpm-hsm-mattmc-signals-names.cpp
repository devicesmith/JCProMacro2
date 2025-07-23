//#include "jcpm-hsm-mattmc-signals.h"

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
  "SIG_MODE_DOWN", 
  "SIG_MODE_UP",
  "SIG_ENC_DOWN",
  "SIG_ENC_UP",
  "SIG_VOL_DOWN",
  "SIG_VOL_UP",
  "SIG_PATTERN_PRESS"
};

const char* pattern_match_text = "Pattern Matched!";

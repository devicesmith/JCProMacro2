#pragma once

// HSM signal constants
enum hsm_signal {
    HSM_SIG_NONE = 0,
    HSM_SIG_SILENT,        // 1  : Falls through to superstate handler
    HSM_SIG_ENTRY,         // 2
    HSM_SIG_EXIT,          // 3
    HSM_SIG_INITIAL_TRANS, // 4
    HSM_SIG_USER           // 5
};
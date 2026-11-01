#include "prx/libc/include/LegacySignals.hpp"

extern "C" int APS5_VABI sigvec_nid_disambig1(int signal, const GuestSignalVector* replacement, GuestSignalVector* previous) {
    return LegacySigvec_nid_no_patch(signal, replacement, previous);
}

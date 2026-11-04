#include "prx/libc/include/LegacySignals.hpp"

extern "C" {

int APS5_VABI sigvec_nid_postfix(int signal, const GuestSignalVector* replacement, GuestSignalVector* previous) {
    return LegacySigvec_nid_no_patch(signal, replacement, previous);
}

int APS5_VABI sigblock_nid_postfix(int mask) {
    const GuestSignalSet set{{static_cast<std::uint32_t>(mask), 0, 0, 0}};
    GuestSignalSet previous{};
    if (_sigprocmask_nid_postfix(1, &set, &previous) != 0) return -1;
    return static_cast<int>(previous.bits[0]);
}

int APS5_VABI sigsetmask_nid_postfix(int mask) {
    const GuestSignalSet set{{static_cast<std::uint32_t>(mask), 0, 0, 0}};
    GuestSignalSet previous{};
    if (_sigprocmask_nid_postfix(3, &set, &previous) != 0) return -1;
    return static_cast<int>(previous.bits[0]);
}

}

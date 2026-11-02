#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_LEGACYSIGNALS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_LEGACYSIGNALS_HPP

#include "prx/libkernel/include/Api.hpp"
#include <cstddef>
#include <cstdint>

struct GuestSignalVector {
    std::uintptr_t handler;
    int mask;
    int flags;
};

static_assert(sizeof(GuestSignalVector) == 16 && offsetof(GuestSignalVector, mask) == 8 && offsetof(GuestSignalVector, flags) == 12);

using LegacySigaction_nid_no_patch = int (APS5_VABI *)(int, const GuestSigaction*, GuestSigaction*);

extern "C" {

void RegisterSigvec_nid_no_patch(LegacySigaction_nid_no_patch action);
int LegacySigvec_nid_no_patch(int signal, const GuestSignalVector* replacement, GuestSignalVector* previous);
int APS5_VABI sigvec_nid_disambig1(int signal, const GuestSignalVector* replacement, GuestSignalVector* previous);
int APS5_VABI sigvec_nid_postfix(int signal, const GuestSignalVector* replacement, GuestSignalVector* previous);

}

#endif

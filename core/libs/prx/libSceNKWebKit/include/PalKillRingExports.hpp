#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

namespace NKWebKit::PalAbi {

struct KillRing {
    char empty;
};

static_assert(sizeof(KillRing) == 1);

}  // namespace NKWebKit::PalAbi

extern "C" {

void APS5_VABI _ZN3PAL8KillRing6appendERKN3WTF6StringE(NKWebKit::PalAbi::KillRing* self, const NKWebKit::WtfAbi::String* string);
void APS5_VABI _ZN3PAL8KillRing7prependERKN3WTF6StringE(NKWebKit::PalAbi::KillRing* self, const NKWebKit::WtfAbi::String* string);
NKWebKit::WtfAbi::String APS5_VABI _ZN3PAL8KillRing4yankEv(NKWebKit::PalAbi::KillRing* self);
void APS5_VABI _ZN3PAL8KillRing16startNewSequenceEv(NKWebKit::PalAbi::KillRing* self);
void APS5_VABI _ZN3PAL8KillRing16setToYankedStateEv(NKWebKit::PalAbi::KillRing* self);

}

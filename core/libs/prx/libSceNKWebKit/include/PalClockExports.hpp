#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/PalClockAbi.hpp"

extern "C" {

NKWebKit::PalAbi::Clock** APS5_VABI _ZN3PAL5Clock6createEv(NKWebKit::PalAbi::Clock** result);

}

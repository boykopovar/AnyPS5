#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UMutex;

namespace Icu {

struct InitOnce {
    std::int32_t state;
    std::int32_t errorCode;
};

}

extern "C" {
void APS5_VABI umtx_lock_nid_postfix(UMutex* mutex);
void APS5_VABI umtx_unlock_nid_postfix(UMutex* mutex);
std::int8_t APS5_VABI _ZN6icu_6820umtx_initImplPreInitERNS_9UInitOnceE_nid_postfix(Icu::InitOnce* once);
void APS5_VABI _ZN6icu_6821umtx_initImplPostInitERNS_9UInitOnceE_nid_postfix(Icu::InitOnce* once);
}

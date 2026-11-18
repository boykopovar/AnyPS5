#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"

struct UMutex;

extern "C" {
void APS5_VABI umtx_lock_nid_postfix(UMutex* mutex);
void APS5_VABI umtx_unlock_nid_postfix(UMutex* mutex);
}

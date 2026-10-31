#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcAcbAtomicGds_nid_no_patch(void);
int APS5_VABI sceAgcSuspendPointAndCheckStatus_nid_no_patch(void);

}

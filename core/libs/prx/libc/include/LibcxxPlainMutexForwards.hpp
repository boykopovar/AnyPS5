#pragma once

#include "SceTypes.hpp"
#include "general/VabiMacros.hpp"

struct LibcxxPlainMutexForwards_nid_no_patch {
    int (APS5_VABI* destroy)(PthreadMutex*);
    int (APS5_VABI* lock)(PthreadMutex*);
    int (APS5_VABI* trylock)(PthreadMutex*);
    int (APS5_VABI* unlock)(PthreadMutex*);
};

extern "C" void RegisterLibcxxPlainMutexForwards_nid_no_patch(const LibcxxPlainMutexForwards_nid_no_patch* forwards);

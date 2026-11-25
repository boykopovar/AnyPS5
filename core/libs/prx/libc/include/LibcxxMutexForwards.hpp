#pragma once

#include "SceTypes.hpp"
#include "general/VabiMacros.hpp"

struct LibcxxMutexForwards_nid_no_patch {
    int (APS5_VABI* mutexAttrInit)(PthreadMutexattr*);
    int (APS5_VABI* mutexAttrSettype)(PthreadMutexattr*, int);
    int (APS5_VABI* mutexAttrDestroy)(PthreadMutexattr*);
    int (APS5_VABI* mutexInit)(PthreadMutex*, const PthreadMutexattr*);
    int (APS5_VABI* mutexDestroy)(PthreadMutex*);
    int (APS5_VABI* mutexLock)(PthreadMutex*);
    int (APS5_VABI* mutexTrylock)(PthreadMutex*);
    int (APS5_VABI* mutexUnlock)(PthreadMutex*);
};

extern "C" void RegisterLibcxxMutexForwards_nid_no_patch(const LibcxxMutexForwards_nid_no_patch* forwards);

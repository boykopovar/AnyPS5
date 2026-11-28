#pragma once

#include "SceTypes.hpp"
#include "general/VabiMacros.hpp"

struct LibcxxConditionForwards_nid_no_patch {
    int (APS5_VABI* destroy)(PthreadCond*);
    int (APS5_VABI* broadcast)(PthreadCond*);
    int (APS5_VABI* signal)(PthreadCond*);
    int (APS5_VABI* timedwait)(PthreadCond*, PthreadMutex*, const KernelTimespec*);
    int (APS5_VABI* wait)(PthreadCond*, PthreadMutex*);
};

extern "C" void RegisterLibcxxConditionForwards_nid_no_patch(const LibcxxConditionForwards_nid_no_patch* forwards);

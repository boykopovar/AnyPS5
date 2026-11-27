#pragma once

#include "SceTypes.hpp"
#include "general/VabiMacros.hpp"

struct LibcxxThreadForwards_nid_no_patch {
    int (APS5_VABI* keyCreate)(PthreadKey*, pthread_key_destructor_func_t);
};

extern "C" void RegisterLibcxxThreadForwards_nid_no_patch(const LibcxxThreadForwards_nid_no_patch* forwards);

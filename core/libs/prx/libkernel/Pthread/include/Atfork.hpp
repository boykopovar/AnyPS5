#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_ATFORK_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_ATFORK_HPP

#include "prx/libc/include/general/VabiMacros.hpp"
#include <vector>

using PthreadAtforkHandler = void (APS5_VABI*)();

struct PthreadAtforkHandlers {
    PthreadAtforkHandler prepare;
    PthreadAtforkHandler parent;
    PthreadAtforkHandler child;
};

extern "C" int APS5_VABI pthread_atfork_nid_postfix(PthreadAtforkHandler prepare, PthreadAtforkHandler parent, PthreadAtforkHandler child);

std::vector<PthreadAtforkHandlers> PthreadAtforkHandlers_nid_no_patch();

#endif

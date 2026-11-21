#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRAWCOMMITVIEWSLOT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRAWCOMMITVIEWSLOT_HPP

#include "prx/libSceAgcDriver/Execution/include/Recipe.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"

namespace AgcDriver {

struct DrawCommitViewTag {};
using DrawCommitViewSlot = HostThreadSlot<DrawCommitView*, DrawCommitViewTag>;

}

#endif

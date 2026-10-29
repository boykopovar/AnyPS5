#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_INSTANCELOCKHOST_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_INCLUDE_INSTANCELOCKHOST_HPP

#include "prx/libkernel/System/include/InstanceLock.hpp"

#include <memory>

std::unique_ptr<IInstanceLockBackend> MakeHostInstanceLockBackend();
void ReleaseHostInstanceLock();

#endif

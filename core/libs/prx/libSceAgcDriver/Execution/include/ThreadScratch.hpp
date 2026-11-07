#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_THREADSCRATCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_THREADSCRATCH_HPP

#include "prx/libc/include/HostThreadSlot.hpp"
#include "ThreadOwned.hpp"

namespace AgcDriver {

template<typename T, typename Tag>
T& ThreadScratch() {
    auto* object = HostThreadSlot<T*, Tag>::Get();
    if (object == nullptr) {
        ShaderRecompiler::ThreadOwned(object);
        HostThreadSlot<T*, Tag>::Set(object);
    }
    return *object;
}

}

#endif

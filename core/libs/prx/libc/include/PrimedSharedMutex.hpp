#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_PRIMEDSHAREDMUTEX_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_PRIMEDSHAREDMUTEX_HPP

#include <shared_mutex>

class PrimedSharedMutex : public std::shared_mutex {
public:
    PrimedSharedMutex() {
        lock();
        unlock();
    }
};

#endif

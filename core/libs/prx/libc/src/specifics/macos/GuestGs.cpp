#ifdef __APPLE__
#include "prx/libc/include/GuestGs.hpp"
#include <pthread.h>
#include <cstdio>
#include <cstdlib>

extern "C" std::uint64_t Aps5GuestGsKey_nid_no_patch = [] {
    pthread_key_t key;
    if (pthread_key_create(&key, nullptr) != 0) {
        std::fprintf(stderr, "guest GS: cannot create the base key\n");
        std::abort();
    }
    return static_cast<std::uint64_t>(key);
}();

extern "C" std::uint64_t GuestGsGet_nid_no_patch() {
    return reinterpret_cast<std::uintptr_t>(pthread_getspecific(static_cast<pthread_key_t>(Aps5GuestGsKey_nid_no_patch)));
}

extern "C" void GuestGsSet_nid_no_patch(std::uint64_t base) {
    if (pthread_setspecific(static_cast<pthread_key_t>(Aps5GuestGsKey_nid_no_patch), reinterpret_cast<void*>(static_cast<std::uintptr_t>(base))) != 0) {
        std::fprintf(stderr, "guest GS: cannot store the base\n");
        std::abort();
    }
}
#endif

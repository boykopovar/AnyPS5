#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Pthread/Posix/Common.hpp"

#ifndef _WIN32
#include <dlfcn.h>
#endif

extern "C" {
int APS5_VABI scePthreadCancel(Pthread thread);
}

extern "C" {

int APS5_VABI sceCoredumpWriteUserData() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* APS5_VABI __tls_get_addr_nid_postfix(const void* index) {
#ifndef _WIN32
    static const auto host = reinterpret_cast<void* (*)(const void*)>(::dlsym(RTLD_DEFAULT, "__tls_get_addr"));
    if (host == nullptr) throw std::runtime_error("host __tls_get_addr is unavailable");
    return host(index);
#else
    throw std::runtime_error("__tls_get_addr is not supported on Windows");
#endif
}

// Canonical lib is libScePosix (dead import of Cyberpunk 2077): 35 of its
// 36 sibling imports resolve to libkernel, and libScePosix is not in the
// game's NEEDED list so only a NEEDED module can satisfy the loader here.
int APS5_VABI pthread_cancel_nid_postfix(Pthread thread) {
    if (!thread) return PosixThread::GUEST_EINVAL;
    return PosixThread::ToErrno(scePthreadCancel(thread));
}

}

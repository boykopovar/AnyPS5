#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
bool IsInitialPthread_nid_no_patch();

int APS5_VABI pthread_main_np_nid_postfix() {
    return IsInitialPthread_nid_no_patch() ? 1 : 0;
}

int APS5_VABI scePthreadMain(void) {
    return pthread_main_np_nid_postfix();
}


int APS5_VABI sceKernelGetThreadName(uint32_t id, char* name) {
    constexpr std::size_t capacity = 32;
    std::string found;
    if (!LiveGuestThreadName(id, &found)) return SCE_KERNEL_ERROR_ESRCH;
    found.resize(std::min(found.size(), capacity - 1));
    if (!GuestCopyOut_nid_no_patch(name, found.c_str(), found.size() + 1)) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

}

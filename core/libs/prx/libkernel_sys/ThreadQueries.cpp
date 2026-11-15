#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
bool IsInitialPthread_nid_no_patch();
bool LiveGuestThreadName_nid_no_patch(std::int64_t tid, char* name, std::size_t capacity);

int APS5_VABI pthread_main_np_nid_postfix() {
    return IsInitialPthread_nid_no_patch() ? 1 : 0;
}

int APS5_VABI scePthreadMain(void) {
    return pthread_main_np_nid_postfix();
}


int APS5_VABI sceKernelGetThreadName(uint32_t id, char* name) {
    constexpr std::size_t capacity = 32;
    char found[capacity];
    if (!LiveGuestThreadName_nid_no_patch(id, found, capacity)) return SCE_KERNEL_ERROR_ESRCH;
    if (!GuestCopyOut_nid_no_patch(name, found, std::strlen(found) + 1)) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

}

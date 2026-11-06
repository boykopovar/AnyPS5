#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"

extern "C" {
bool IsInitialPthread_nid_no_patch();

int APS5_VABI pthread_main_np_nid_postfix() {
    return IsInitialPthread_nid_no_patch() ? 1 : 0;
}

int APS5_VABI scePthreadMain(void) {
    return pthread_main_np_nid_postfix();
}

int APS5_VABI pthread_getstack_np_nid_postfix(void** stackAddress, std::size_t* stackSize) {
    const Pthread thread = pthread_self_nid_postfix();
    const void* address = thread->stackAddress;
    const std::size_t size = thread->stackSize;
    if (!GuestCopyOut_nid_no_patch(stackAddress, &address, sizeof(address))) return 14;
    if (!GuestCopyOut_nid_no_patch(stackSize, &size, sizeof(size))) return 14;
    return static_cast<int>(size);
}

}

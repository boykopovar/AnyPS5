#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/AtomicWaitForwards.hpp"

namespace {

struct AtomicWaitForwardsRegistration {
    AtomicWaitForwardsRegistration() {
        static const AtomicWaitForwards_nid_no_patch forwards{
            sceKernelSyncOnAddressWait32,
            sceKernelSyncOnAddressWait64,
            sceKernelSyncOnAddressWake,
        };
        RegisterAtomicWaitForwards_nid_no_patch(&forwards);
    }
    ~AtomicWaitForwardsRegistration() { RegisterAtomicWaitForwards_nid_no_patch(nullptr); }
    AtomicWaitForwardsRegistration(const AtomicWaitForwardsRegistration&) = delete;
    AtomicWaitForwardsRegistration& operator=(const AtomicWaitForwardsRegistration&) = delete;
};

const AtomicWaitForwardsRegistration registration;

}

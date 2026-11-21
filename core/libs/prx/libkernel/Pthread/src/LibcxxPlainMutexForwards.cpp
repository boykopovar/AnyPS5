#include "prx/libc/include/LibcxxPlainMutexForwards.hpp"
#include "prx/libkernel/include/Api.hpp"

namespace {

struct LibcxxPlainMutexForwardsRegistration {
    LibcxxPlainMutexForwardsRegistration() {
        static const LibcxxPlainMutexForwards_nid_no_patch forwards{
            pthread_mutex_destroy_nid_postfix,
            pthread_mutex_lock_nid_postfix,
            pthread_mutex_trylock_nid_postfix,
            pthread_mutex_unlock_nid_postfix,
        };
        RegisterLibcxxPlainMutexForwards_nid_no_patch(&forwards);
    }
    ~LibcxxPlainMutexForwardsRegistration() { RegisterLibcxxPlainMutexForwards_nid_no_patch(nullptr); }
    LibcxxPlainMutexForwardsRegistration(const LibcxxPlainMutexForwardsRegistration&) = delete;
    LibcxxPlainMutexForwardsRegistration& operator=(const LibcxxPlainMutexForwardsRegistration&) = delete;
};

const LibcxxPlainMutexForwardsRegistration registration;

}

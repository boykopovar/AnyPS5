#include "prx/libc/include/LibcxxMutexForwards.hpp"
#include "prx/libkernel/include/Api.hpp"

namespace {

struct LibcxxMutexForwardsRegistration {
    LibcxxMutexForwardsRegistration() {
        static const LibcxxMutexForwards_nid_no_patch forwards{
            pthread_mutexattr_init_nid_postfix,
            pthread_mutexattr_settype_nid_postfix,
            pthread_mutexattr_destroy_nid_postfix,
            pthread_mutex_init_nid_postfix,
            pthread_mutex_destroy_nid_postfix,
            pthread_mutex_lock_nid_postfix,
            pthread_mutex_trylock_nid_postfix,
            pthread_mutex_unlock_nid_postfix,
        };
        RegisterLibcxxMutexForwards_nid_no_patch(&forwards);
    }
    ~LibcxxMutexForwardsRegistration() { RegisterLibcxxMutexForwards_nid_no_patch(nullptr); }
    LibcxxMutexForwardsRegistration(const LibcxxMutexForwardsRegistration&) = delete;
    LibcxxMutexForwardsRegistration& operator=(const LibcxxMutexForwardsRegistration&) = delete;
};

const LibcxxMutexForwardsRegistration registration;

}

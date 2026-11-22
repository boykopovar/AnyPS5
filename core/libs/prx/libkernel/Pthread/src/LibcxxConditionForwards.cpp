#include "prx/libc/include/LibcxxConditionForwards.hpp"
#include "prx/libkernel/include/Api.hpp"

namespace {

struct LibcxxConditionForwardsRegistration {
    LibcxxConditionForwardsRegistration() {
        static const LibcxxConditionForwards_nid_no_patch forwards{
            pthread_cond_destroy_nid_postfix,
            pthread_cond_broadcast_nid_postfix,
            pthread_cond_signal_nid_postfix,
            pthread_cond_timedwait_nid_postfix,
            pthread_cond_wait_nid_postfix,
        };
        RegisterLibcxxConditionForwards_nid_no_patch(&forwards);
    }
    ~LibcxxConditionForwardsRegistration() { RegisterLibcxxConditionForwards_nid_no_patch(nullptr); }
    LibcxxConditionForwardsRegistration(const LibcxxConditionForwardsRegistration&) = delete;
    LibcxxConditionForwardsRegistration& operator=(const LibcxxConditionForwardsRegistration&) = delete;
};

const LibcxxConditionForwardsRegistration registration;

}

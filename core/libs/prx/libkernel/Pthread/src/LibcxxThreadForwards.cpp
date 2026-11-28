#include "prx/libc/include/LibcxxThreadForwards.hpp"
#include "prx/libkernel/include/Api.hpp"

namespace {

struct LibcxxThreadForwardsRegistration {
    LibcxxThreadForwardsRegistration() {
        static const LibcxxThreadForwards_nid_no_patch forwards{pthread_key_create_nid_postfix};
        RegisterLibcxxThreadForwards_nid_no_patch(&forwards);
    }
    ~LibcxxThreadForwardsRegistration() { RegisterLibcxxThreadForwards_nid_no_patch(nullptr); }
    LibcxxThreadForwardsRegistration(const LibcxxThreadForwardsRegistration&) = delete;
    LibcxxThreadForwardsRegistration& operator=(const LibcxxThreadForwardsRegistration&) = delete;
};

const LibcxxThreadForwardsRegistration registration;

}

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

struct DiscId;
struct DiscId2;

extern "C" {

int APS5_VABI sceDiscIdGet(DiscId* discId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDiscIdGet2(DiscId2* discId, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

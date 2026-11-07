#include "prx/libSceDiscId/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceDiscIdGet(DiscId* discId) {
    (void)discId;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceDiscIdGet2(DiscId2* discId, std::uint32_t flags) {
    (void)discId;
    (void)flags;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

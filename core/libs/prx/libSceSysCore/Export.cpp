#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

struct ApplicationInfo {
    std::byte data[152];
};

static_assert(sizeof(ApplicationInfo) == 152);

extern "C" {

int APS5_VABI sceApplicationGetAppInfoByAppId(std::uint32_t applicationId, ApplicationInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

#include "prx/libSceRandom/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"

namespace {

constexpr int SCE_RANDOM_ERROR_INVALID = static_cast<int>(0x817C0016);
constexpr std::size_t SCE_RANDOM_MAX_SIZE = 64;

}

extern "C" {

int APS5_VABI sceRandomGetRandomNumber(void* buf, size_t size) {
    if (!buf || size > SCE_RANDOM_MAX_SIZE) return SCE_RANDOM_ERROR_INVALID;
    HostEntropy_nid_no_patch(buf, size);
    return 0;
}

}

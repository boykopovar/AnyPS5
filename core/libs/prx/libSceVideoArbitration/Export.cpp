#include "prx/libSceVideoArbitration/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_VIDEO_ARBITRATION_ERROR_NOT_INITIALIZED = static_cast<int>(0x819E0001);
constexpr int SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT = static_cast<int>(0x819E0002);
constexpr std::uint64_t VideoArbitrationParamSize = 0x18;

}

extern "C" {

int APS5_VABI sceVideoArbitrationAcceptEvent() {
    return SCE_VIDEO_ARBITRATION_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoArbitrationEnable() {
    return SCE_VIDEO_ARBITRATION_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceVideoArbitrationInitialize(const void* param) {
    if (param == nullptr) return SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT;
    std::uint64_t size = 0;
    if (!GuestCopyIn_nid_no_patch(&size, param, sizeof(size))) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    if (size != VideoArbitrationParamSize) return SCE_VIDEO_ARBITRATION_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

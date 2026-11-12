#include "prx/libSceVideoDecoderArbitration/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT = static_cast<int>(0x81570001);
constexpr int SCE_VIDEO_DECODER_ARBITRATION_ERROR_OUT_OF_RANGE = static_cast<int>(0x81570004);
constexpr std::uint64_t DecoderArbitrationParamSize = 0x18;
constexpr std::size_t DecoderArbitrationValueOffset = 0x8;
constexpr std::size_t DecoderArbitrationCountOffset = 0x10;
constexpr std::uint32_t DecoderArbitrationValueMin = 0x100;
constexpr std::uint32_t DecoderArbitrationValueMax = 0x2FF;
constexpr std::uint64_t DecoderArbitrationCountMax = 0x7F;
constexpr std::uint32_t DecoderArbitrationEventMax = 1;

}

extern "C" {

int APS5_VABI sceVideoDecoderArbitrationAcceptEvent(std::uint32_t event) {
    if (event > DecoderArbitrationEventMax) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoDecoderArbitrationEnable(std::uint64_t reserved, void* callback) {
    if (reserved != 0 || callback == nullptr) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideoDecoderArbitrationEnableSuspendMode() {
    return 0;
}

int APS5_VABI sceVideoDecoderArbitrationInitialize(const void* param) {
    if (param == nullptr) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    const auto* bytes = static_cast<const std::uint8_t*>(param);
    std::uint64_t size = 0;
    if (!GuestCopyIn_nid_no_patch(&size, bytes, sizeof(size))) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    if (size != DecoderArbitrationParamSize) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    std::uint32_t value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, bytes + DecoderArbitrationValueOffset, sizeof(value))) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    if (value < DecoderArbitrationValueMin || value > DecoderArbitrationValueMax) {
        return SCE_VIDEO_DECODER_ARBITRATION_ERROR_OUT_OF_RANGE;
    }
    std::uint64_t count = 0;
    if (!GuestCopyIn_nid_no_patch(&count, bytes + DecoderArbitrationCountOffset, sizeof(count))) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    if (count == 0 || count > DecoderArbitrationCountMax) return SCE_VIDEO_DECODER_ARBITRATION_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

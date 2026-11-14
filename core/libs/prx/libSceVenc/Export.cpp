#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceVenc/include/InventoryExports.hpp"
#include "prx/libSceVenc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int VencError(std::uint32_t code) {
    return static_cast<int>(0x80C70000u | code);
}

constexpr std::uint64_t MemoryRegionSize = 0x20;
constexpr std::uint64_t PresetMinimumSize = 0x30;
constexpr std::uint64_t PresetMaximumSize = 0x38;
constexpr std::uint32_t ModeWithExtraPointer = 2;
constexpr std::uint32_t SocIdThreshold = 0x840F80;

template <typename TValue>
bool TryFieldAt(TValue& outValue, const void* object, std::size_t offset) {
    if (object == nullptr) return false;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&outValue, static_cast<const std::uint8_t*>(object) + offset, sizeof(outValue));
    errno = savedErrno;
    return copied;
}

template <typename TValue>
TValue FieldAt(const void* object, std::size_t offset) {
    TValue value{};
    TryFieldAt(value, object, offset);
    return value;
}

bool PointerFieldIsNull(const void* object, std::size_t offset) {
    const void* ptr = nullptr;
    if (!TryFieldAt(ptr, object, offset)) {
        return true;
    }
    return ptr == nullptr;
}

bool MemoryIsIncomplete(const void* memory) {
    return memory == nullptr || PointerFieldIsNull(memory, 0x8) || PointerFieldIsNull(memory, 0x10) ||
        PointerFieldIsNull(memory, 0x18);
}

int CheckRegion(const void* encoder, const void* region) {
    if (encoder == nullptr) {
        return VencError(0x00);
    }
    std::uint64_t regionSize = 0;
    if (region == nullptr || !TryFieldAt(regionSize, region, 0) || regionSize != MemoryRegionSize) {
        return VencError(0x10);
    }
    return 0;
}

int SetEncoderConfig(void* encoder, const void* config, std::uint32_t nullConfigCode, const char* name) {
    if (encoder == nullptr) {
        return VencError(0x00);
    }
    if (config == nullptr) {
        return VencError(nullConfigCode);
    }
    NotImplemented_nid_no_patch(name);
}

int RequireEncoder(const void* encoder, const char* name) {
    if (encoder == nullptr) {
        return VencError(0x00);
    }
    NotImplemented_nid_no_patch(name);
}

}

extern "C" {

int APS5_VABI sceVencCreateEncoder(const void* parameters, const void* memory, void** encoder) {
    if (encoder == nullptr) {
        return VencError(0x00);
    }
    void* localEncoder = nullptr;
    const int savedErrno = errno;
    if (!GuestCopyOut_nid_no_patch(encoder, &localEncoder, sizeof(localEncoder))) {
        errno = savedErrno;
        return VencError(0x00);
    }
    if (parameters == nullptr) {
        return VencError(0x01);
    }
    if (PointerFieldIsNull(parameters, 0x8)) {
        return VencError(0x04);
    }
    if (PointerFieldIsNull(parameters, 0x10)) {
        return VencError(0x05);
    }
    if (MemoryIsIncomplete(memory)) {
        return VencError(0x10);
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVencDeleteEncoder(void* encoder) {
    return RequireEncoder(encoder, __func__);
}

int APS5_VABI sceVencGetConfig(void* encoder, void* config) {
    (void)config;
    return RequireEncoder(encoder, __func__);
}

int APS5_VABI sceVencMapMemory(void* encoder, const void* region) {
    const int status = CheckRegion(encoder, region);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVencQueryHeader(const void* parameters, void* header) {
    if (parameters == nullptr) {
        return VencError(0x01);
    }
    if (PointerFieldIsNull(parameters, 0x8)) {
        return VencError(0x04);
    }
    if (header == nullptr) {
        return VencError(0x0F);
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVencQueryMemorySize(const void* parameters, void* memory) {
    (void)memory;
    if (parameters == nullptr) {
        return VencError(0x01);
    }
    const void* nestedParameters = FieldAt<const void*>(parameters, 0x8);
    if (nestedParameters == nullptr) {
        return VencError(0x04);
    }
    if (PointerFieldIsNull(parameters, 0x10)) {
        return VencError(0x05);
    }
    if (PointerFieldIsNull(parameters, 0x18)) {
        return VencError(0x06);
    }
    if (FieldAt<std::uint32_t>(nestedParameters, 0x20) == ModeWithExtraPointer && PointerFieldIsNull(parameters, 0x28)) {
        return VencError(0x07);
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVencQueryPreset(const void* preset, void* parameters, void* startParameters) {
    return sceVencQueryPreset2(preset, parameters, startParameters, nullptr);
}

int APS5_VABI sceVencSetConfig(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x03, __func__);
}

int APS5_VABI sceVencSetDecodedFrame(void* encoder, const void* frame) {
    if (encoder == nullptr) {
        return VencError(0x00);
    }
    if (frame != nullptr && PointerFieldIsNull(frame, 0x8)) {
        return VencError(0x11);
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVencSetPictureConfig(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x06, __func__);
}

int APS5_VABI sceVencSetRateControlConfig(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x08, __func__);
}

int APS5_VABI sceVencSetReferenceFrameInvalidationConfig(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x0E, __func__);
}

int APS5_VABI sceVencStartEncode(void* encoder, const void* startParameters) {
    (void)startParameters;
    return RequireEncoder(encoder, __func__);
}

int APS5_VABI sceVencStopEncode(void* encoder) {
    return RequireEncoder(encoder, __func__);
}

int APS5_VABI sceVencGetHeader(const void* parameters, const void* startParameters, const void* config,
    const void* memory, void* header) {
    (void)startParameters;
    (void)config;
    if (parameters == nullptr) {
        return VencError(0x01);
    }
    if (PointerFieldIsNull(parameters, 0x8)) {
        return VencError(0x04);
    }
    if (PointerFieldIsNull(parameters, 0x10)) {
        return VencError(0x05);
    }
    if (PointerFieldIsNull(parameters, 0x18)) {
        return VencError(0x06);
    }
    if (MemoryIsIncomplete(memory)) {
        return VencError(0x10);
    }
    if (header == nullptr) {
        return VencError(0x0F);
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVencSetNormalPrivacyProtectionConfig(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x0A, __func__);
}

int APS5_VABI sceVencSetImagePasteConfig(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x09, __func__);
}

int APS5_VABI sceVencQueryPreset2(const void* preset, void* parameters, void* startParameters, void* config) {
    (void)parameters;
    (void)startParameters;
    (void)config;
    if (preset == nullptr) {
        return VencError(0x04);
    }
    std::uint64_t size = 0;
    if (!TryFieldAt(size, preset, 0) || size < PresetMinimumSize || size > PresetMaximumSize) {
        return VencError(0x04);
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVencSetCodecConfig(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x05, __func__);
}

}

extern "C" {

int APS5_VABI sceVencUnmapMemory(void* encoder, const void* region) {
    const int status = CheckRegion(encoder, region);
    if (status != 0) {
        return status;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceVencUnknown_6nGfjRPqCUM_nid_no_patch(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x0D, __func__);
}
APS5_EXPORT("6nGfjRPqCUM", libSceVencUnknown_6nGfjRPqCUM_nid_no_patch);

int APS5_VABI libSceVencUnknown_B5iyzWMm_plus_DY_nid_no_patch(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x0B, __func__);
}
APS5_EXPORT("B5iyzWMm+DY", libSceVencUnknown_B5iyzWMm_plus_DY_nid_no_patch);

int APS5_VABI libSceVencUnknown_Vm5uPoERWmg_nid_no_patch(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x0C, __func__);
}
APS5_EXPORT("Vm5uPoERWmg", libSceVencUnknown_Vm5uPoERWmg_nid_no_patch);

int APS5_VABI libSceVencUnknown_JoBLKJB_minus_Nck_nid_no_patch(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x13, __func__);
}
APS5_EXPORT("JoBLKJB-Nck", libSceVencUnknown_JoBLKJB_minus_Nck_nid_no_patch);

int APS5_VABI libSceVencUnknown_766UkIIwtxU_nid_no_patch(void* encoder, const void* config) {
    return SetEncoderConfig(encoder, config, 0x14, __func__);
}
APS5_EXPORT("766UkIIwtxU", libSceVencUnknown_766UkIIwtxU_nid_no_patch);

int APS5_VABI libSceVencUnknown_2FTI1YJYNBw_nid_no_patch(void* encoder, void* argument) {
    (void)argument;
    return RequireEncoder(encoder, __func__);
}
APS5_EXPORT("2FTI1YJYNBw", libSceVencUnknown_2FTI1YJYNBw_nid_no_patch);

bool APS5_VABI libSceVencUnknown_39_plus_oWg0ZObY_nid_no_patch(int type) {
    if (type != 100 && type != 200) {
        return false;
    }
    return static_cast<std::uint32_t>(sceKernelGetMainSocId()) >= SocIdThreshold;
}
APS5_EXPORT("39+oWg0ZObY", libSceVencUnknown_39_plus_oWg0ZObY_nid_no_patch);

}

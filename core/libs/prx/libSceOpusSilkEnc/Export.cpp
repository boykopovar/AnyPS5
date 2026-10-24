#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

struct OpusSilkEncoder;

extern "C" {

int APS5_VABI sceOpusSilkEncCancel(OpusSilkEncoder* encoder, std::uint32_t batchId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncCreate(OpusSilkEncoder* encoder, std::int32_t sampleRate, std::int32_t channels, std::int32_t application) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncCreateEx(const std::uint32_t* contextId, OpusSilkEncoder* encoder, std::int32_t sampleRate, std::int32_t channels, std::int32_t application) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncCtl(OpusSilkEncoder* encoder, std::int32_t request, ...) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncDestroy(OpusSilkEncoder* encoder) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncEncode(OpusSilkEncoder* encoder, const std::int16_t* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncEncodeFloat(OpusSilkEncoder* encoder, const float* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncEncodeFloatWithPriority(OpusSilkEncoder* encoder, const float* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes, std::uint32_t priority) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncEncodeFloatWithPriorityAndTimeout(OpusSilkEncoder* encoder, const float* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes, std::uint32_t priority, std::uint32_t timeout, std::uint32_t* batchId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncEncodeWithPriority(OpusSilkEncoder* encoder, const std::int16_t* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes, std::uint32_t priority) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncEncodeWithPriorityAndTimeout(OpusSilkEncoder* encoder, const std::int16_t* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes, std::uint32_t priority, std::uint32_t timeout, std::uint32_t* batchId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncGetSize(std::int32_t channels) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncInitialize(std::uint32_t* contextId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncTerminate(const std::uint32_t* contextId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

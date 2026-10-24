#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

struct OpusCeltEncoder;

extern "C" {

int APS5_VABI sceOpusCeltEncCreate(OpusCeltEncoder* encoder, std::int32_t sampleRate, std::int32_t channels, std::int32_t application) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncCreateEx(const std::uint32_t* contextId, OpusCeltEncoder* encoder, std::int32_t sampleRate, std::int32_t channels, std::int32_t application) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncCtl(OpusCeltEncoder* encoder, std::int32_t request, ...) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncDestroy(OpusCeltEncoder* encoder) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncEncode(OpusCeltEncoder* encoder, const std::int16_t* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncEncodeFloat(OpusCeltEncoder* encoder, const float* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncEncodeFloatWithPriority(OpusCeltEncoder* encoder, const float* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes, std::uint32_t priority) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncEncodeWithPriority(OpusCeltEncoder* encoder, const std::int16_t* samples, std::int32_t frameSize, std::uint8_t* data, std::int32_t maxDataBytes, std::uint32_t priority) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncGetSize(std::int32_t channels) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncInitialize(std::uint32_t* contextId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncTerminate(const std::uint32_t* contextId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

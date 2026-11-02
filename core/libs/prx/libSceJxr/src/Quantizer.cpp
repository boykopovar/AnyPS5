#include "prx/libSceJxr/libSceJxr.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" {
void JxrRemapQp_nid_no_patch(void* quantizer, std::int32_t shift, std::int32_t scaled);
void JxrFormatQuantizer_nid_no_patch(void** quantizers, std::uint8_t mode, std::size_t channels, std::size_t position,
                                     std::int32_t shiftedUv, std::int32_t scaled);
std::int32_t JxrGetAcPredMode_nid_no_patch(void* macroblock, std::int32_t format);
void JxrInitZigzagScan_nid_no_patch(void* context);
}

namespace {

constexpr std::size_t MaxChannels = 16;

void Require(bool condition, const char* function, const char* message) {
    if (!condition) throw std::invalid_argument(std::string(function) + ": " + message);
}

void RequireShift(const void* quantizer, std::int32_t shift, std::int32_t scaled, const char* function) {
    Require(quantizer != nullptr, function, "null quantizer");
    const auto index = *static_cast<const std::uint8_t*>(quantizer);
    if (index == 0 || !scaled) return;
    const auto exponent = index < 16 ? shift : (index >> 4) - 1 + shift;
    Require(exponent >= 0 && exponent <= 24, function, "shift leaves the QP outside 32 bits");
}

} // namespace

extern "C" {

void APS5_VABI remapQP_nid_postfix(void* quantizer, std::int32_t shift, std::int32_t scaled) {
    RequireShift(quantizer, shift, scaled, __func__);
    JxrRemapQp_nid_no_patch(quantizer, shift, scaled);
}

void APS5_VABI formatQuantizer_nid_postfix(void** quantizers, std::uint8_t mode, std::size_t channels,
                                           std::size_t position, std::int32_t shiftedUv, std::int32_t scaled) {
    if (channels == 0) return;
    Require(quantizers != nullptr, __func__, "null quantizer table");
    Require(channels <= MaxChannels, __func__, "more than 16 channels");
    for (std::size_t channel = 0; channel < channels; ++channel)
        Require(quantizers[channel] != nullptr, __func__, "null channel quantizer");
    JxrFormatQuantizer_nid_no_patch(quantizers, mode, channels, position, shiftedUv, scaled);
}

std::int32_t APS5_VABI getACPredMode_nid_postfix(void* macroblock, std::int32_t format) {
    Require(macroblock != nullptr, __func__, "null macroblock");
    return JxrGetAcPredMode_nid_no_patch(macroblock, format);
}

void APS5_VABI InitZigzagScan_nid_postfix(void* context) {
    JxrInitZigzagScan_nid_no_patch(context);
}

} // extern "C"

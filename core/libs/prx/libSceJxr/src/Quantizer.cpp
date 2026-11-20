#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cerrno>
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

struct QuantizerLayout {
    std::uint8_t index;
    std::int32_t qp;
    std::int32_t offset;
    std::int32_t mantissa;
    std::int32_t exponent;
};

static_assert(sizeof(QuantizerLayout) == 20);
static_assert(offsetof(QuantizerLayout, qp) == 4);

void Require(bool condition, const char* function, const char* message) {
    if (!condition) throw std::invalid_argument(std::string(function) + ": " + message);
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

void RequireShift(const void* quantizer, std::int32_t shift, std::int32_t scaled, const char* function) {
    Require(quantizer != nullptr, function, "null quantizer");
    const auto index = *static_cast<const std::uint8_t*>(quantizer);
    if (index == 0 || !scaled) return;
    const std::int64_t exponent = static_cast<std::int64_t>(shift) + (index < 16 ? 0 : (index >> 4) - 1);
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
    std::array<void*, MaxChannels> table{};
    Require(CopyIn(table.data(), quantizers, channels * sizeof(void*)), __func__, "unreadable quantizer table");
    for (std::size_t channel = 0; channel < channels; ++channel)
        Require(table[channel] != nullptr, __func__, "null channel quantizer");
    JxrFormatQuantizer_nid_no_patch(table.data(), mode, channels, position, shiftedUv, scaled);
}

std::int32_t APS5_VABI getACPredMode_nid_postfix(void* macroblock, std::int32_t format) {
    Require(macroblock != nullptr, __func__, "null macroblock");
    return JxrGetAcPredMode_nid_no_patch(macroblock, format);
}

void APS5_VABI InitZigzagScan_nid_postfix(void* context) {
    JxrInitZigzagScan_nid_no_patch(context);
}

int APS5_VABI allocateQuantizer_nid_postfix(void** quantizers, std::size_t channels, std::size_t quantizerCount) {
    if (quantizerCount > MaxChannels || channels > MaxChannels) return -1;
    if (!quantizers) throw std::invalid_argument(std::string(__func__) + ": null quantizer table");

    auto* const allocation = static_cast<QuantizerLayout*>(
        malloc_nid_postfix(quantizerCount * sizeof(QuantizerLayout) * channels));
    std::array<void*, MaxChannels> table{};
    table[0] = allocation;
    std::size_t entries = 1;
    if (allocation) {
        for (std::size_t channel = 1; channel < channels; ++channel)
            table[channel] = quantizerCount == 0 ? allocation : allocation + channel * quantizerCount;
        entries = channels == 0 ? 1 : channels;
    }
    if (!CopyOut(quantizers, table.data(), entries * sizeof(void*))) {
        if (allocation) free_nid_postfix(allocation);
        throw std::invalid_argument(std::string(__func__) + ": unwritable quantizer table");
    }
    return allocation ? 0 : -1;
}

void APS5_VABI freeQuantizer_nid_postfix(void** quantizers) {
    Require(quantizers != nullptr, __func__, "null quantizer table");
    void* first = nullptr;
    Require(CopyIn(&first, quantizers, sizeof(first)), __func__, "unreadable quantizer table");
    if (first) free_nid_postfix(first);
}

} // extern "C"

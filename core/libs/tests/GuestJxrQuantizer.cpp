#include "prx/libSceJxr/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
void JxrQuantizerReferenceRemap_nid_no_patch(void* quantizer, std::int32_t shift, std::int32_t scaled);
void JxrQuantizerReferenceFormat_nid_no_patch(void** quantizers, std::uint8_t mode, std::size_t channels,
                                              std::size_t position, std::int32_t shiftedUv, std::int32_t scaled);
std::int32_t JxrQuantizerReferenceAcMode_nid_no_patch(void* macroblock, std::int32_t format);
void JxrQuantizerReferenceZigzag_nid_no_patch(void* context);
}

namespace {

constexpr std::size_t QuantizerSize = 20;
constexpr std::size_t MacroblockSize = 1160;
constexpr std::size_t ContextSize = 704;

struct Quantizer {
    std::uint8_t index;
    std::int32_t qp;
    std::int32_t offset;
    std::int32_t mantissa;
    std::int32_t exponent;
};
static_assert(sizeof(Quantizer) == QuantizerSize);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR quantizer: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireInvalidArgument(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

std::uint32_t seed = 0x13579bdf;
std::uint32_t Next() {
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}

void TestRemap() {
    for (std::int32_t scaled = 0; scaled < 2; ++scaled) {
        for (std::int32_t shift = 0; shift < 3; ++shift) {
            for (int index = 0; index < 256; ++index) {
                Quantizer guest{};
                guest.index = static_cast<std::uint8_t>(index);
                guest.qp = -1;
                Quantizer native = guest;
                remapQP_nid_postfix(&guest, shift, scaled);
                JxrQuantizerReferenceRemap_nid_no_patch(&native, shift, scaled);
                Require(std::memcmp(&guest, &native, sizeof(guest)) == 0, "remapQP differs from jxrlib");
            }
        }
    }
    Quantizer lossless{};
    lossless.qp = 9;
    lossless.offset = lossless.mantissa = lossless.exponent = 7;
    remapQP_nid_postfix(&lossless, 1, 1);
    Require(lossless.qp == 1 && lossless.offset == 0 && lossless.mantissa == 0 && lossless.exponent == 0,
            "index 0 is lossless");
    Quantizer plain{};
    plain.index = 4;
    remapQP_nid_postfix(&plain, 1, 0);
    Require(plain.qp == 1 && plain.offset == 0, "index 4 maps to QP 1 without scaled arithmetic");
    plain.index = 255;
    remapQP_nid_postfix(&plain, 1, 1);
    Require(plain.qp == 31 << 15 && plain.offset == ((plain.qp * 3 + 1) >> 3), "index 255 with shift 1");

    RequireInvalidArgument([] { remapQP_nid_postfix(nullptr, 1, 1); }, "null quantizer");
    Quantizer wide{};
    wide.index = 255;
    RequireInvalidArgument([&] { remapQP_nid_postfix(&wide, 11, 1); }, "QP over 32 bits");
    wide.index = 3;
    RequireInvalidArgument([&] { remapQP_nid_postfix(&wide, -1, 1); }, "negative shift");
    remapQP_nid_postfix(&wide, -1, 0);
}

void TestFormat() {
    for (int round = 0; round < 2000; ++round) {
        const std::size_t channels = 1 + Next() % 16;
        const std::size_t cqp = 1 + Next() % 16;
        const std::size_t position = Next() % cqp;
        const auto mode = static_cast<std::uint8_t>(Next() % 3);
        const std::int32_t shifted = static_cast<std::int32_t>(Next() % 2);
        const std::int32_t scaled = static_cast<std::int32_t>(Next() % 2);
        std::array<Quantizer, 256> guestStore{}, nativeStore{};
        for (std::size_t i = 0; i < channels * cqp; ++i) {
            guestStore[i].index = static_cast<std::uint8_t>(Next());
            guestStore[i].qp = static_cast<std::int32_t>(Next());
        }
        nativeStore = guestStore;
        std::array<void*, 16> guest{}, native{};
        for (std::size_t channel = 0; channel < channels; ++channel) {
            guest[channel] = &guestStore[channel * cqp];
            native[channel] = &nativeStore[channel * cqp];
        }
        formatQuantizer_nid_postfix(guest.data(), mode, channels, position, shifted, scaled);
        JxrQuantizerReferenceFormat_nid_no_patch(native.data(), mode, channels, position, shifted, scaled);
        Require(std::memcmp(guestStore.data(), nativeStore.data(), sizeof(guestStore)) == 0,
                "formatQuantizer differs from jxrlib");
    }
    formatQuantizer_nid_postfix(nullptr, 0, 0, 0, 0, 0);
    std::array<void*, 16> table{};
    RequireInvalidArgument([] { formatQuantizer_nid_postfix(nullptr, 0, 1, 0, 0, 0); }, "null quantizer table");
    RequireInvalidArgument([&] { formatQuantizer_nid_postfix(table.data(), 0, 1, 0, 0, 0); }, "null channel");
    RequireInvalidArgument([&] { formatQuantizer_nid_postfix(table.data(), 0, 17, 0, 0, 0); }, "17 channels");
}

void TestAcPredMode() {
    std::array<std::int32_t, MacroblockSize / 4> block{};
    for (int round = 0; round < 20000; ++round) {
        for (auto& value : block) value = static_cast<std::int32_t>(Next() % 2001) - 1000;
        if (round % 3 == 0)
            for (std::size_t i = 0; i < 48; ++i) block[i] /= 1 + static_cast<std::int32_t>(Next() % 64);
        const auto format = static_cast<std::int32_t>(round % 7);
        Require(getACPredMode_nid_postfix(block.data(), format) ==
                    JxrQuantizerReferenceAcMode_nid_no_patch(block.data(), format),
                "getACPredMode differs from jxrlib");
    }
    block.fill(0);
    Require(getACPredMode_nid_postfix(block.data(), 0) == 2, "no energy");
    block[1] = 5;
    Require(getACPredMode_nid_postfix(block.data(), 0) == 0, "horizontal energy");
    block[4] = 5;
    Require(getACPredMode_nid_postfix(block.data(), 0) == 2, "balanced energy");
    block[1] = 0;
    Require(getACPredMode_nid_postfix(block.data(), 0) == 1, "vertical energy");
    RequireInvalidArgument([] { getACPredMode_nid_postfix(nullptr, 0); }, "null macroblock");
}

void TestZigzag() {
    std::array<std::uint8_t, ContextSize> guest{}, native{};
    for (std::size_t i = 0; i < ContextSize; ++i) guest[i] = native[i] = static_cast<std::uint8_t>(Next());
    InitZigzagScan_nid_postfix(guest.data());
    JxrQuantizerReferenceZigzag_nid_no_patch(native.data());
    Require(guest == native, "InitZigzagScan differs from jxrlib");
    std::uint32_t first = 0;
    std::memcpy(&first, guest.data() + 216 + 4, sizeof(first));
    Require(first == 0, "lowpass scan starts at coefficient 0");
    InitZigzagScan_nid_postfix(nullptr);
}

} // namespace

int main() {
    TestRemap();
    TestFormat();
    TestAcPredMode();
    TestZigzag();
    return 0;
}

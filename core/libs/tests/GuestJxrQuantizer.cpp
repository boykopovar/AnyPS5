#include "prx/libSceJxr/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
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

bool failAllocation = false;
int liveBlocks = 0;

void* APS5_VABI Allocate(std::size_t size) {
    if (failAllocation) return nullptr;
    ++liveBlocks;
    return std::malloc(size);
}
void APS5_VABI Free(void* pointer) {
    if (pointer) --liveBlocks;
    std::free(pointer);
}
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    if (failAllocation) return nullptr;
    ++liveBlocks;
    return std::calloc(count, size);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&Allocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&Free));
    write(replacement, 0x30, reinterpret_cast<void*>(&Calloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&Reallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&Align));
    write(replacement, 0x48, reinterpret_cast<void*>(&Realign));
    write(replacement, 0x50, reinterpret_cast<void*>(&PosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}

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
    for (const std::uint8_t index : {std::uint8_t{3}, std::uint8_t{255}}) {
        wide.index = index;
        RequireInvalidArgument([&] { remapQP_nid_postfix(&wide, INT32_MAX, 1); }, "largest shift");
        RequireInvalidArgument([&] { remapQP_nid_postfix(&wide, INT32_MIN, 1); }, "smallest shift");
    }
    wide.index = 3;
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

void TestFree() {
    RequireInvalidArgument([] { freeQuantizer_nid_postfix(nullptr); }, "freeQuantizer accepted a null table");
    void* table[16] = {};
    const int empty = liveBlocks;
    freeQuantizer_nid_postfix(table);
    Require(liveBlocks == empty, "freeQuantizer of an empty table released a block");
    for (void* entry : table) Require(entry == nullptr, "freeQuantizer changed an empty table");
    auto* const block = static_cast<std::uint8_t*>(malloc_nid_postfix(3 * 4 * 20));
    Require(block != nullptr, "allocation");
    for (std::size_t channel = 0; channel < 3; ++channel) table[channel] = block + channel * 4 * 20;
    const int live = liveBlocks;
    freeQuantizer_nid_postfix(table);
    Require(liveBlocks == live - 1, "freeQuantizer did not release the shared block once");
    for (std::size_t channel = 0; channel < 3; ++channel)
        Require(table[channel] == block + channel * 4 * 20, "freeQuantizer changed the channel pointers");
}

int main() {
    InitializeHeap();
    TestRemap();
    TestFormat();
    TestAcPredMode();
    TestZigzag();
    TestFree();
    return 0;
}

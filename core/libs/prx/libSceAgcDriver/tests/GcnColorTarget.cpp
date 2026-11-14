#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestArena.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", what);
    ++failures;
}

template <typename Function>
std::string Failure(Function function) {
    try {
        function();
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
constexpr std::uint32_t Format8888 = 10u;
constexpr std::uint32_t BlendClamp = 1u << 15u;
constexpr std::uint32_t FastClear = 1u << 13u;
constexpr std::uint32_t Compression = 1u << 14u;
constexpr std::uint32_t Thin1D = 13u;
constexpr std::uint32_t Display2D = 10u;
constexpr std::uint32_t Thin2D = 14u;
constexpr std::uint32_t TiledWidth = 128;
constexpr std::uint32_t TiledHeight = 64;
constexpr std::size_t TiledSliceBytes = TiledWidth * TiledHeight * 4u;
constexpr std::size_t SliceMemoryBytes = 4u * TiledSliceBytes;

constexpr std::size_t ColorBytes = Width * Height * 4u;
constexpr std::size_t CmaskBytes = 512u;
constexpr std::size_t MemoryBytes = 65536u;
std::byte* colorMemory = nullptr;
std::byte* cmaskMemory = nullptr;

std::byte* LowMemory(std::size_t bytes) {
#ifdef _WIN32
    auto* block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
    if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
    return block;
#elif defined(__APPLE__)
    constexpr std::uint32_t ReadWriteProtection = 0x04;
    auto* block = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536));
    if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, ReadWriteProtection, bytes);
    return block;
#else
#ifdef ANYPS5_ENABLE_ASAN
    constexpr std::uintptr_t FirstHint = 0x60000000;
#else
    constexpr std::uintptr_t FirstHint = std::uintptr_t{1} << 32u;
#endif
    for (std::uintptr_t hint = FirstHint; hint < (std::uintptr_t{1} << 40u); hint += std::uintptr_t{1} << 32u) {
        void* mapped = mmap(reinterpret_cast<void*>(hint), bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (mapped != MAP_FAILED) return static_cast<std::byte*>(mapped);
    }
    return nullptr;
#endif
}

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

void SetTarget(AgcDriver::Registers& cx, std::uint32_t samples, std::uint32_t fragments, std::uint32_t flags, bool cmask = true) {
    cx[0x318] = static_cast<std::uint32_t>(AddressOf(colorMemory) >> 8u);
    cx[0x319] = Width / 8u - 1u;
    cx[0x31a] = Width * Height / 64u - 1u;
    cx[0x31b] = 0u;
    cx[0x31c] = (Format8888 << 2u) | BlendClamp | flags;
    cx[0x31d] = Thin1D | (static_cast<std::uint32_t>(std::countr_zero(samples)) << 12u) | (static_cast<std::uint32_t>(std::countr_zero(fragments)) << 15u);
    cx[0x31f] = cmask ? static_cast<std::uint32_t>(AddressOf(cmaskMemory) >> 8u) : 0u;
    cx[0x320] = static_cast<std::uint32_t>(CmaskBytes / 128u - 1u);
}

AgcDriver::Graphics::ColorTarget Decode(std::uint32_t samples, std::uint32_t fragments, std::uint32_t flags, bool cmask = true) {
    AgcDriver::Registers cx;
    SetTarget(cx, samples, fragments, flags, cmask);
    return AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
}

void MultisampledTargetsDecode() {
    const auto cleared = Decode(4u, 4u, FastClear | Compression);
    Check(cleared.samples == 4u, "a 4x GCN color target did not decode with 4 samples");
    Check(cleared.cmaskAddress == AddressOf(cmaskMemory) && cleared.cmaskBytes == CmaskBytes, "a 4x GCN color target lost its CMASK address or size");
    Check(cleared.cmaskFastClear, "a 4x GCN color target with FAST_CLEAR is not a CMASK fast clear");
    const auto compressed = Decode(8u, 8u, Compression);
    Check(compressed.samples == 8u && compressed.cmaskAddress == AddressOf(cmaskMemory) && !compressed.cmaskFastClear, "an 8x FMASK-compressed GCN color target decoded wrong");
    const auto plain = Decode(2u, 2u, 0u);
    Check(plain.samples == 2u && plain.cmaskAddress == 0u, "a 2x GCN color target without compression took a CMASK");
    Check(Failure([] { Decode(4u, 4u, FastClear, false); }).find("has no CMASK address") != std::string::npos, "a 4x GCN fast clear without a CMASK address was accepted");
}

std::string FmaskDecompress(std::uint32_t visibleHeight) {
    AgcDriver::QueueState queue;
    auto& cx = queue.context;
    SetTarget(cx, 4u, 4u, Compression);
    cx[0x202] = 0xcc0000u | (5u << 4u);
    cx[0x010] = 0u;
    cx[0x011] = 0u;
    cx[0x8e] = 0xfu;
    cx[0x292] = 0u;
    cx[0x10f] = std::bit_cast<std::uint32_t>(Width / 2.0f);
    cx[0x110] = std::bit_cast<std::uint32_t>(Width / 2.0f);
    cx[0x111] = std::bit_cast<std::uint32_t>(visibleHeight / 2.0f);
    cx[0x112] = std::bit_cast<std::uint32_t>(visibleHeight / 2.0f);
    cx[0x90] = 0u;
    cx[0x91] = (visibleHeight << 16u) | Width;
    return Failure([&] {
        const auto pass = AgcDriver::Graphics::DecodeColorMetadataPass(queue);
        if (!pass || pass->targets.size() != 1u || pass->targets[0].samples != 4u) throw std::runtime_error("the FMASK decompress did not decode its 4x target");
    });
}

AgcDriver::Graphics::ColorTarget DecodeSlice(std::uint64_t base, std::uint32_t tileIndex, std::uint32_t slice, std::uint32_t samples) {
    AgcDriver::Registers cx;
    cx[0x318] = static_cast<std::uint32_t>(base >> 8u);
    cx[0x319] = TiledWidth / 8u - 1u;
    cx[0x31a] = TiledWidth * TiledHeight / 64u - 1u;
    cx[0x31b] = slice | (slice << 13u);
    cx[0x31c] = (Format8888 << 2u) | BlendClamp;
    const auto log2Samples = static_cast<std::uint32_t>(std::countr_zero(samples));
    cx[0x31d] = tileIndex | (log2Samples << 12u) | (log2Samples << 15u);
    return AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
}

void LaterSlicesDecode() {
    using AgcDriver::Graphics::GcnLayerBankXor;
    using AgcDriver::Graphics::GcnTiling;
    using AgcDriver::Graphics::TextureTileMode;
    static std::byte* const sliceMemory = LowMemory(SliceMemoryBytes);
    const auto base = AddressOf(sliceMemory);
    if (sliceMemory == nullptr || base + SliceMemoryBytes > (std::uint64_t{1} << 40u)) {
        Check(false, "cannot allocate array slice test memory below 1 TiB");
        return;
    }
    const auto third = DecodeSlice(base, Thin2D, 3u, 1u);
    Check(third.surfaceAddress == base + 3u * TiledSliceBytes && third.address == third.surfaceAddress && third.bytes == TiledSliceBytes, "slice 3 of a 2D-tiled GCN array is not at the array base plus three slices");
    Check(third.pipeBankXor == GcnLayerBankXor(GcnTiling(TextureTileMode::kGcnThin2D, 4u), 3u) && third.pipeBankXor == (5u << 11u), "slice 3 of a 2D-tiled GCN array does not take the bank rotation the texture path gives array layer 3, (16 / 2 - 1) * 3 modulo 16 on the bank bits");
    const auto display = DecodeSlice(base, Display2D, 1u, 1u);
    Check(display.surfaceAddress == base + TiledSliceBytes && display.pipeBankXor == GcnLayerBankXor(GcnTiling(TextureTileMode::kGcnDisplay2D, 4u), 1u) && display.pipeBankXor == (7u << 11u), "slice 1 of a display 2D-tiled GCN array does not take the bank rotation of array layer 1");
    Check(DecodeSlice(base, Thin2D, 0u, 1u).pipeBankXor == 0u, "slice 0 of a 2D-tiled GCN array has a bank XOR");
    const auto micro = DecodeSlice(base, Thin1D, 2u, 1u);
    Check(micro.surfaceAddress == base + 2u * TiledSliceBytes && micro.pipeBankXor == 0u, "slice 2 of a 1D-tiled GCN array is not at the base plus two slices without a bank XOR");
    Check(DecodeSlice(base, Thin2D, 0u, 4u).samples == 4u, "slice 0 of a 2D-tiled 4x GCN array did not decode");
    Check(Failure([base] { DecodeSlice(base, Thin2D, 1u, 4u); }).find("multisampled color views of a later slice of a 2D-tiled array") != std::string::npos, "slice 1 of a 2D-tiled 4x GCN array was accepted");
}

void PaddedTargetsAreCovered() {
    Check(FmaskDecompress(Height).empty(), "an FMASK decompress over a whole 4x GCN target was refused");
    const auto padded = FmaskDecompress(Height - 4u);
    Check(padded.empty(), ("an FMASK decompress over the rows of a 4x GCN target its tile padding does not add was refused: " + padded).c_str());
    Check(FmaskDecompress(Height - 16u).find("over part of a color target") != std::string::npos, "an FMASK decompress over part of a 4x GCN target was accepted");
}

}

int main() {
    try {
        AgcDriverUseGcnShaders_nid_postfix();
        colorMemory = LowMemory(MemoryBytes);
        if (colorMemory == nullptr || AddressOf(colorMemory) + MemoryBytes > (std::uint64_t{1} << 40u)) {
            std::fprintf(stderr, "cannot allocate test memory below 1 TiB\n");
            return 1;
        }
        cmaskMemory = colorMemory + ColorBytes;
        MultisampledTargetsDecode();
        PaddedTargetsAreCovered();
        LaterSlicesDecode();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (failures != 0) return 1;
    std::puts("GCN multisampled color targets decode with their CMASK, and a metadata pass over the visible rows of a padded target is covered");
    return 0;
}

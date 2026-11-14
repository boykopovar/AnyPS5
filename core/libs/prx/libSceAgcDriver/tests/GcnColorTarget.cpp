#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
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

constexpr std::size_t ColorBytes = Width * Height * 4u;
constexpr std::size_t CmaskBytes = 512u;
constexpr std::size_t MemoryBytes = 65536u;
constexpr std::size_t FmaskOffset = ColorBytes + 4096u;
constexpr std::size_t MultisampledSliceMemoryBytes = 8u * ColorBytes;
std::byte* colorMemory = nullptr;
std::byte* cmaskMemory = nullptr;
std::byte* fmaskMemory = nullptr;

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

void SetTarget(AgcDriver::Registers& cx, std::uint32_t samples, std::uint32_t fragments, std::uint32_t flags, bool cmask = true, std::uint64_t fmask = 0) {
    cx[0x318] = static_cast<std::uint32_t>(AddressOf(colorMemory) >> 8u);
    cx[0x319] = Width / 8u - 1u;
    cx[0x31a] = Width * Height / 64u - 1u;
    cx[0x31b] = 0u;
    cx[0x31c] = (Format8888 << 2u) | BlendClamp | flags;
    cx[0x31d] = Thin1D | (static_cast<std::uint32_t>(std::countr_zero(samples)) << 12u) | (static_cast<std::uint32_t>(std::countr_zero(fragments)) << 15u);
    cx[0x31f] = cmask ? static_cast<std::uint32_t>(AddressOf(cmaskMemory) >> 8u) : 0u;
    cx[0x320] = static_cast<std::uint32_t>(CmaskBytes / 128u - 1u);
    cx[0x321] = static_cast<std::uint32_t>(fmask >> 8u);
    cx[0x322] = Width * Height / 64u - 1u;
}

AgcDriver::Graphics::ColorTarget Decode(std::uint32_t samples, std::uint32_t fragments, std::uint32_t flags, bool cmask = true, std::uint64_t fmask = 0) {
    AgcDriver::Registers cx;
    SetTarget(cx, samples, fragments, flags, cmask, fmask);
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

void FewerFragmentsDecode() {
    const auto fmask = AddressOf(fmaskMemory);
    const auto eqaa = Decode(4u, 2u, Compression, true, fmask);
    Check(eqaa.samples == 4u && eqaa.coverageSamples == 4u && eqaa.eqaaFragments == 2u, "a 4-sample 2-fragment GCN color target did not decode as EQAA at its 4 coverage samples");
    Check(eqaa.fmaskAddress == fmask && eqaa.cmaskAddress == AddressOf(cmaskMemory), "a 4-sample 2-fragment GCN color target lost its FMASK or CMASK base");
    Check(eqaa.gcnFmaskBytes == Width * Height, "the FMASK of a 4-sample 2-fragment GCN color target does not hold 2 bits per sample, one byte per pixel");
    const auto eightTwo = Decode(8u, 2u, Compression, true, fmask);
    Check(eightTwo.samples == 8u && eightTwo.eqaaFragments == 2u && eightTwo.gcnFmaskBytes == Width * Height * 2u, "the FMASK of an 8-sample 2-fragment GCN color target does not hold 2 bits per sample, two bytes per pixel");
    const auto eightFour = Decode(8u, 4u, Compression, true, fmask);
    Check(eightFour.samples == 8u && eightFour.eqaaFragments == 4u && eightFour.gcnFmaskBytes == Width * Height * 4u, "the FMASK of an 8-sample 4-fragment GCN color target does not hold 4 bits per sample, four bytes per pixel");
    const auto oneFragment = Decode(4u, 1u, 0u, true, fmask);
    Check(oneFragment.samples == 1u && oneFragment.coverageSamples == 4u && oneFragment.eqaaFragments == 0u, "a 4-sample 1-fragment GCN color target did not decode as one stored sample with 4 coverage samples");
    Check(oneFragment.fmaskAddress == 0u && oneFragment.gcnFmaskBytes == 0u && oneFragment.cmaskAddress == 0u, "an uncompressed 4-sample 1-fragment GCN color target took an FMASK or CMASK");
    const auto compressedOne = Decode(4u, 1u, Compression, true, fmask);
    Check(compressedOne.samples == 1u && compressedOne.fmaskAddress == fmask && compressedOne.gcnFmaskBytes == Width * Height, "the FMASK of a compressed 4-sample 1-fragment GCN color target does not hold 1 bit for each of 8 samples, one byte per pixel");
    Check(Failure([] { Decode(4u, 2u, 0u, true, AddressOf(fmaskMemory)); }).find("supported only with FMASK") != std::string::npos, "a 4-sample 2-fragment GCN color target without COMPRESSION was accepted");
    Check(Failure([] { Decode(4u, 2u, Compression, true, 0u); }).find("has no FMASK address") != std::string::npos, "a compressed 4-sample 2-fragment GCN color target without an FMASK base was accepted");
    Check(Failure([] { Decode(2u, 4u, 0u); }).find("more fragments than samples") != std::string::npos, "a 2-sample 4-fragment GCN color target was accepted");
    Check(Failure([] { Decode(4u, 1u, FastClear, true, AddressOf(fmaskMemory)); }).find("fast clear (CMASK) is not implemented") != std::string::npos, "a fast-cleared 4-sample 1-fragment GCN color target was accepted");
}

AgcDriver::Graphics::ColorTarget DecodeMultisampledSlice(std::uint64_t base, std::uint32_t samples, std::uint32_t fragments, std::uint32_t flags, std::uint32_t slice) {
    AgcDriver::Registers cx;
    SetTarget(cx, samples, fragments, flags, true, AddressOf(fmaskMemory));
    cx[0x318] = static_cast<std::uint32_t>(base >> 8u);
    cx[0x31b] = slice | (slice << 13u);
    return AgcDriver::Graphics::DecodeColorBuffer(cx, 0);
}

void MultisampledSlicesDecode() {
    static std::byte* const memory = LowMemory(MultisampledSliceMemoryBytes);
    const auto base = AddressOf(memory);
    if (memory == nullptr || base + MultisampledSliceMemoryBytes > (std::uint64_t{1} << 40u)) {
        Check(false, "cannot allocate multisampled array slice test memory below 1 TiB");
        return;
    }
    const auto cmask = AddressOf(cmaskMemory);
    const auto fmask = AddressOf(fmaskMemory);
    const auto msaa = DecodeMultisampledSlice(base, 4u, 4u, FastClear | Compression, 1u);
    Check(msaa.address == base + 4u * ColorBytes && msaa.surfaceAddress == msaa.address && msaa.bytes == ColorBytes, "slice 1 of a 4x GCN array does not start one slice of its 4 fragments past the array base");
    Check(msaa.cmaskAddress == cmask + CmaskBytes && msaa.cmaskBytes == CmaskBytes, "the CMASK of slice 1 of a 4x GCN array does not start one CMASK slice past the CMASK base");
    const auto eqaa = DecodeMultisampledSlice(base, 4u, 2u, Compression, 2u);
    Check(eqaa.address == base + 2u * 2u * ColorBytes, "slice 2 of a 4-sample 2-fragment GCN array does not start two slices of its 2 fragments past the array base");
    Check(eqaa.gcnFmaskBytes == Width * Height && eqaa.fmaskAddress == fmask + 2u * eqaa.gcnFmaskBytes, "the FMASK of slice 2 of a 4-sample 2-fragment GCN array does not start two FMASK slices past the FMASK base");
    Check(eqaa.cmaskAddress == cmask + 2u * CmaskBytes, "the CMASK of slice 2 of a 4-sample 2-fragment GCN array does not start two CMASK slices past the CMASK base");
    const auto oneFragment = DecodeMultisampledSlice(base, 4u, 1u, Compression, 3u);
    Check(oneFragment.address == base + 3u * ColorBytes && oneFragment.cmaskAddress == 0u, "slice 3 of a compressed 4-sample 1-fragment GCN array does not start three one-fragment slices past the array base");
    Check(oneFragment.gcnFmaskBytes == Width * Height && oneFragment.fmaskAddress == fmask + 3u * oneFragment.gcnFmaskBytes, "the FMASK of slice 3 of a compressed 4-sample 1-fragment GCN array does not start three FMASK slices past the FMASK base");
    const auto first = DecodeMultisampledSlice(base, 4u, 2u, Compression, 0u);
    Check(first.address == base && first.fmaskAddress == fmask && first.cmaskAddress == cmask, "slice 0 of a 4-sample 2-fragment GCN array does not start at the array, FMASK and CMASK bases");
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
        fmaskMemory = colorMemory + FmaskOffset;
        MultisampledTargetsDecode();
        FewerFragmentsDecode();
        MultisampledSlicesDecode();
        PaddedTargetsAreCovered();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    if (failures != 0) return 1;
    std::puts("GCN multisampled color targets decode with their CMASK, EQAA and one-fragment targets with their FMASK base and size, and a metadata pass over the visible rows of a padded target is covered");
    return 0;
}

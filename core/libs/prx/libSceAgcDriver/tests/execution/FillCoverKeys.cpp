#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::DccKeyCount;
using AgcDriver::Graphics::DccKeys;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using Cover = StorageTexture::FillCover;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Format8888Unorm = 56;
constexpr std::uint32_t TileRenderTarget64KB = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t SurfaceBytes = 65536;
constexpr std::size_t KeyBytes = SurfaceBytes / 256;
// The pipe-aligned key extent of the 128x128 surface (one 4 KiB metadata block): longer than the
// one-key-per-256-bytes count, so a fill between the two is told apart from a key-sized one.
constexpr std::size_t KeyExtent = 4096;
// Each scenario's images sit in their own region, so no fill of one overlaps the other's images.
constexpr std::size_t Region = 0x40000;
constexpr std::size_t BlockBytes = 2 * Region;

alignas(256) constexpr std::array<std::uint32_t, 8> WriteCode{
    0x7e020280, 0x7e0402f2, 0x7e0602f2, 0x7e0802f2, 0x7e0a02f2, 0xf0201f08, 0x00010200, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

// Guest memory for the surfaces, registered as allocated so the driver reads and writes it.
class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, BlockBytes));
#endif
        Require(block != nullptr, "fill cover test: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

const char* CoverName(Cover cover) {
    switch (cover) {
        case Cover::None: return "none";
        case Cover::Exact: return "exact";
        case Cover::Inside: return "inside";
        case Cover::Around: return "around";
        case Cover::Straddle: return "straddle";
        case Cover::Several: return "several";
        case Cover::Keys: return "keys";
        case Cover::Layer: return "layer";
    }
    return "?";
}

void ExpectCover(std::uint64_t address, std::size_t bytes, Cover expected, const char* what) {
    const auto coverage = StorageTexture::ClassifyFill(address, bytes);
    if (coverage.cover == expected) return;
    char text[256];
    std::snprintf(text, sizeof(text), "%s: fill 0x%llx+0x%zx is %s, expected %s", what, static_cast<unsigned long long>(address), bytes, CoverName(coverage.cover), CoverName(expected));
    Expect(false, text);
}

// The 128x128 RGBA8 surface at `address` with its DCC keys at `keys` (none when 0), with the words
// DccKeysFirstWrite.cpp lays out; the keys are pipe-aligned so their extent is KeyExtent.
std::array<std::uint32_t, 8> SurfaceWords(std::uint64_t address, std::uint64_t keys) {
    const auto meta = keys >> 8u;
    const std::uint32_t metaBits = keys != 0 ? (1u << 19u) | (1u << 20u) | (1u << 21u) : 0u;
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888Unorm << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileRenderTarget64KB << 20u) | (Type2D << 28u),
        0u,
        0u,
        metaBits | (static_cast<std::uint32_t>(meta & 0xffu) << 24u),
        static_cast<std::uint32_t>(meta >> 8u),
    };
}

void Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

// The storage image the title's first write creates for the surface at `address`: a dispatch that
// stores into it, flushed at once so that no later write-back of it covers memory another image is
// checked over. The GPU mutex is taken here, not held by the caller.
std::shared_ptr<StorageTexture> Create(AgcDriver::VulkanDevice& device, std::uint64_t address, std::uint64_t keys) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto words = SurfaceWords(address, keys);
    std::copy(words.begin(), words.end(), userData.begin() + 4);
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    Dispatch(device, WriteCode, userData);
    StorageTexture::FlushPending(address, SurfaceBytes, nullptr, "fill cover test");
    device.WaitIdle();
    const auto image = StorageTexture::FindLive(address, SurfaceBytes);
    Require(image != nullptr && image->Descriptor().dccAddress == keys, "fill cover test: the dispatch did not create the image under its keys");
    return image;
}

// Scenario one: image B (no DCC) and the image C beside it cover [0, 0x20000) of the region; image A
// (texels at 0x20000) has its keys at 0x8000, inside B, so a fill there meets B and only B.
// Scenario two: image D (texels at the region's start) has its keys at 0x10000, outside every other
// image, so a fill there meets nothing.
void Run(AgcDriver::VulkanDevice& device, std::uint8_t* block) {
    const auto base = AddressOf(block);
    const auto aKeys = base + 0x8000;
    const auto dKeys = base + Region + 0x10000;
    std::vector<std::shared_ptr<StorageTexture>> images;

    std::memset(block, 0x55, 0x30000);
    images.push_back(Create(device, base, 0));
    images.push_back(Create(device, base + 0x10000, 0));
    // The keys go in after B's write-back, which would store B's texels over them.
    std::memset(block + 0x8000, 0xff, KeyExtent);
    images.push_back(Create(device, base + 0x20000, aKeys));
    const auto& keyed = *images.back();
    const auto extent = DccKeyCount(keyed.Descriptor(), keyed.GuestBytes());
    Require(extent == KeyExtent, "fill cover test: the keyed image's key extent is not the pipe-aligned one");

    std::memset(block + Region, 0x55, SurfaceBytes);
    std::memset(block + Region + 0x10000, 0xff, KeyExtent);
    images.push_back(Create(device, base + Region, dKeys));
    const auto alone = images.back();

    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    // (a) a key-sized fill (and one up to the whole key extent) at the keys of an image whose keys
    // lie in another live image: the key fill the bloom target's keys get over dead aliased images.
    ExpectCover(aKeys, KeyBytes, Cover::Keys, "a key-sized fill over the keys of an image inside another live image");
    ExpectCover(aKeys, extent, Cover::Keys, "a fill of the whole key extent over the keys of an image inside another live image");
    // (c) past the key extent, the same fill also writes the memory after the keys, which the
    // overlapping image may own: not a key fill (inferred, see docs/dev/TechnicalDebt.md).
    ExpectCover(aKeys, extent + 1, Cover::Inside, "a fill one byte past the key extent, over another live image");
    // (b) the same lengths not at a key address stay what they were.
    ExpectCover(aKeys + 0x100, KeyBytes, Cover::Inside, "a key-sized fill not at a dccAddress, over another live image");
    ExpectCover(aKeys + 0x100, 0x10000, Cover::Several, "a fill not at a dccAddress over two live images");
    // The consumer applies the same match: a fill past the key extent notes no keys, a key-sized one
    // notes them on its image.
    Expect(StorageTexture::NoteKeysFill(aKeys, extent + 1, 0x00) == 0 && keyed.FilledKeys() == DccKeys::Uncompressed, "NoteKeysFill noted keys for a fill past the key extent over another live image");
    Expect(StorageTexture::NoteKeysFill(aKeys, KeyBytes, 0x00) == 1 && keyed.FilledKeys() == DccKeys::Clear0000, "NoteKeysFill did not note a key-sized fill over an image inside another live image");
    // (d) with nothing overlapping, the length rule alone decides, as before the bound.
    ExpectCover(dKeys, KeyBytes, Cover::Keys, "a key-sized fill at the keys of an image nothing else overlaps");
    ExpectCover(dKeys, 2 * extent, Cover::Keys, "a fill past the key extent at the keys of an image nothing else overlaps");
    Expect(StorageTexture::NoteKeysFill(dKeys, 2 * extent, 0x00) == 1 && alone->FilledKeys() == DccKeys::Clear0000, "NoteKeysFill refused a fill past the key extent with nothing overlapping");
}

}

int main() {
    try {
        // The block outlives the device: the device's images are created over it.
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device, block.Data());
        if (failures != 0) {
            std::fprintf(stderr, "%d fill cover checks failed\n", failures);
            return 1;
        }
        std::puts("fill cover keys tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

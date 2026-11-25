#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::uint32_t Size = 2048;
constexpr std::uint32_t Levels = 8;
constexpr std::uint32_t MinLodLevel = 2;
constexpr std::uint32_t FormatBc1Unorm = 169;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockAlignment = 65536;

std::array<std::uint32_t, 8> Descriptor(std::uint64_t address, std::uint32_t minLodLevel) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | ((minLodLevel * 256u) << 8u) | (FormatBc1Unorm << 20u) | (((Size - 1u) & 3u) << 30u),
        ((Size - 1u) >> 2u) | ((Size - 1u) << 14u),
        0xfacu | ((Levels - 1u) << 16u) | (Type2D << 28u),
        0u,
        (Levels - 1u) << 4u,
        0u,
        0u,
    };
}

std::uint64_t ResidentEnd(const SurfaceGeometry& geometry, std::uint32_t firstLevel) {
    std::uint64_t end = 0;
    for (std::size_t level = firstLevel; level < geometry.mips.size(); ++level) end = std::max(end, geometry.mips[level].tiledOffset + geometry.mips[level].tiledSize);
    return end;
}

void CheckResidency() {
    constexpr std::uint64_t Address = 0x10000000ull;
    const auto clamped = DecodeTextureResource(Descriptor(Address, MinLodLevel));
    const auto geometry = DescribeSurface(clamped);
    const auto residency = MinLodResidentLevels(clamped);
    Require(residency.has_value() && residency->levels == MinLodLevel, "min LOD residency: a 2D T# with MIN_LOD 2 and base level 0 does not leave its levels 0 and 1 out");
    Require(residency->bytes == ResidentEnd(geometry, MinLodLevel) && residency->bytes <= geometry.mips[MinLodLevel - 1u].tiledOffset && residency->bytes < geometry.guestBytes, "min LOD residency: the resident bytes are not the levels from 2 on, which end before level 1 starts");
    Require(!MinLodResidentLevels(DecodeTextureResource(Descriptor(Address, 0u))).has_value(), "min LOD residency: a T# without a MIN_LOD clamp left levels out");
    auto based = clamped;
    based.baseLevel = 1;
    Require(!MinLodResidentLevels(based).has_value(), "min LOD residency: a T# with a base level above 0 left levels out");
    auto compressed = clamped;
    compressed.dccAddress = Address + geometry.guestBytes;
    Require(!MinLodResidentLevels(compressed).has_value(), "min LOD residency: a T# with DCC metadata left levels out");
}

class GuestBlock {
public:
    explicit GuestBlock(std::size_t requested) : bytes((requested + BlockAlignment - 1u) / BlockAlignment * BlockAlignment) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(bytes, BlockAlignment));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
        void* mapped = mmap(nullptr, bytes + BlockAlignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + BlockAlignment - 1) & ~(static_cast<std::uintptr_t>(BlockAlignment) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + bytes != begin + bytes + BlockAlignment) munmap(reinterpret_cast<void*>(aligned + bytes), begin + BlockAlignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
        }
#endif
        Require(block != nullptr, "min LOD residency: cannot allocate the guest block");
        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(block), bytes);
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, bytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
#else
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, bytes);
        munmap(block, bytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }
    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

private:
    std::uint8_t* block = nullptr;
    std::size_t bytes = 0;
};

void CheckCache(const Context& context, GuestBlock& block) {
    const auto clampedWords = Descriptor(block.Address(), MinLodLevel);
    const auto wholeWords = Descriptor(block.Address(), 0u);
    const auto geometry = DescribeSurface(DecodeTextureResource(clampedWords));
    const auto excluded = static_cast<std::size_t>(geometry.mips[0].tiledOffset);
    const auto lookup = [&](const std::array<std::uint32_t, 8>& words) {
        AgcDriver::GuestMemory::BumpCollectEpoch();
        return CachedSampledTexture(context, words);
    };
    ClearCachedTextures(context.device);
    std::memset(block.Data(), 0x5a, static_cast<std::size_t>(geometry.guestBytes));
    const auto clamped = lookup(clampedWords);
    const auto whole = lookup(wholeWords);
    Require(clamped != nullptr && whole != nullptr && clamped != whole, "min LOD residency: the clamped and the unclamped T# were not sampled as two textures");
    Require(lookup(clampedWords) == clamped && lookup(wholeWords) == whole, "min LOD residency: an unchanged texture was not reused");
    block.Data()[excluded] ^= 0xffu;
    Require(lookup(clampedWords) == clamped, "min LOD residency: a CPU store into level 0, below the T#'s MIN_LOD, replaced the clamped texture, which never samples that level");
    const auto wholeAfter = lookup(wholeWords);
    Require(wholeAfter != nullptr && wholeAfter != whole, "min LOD residency: a CPU store into level 0 did not replace the texture that samples it");
    block.Data()[0] ^= 0xffu;
    const auto clampedAfter = lookup(clampedWords);
    Require(clampedAfter != nullptr && clampedAfter != clamped, "min LOD residency: a CPU store into a level the clamped texture samples did not replace it");
    Require(lookup(clampedWords) == clampedAfter, "min LOD residency: the replaced clamped texture was not reused");
    ClearCachedTextures(context.device);
}

}

int main() {
    try {
        CheckResidency();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
        const auto context = device->graphicsContext();
        if (!context.textureCompressionBC || (!context.imageViewMinLod && !context.shaderImageMinLod)) {
            std::puts("skipped, the device has no BC texture compression or minimum LOD clamp");
            return VulkanTestSkipped;
        }
        GuestBlock block(static_cast<std::size_t>(DescribeSurface(DecodeTextureResource(Descriptor(0x10000000ull, 0u))).guestBytes));
        CheckCache(context, block);
        std::puts("min LOD residency tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

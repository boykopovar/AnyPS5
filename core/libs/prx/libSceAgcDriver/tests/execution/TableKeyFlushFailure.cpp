#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t TiledSide = 128;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Initial = 0x5a5a5a5au;
constexpr std::uint32_t Stored = 0x12345678u;
constexpr std::uint64_t KeyOffset = 0x1000u;
constexpr std::uint64_t KeyBytes = 0x200u;

alignas(256) constexpr std::array<std::uint32_t, 5> StoreRowsCode{
    0x7e020210, 0x7e04020c, 0xf0201108, 0x00010200, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "table key flush failure: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint8_t* texels, std::uint32_t width, std::uint32_t height, std::uint32_t tile) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u),
        0xfacu | (tile << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

void StoreAll(AgcDriver::VulkanDevice& device, const std::uint8_t* texels) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, TiledSide, TiledSide, TileR64KBX);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = Stored;
    const std::span<const std::uint32_t> code(StoreRowsCode);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{TiledSide, 1, 1}, 0u, {false, true, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, TiledSide, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
}

}

int main() {
    try {
        if (!AgcDriver::Graphics::TableKeyFlushEnabled()) {
            std::cout << "table key flush off (APS5_NO_TABLE_KEY_FLUSH): skipped\n";
            return VulkanTestSkipped;
        }
        GuestBlock block;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        auto* texels = block.Data();
        const auto surface = AddressOf(texels);
        std::fill_n(reinterpret_cast<std::uint32_t*>(texels), BlockBytes / 4u, Initial);
        AgcDriver::GuestMemory::CollectWrites(surface, BlockBytes);
        AgcDriver::GuestMemory::BumpCollectEpoch();
        {
            std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
            StoreAll(*device, texels);
            Require(StorageTexture::FindPending(surface, BlockBytes) != nullptr, "table key flush failure: the tiled store left no pending image");
            const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> keys{{{surface + KeyOffset, surface + KeyOffset + KeyBytes}}};
            static_cast<void>(StorageTexture::TakeFlushFailureForTest());
            StorageTexture::SetFlushFailureForTest(true);
            bool submitted = false;
            try {
                submitted = AgcDriver::Graphics::FlushTableKeys(keys);
            } catch (const std::exception& error) {
                StorageTexture::SetFlushFailureForTest(false);
                Require(false, std::string("table key flush failure: a failed flush escaped the key prepass: ") + error.what());
            }
            StorageTexture::SetFlushFailureForTest(false);
            Require(StorageTexture::TakeFlushFailureForTest() != 0, "table key flush failure: the key prepass never reached the injected flush failure");
            Require(submitted, "table key flush failure: the key prepass did not submit after a failed flush");
            Require(StorageTexture::FindPending(surface, BlockBytes) != nullptr, "table key flush failure: the failed flush dropped the pending image");
            Require(AgcDriver::Graphics::FlushTableKeys(keys), "table key flush failure: the key prepass after the failure did not store the pending image");
            device->WaitIdle();
            Require(StorageTexture::FindPending(surface, BlockBytes) == nullptr, "table key flush failure: the pending image survived the second key prepass");
        }
        const auto* words = reinterpret_cast<const std::uint32_t*>(texels);
        for (std::size_t index = 0; index < BlockBytes / 4u; ++index) {
            Require(words[index] == Stored, "table key flush failure: guest memory does not hold the tiled store after the second key prepass at dword " + std::to_string(index));
        }
        std::cout << "table key flush failure: passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

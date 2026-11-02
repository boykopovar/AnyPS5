#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
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
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Format32Float = 22;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::uint32_t Side = 256;
constexpr std::size_t BlockBytes = 2u << 20u;

alignas(256) constexpr std::array<std::uint32_t, 5> Store2DCode{
    0x7e020280, 0x7e04020c, 0xf0201108, 0x00010200, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 6> Store2DArrayCode{
    0x7e020280, 0x7e040280, 0x7e06020c, 0xf0201128, 0x00010300, 0xbf810000,
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
        Require(block != nullptr, "storage one-slice array: cannot allocate a guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(AddressOf(block), BlockBytes);
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

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint8_t* texels, std::uint32_t type) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Float << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (type << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

ShaderRecompiler::RecompileResult Compile(const AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Store(AgcDriver::VulkanDevice& device, const std::uint8_t* texels, bool arrayDescriptor, bool arrayCode, std::uint32_t value) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = TextureDescriptor(texels, arrayDescriptor ? Type2DArray : Type2D);
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = value;
    const std::span<const std::uint32_t> code = arrayCode ? std::span<const std::uint32_t>(Store2DArrayCode) : std::span<const std::uint32_t>(Store2DCode);
    const auto result = Compile(device, code, userData);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

std::size_t Occurrences(const std::uint8_t* bytes, std::size_t size, std::uint32_t value) {
    std::size_t count = 0;
    for (std::size_t offset = 0; offset + 4u <= size; offset += 4u) {
        std::uint32_t word = 0;
        std::memcpy(&word, bytes + offset, 4u);
        if (word == value) ++count;
    }
    return count;
}

void Run(AgcDriver::VulkanDevice& device, const std::string& name, bool arrayFirst, bool flatCode) {
    GuestBlock block;
    auto* texels = block.Data();
    const auto surface = AddressOf(texels);
    const auto bytes = static_cast<std::size_t>(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(texels, Type2D))).guestBytes);
    Require(bytes <= BlockBytes, name + ": the surface does not fit the guest block");
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    const std::array<std::uint32_t, 3> values{0x3f800000u, 0x40000000u, 0x40400000u};
    std::shared_ptr<StorageTexture> image;
    for (std::size_t step = 0; step < values.size(); ++step) {
        const bool array = (step % 2u == 0u) == arrayFirst;
        Store(device, texels, array, array && !flatCode, values[step]);
        const auto pending = StorageTexture::FindPending(surface, bytes);
        Require(pending != nullptr, name + ": store " + std::to_string(step) + " left no pending image over the surface");
        if (image == nullptr) image = pending;
        Require(image->Descriptor().dimension == AgcDriver::Graphics::TextureDimension::k2D, name + ": the surface's storage image is not a 2D image");
        const auto store = (array ? std::string(flatCode ? "one-slice 2D array through a 2D image instruction" : "one-slice 2D array") : std::string("2D")) + " store " + std::to_string(step);
        Require(pending == image, name + ": the " + store + " wrote another image than the surface's first store, so the surface has two storage images");
        StorageTexture::FlushPending(surface, bytes, nullptr, "test");
        device.WaitIdle();
        Require(Occurrences(texels, bytes, values[step]) == Threads, name + ": guest memory does not hold the " + store + " in every written texel");
        Require(step == 0u || Occurrences(texels, bytes, values[step - 1u]) == 0u, name + ": guest memory still holds the store before the " + store);
    }
    AgcDriver::Graphics::ClearCachedTextures(device.Device());
}

}

int main(int argc, char** argv) {
#ifdef _WIN32
    if (std::getenv("APS5_TEMPLATE_AUDIT_MS") == nullptr) _putenv_s("APS5_TEMPLATE_AUDIT_MS", "0");
#else
    setenv("APS5_TEMPLATE_AUDIT_MS", "0", 0);
#endif
    struct Case {
        const char* key;
        const char* name;
        bool arrayFirst;
        bool flatCode;
    };
    constexpr std::array<Case, 3> cases{{
        {"flat-first", "2D store first", false, false},
        {"array-first", "one-slice 2D array store first", true, false},
        {"array-flat-code", "one-slice 2D array T# through a 2D image instruction first", true, true},
    }};
    try {
        const std::string only = argc == 2 ? argv[1] : "";
        Require(only.empty() || std::any_of(cases.begin(), cases.end(), [&](const Case& entry) { return only == entry.key; }), "unknown case " + only);
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::size_t failed = 0;
        for (const auto& entry : cases) {
            if (!only.empty() && only != entry.key) continue;
            try {
                Run(*device, entry.name, entry.arrayFirst, entry.flatCode);
            } catch (const std::exception& error) {
                AgcDriver::Graphics::ClearCachedTextures(device->Device());
                std::cerr << error.what() << '\n';
                ++failed;
            }
        }
        if (failed != 0) return 1;
        std::puts("storage one-slice array tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

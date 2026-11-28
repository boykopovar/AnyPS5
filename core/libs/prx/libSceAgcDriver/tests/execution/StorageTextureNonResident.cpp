#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
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
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t Block = 0x10000;
constexpr std::size_t SurfaceBytes = 4 * Block;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Stored = 0x12345678u;
constexpr std::uint32_t Unread = 0xa5a5a5a5u;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> LoadCode{
    0x7e020280, 0xf0001108, 0x00020300, 0x34080082, 0xbf8c3f70, 0xe0701000, 0x80000304, 0xbf810000,
};

alignas(256) std::array<std::uint32_t, Threads> Output{};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((Side - 1u) & 3u) << 30u),
        ((Side - 1u) >> 2u) | ((Side - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Dispatch(const std::uint32_t* texels, std::span<const std::uint32_t> code) {
    std::array<std::uint32_t, 16> userData{};
    const auto buffer = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * sizeof(std::uint32_t)));
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    const auto texture = TextureDescriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::vector<std::uint32_t> words;
    const auto registers = [&](const std::uint32_t first, const std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{Threads, 1, 1};
    registers(0x207, shape);
    const auto address = AddressOf(code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> layout{static_cast<std::uint32_t>(userData.size()) << 1u};
    registers(0x213, layout);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
}

void Protect(std::uint32_t* texels, bool committed) {
    GuestAllocations::Mutation().Protect(texels, SurfaceBytes, committed, committed, committed, committed, [&] {
#ifdef _WIN32
        DWORD prior = 0;
        Require(VirtualProtect(texels, SurfaceBytes, committed ? PAGE_READWRITE : PAGE_NOACCESS, &prior) != 0, "non-resident storage texture: cannot change the surface's protection");
#else
        Require(mprotect(texels, SurfaceBytes, committed ? PROT_READ | PROT_WRITE : PROT_NONE) == 0, "non-resident storage texture: cannot change the surface's protection");
#endif
    });
}

std::uint32_t* MapSurface() {
#ifdef _WIN32
    auto* texels = static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, Block));
    if (texels != nullptr) GuestArena::GuestArenaCommit_nid_postfix(texels, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
    return texels;
#else
    void* mapped = mmap(nullptr, SurfaceBytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) return nullptr;
    const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
    const auto aligned = (begin + Block - 1) & ~std::uintptr_t{Block - 1};
    if (aligned != begin) munmap(mapped, aligned - begin);
    if (aligned != begin + Block) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + Block - aligned);
    auto* texels = reinterpret_cast<std::uint32_t*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(texels, SurfaceBytes);
    return texels;
#endif
}

void ReleaseSurface(std::uint32_t* texels) {
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(texels, SurfaceBytes);
    GuestArena::GuestArenaRelease_nid_postfix(texels, SurfaceBytes);
#else
    munmap(texels, SurfaceBytes);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(texels, SurfaceBytes);
#endif
}

void RequireOutput(std::uint32_t expected, const std::string& what) {
    for (std::uint32_t x = 0; x < Threads; ++x) Require(Output[x] == expected, "non-resident storage texture: " + what + ": texel " + std::to_string(x) + " reads " + std::to_string(Output[x]) + ", expected " + std::to_string(expected));
}

void Load(const std::uint32_t* texels, const std::string& what) {
    Output.fill(Unread);
    Dispatch(texels, LoadCode);
    RequireOutput(0u, what);
}

void Run() {
    auto* texels = MapSurface();
    Require(texels != nullptr, "non-resident storage texture: cannot allocate the guest surface");
    std::fill(texels, texels + SurfaceBytes / 4u, Untouched);
    GuestAllocations::Mutation().Add(texels, SurfaceBytes, true, true, true);
    struct Surface {
        std::uint32_t* texels;
        ~Surface() {
            try {
                AgcDriverWaitIdle_nid_postfix();
            } catch (...) {
            }
            GuestAllocations::Mutation().Remove(texels);
            ReleaseSurface(texels);
        }
    } surface{texels};
    const auto address = AddressOf(texels);
    Require(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(texels))).guestBytes == SurfaceBytes, "non-resident storage texture: unexpected surface size");
    Protect(texels, false);
    Require(AgcDriver::GuestMemory::CommittedRanges(address, SurfaceBytes).empty(), "non-resident storage texture: the decommitted surface still has a committed page");
    Load(texels, "a load from a decommitted surface");
    Dispatch(texels, StoreCode);
    StorageTexture::FlushAllPending("non-resident storage texture");
    AgcDriverWaitIdle_nid_postfix();
    Require(AgcDriver::GuestMemory::CommittedRanges(address, SurfaceBytes).empty(), "non-resident storage texture: a store committed a page of the decommitted surface");
    Load(texels, "a load after a store to a decommitted surface");
    StorageTexture::FlushAllPending("non-resident storage texture");
    AgcDriverWaitIdle_nid_postfix();
    Protect(texels, true);
    Require(std::all_of(texels, texels + SurfaceBytes / 4u, [](std::uint32_t value) { return value == Untouched; }), "non-resident storage texture: a store to a decommitted surface reached guest memory");
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        Run();
        std::puts("non-resident storage texture tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
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
#include <cstdio>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Width = 128;
constexpr std::uint32_t Height = 120;
constexpr std::uint32_t FormatRgba16Float = 71;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Standard64KB = 0x09;
constexpr std::size_t SurfaceBytes = 0x20000;
constexpr std::size_t AllocationBytes = std::size_t{Width} * Height * 8u;
constexpr std::uint8_t Sentinel = 0xa5;

alignas(256) constexpr std::array<std::uint32_t, 11> WriteCode{0xd7460004u, 0x0401060eu, 0xd7460005u, 0x0405060fu, 0x7e100d04u, 0x7e120d05u, 0x7e140c0du, 0x7e1602f2u, 0xf0200f08u, 0x00000804u, 0xbf810000u};

std::array<std::uint32_t, 8> TextureDescriptor(const void* texels) {
    const auto address = reinterpret_cast<std::uintptr_t>(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (FormatRgba16Float << 20u) | (((Width - 1u) & 3u) << 30u),
        ((Width - 1u) >> 2u) | ((Height - 1u) << 14u),
        0xfacu | (Standard64KB << 20u) | (Type2D << 28u),
        0u, 0u, 0u, 0u,
    };
}

void Write(std::uint8_t* texels) {
    std::vector<std::uint32_t> data;
    const auto texture = TextureDescriptor(texels);
    data.insert(data.end(), texture.begin(), texture.end());
    data.resize(14, 0u);
    data[13] = 7u;
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{8, 8, 1};
    registers(0x207, shape);
    const auto code = reinterpret_cast<std::uintptr_t>(WriteCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(static_cast<std::uint32_t>(data.size()) << 1u) | (7u << 7u) | (1u << 11u)};
    registers(0x213, resource);
    registers(0x240, data);
    words.insert(words.end(), {0xc0031500u, Width / 8u, Height / 8u, 1u, 0x41u});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::Graphics::StorageTexture::FlushAllPending("test");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(texels), SurfaceBytes);
}

class GuestSurface {
public:
    explicit GuestSurface(bool split) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, Alignment));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes + Alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + Alignment - 1) & ~(static_cast<std::uintptr_t>(Alignment) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + SurfaceBytes != begin + SurfaceBytes + Alignment) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + Alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, SurfaceBytes);
        }
#endif
        Require(block != nullptr, "storage write-back overhang: cannot allocate the guest surface");
        std::memset(block, 0, AllocationBytes);
        std::memset(block + AllocationBytes, Sentinel, SurfaceBytes - AllocationBytes);
        GuestAllocations::Mutation mutation;
        if (split) {
            mutation.Add(block, AllocationBytes, true, true, true);
            mutation.Add(block + AllocationBytes, SurfaceBytes - AllocationBytes, true, true, true);
        } else {
            mutation.Add(block, SurfaceBytes, true, true, true);
        }
        this->split = split;
    }

    ~GuestSurface() {
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(block);
            if (split) mutation.Remove(block + AllocationBytes);
        }
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, SurfaceBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, SurfaceBytes);
#else
        munmap(block, SurfaceBytes);
        if (AgcDriver::GuestMemory::WriteWatched()) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, SurfaceBytes);
#endif
    }

    GuestSurface(const GuestSurface&) = delete;
    GuestSurface& operator=(const GuestSurface&) = delete;

    std::uint8_t* Data() { return block; }

private:
    static constexpr std::size_t Alignment = 65536;
    std::uint8_t* block = nullptr;
    bool split = false;
};

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        GuestSurface whole(false);
        GuestSurface split(true);
        Write(whole.Data());
        Write(split.Data());
        const auto* expected = whole.Data();
        const auto* stored = split.Data();
        const auto texel = [](const std::uint8_t* at) { return !std::all_of(at, at + 8, [](std::uint8_t byte) { return byte == 0 || byte == Sentinel; }); };
        std::size_t written = 0;
        std::size_t past = 0;
        for (std::size_t at = 0; at < SurfaceBytes; at += 8) {
            written += texel(expected + at);
            past += at >= AllocationBytes && texel(expected + at);
        }
        Require(written != 0, "storage write-back overhang: the fully registered surface received no texels");
        Require(past != 0, "storage write-back overhang: no texel of the fully registered surface lies past pitch x height");
        const auto differs = std::mismatch(expected, expected + SurfaceBytes, stored);
        Require(differs.first == expected + SurfaceBytes, "storage write-back overhang: byte 0x" + [&] { char text[32]; std::snprintf(text, sizeof(text), "%zx", static_cast<std::size_t>(differs.first - expected)); return std::string(text); }() + " differs from the fully registered surface (" + std::to_string(written) + " texels, " + std::to_string(past) + " past pitch x height)");
        Require(std::any_of(stored + AllocationBytes, stored + SurfaceBytes, [](std::uint8_t byte) { return byte == Sentinel; }), "storage write-back overhang: the padding past the image's rows was overwritten");
        AgcDriverShutdown_nid_postfix();
        std::puts("storage write-back overhang tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}

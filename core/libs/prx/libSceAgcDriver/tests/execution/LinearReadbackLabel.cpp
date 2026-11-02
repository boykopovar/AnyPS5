#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 8;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Format8888Uint = 60;
constexpr std::uint32_t TexelBytes = 4;
constexpr std::uint32_t IdentitySwizzle = 0xfacu;
constexpr std::size_t BlockBytes = 0x20000;
constexpr std::size_t LabelOffset = 0x8000;
constexpr std::uint32_t LabelValue = 0x5eedf00du;
constexpr std::uint32_t TiledThreads = 8;
constexpr std::uint32_t TiledSide = 16;
constexpr std::uint32_t Format32Uint = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::size_t TiledOffset = 0x10000;
constexpr std::size_t TiledLabelOffset = 0x8100;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Written = 0x12345678u;

alignas(256) constexpr std::array<std::uint32_t, 24> StoreCode{
    0x4a500081, 0xd5690014, 0x0201ff28, 0x9e3779b1, 0x4a2a28ff, 0x7f4a7c15, 0x4a2c28ff, 0xfe94f82a,
    0x4a2e28ff, 0x7ddf743f, 0x7e3c0300, 0x7e3e0280, 0xf0281f08, 0x0001141e, 0x7e3e0281, 0xf0281508,
    0x0001141e, 0x7e3e0282, 0xf0281a08, 0x0001141e, 0x7e3e0283, 0xf0281808, 0x0001141e, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> TiledStoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Written, 0xf0200108, 0x00020102, 0xbf810000,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "linear readback label: cannot allocate the guest block");
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

std::uint32_t Data(std::uint32_t tid, std::uint32_t index) {
    return (tid + 1u) * 0x9e3779b1u + index * 0x7f4a7c15u;
}

AgcDriver::Graphics::TileMipLayout Layout() {
    return AgcDriver::Graphics::ComputeMipLayout(AgcDriver::Graphics::TextureTileMode::kLinear, Format8888Uint, Width, Height, 1).front();
}

std::vector<std::uint8_t> Expected(const std::vector<std::uint8_t>& initial) {
    const auto mip = Layout();
    auto bytes = initial;
    constexpr std::array<std::uint32_t, 4> Dmasks{0xfu, 0x5u, 0xau, 0x8u};
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        for (std::uint32_t row = 0; row < Dmasks.size(); ++row) {
            std::array<std::uint32_t, 4> words{};
            std::uint32_t next = 0;
            for (std::uint32_t word = 0; word < 4u; ++word) {
                if (((Dmasks[row] >> word) & 1u) != 0u) words[word] = Data(tid, next++);
            }
            std::memcpy(bytes.data() + mip.tiledOffset + static_cast<std::uint64_t>(row) * mip.pitchBytes + static_cast<std::uint64_t>(tid) * TexelBytes, words.data(), TexelBytes);
        }
    }
    return bytes;
}

std::array<std::uint32_t, 16> UserData(const void* texels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    std::array<std::uint32_t, 16> data{};
    data[4] = static_cast<std::uint32_t>(address >> 8u);
    data[5] = static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format8888Uint << 20u) | (((Width - 1u) & 3u) << 30u);
    data[6] = ((Width - 1u) >> 2u) | ((Height - 1u) << 14u);
    data[7] = IdentitySwizzle | (Type2D << 28u);
    return data;
}

std::array<std::uint32_t, 8> TiledDescriptor(const void* texels) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32Uint << 20u) | (((TiledSide - 1u) & 3u) << 30u),
        ((TiledSide - 1u) >> 2u) | ((TiledSide - 1u) << 14u),
        IdentitySwizzle | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 16> TiledUserData(const void* texels) {
    std::array<std::uint32_t, 16> data{};
    const auto texture = TiledDescriptor(texels);
    std::copy(texture.begin(), texture.end(), data.begin() + 8);
    return data;
}

std::vector<std::uint32_t> Commands(std::span<const std::uint32_t> code, std::uint32_t threads, std::span<const std::uint32_t> userData, const void* label) {
    std::vector<std::uint32_t> words;
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{threads, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{static_cast<std::uint32_t>(userData.size()) << 1u};
    registers(0x213, resource);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u | 0x8000u});
    const auto at = reinterpret_cast<std::uintptr_t>(label);
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(at), static_cast<std::uint32_t>(at >> 32u), LabelValue, 0, 0});
    return words;
}

void SubmitAndWait(std::vector<std::uint32_t> words, std::uint32_t* label, const char* what) {
    *label = 0;
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    const auto start = std::chrono::steady_clock::now();
    while (std::atomic_ref<std::uint32_t>(*label).load(std::memory_order_acquire) != LabelValue) {
        Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(20), std::string(what) + ": the label after the dispatch never landed");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void ReadAfterLabel(std::uint8_t* block) {
    auto* texels = block;
    auto* label = reinterpret_cast<std::uint32_t*>(block + LabelOffset);
    const auto mip = Layout();
    const auto bytes = static_cast<std::size_t>(mip.tiledOffset + mip.pitchBytes * Height);
    std::vector<std::uint8_t> initial(bytes);
    for (std::size_t index = 0; index < initial.size(); ++index) initial[index] = static_cast<std::uint8_t>(0x80u + 37u * index + 11u * (index >> 4u));
    std::memcpy(texels, initial.data(), initial.size());
    const auto userData = UserData(texels);
    SubmitAndWait(Commands(StoreCode, Threads, userData, label), label, "linear readback label");
    const auto expected = Expected(initial);
    for (std::uint32_t row = 0; row < Height; ++row) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto offset = mip.tiledOffset + static_cast<std::uint64_t>(row) * mip.pitchBytes + static_cast<std::uint64_t>(x) * TexelBytes;
            std::uint32_t actual = 0;
            std::uint32_t wanted = 0;
            std::memcpy(&actual, texels + offset, TexelBytes);
            std::memcpy(&wanted, expected.data() + offset, TexelBytes);
            Require(actual == wanted, "linear readback label: texel (" + std::to_string(x) + ", " + std::to_string(row) + ") read after the label is 0x" + [&] { char text[16]; std::snprintf(text, sizeof(text), "%08x", actual); return std::string(text); }() + ", the dispatch before the label stored a different value");
        }
    }
}

void ReadTiledAfterLabel(std::uint8_t* block) {
    auto* texels = reinterpret_cast<std::uint32_t*>(block + TiledOffset);
    auto* label = reinterpret_cast<std::uint32_t*>(block + TiledLabelOffset);
    const auto bytes = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TiledDescriptor(texels))).guestBytes;
    Require(bytes <= AgcDriver::Graphics::StorageTexture::SmallStoreBytes && bytes <= BlockBytes - TiledOffset, "tiled readback label: the test image is larger than a small image");
    std::fill(texels, texels + bytes / 4u, Untouched);
    const auto userData = TiledUserData(texels);
    SubmitAndWait(Commands(TiledStoreCode, TiledThreads, userData, label), label, "tiled readback label");
    const volatile std::uint32_t* cpu = texels;
    std::uint32_t written = 0;
    for (std::size_t index = 0; index < bytes / 4u; ++index) written += cpu[index] == Written ? 1u : 0u;
    Require(written == TiledThreads, "tiled readback label: " + std::to_string(written) + " texels of a small tiled image read after the label hold the value the dispatch before it stored, expected " + std::to_string(TiledThreads));
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        GuestBlock block;
        ReadAfterLabel(block.Data());
        ReadTiledAfterLabel(block.Data());
        AgcDriverWaitIdle_nid_postfix();
        AgcDriverShutdown_nid_postfix();
        std::puts("linear readback label tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}

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
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
constexpr std::uint32_t Side = 128;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t Block = 0x10000;
constexpr std::size_t SurfaceBytes = Block;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Stored = 0x12345678u;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 7> StoreRowsCode{
    0x7e040300, 0x7e060210, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> LoadRowCode{
    0x7e020280, 0xf0001108, 0x00010300, 0x34080082, 0xbf8c3f70, 0xe0701000, 0x80000304, 0xbf810000,
};

constexpr std::uint32_t RowThreads = 32;
constexpr std::uint32_t LinearWidth = 256;
constexpr std::uint32_t LinearHeight = 64;
constexpr std::uint32_t TileLinear = 0;
constexpr std::uint32_t TgidYEnable = 1u << 8u;

enum class Mode { Enabled, Disabled, Unimported };

alignas(256) std::array<std::uint32_t, RowThreads> Output{};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels, std::uint32_t width = Side, std::uint32_t height = Side, std::uint32_t tile = TileR64KBX) {
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

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = AddressOf(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void Dispatch(std::vector<std::uint32_t>& words, std::span<const std::uint32_t> code, std::uint32_t threads, std::uint32_t resource, std::span<const std::uint32_t> userData, std::uint32_t groups, std::uint32_t rows = 1) {
    const auto registers = [&](const std::uint32_t first, const std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{threads, 1, 1};
    registers(0x207, shape);
    const auto address = AddressOf(code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> layout{resource};
    registers(0x213, layout);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, groups, rows, 1, 0x41u});
}

void Store(std::vector<std::uint32_t>& words, const std::uint32_t* texels) {
    std::array<std::uint32_t, 16> userData{};
    const auto texture = TextureDescriptor(texels);
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    Dispatch(words, StoreCode, 8, static_cast<std::uint32_t>(userData.size()) << 1u, userData, 1);
}

class Surface {
public:
    Surface() {
#ifdef _WIN32
        region = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(SurfaceBytes, Block));
        if (region != nullptr) GuestArena::GuestArenaCommit_nid_postfix(region, SurfaceBytes, PAGE_READWRITE, SurfaceBytes);
#else
        void* mapped = mmap(nullptr, SurfaceBytes + Block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + Block - 1) & ~std::uintptr_t{Block - 1};
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned != begin + Block) munmap(reinterpret_cast<void*>(aligned + SurfaceBytes), begin + Block - aligned);
            region = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(region, SurfaceBytes);
        }
#endif
        Require(region != nullptr, "label ordered flush: cannot allocate the guest surface");
        texels = reinterpret_cast<std::uint32_t*>(region);
        std::fill(texels, texels + SurfaceBytes / 4u, Untouched);
        GuestAllocations::Mutation().Add(region, SurfaceBytes, true, true, true);
    }

    ~Surface() {
        GuestAllocations::Mutation().Remove(region);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(region, SurfaceBytes);
        GuestArena::GuestArenaRelease_nid_postfix(region, SurfaceBytes);
#else
        munmap(region, SurfaceBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(region, SurfaceBytes);
#endif
    }

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    std::uint32_t* Texels() const { return texels; }

private:
    std::uint8_t* region = nullptr;
    std::uint32_t* texels = nullptr;
};

void LabelAfterStore(const Surface& surface, std::uint32_t* label) {
    std::vector<std::uint32_t> words;
    Store(words, surface.Texels());
    const auto labelAddress = AddressOf(label);
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(labelAddress), static_cast<std::uint32_t>(labelAddress >> 32u), 1u, 0, 0});
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::atomic_ref<std::uint32_t>(*label).load() == 0) {
        Require(std::chrono::steady_clock::now() < deadline, "label ordered flush: the label never landed");
        std::this_thread::yield();
    }
    std::vector<std::uint32_t> landed(SurfaceBytes / sizeof(std::uint32_t));
    std::memcpy(landed.data(), surface.Texels(), SurfaceBytes);
    const auto stored = std::count(landed.begin(), landed.end(), Stored);
    Require(stored >= 8, "label ordered flush: the label was visible before the image store it follows");
    AgcDriverWaitIdle_nid_postfix();
}

void SubmitWords(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    AgcDriverWaitIdle_nid_postfix();
}

void LoadRow(const Surface& surface) {
    Output.fill(0xdeadbeefu);
    std::array<std::uint32_t, 16> userData{};
    const auto buffer = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * sizeof(std::uint32_t)));
    const auto texture = TextureDescriptor(surface.Texels(), LinearWidth, LinearHeight, TileLinear);
    std::copy(buffer.begin(), buffer.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    std::vector<std::uint32_t> words;
    Dispatch(words, LoadRowCode, RowThreads, static_cast<std::uint32_t>(userData.size()) << 1u, userData, 1);
    SubmitWords(words);
}

void RefreshAfterStore(const Surface& surface) {
    LoadRow(surface);
    Require(std::all_of(Output.begin(), Output.end(), [](std::uint32_t value) { return value == Untouched; }), "label ordered flush: the linear view did not read the untouched guest bytes");
    std::array<std::uint32_t, 16> userData{};
    const auto texture = TextureDescriptor(surface.Texels());
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    std::vector<std::uint32_t> words;
    Dispatch(words, StoreRowsCode, Side, (static_cast<std::uint32_t>(userData.size()) << 1u) | TgidYEnable, userData, 1, Side);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    LoadRow(surface);
    for (std::uint32_t index = 0; index < RowThreads; ++index) {
        Require(Output[index] == Stored, "label ordered flush: the refreshed linear view reads " + std::to_string(Output[index]) + " at texel " + std::to_string(index) + " instead of the tiled store");
        Require(Output[index] == surface.Texels()[index], "label ordered flush: the refreshed linear view disagrees with guest memory at texel " + std::to_string(index));
    }
}

}

int main(int argc, char** argv) {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        {
            Surface probe;
            const auto described = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(probe.Texels()))).guestBytes;
            Require(described == SurfaceBytes && described <= StorageTexture::SmallStoreBytes, "label ordered flush: the surface is not a small storage image");
        }
        alignas(256) static std::uint32_t words[64]{};
        GuestAllocations::Mutation().Add(words, sizeof(words), true, true, true);
        struct Unregister {
            ~Unregister() { GuestAllocations::Mutation().Remove(words); }
        } unregister;
        const std::string option = argc > 1 ? argv[1] : "";
        const Mode mode = option == "--disabled" ? Mode::Disabled : option == "--unimported" ? Mode::Unimported : Mode::Enabled;
        static_cast<void>(StorageTexture::TakeOrderedSkipCounts());
        constexpr int Runs = 8;
        for (int run = 0; run < Runs; ++run) {
            Surface surface;
            auto* label = &words[0];
            *label = 0;
            LabelAfterStore(surface, label);
        }
        const auto labelCounts = StorageTexture::TakeOrderedSkipCounts();
        if (mode == Mode::Enabled) Require(labelCounts.flushes != 0, "label ordered flush: no label store took the GPU-ordered flush over the host import");
        else Require(labelCounts.flushes == 0, "label ordered flush: a label store skipped its writer wait with the ordered flush off or no host import");
        GuestAllocations::Mutation().Add(Output.data(), sizeof(Output), true, true, true);
        struct UnregisterOutput {
            ~UnregisterOutput() { GuestAllocations::Mutation().Remove(Output.data()); }
        } unregisterOutput;
        const int refreshRuns = mode == Mode::Enabled ? 64 : 2;
        StorageTexture::OrderedSkipCounts refreshCounts{};
        for (int run = 0; run < refreshRuns; ++run) {
            Surface surface;
            RefreshAfterStore(surface);
            const auto counts = StorageTexture::TakeOrderedSkipCounts();
            refreshCounts.refreshes += counts.refreshes;
            if (refreshCounts.refreshes != 0) break;
        }
        if (mode == Mode::Enabled) Require(refreshCounts.refreshes != 0, "label ordered flush: no storage refresh took the GPU-ordered flush over the pending image");
        else Require(refreshCounts.refreshes == 0, "label ordered flush: a storage refresh skipped its writer wait with the ordered refresh off or no host import");
        AgcDriverShutdown_nid_postfix();
        std::puts("label ordered flush tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

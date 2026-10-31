#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr std::uint32_t Threads = 8;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Stored = 0x12345678u;
constexpr std::uint32_t FarRow = 1000;
constexpr std::uint64_t Piece = 4u << 20u;

alignas(256) constexpr std::array<std::uint32_t, 8> StoreRowZeroCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 8> StoreFarRowCode{
    0x7e040300, 0x7e0602ff, FarRow, 0x7e0202ff, Stored, 0xf0200108, 0x00020102, 0xbf810000,
};

std::uint64_t AddressOf(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 8> TextureDescriptor(const std::uint32_t* texels, std::uint32_t width, std::uint32_t height) {
    const auto address = AddressOf(texels);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (Format32UInt << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u),
        0xfacu | (TileR64KBX << 20u) | (Type2D << 28u),
        0u,
        0u,
        0u,
        0u,
    };
}

struct Surface {
    std::uint32_t width;
    std::uint32_t height;
    std::vector<std::uint32_t> storage;
    std::uint32_t* texels = nullptr;
    std::size_t bytes = 0;

    Surface(std::uint32_t width, std::uint32_t height) : width(width), height(height) {
        const std::size_t reserve = static_cast<std::size_t>(width) * height * 4u + 0x20000u;
        storage.resize(reserve / 4u);
        texels = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu});
        bytes = static_cast<std::size_t>(AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(Descriptor())).guestBytes);
        Require(AddressOf(texels) + bytes <= AddressOf(storage.data() + storage.size()), "idle yield fixture: a surface does not fit its storage");
        std::fill(texels, texels + bytes / 4u, Untouched);
    }

    std::array<std::uint32_t, 8> Descriptor() const { return TextureDescriptor(texels, width, height); }
    std::uint64_t Address() const { return AddressOf(texels); }
};

void AppendStore(std::vector<std::uint32_t>& words, const Surface& surface, const std::array<std::uint32_t, 8>& code) {
    std::vector<std::uint32_t> userData(16, 0u);
    const auto texture = surface.Descriptor();
    std::copy(texture.begin(), texture.end(), userData.begin() + 8);
    const auto registers = [&](const std::uint32_t first, const std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{Threads, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{static_cast<std::uint32_t>(userData.size()) << 1u};
    registers(0x213, resource);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u});
}

bool Pending(std::uint64_t begin, std::uint64_t end) {
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> range{{{begin, end}}};
    return AgcDriver::Graphics::StorageTexture::AnyPendingOverlaps(range);
}

bool Contains(std::uint64_t begin, std::uint64_t end) {
    std::vector<std::byte> copy(static_cast<std::size_t>(end - begin));
    AgcDriver::GuestMemory::Read(begin, copy);
    const volatile std::uint32_t* words = reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(begin));
    for (std::size_t index = 0; index < copy.size() / 4u; ++index) {
        if (words[index] == Stored) return true;
    }
    return false;
}

void SubmitAndWaitForPending(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!AgcDriver::Graphics::StorageTexture::AnyPending() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "idle yield fixture did not observe pending storage results");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

void StopsBetweenImages() {
    Surface first(256, 256);
    Surface second(256, 256);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(first.texels, first.bytes, true, true, true);
        mutation.Add(second.texels, second.bytes, true, true, true);
    }
    std::vector<std::uint32_t> words;
    AppendStore(words, first, StoreRowZeroCode);
    AppendStore(words, second, StoreRowZeroCode);
    SubmitAndWaitForPending(words);
    const auto pending = [](const Surface& surface) { return Pending(surface.Address(), surface.Address() + surface.bytes); };
    {
        std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex());
        Require(pending(first) && pending(second), "the queue-idle store ran before the fixture could take the GPU mutex");
        std::size_t checks = 0;
        const bool finished = AgcDriver::Graphics::StorageTexture::FlushAllPendingUntil("idle", [&] { return checks++ != 0; });
        Require(!finished, "a store asked to stop after one image reported that every image was stored");
        Require(pending(first) != pending(second), "a store asked to stop after one image did not store exactly one of the two pending images");
        Require(AgcDriver::Graphics::StorageTexture::FlushAllPendingUntil("idle", [] { return false; }), "a store that was never asked to stop reported unstored images");
        Require(!pending(first) && !pending(second), "the store after the stopped one left an image pending");
    }
    Require(Contains(first.Address(), first.Address() + first.bytes) && Contains(second.Address(), second.Address() + second.bytes), "the two stores did not bring both results to guest memory");
    GuestAllocations::Mutation mutation;
    mutation.Remove(first.texels);
    mutation.Remove(second.texels);
}

void StopsWithinAnImage() {
    Surface surface(2048, 1024);
    Require(surface.bytes > Piece, "idle yield fixture: the large surface is not larger than one stored piece");
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(surface.texels, surface.bytes, true, true, true);
    }
    std::vector<std::uint32_t> words;
    AppendStore(words, surface, StoreRowZeroCode);
    AppendStore(words, surface, StoreFarRowCode);
    SubmitAndWaitForPending(words);
    const auto begin = surface.Address();
    const auto split = begin + Piece;
    const auto end = begin + surface.bytes;
    {
        std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex());
        Require(Pending(begin, end), "the large image has no pending results");
        std::size_t checks = 0;
        const bool finished = AgcDriver::Graphics::StorageTexture::FlushAllPendingUntil("idle", [&] { return checks++ != 0; });
        Require(!finished, "a store asked to stop after one piece reported that the large image was stored");
        Require(!AgcDriver::Graphics::StorageTexture::FlushPending(begin, static_cast<std::size_t>(Piece), nullptr, "idle yield fixture"), "a store asked to stop after one piece left results pending in the first piece");
        Require(AgcDriver::Graphics::StorageTexture::FlushPending(split, static_cast<std::size_t>(end - split), nullptr, "idle yield fixture"), "a store asked to stop after one piece stored the rest of the large image as well");
        Require(!Pending(begin, end), "storing the rest of the large image left it pending");
    }
    Require(Contains(begin, split) && Contains(split, end), "the two stores did not bring both rows of the large image to guest memory");
    GuestAllocations::Mutation mutation;
    mutation.Remove(surface.texels);
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
        StopsBetweenImages();
        StopsWithinAnImage();
        AgcDriverShutdown_nid_postfix();
        std::puts("idle store yields tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

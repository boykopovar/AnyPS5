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
#include <string_view>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr std::uint32_t Threads = 8;
constexpr std::uint32_t Side = 256;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t TileR64KBX = 0x1b;
constexpr std::uint32_t Type2D = 9;
constexpr std::size_t BlockBytes = 0x60000;
constexpr std::uint32_t Untouched = 0xdeadbeefu;

alignas(256) constexpr std::array<std::uint32_t, 7> StoreCode{
    0x7e040300, 0x7e060280, 0x7e0202ff, 0x12345678u, 0xf0200108, 0x00020102, 0xbf810000,
};

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

std::vector<std::uint32_t> Commands(const std::uint32_t* texels) {
    std::vector<std::uint32_t> userData(16, 0u);
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
    const auto address = reinterpret_cast<std::uintptr_t>(StoreCode.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{static_cast<std::uint32_t>(userData.size()) << 1u};
    registers(0x213, resource);
    registers(0x240, userData);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x41u});
    return words;
}

void Store(const std::uint32_t* texels) {
    auto words = Commands(texels);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
}

}

int main(int argc, char** argv) {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        std::vector<std::uint32_t> storage((BlockBytes + 0x10000u) / 4u);
        const bool wake = argc == 2 && std::string_view(argv[1]) == "--idle-epoch-wake";
        std::vector<std::uint32_t> nextStorage(wake ? (BlockBytes + 0x10000u) / 4u : 0u);
        auto* nextTexels = wake ? reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(nextStorage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu}) : nullptr;
        struct ShutdownGuard {
            ~ShutdownGuard() { try { AgcDriverShutdown_nid_postfix(); } catch (...) {} }
        } shutdown;
        auto* texels = reinterpret_cast<std::uint32_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu});
        std::fill(texels, texels + BlockBytes / 4u, Untouched);
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(texels, BlockBytes, true, true, true);
            if (nextTexels != nullptr) {
                std::fill(nextTexels, nextTexels + BlockBytes / 4u, Untouched);
                mutation.Add(nextTexels, BlockBytes, true, true, true);
            }
        }
        const auto bytes = AgcDriver::Graphics::DescribeSurface(AgcDriver::Graphics::DecodeTextureResource(TextureDescriptor(texels))).guestBytes;
        const volatile std::uint32_t* cpu = texels;
        const auto stored = [&] {
            for (std::size_t index = 0; index < bytes / 4u; ++index) {
                if (cpu[index] == 0x12345678u) return true;
            }
            return false;
        };

        Store(texels);
        if (wake) {
            const auto pendingDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (!AgcDriver::Graphics::StorageTexture::AnyPending() && std::chrono::steady_clock::now() < pendingDeadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            Require(AgcDriver::Graphics::StorageTexture::AnyPending(), "idle wake fixture did not observe pending storage results");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            {
                std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex());
                AgcDriver::Graphics::StorageTexture::FlushAllPending("idle wake fixture");
            }
            std::array<std::uint32_t, 2> nop{0xc0001000u, 0u};
            Packet packet{nop.data(), static_cast<std::uint32_t>(nop.size()), 0, {}};
            AgcDriver::Submit(&packet, 0x30u);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cpu = nextTexels;
            Store(nextTexels);
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!stored() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Require(stored(), "the queue-idle store did not bring the result to guest memory without a direct flush or further submission");

        AgcDriverShutdown_nid_postfix();
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(texels);
            if (nextTexels != nullptr) mutation.Remove(nextTexels);
        }
        std::puts("store idle results tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

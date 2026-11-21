#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "VulkanTestDevice.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t CopyBytes = 0x40000;
constexpr std::size_t LabelOffset = 2 * CopyBytes;
constexpr std::size_t BlockBytes = LabelOffset + 0x10000;
constexpr int Rounds = 32;

std::uint64_t AddressOf(const volatile void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::uint8_t Pattern(int round, std::size_t at) {
    return static_cast<std::uint8_t>(0x31u + (at + static_cast<std::size_t>(round) * 7u) % 67u);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::vector<std::uint8_t> storage(BlockBytes + 0x10000u);
        auto* block = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 0xffffu) & ~std::uintptr_t{0xffffu});
        auto* source = block;
        auto* destination = block + CopyBytes;
        auto* label = reinterpret_cast<volatile std::uint32_t*>(block + LabelOffset);
        std::memset(block, 0, BlockBytes);
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(block, BlockBytes, true, true, true);
        }
        std::vector<std::uint8_t> seen(CopyBytes);
        for (int round = 0; round < Rounds; ++round) {
            for (std::size_t at = 0; at < CopyBytes; ++at) source[at] = Pattern(round, at);
            std::memset(destination, 0xa5, CopyBytes);
            *label = 0;
            const std::uint32_t value = 0x600d0001u + static_cast<std::uint32_t>(round);
            {
                std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
                const auto outcome = device->CopyBuffer(AddressOf(destination), AddressOf(source), CopyBytes, 0, std::numeric_limits<std::size_t>::max(), 0, 0, 0, [](std::span<const std::byte>, std::uint64_t) {});
                if (outcome.path != 1) {
                    std::printf("skipped, the copy took path %d instead of the GPU transfer\n", outcome.path);
                    return VulkanTestSkipped;
                }
                const int reason = device->WriteLabelOnGpu(AddressOf(label), std::as_bytes(std::span(&value, 1)), static_cast<std::uint64_t>(round) + 1u, 0);
                if (reason != 0) {
                    std::printf("skipped, the label was not recorded on the GPU (reason %d)\n", reason);
                    return VulkanTestSkipped;
                }
                device->SubmitRecorded(true);
            }
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (*label != value) {
                Require(std::chrono::steady_clock::now() < deadline, "the label after the copy never landed");
                std::this_thread::yield();
            }
            std::memcpy(seen.data(), destination, CopyBytes);
            std::size_t stale = 0;
            for (std::size_t at = 0; at < CopyBytes; ++at) stale += seen[at] != Pattern(round, at);
            Require(stale == 0, "round " + std::to_string(round) + ": a CPU read after the label that followed the copy found " + std::to_string(stale) + " of " + std::to_string(CopyBytes) + " destination bytes not yet copied");
            device->WaitIdle();
        }
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            AgcDriver::Graphics::FlushCachedTextures(device->Device());
        }
        device->WaitIdle();
        {
            GuestAllocations::Mutation mutation;
            mutation.Remove(block);
        }
        std::puts("label after import copy tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

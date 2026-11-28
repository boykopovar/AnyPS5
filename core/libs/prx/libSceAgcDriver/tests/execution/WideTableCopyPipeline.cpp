#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/BufferCopy.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
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
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;
using AgcDriver::DriverDetail::WideTableCopyFormat;
using AgcDriver::DriverDetail::WideTableCopyKernel;

alignas(256) constexpr std::array<std::uint32_t, 11> Code = WideTableCopyKernel;
constexpr std::uint32_t Records = 64;
constexpr std::uint32_t Bytes = Records * 16u;
constexpr std::uint32_t DispatchOpcode = 0x15;
constexpr std::size_t BlockBytes = 65536;

std::uint32_t* Source = nullptr;
std::uint32_t* Destination = nullptr;
std::uint32_t* Table = nullptr;

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

std::uint32_t* NewBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    if (block == nullptr) throw std::runtime_error("wide table copy behind the pipeline: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, false);
    return block;
}

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("wide table copy behind the pipeline: " + what);
}

template <typename Ready>
void WaitUntil(Ready ready, const std::string& what) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!ready()) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("wide table copy behind the pipeline: timed out waiting for " + what);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::vector<std::uint32_t> FillPacket(std::uint32_t value, const void* destination, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(destination);
    return {0xc0055000u, 2u << 29u, value, 0u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), bytes};
}

std::vector<std::uint32_t> CopyDispatch() {
    const auto code = reinterpret_cast<std::uintptr_t>(Code.data());
    const auto table = reinterpret_cast<std::uintptr_t>(Table);
    return {
        0xc0037600u, 0x207u, 64u, 1u, 1u,
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(code >> 8u), static_cast<std::uint32_t>(code >> 40u),
        0xc0017600u, 0x213u, (2u << 1u) | (1u << 7u),
        0xc0027600u, 0x240u, static_cast<std::uint32_t>(table), static_cast<std::uint32_t>(table >> 32u),
        0xc0031500u, 1u, 1u, 1u, 0x41u,
    };
}

void Submit(std::vector<std::uint32_t> words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
}

std::uint64_t Taken() {
    using namespace AgcDriver::DriverDetail;
    return copyPathCounts[CopyCpu].load() + copyPathCounts[CopyGpu].load() + copyPathCounts[CopyAliased].load();
}

void Round(std::uint32_t stale, std::uint32_t fresh) {
    Submit(FillPacket(stale, Source, Bytes));
    Submit(FillPacket(0xcafebabeu, Destination, Bytes));
    AgcDriverWaitIdle_nid_postfix();
    WaitUntil([] { return DrawPipeline::QueuedItems() == 0; }, "the pipeline to empty");
    const auto released = std::make_shared<std::atomic<bool>>(false);
    struct Release {
        std::atomic<bool>& flag;
        ~Release() { flag.store(true, std::memory_order_release); }
    } release{*released};
    DrawPipeline::Queue0().Enqueue([released] {
        while (!released->load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }, {});
    Submit(FillPacket(fresh, Source, Bytes));
    WaitUntil([] { return DrawPipeline::QueuedItems() >= 2; }, "the source fill queued behind the held pipeline");
    const auto taken = Taken();
    const auto drains = DrawPipeline::DrainRequestsByOpcode(DispatchOpcode);
    Submit(CopyDispatch());
    WaitUntil([&] { return Taken() != taken || DrawPipeline::DrainRequestsByOpcode(DispatchOpcode) != drains; }, "the copy to drain the held pipeline or run");
    released->store(true, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Destination), Bytes);
    check(Taken() - taken == 1, "the copy did not take the replacement");
    for (std::uint32_t word = 0; word < Bytes / 4u; ++word) {
        check(Destination[word] == fresh, "word " + std::to_string(word) + " of the destination is " + std::to_string(Destination[word]) + ", expected " + std::to_string(fresh) + " (the copy read the source before the fill queued ahead of it in the draw pipeline)");
    }
}

}

int main() {
    SetEnvironment("APS5_PIPELINED_DRAWS", "1");
    SetEnvironment("APS5_PIPELINE_DISPATCH", "1");
    try {
        if (std::getenv("APS5_NO_COPY_HLE") != nullptr || std::getenv("APS5_NO_WIDE_TABLE_COPY_HLE") != nullptr) {
            std::puts("skipped, the wide table copy replacement is switched off");
            return VulkanTestSkipped;
        }
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        Source = NewBlock();
        Destination = NewBlock();
        Table = NewBlock();
        const auto from = reinterpret_cast<std::uintptr_t>(Source);
        const auto to = reinterpret_cast<std::uintptr_t>(Destination);
        const std::array<std::uint32_t, 8> descriptors{static_cast<std::uint32_t>(from), static_cast<std::uint32_t>((from >> 32u) & 0xffffu) | (16u << 16u), Records, WideTableCopyFormat,
                                                        static_cast<std::uint32_t>(to), static_cast<std::uint32_t>((to >> 32u) & 0xffffu) | (16u << 16u), Records, WideTableCopyFormat};
        std::copy(descriptors.begin(), descriptors.end(), Table);
        Round(0x11111111u, 0x5a5a0001u);
        Round(0x22222222u, 0x5a5a0002u);
        AgcDriverShutdown_nid_postfix();
        std::puts("wide table copy behind the pipeline tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}

#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t StagedBytes = 16384;
constexpr std::uint32_t InPlaceBytes = Lanes * 4u;
constexpr std::uint32_t ComputeQueue = 0x20;
constexpr std::size_t BlockBytes = 65536;

alignas(256) constexpr std::array<std::uint32_t, 5> StoreOne{0x34020082u, 0x4a040081u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 5> StoreTwo{0x34020082u, 0x4a040082u, 0xe0701000u, 0x80000201u, 0xbf810000u};

std::uint32_t* NewBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    if (block == nullptr) throw std::runtime_error("profiled dispatch run: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return block;
}

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("profiled dispatch run: " + what);
}

void Register(const std::uint32_t* code, std::size_t bytes) {
    Shader shader{};
    shader.file_header = 0x34333231;
    shader.version = 0x18;
    shader.header_size = sizeof(Shader);
    shader.shader_size = static_cast<std::uint32_t>(bytes);
    shader.code = code;
    AgcDriverRegisterShader_nid_postfix(&shader);
}

void AddDispatch(std::vector<std::uint32_t>& words, const std::uint32_t* code, const std::uint32_t* buffer, std::uint32_t bytes) {
    const auto data = reinterpret_cast<std::uintptr_t>(buffer);
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code);
    words.insert(words.end(), {0xc0047600u, 0x240u, static_cast<std::uint32_t>(data), static_cast<std::uint32_t>((data >> 32u) & 0xffffu), bytes, 0x31016facu});
    words.insert(words.end(), {0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u), 0xc0031500u, 1u, 1u, 1u, 0x8041u});
}

void Round(std::uint32_t round, std::uint32_t* staged, std::uint32_t* inPlace) {
    std::memset(staged, 0xff, StagedBytes);
    std::memset(inPlace, 0xff, InPlaceBytes);
    std::vector<std::uint32_t> words{0xc0037600u, 0x207u, Lanes, 1u, 1u, 0xc0017600u, 0x213u, 4u << 1u};
    AddDispatch(words, StoreOne.data(), staged, StagedBytes);
    AddDispatch(words, StoreTwo.data(), inPlace, InPlaceBytes);
    const auto before = AgcDriver::Graphics::Recorder::OverlapCounts().overlapped;
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &packet) == 0, "submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto joined = AgcDriver::Graphics::Recorder::OverlapCounts().overlapped - before;
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(staged), StagedBytes);
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(inPlace), InPlaceBytes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto first = std::atomic_ref<std::uint32_t>(staged[lane]).load(std::memory_order_acquire);
        const auto second = std::atomic_ref<std::uint32_t>(inPlace[lane]).load(std::memory_order_acquire);
        check(first == lane + 1u, "round " + std::to_string(round) + ": the staged dispatch stored " + std::to_string(first) + " at lane " + std::to_string(lane) + ", expected " + std::to_string(lane + 1u));
        check(second == lane + 2u, "round " + std::to_string(round) + ": the dispatch that joined the run stored " + std::to_string(second) + " at lane " + std::to_string(lane) + ", expected " + std::to_string(lane + 2u));
    }
    check(joined == 1, "round " + std::to_string(round) + ": " + std::to_string(joined) + " dispatches joined the run under GPU timing, expected 1");
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_PROFILE_GPU", "1");
#else
        setenv("APS5_PROFILE_GPU", "1", 1);
#endif
        {
            const auto probe = OpenVulkanTestDevice();
            if (!probe) return VulkanTestSkipped;
        }
        check(AgcDriver::Graphics::Recorder::GpuTimingEnabled(), "GPU timing is off");
        Register(StoreOne.data(), sizeof(StoreOne));
        Register(StoreTwo.data(), sizeof(StoreTwo));
        auto* staged = NewBlock();
        auto* inPlace = NewBlock();
        for (std::uint32_t round = 1; round <= 3; ++round) Round(round, staged, inPlace);
        std::puts("profiled dispatch run: a dispatch joining a run that owes a staged copy-back ran its own program under GPU timing");
        LibcRunShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}

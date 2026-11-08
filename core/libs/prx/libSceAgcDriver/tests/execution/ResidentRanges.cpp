#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
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
constexpr std::uint32_t OutputLane = 64;
constexpr std::uint32_t ComputeQueue = 0x20;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t PromoteAfter = 8;

std::uint32_t* Data = nullptr;

alignas(256) constexpr std::array<std::uint32_t, 7> CopySelfCode{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0xe0701100u, 0x80000201u, 0xbf810000u};

std::uint32_t* NewBlock() {
    std::uint32_t* block = nullptr;
    if (AgcDriver::GuestMemory::WriteWatched()) {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, BlockBytes));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        void* raw = mmap(nullptr, 2 * BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (raw == MAP_FAILED) throw std::runtime_error("resident ranges: cannot map a guest block");
        const auto begin = reinterpret_cast<std::uintptr_t>(raw);
        const auto aligned = (begin + BlockBytes - 1) & ~(static_cast<std::uintptr_t>(BlockBytes) - 1);
        if (aligned != begin) munmap(raw, aligned - begin);
        if (aligned + BlockBytes != begin + 2 * BlockBytes) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + BlockBytes - aligned);
        block = reinterpret_cast<std::uint32_t*>(aligned);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
#endif
    } else {
#ifdef _WIN32
        block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    }
    if (block == nullptr) throw std::runtime_error("resident ranges: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return block;
}

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("resident ranges: " + what);
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

std::uint64_t Delta(std::uint64_t before, std::uint64_t after) {
    return after >= before ? after - before : after;
}

void SetInputs(std::uint32_t value) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) Data[lane] = value + lane;
    AgcDriver::GuestMemory::MarkWritten(reinterpret_cast<std::uintptr_t>(Data), Lanes * 4u);
}

AgcDriver::Graphics::ResidentStatistics Build(std::uint32_t expected, const std::string& what) {
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(CopySelfCode.data());
    const auto data = reinterpret_cast<std::uintptr_t>(Data);
    std::vector<std::uint32_t> words{
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u),
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 4u << 1u,
        0xc0047600u, 0x240u, static_cast<std::uint32_t>(data), static_cast<std::uint32_t>((data >> 32u) & 0xffffu), static_cast<std::uint32_t>(BlockBytes), 0x31016facu,
        0xc0031500u, 1u, 1u, 1u, 0x8041u,
    };
    const auto before = AgcDriver::Graphics::ResidentCounts();
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &packet) == 0, what + ": submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto after = AgcDriver::Graphics::ResidentCounts();
    AgcDriver::GuestMemory::FlushGpuWrites(data, BlockBytes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto value = std::atomic_ref<std::uint32_t>(Data[OutputLane + lane]).load(std::memory_order_acquire);
        check(value == expected + lane, what + ": output lane " + std::to_string(lane) + " is " + std::to_string(value) + ", expected " + std::to_string(expected + lane));
    }
    return AgcDriver::Graphics::ResidentStatistics{after.resident, Delta(before.promoted, after.promoted), Delta(before.bound, after.bound), Delta(before.skipped, after.skipped), Delta(before.copied, after.copied), Delta(before.invalidated, after.invalidated)};
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_NO_ADAPTIVE_STAGING", "1");
        _putenv_s("APS5_NO_RESOURCE_CACHE", "1");
#else
        setenv("APS5_NO_ADAPTIVE_STAGING", "1", 1);
        setenv("APS5_NO_RESOURCE_CACHE", "1", 1);
#endif
        {
            const auto probe = OpenVulkanTestDevice();
            if (!probe) return VulkanTestSkipped;
        }

        Register(CopySelfCode.data(), sizeof(CopySelfCode));
        Data = NewBlock();

        SetInputs(100);
        for (std::uint32_t build = 1; build < PromoteAfter; ++build) {
            const auto counts = Build(100, "build " + std::to_string(build) + " before promotion");
            check(counts.resident == 0, "build " + std::to_string(build) + " of " + std::to_string(PromoteAfter) + " already made the range resident");
            check(counts.promoted == 0 && counts.skipped == 0, "build " + std::to_string(build) + " promoted or skipped before the eighth staging");
        }
        auto counts = Build(100, "the promoting build");
        check(counts.resident == 1, "eight stagings of one 64 KiB range left " + std::to_string(counts.resident) + " resident ranges, expected 1");
        check(counts.promoted == 1 && counts.copied == 1 && counts.skipped == 0, "the promoting build reported promoted " + std::to_string(counts.promoted) + ", copied " + std::to_string(counts.copied) + ", skipped " + std::to_string(counts.skipped) + ", expected 1 / 1 / 0");
        std::puts("resident ranges: the eighth staging of a 64 KiB range made it resident and copied it in");

        for (std::uint32_t round = 1; round <= 3; ++round) {
            counts = Build(100, "unchanged build " + std::to_string(round));
            check(counts.resident == 1 && counts.bound == 1, "unchanged build " + std::to_string(round) + " did not bind the resident range");
            check(counts.skipped == 1 && counts.copied == 0 && counts.invalidated == 0, "unchanged build " + std::to_string(round) + " reported skipped " + std::to_string(counts.skipped) + ", copied " + std::to_string(counts.copied) + ", invalidated " + std::to_string(counts.invalidated) + ", expected 1 / 0 / 0");
        }
        std::puts("resident ranges: builds over unchanged guest memory skipped the copy-in");

        SetInputs(200);
        counts = Build(200, "the build after a CPU store");
        check(counts.resident == 1 && counts.bound == 1, "the build after a CPU store did not bind the resident range");
        check(counts.skipped == 0 && counts.copied == 1 && counts.invalidated == 1, "the build after a CPU store reported skipped " + std::to_string(counts.skipped) + ", copied " + std::to_string(counts.copied) + ", invalidated " + std::to_string(counts.invalidated) + ", expected 0 / 1 / 1");
        counts = Build(200, "the build after the re-copy");
        check(counts.skipped == 1 && counts.copied == 0, "the build after the re-copy reported skipped " + std::to_string(counts.skipped) + ", copied " + std::to_string(counts.copied) + ", expected 1 / 0");
        std::puts("resident ranges: a CPU store invalidated the resident copy and the next build copied the new bytes in");

        GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(Data), BlockBytes);
        counts = Build(200, "the build after the pages were forgotten");
        check(counts.skipped == 0 && counts.promoted == 1 && counts.copied == 1, "the build after ForgetPages reported skipped " + std::to_string(counts.skipped) + ", promoted " + std::to_string(counts.promoted) + ", copied " + std::to_string(counts.copied) + ", expected 0 / 1 / 1");
        std::puts("resident ranges: forgetting the range's pages dropped the resident copy");

        LibcRunShutdown_nid_postfix();
        counts = AgcDriver::Graphics::ResidentCounts();
        check(counts.resident == 0, "device teardown left " + std::to_string(counts.resident) + " resident ranges holding device buffers");
        std::puts("resident ranges: device teardown released the resident buffers");
        std::puts("resident ranges tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}

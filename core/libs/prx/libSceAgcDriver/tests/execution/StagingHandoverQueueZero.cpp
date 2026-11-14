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
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Recorder;

constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t BufferBytes = Lanes * 4u;
constexpr std::uint32_t ComputeQueue = 0x20;
constexpr std::size_t BlockBytes = 65536;

alignas(256) constexpr std::array<std::uint32_t, 8> AddOne{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a040481u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> AddTwo{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a040482u, 0xe0701000u, 0x80000201u, 0xbf810000u};

std::uint32_t* NewBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    if (block == nullptr) throw std::runtime_error("staging handover queue zero: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return block;
}

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("staging handover queue zero: " + what);
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

std::vector<std::uint32_t> Dispatches(const std::uint32_t* data, std::span<const std::uint32_t* const> programs) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    std::vector<std::uint32_t> words{
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 4u << 1u,
        0xc0047600u, 0x240u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), BufferBytes, 0x31016facu,
    };
    for (const auto* program : programs) {
        const auto codeAddress = reinterpret_cast<std::uintptr_t>(program);
        words.insert(words.end(), {0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u), 0xc0031500u, 1u, 1u, 1u, 0x8041u});
    }
    return words;
}

std::uint64_t Round(std::uint32_t queue, std::uint32_t* data, std::span<const std::uint32_t* const> programs, std::uint32_t added, std::uint32_t& expected, const std::string& what, bool active) {
    auto words = Dispatches(data, programs);
    const auto before = Recorder::HandoverCounts();
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check((queue == 0 ? sceAgcDriverSubmitDcb(&packet) : sceAgcDriverSubmitAcb(queue, &packet)) == 0, what + ": the submission failed");
    AgcDriverWaitIdle_nid_postfix();
    expected += added;
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(data), BufferBytes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto value = std::atomic_ref<std::uint32_t>(data[lane]).load(std::memory_order_acquire);
        check(value == expected, what + ": lane " + std::to_string(lane) + " is " + std::to_string(value) + ", expected " + std::to_string(expected));
    }
    for (std::uint32_t word = Lanes; word < Lanes * 2u; ++word) check(data[word] == 0u, what + ": dword " + std::to_string(word) + " past the buffer was written");
    const auto after = Recorder::HandoverCounts();
    std::printf("staging handover queue zero: %s: %llu handovers, %llu copy-ins skipped, %llu redirected, %llu copy-backs elided, %llu landed\n", what.c_str(), static_cast<unsigned long long>(after.taken - before.taken), static_cast<unsigned long long>(after.skipped - before.skipped), static_cast<unsigned long long>(after.redirected - before.redirected), static_cast<unsigned long long>(after.elided - before.elided), static_cast<unsigned long long>(after.landed - before.landed));
    if (!Recorder::StagingHandoverEnabled()) check(after.taken == 0 && after.elided == 0 && after.landed == 0, what + ": copy-backs were deferred although the handover is off");
    if (active) {
        check(after.elided - before.elided == programs.size() - 1, what + ": " + std::to_string(after.elided - before.elided) + " copy-backs were elided, expected " + std::to_string(programs.size() - 1));
        check(after.landed - before.landed == 1, what + ": " + std::to_string(after.landed - before.landed) + " copy-backs landed, expected 1");
    }
    return after.landed - before.landed;
}

void Groups(std::uint32_t queue, std::uint32_t* first, std::uint32_t* second, std::uint32_t rounds, std::uint32_t& expected, const std::string& what, bool active) {
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(AddOne.data());
    std::vector<std::uint32_t> words{
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 4u << 1u,
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u),
    };
    for (std::uint32_t round = 0; round < rounds; ++round) {
        for (const auto* data : {first, second}) {
            const auto address = reinterpret_cast<std::uintptr_t>(data);
            words.insert(words.end(), {0xc0047600u, 0x240u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), BufferBytes, 0x31016facu, 0xc0031500u, 1u, 1u, 1u, 0x8041u});
        }
    }
    const auto before = Recorder::HandoverCounts();
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check((queue == 0 ? sceAgcDriverSubmitDcb(&packet) : sceAgcDriverSubmitAcb(queue, &packet)) == 0, what + ": the submission failed");
    AgcDriverWaitIdle_nid_postfix();
    expected += rounds;
    for (const auto* data : {first, second}) {
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(data), BufferBytes);
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            const auto value = std::atomic_ref<const std::uint32_t>(data[lane]).load(std::memory_order_acquire);
            check(value == expected, what + ": lane " + std::to_string(lane) + " is " + std::to_string(value) + ", expected " + std::to_string(expected));
        }
    }
    const auto after = Recorder::HandoverCounts();
    std::printf("staging handover queue zero: %s: %llu handovers, %llu copy-ins skipped, %llu redirected, %llu copy-backs elided, %llu landed\n", what.c_str(), static_cast<unsigned long long>(after.taken - before.taken), static_cast<unsigned long long>(after.skipped - before.skipped), static_cast<unsigned long long>(after.redirected - before.redirected), static_cast<unsigned long long>(after.elided - before.elided), static_cast<unsigned long long>(after.landed - before.landed));
    if (!Recorder::StagingHandoverEnabled()) check(after.taken == before.taken && after.elided == before.elided && after.landed == before.landed, what + ": copy-backs were deferred although the handover is off");
    if (active) {
        check(after.elided - before.elided == rounds * 2u - 2u, what + ": " + std::to_string(after.elided - before.elided) + " copy-backs were elided, expected " + std::to_string(rounds * 2u - 2u));
        check(after.landed - before.landed == 2, what + ": " + std::to_string(after.landed - before.landed) + " copy-backs landed, expected one of each group");
    }
}

}

int main(int argc, char** argv) {
    try {
        const bool off = argc > 1 && std::string(argv[1]) == "--off";
#ifdef _WIN32
        _putenv_s("APS5_WRITTEN_SHADOW_MIN_KIB", "0");
        _putenv_s("APS5_NO_CHANGED_COPY_BACK", "1");
        if (off) _putenv_s("APS5_NO_STAGING_HANDOVER", "1");
#else
        setenv("APS5_WRITTEN_SHADOW_MIN_KIB", "0", 1);
        setenv("APS5_NO_CHANGED_COPY_BACK", "1", 1);
        if (off) setenv("APS5_NO_STAGING_HANDOVER", "1", 1);
#endif
        {
            const auto probe = OpenVulkanTestDevice();
            if (!probe) return VulkanTestSkipped;
        }
        check(Recorder::StagingHandoverEnabled() == !off, "APS5_NO_STAGING_HANDOVER does not match the run");
        Register(AddOne.data(), sizeof(AddOne));
        Register(AddTwo.data(), sizeof(AddTwo));
        const std::array<const std::uint32_t*, 1> one{AddOne.data()};
        const std::array<const std::uint32_t*, 6> six{AddOne.data(), AddOne.data(), AddOne.data(), AddOne.data(), AddOne.data(), AddOne.data()};
        const std::array<const std::uint32_t*, 6> alternating{AddOne.data(), AddTwo.data(), AddOne.data(), AddTwo.data(), AddOne.data(), AddTwo.data()};
        for (const std::uint32_t queue : {0u, ComputeQueue}) {
            auto* data = NewBlock();
            const std::string where = queue == 0 ? "queue 0" : "a compute queue";
            std::uint32_t expected = 0;
            const bool active = Round(queue, data, one, 1, expected, "one dispatch on " + where, false) == 1;
            check(!(off && active), "a copy-back was deferred on " + where + " although the handover is off");
            if (!off && !active) std::printf("staging handover queue zero: the device stages no written buffer in device memory on %s, results only\n", where.c_str());
            Round(queue, data, six, 6, expected, "six dispatches of one program on " + where, active);
            Round(queue, data, alternating, 9, expected, "two alternating programs on " + where, active);
            Round(queue, data, six, 6, expected, "six more dispatches of one program on " + where, active);
            auto* first = NewBlock();
            auto* second = NewBlock();
            std::uint32_t grouped = 0;
            Groups(queue, first, second, 3, grouped, "two alternating groups on " + where, active);
            Groups(queue, first, second, 4, grouped, "two alternating groups again on " + where, active);
        }
        const auto counts = Recorder::HandoverCounts();
        for (std::size_t site = 0; site < Recorder::LandingSites; ++site) {
            if (counts.landedAt[site] != 0) std::printf("staging handover queue zero: %llu copy-backs landed by %s\n", static_cast<unsigned long long>(counts.landedAt[site]), Recorder::LandingSiteName(static_cast<Recorder::LandingSite>(site)));
        }
        LibcRunShutdown_nid_postfix();
        std::puts(off ? "staging handover queue zero tests passed with the handover off" : "staging handover queue zero tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}

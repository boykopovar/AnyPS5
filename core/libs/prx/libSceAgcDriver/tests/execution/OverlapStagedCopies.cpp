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
constexpr std::uint32_t BufferBytes = Lanes * 4u;
constexpr std::uint32_t ComputeQueue = 0x20;
constexpr std::size_t BlockBytes = 65536;

std::uint32_t* Data = nullptr;
std::uint32_t* Counters = nullptr;

alignas(256) constexpr std::array<std::uint32_t, 8> IncrementCode{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a040481u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 5> AtomicAddCode{0x34020082u, 0x7e040281u, 0xe0c81000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 6> AtomicAddCopy{0x34020082u, 0x7e040281u, 0xbf800000u, 0xe0c81000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 5> StoreCode{0x34020082u, 0x4a040087u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 7> CopyCode{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0xe0701000u, 0x80010201u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 5> StoreOne{0x34020082u, 0x4a040081u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 5> StoreTwo{0x34020082u, 0x4a040082u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 5> StoreThree{0x34020082u, 0x4a040083u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 5> StoreFour{0x34020082u, 0x4a040084u, 0xe0701000u, 0x80000201u, 0xbf810000u};
const std::array<const std::uint32_t*, 4> StoreCodes{StoreOne.data(), StoreTwo.data(), StoreThree.data(), StoreFour.data()};

std::uint32_t* Source = nullptr;
std::uint32_t* Target = nullptr;
std::array<std::uint32_t*, 4> Staged{};

std::uint32_t* NewBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    if (block == nullptr) throw std::runtime_error("overlap staged copies: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return block;
}

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("overlap staged copies: " + what);
}

std::vector<std::uint32_t> Dispatches(std::uint32_t count) {
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(IncrementCode.data());
    const auto data = reinterpret_cast<std::uintptr_t>(Data);
    std::vector<std::uint32_t> words{
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u),
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 4u << 1u,
        0xc0047600u, 0x240u, static_cast<std::uint32_t>(data), static_cast<std::uint32_t>((data >> 32u) & 0xffffu), BufferBytes, 0x31016facu,
    };
    for (std::uint32_t index = 0; index < count; ++index) words.insert(words.end(), {0xc0031500u, 1u, 1u, 1u, 0x8041u});
    return words;
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

std::vector<std::uint32_t> AtomicPair() {
    const auto data = reinterpret_cast<std::uintptr_t>(Counters);
    std::vector<std::uint32_t> words{
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 4u << 1u,
        0xc0047600u, 0x240u, static_cast<std::uint32_t>(data), static_cast<std::uint32_t>((data >> 32u) & 0xffffu), BufferBytes, 0x31016facu,
    };
    for (const auto* code : {AtomicAddCode.data(), AtomicAddCopy.data()}) {
        const auto codeAddress = reinterpret_cast<std::uintptr_t>(code);
        words.insert(words.end(), {0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u), 0xc0031500u, 1u, 1u, 1u, 0x8041u});
    }
    return words;
}

void AtomicRound(std::uint32_t& expected, const std::string& what) {
    auto words = AtomicPair();
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &packet) == 0, "submission failed");
    AgcDriverWaitIdle_nid_postfix();
    expected += 2;
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Counters), BufferBytes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto value = std::atomic_ref<std::uint32_t>(Counters[lane]).load(std::memory_order_acquire);
        check(value == expected, what + ": counter " + std::to_string(lane) + " is " + std::to_string(value) + ", expected " + std::to_string(expected));
    }
}

void ReadRound(std::uint32_t round) {
    std::memset(Source, 0, BufferBytes);
    std::memset(Target, 0xff, BufferBytes);
    const auto source = reinterpret_cast<std::uintptr_t>(Source);
    const auto target = reinterpret_cast<std::uintptr_t>(Target);
    std::vector<std::uint32_t> words{
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 8u << 1u,
        0xc0087600u, 0x240u, static_cast<std::uint32_t>(source), static_cast<std::uint32_t>((source >> 32u) & 0xffffu), BufferBytes, 0x31016facu,
        static_cast<std::uint32_t>(target), static_cast<std::uint32_t>((target >> 32u) & 0xffffu), BufferBytes, 0x31016facu,
    };
    for (const auto* code : {StoreCode.data(), CopyCode.data()}) {
        const auto codeAddress = reinterpret_cast<std::uintptr_t>(code);
        words.insert(words.end(), {0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u), 0xc0031500u, 1u, 1u, 1u, 0x8041u});
    }
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &packet) == 0, "submission failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(target, BufferBytes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto value = std::atomic_ref<std::uint32_t>(Target[lane]).load(std::memory_order_acquire);
        check(value == lane + 7u, "a read of an earlier unordered dispatch's staged output, round " + std::to_string(round) + ": lane " + std::to_string(lane) + " read " + std::to_string(value) + ", expected " + std::to_string(lane + 7u));
    }
}

bool Inline = false;

void StagedRun(std::uint32_t round) {
    std::vector<std::uint32_t> words{0xc0037600u, 0x207u, Lanes, 1u, 1u, 0xc0017600u, 0x213u, 4u << 1u};
    for (std::size_t build = 0; build < StoreCodes.size(); ++build) {
        std::memset(Staged[build], 0xff, BufferBytes);
        const auto data = reinterpret_cast<std::uintptr_t>(Staged[build]);
        const auto codeAddress = reinterpret_cast<std::uintptr_t>(StoreCodes[build]);
        words.insert(words.end(), {0xc0047600u, 0x240u, static_cast<std::uint32_t>(data), static_cast<std::uint32_t>((data >> 32u) & 0xffffu), BufferBytes, 0x31016facu});
        words.insert(words.end(), {0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u), 0xc0031500u, 1u, 1u, 1u, 0x8041u});
    }
    const auto counts = AgcDriver::Graphics::Recorder::OverlapCounts();
    const auto before = counts.overlapped;
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &packet) == 0, "submission failed");
    AgcDriverWaitIdle_nid_postfix();
    const auto after = AgcDriver::Graphics::Recorder::OverlapCounts();
    const auto joined = after.overlapped - before;
    const auto prelude = after.preludeDispatches - counts.preludeDispatches;
    for (std::size_t build = 0; build < StoreCodes.size(); ++build) {
        AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Staged[build]), BufferBytes);
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            const auto value = std::atomic_ref<std::uint32_t>(Staged[build][lane]).load(std::memory_order_acquire);
            check(value == lane + static_cast<std::uint32_t>(build) + 1u, "staged unordered dispatches, round " + std::to_string(round) + ", build " + std::to_string(build + 1) + ": lane " + std::to_string(lane) + " is " + std::to_string(value));
        }
    }
    check(joined == StoreCodes.size() - 1, "staged unordered dispatches of four builds, round " + std::to_string(round) + ": " + std::to_string(joined) + " joined a run, expected 3");
    const std::uint64_t expected = Inline ? 0 : StoreCodes.size() - 1;
    check(prelude == expected, "staged unordered dispatches of four builds, round " + std::to_string(round) + ": " + std::to_string(prelude) + " joining dispatches copied in through the run's prelude, expected " + std::to_string(expected));
}

void Round(std::uint32_t dispatches, std::uint32_t& expected, const std::string& what) {
    auto words = Dispatches(dispatches);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &packet) == 0, "submission failed");
    AgcDriverWaitIdle_nid_postfix();
    expected += dispatches;
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Data), BufferBytes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto value = std::atomic_ref<std::uint32_t>(Data[lane]).load(std::memory_order_acquire);
        check(value == expected, what + ": lane " + std::to_string(lane) + " is " + std::to_string(value) + ", expected " + std::to_string(expected));
    }
}

}

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        _putenv_s("APS5_WRITTEN_SHADOW_MIN_KIB", "0");
#else
        setenv("APS5_WRITTEN_SHADOW_MIN_KIB", "0", 1);
#endif
        {
            const auto probe = OpenVulkanTestDevice();
            if (!probe) return VulkanTestSkipped;
        }

        Inline = argc > 1 && std::string(argv[1]) == "--inline";
        for (const auto* code : StoreCodes) Register(code, sizeof(StoreOne));
        for (auto& block : Staged) block = NewBlock();
        for (std::uint32_t round = 1; round <= 3; ++round) StagedRun(round);
        std::puts("overlap staged copies: unordered staged dispatches of four builds ran in one run");
        Register(IncrementCode.data(), sizeof(IncrementCode));
        Register(AtomicAddCode.data(), sizeof(AtomicAddCode));
        Register(AtomicAddCopy.data(), sizeof(AtomicAddCopy));

        Data = NewBlock();
        Counters = NewBlock();
        std::uint32_t expected = 0;
        Round(1, expected, "one dispatch");
        Round(2, expected, "two unordered dispatches of one build");
        Round(4, expected, "four unordered dispatches of one build");
        std::printf("overlap staged reuse: %u increments kept\n", expected);
        std::uint32_t counted = 0;
        for (int round = 0; round < 3; ++round) AtomicRound(counted, "two unordered atomic dispatches, round " + std::to_string(round + 1));
        std::printf("overlap staged atomics: %u atomic adds kept\n", counted);
        Register(StoreCode.data(), sizeof(StoreCode));
        Register(CopyCode.data(), sizeof(CopyCode));
        Source = NewBlock();
        Target = NewBlock();
        for (std::uint32_t round = 1; round <= 3; ++round) ReadRound(round);
        std::puts("overlap staged reads: a later unordered dispatch read the earlier one's staged output");

        LibcRunShutdown_nid_postfix();
        std::puts("overlap staged copies tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}

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
constexpr std::uint32_t Builds = 4;

alignas(256) constexpr std::array<std::uint32_t, 5> StoreOne{0x34020082u, 0x4a040081u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 5> StoreTwo{0x34020082u, 0x4a040082u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 5> StoreThree{0x34020082u, 0x4a040083u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 5> StoreFour{0x34020082u, 0x4a040084u, 0xe0701000u, 0x80000201u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 7> CopyCode{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0xe0701000u, 0x80010201u, 0xbf810000u};

const std::array<const std::uint32_t*, Builds> StoreCodes{StoreOne.data(), StoreTwo.data(), StoreThree.data(), StoreFour.data()};

std::array<std::uint32_t*, Builds> Targets{};
std::uint32_t* Spare = nullptr;
std::uint32_t* Copied = nullptr;

std::uint32_t* NewBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    if (block == nullptr) throw std::runtime_error("dispatch overlap: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return block;
}

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("dispatch overlap: " + what);
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

void AddBuffer(std::vector<std::uint32_t>& words, std::uint32_t reg, const std::uint32_t* memory) {
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    words.insert(words.end(), {0xc0047600u, reg, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), BufferBytes, 0x31016facu});
}

void AddDispatch(std::vector<std::uint32_t>& words, const std::uint32_t* code) {
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code);
    words.insert(words.end(), {0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u), 0xc0031500u, 1u, 1u, 1u, 0x8041u});
}

void AddPartialFlush(std::vector<std::uint32_t>& words) {
    words.insert(words.end(), {0xc0004600u, 0x407u});
}

std::vector<std::uint32_t> Header() {
    return {0xc0037600u, 0x207u, Lanes, 1u, 1u};
}

void AddUserSgprs(std::vector<std::uint32_t>& words, std::uint32_t count) {
    words.insert(words.end(), {0xc0017600u, 0x213u, count << 1u});
}

void Submit(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(ComputeQueue, &packet) == 0, "submission failed");
    AgcDriverWaitIdle_nid_postfix();
}

void Clear() {
    for (auto* target : Targets) std::memset(target, 0xff, BufferBytes);
    std::memset(Spare, 0xff, BufferBytes);
    std::memset(Copied, 0xff, BufferBytes);
}

void Expect(const std::uint32_t* memory, std::uint32_t add, const std::string& what) {
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(memory), BufferBytes);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        const auto value = std::atomic_ref<std::uint32_t>(const_cast<std::uint32_t&>(memory[lane])).load(std::memory_order_acquire);
        check(value == lane + add, what + ": lane " + std::to_string(lane) + " is " + std::to_string(value) + ", expected " + std::to_string(lane + add));
    }
}

std::uint64_t Overlapped() {
    return AgcDriver::Graphics::Recorder::OverlapCounts().overlapped;
}

std::uint64_t RunBuilds(bool ordered) {
    Clear();
    auto words = Header();
    AddUserSgprs(words, 4);
    for (std::uint32_t build = 0; build < Builds; ++build) {
        if (ordered && build != 0) AddPartialFlush(words);
        AddBuffer(words, 0x240u, Targets[build]);
        AddDispatch(words, StoreCodes[build]);
    }
    AddPartialFlush(words);
    AddUserSgprs(words, 8);
    AddBuffer(words, 0x240u, Targets[Builds - 1]);
    AddBuffer(words, 0x244u, Copied);
    AddDispatch(words, CopyCode.data());
    const auto before = Overlapped();
    Submit(words);
    const auto joined = Overlapped() - before;
    const std::string kind = ordered ? "dispatches ordered by partial flushes" : "unordered dispatches";
    for (std::uint32_t build = 0; build < Builds; ++build) Expect(Targets[build], build + 1, kind + ", build " + std::to_string(build + 1));
    Expect(Copied, Builds, kind + ", the ordered copy of the last result");
    return joined;
}

std::uint64_t ReadAfterWrite(bool dependent) {
    Clear();
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) Targets[1][lane] = lane + 5u;
    auto words = Header();
    AddUserSgprs(words, 4);
    AddBuffer(words, 0x240u, Targets[0]);
    AddDispatch(words, StoreCodes[0]);
    AddUserSgprs(words, 8);
    AddBuffer(words, 0x240u, dependent ? Targets[0] : Targets[1]);
    AddBuffer(words, 0x244u, Copied);
    AddDispatch(words, CopyCode.data());
    const auto before = Overlapped();
    Submit(words);
    const auto joined = Overlapped() - before;
    Expect(Targets[0], 1, dependent ? "a dispatch reading the previous one's output, the writer" : "a dispatch reading other memory, the writer");
    Expect(Copied, dependent ? 1u : 5u, dependent ? "a dispatch reading the previous unordered dispatch's output" : "a dispatch reading memory the previous one does not write");
    return joined;
}

std::uint64_t WriteAfterAccess(bool afterRead) {
    Clear();
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) Targets[0][lane] = lane + 5u;
    auto words = Header();
    if (afterRead) {
        AddUserSgprs(words, 8);
        AddBuffer(words, 0x240u, Targets[0]);
        AddBuffer(words, 0x244u, Copied);
        AddDispatch(words, CopyCode.data());
    } else {
        AddUserSgprs(words, 4);
        AddBuffer(words, 0x240u, Targets[0]);
        AddDispatch(words, StoreCodes[0]);
    }
    AddUserSgprs(words, 4);
    AddBuffer(words, 0x240u, Targets[0]);
    AddDispatch(words, StoreCodes[1]);
    const auto before = Overlapped();
    Submit(words);
    const auto joined = Overlapped() - before;
    if (afterRead) Expect(Copied, 5, "a dispatch reading memory the next unordered one overwrites");
    Expect(Targets[0], 2, afterRead ? "a dispatch overwriting what the previous unordered one read" : "a dispatch overwriting what the previous unordered one wrote");
    return joined;
}

void SameBuildTwice() {
    Clear();
    auto words = Header();
    AddUserSgprs(words, 4);
    AddBuffer(words, 0x240u, Targets[0]);
    AddDispatch(words, StoreCodes[0]);
    AddBuffer(words, 0x240u, Spare);
    AddDispatch(words, StoreCodes[0]);
    Submit(words);
    Expect(Targets[0], 1, "one build twice, first target");
    Expect(Spare, 1, "one build twice, second target");
}

}

int main(int argc, char** argv) {
    try {
        {
            const auto probe = OpenVulkanTestDevice();
            if (!probe) return VulkanTestSkipped;
        }
        const bool off = argc > 1 && std::string(argv[1]) == "--off";
        for (const auto* code : StoreCodes) Register(code, sizeof(StoreOne));
        Register(CopyCode.data(), sizeof(CopyCode));
        for (auto& target : Targets) target = NewBlock();
        Spare = NewBlock();
        Copied = NewBlock();

        for (int round = 0; round < 3; ++round) {
            const auto joined = RunBuilds(false);
            const std::uint64_t expected = off ? 0 : Builds - 1;
            check(joined == expected, "unordered dispatches of " + std::to_string(Builds) + " builds, round " + std::to_string(round + 1) + ": " + std::to_string(joined) + " joined a run, expected " + std::to_string(expected));
            const auto ordered = RunBuilds(true);
            check(ordered == 0, "dispatches ordered by partial flushes, round " + std::to_string(round + 1) + ": " + std::to_string(ordered) + " joined a run, expected 0");
            SameBuildTwice();
            const auto dependent = ReadAfterWrite(true);
            check(dependent == 0, "a dispatch reading what the previous unordered one wrote, round " + std::to_string(round + 1) + ": " + std::to_string(dependent) + " joined a run, expected 0");
            const auto afterRead = WriteAfterAccess(true);
            check(afterRead == 0, "a dispatch overwriting what the previous unordered one read, round " + std::to_string(round + 1) + ": " + std::to_string(afterRead) + " joined a run, expected 0");
            const auto afterWrite = WriteAfterAccess(false);
            check(afterWrite == 0, "a dispatch overwriting what the previous unordered one wrote, round " + std::to_string(round + 1) + ": " + std::to_string(afterWrite) + " joined a run, expected 0");
            const auto disjoint = ReadAfterWrite(false);
            const std::uint64_t disjointExpected = off ? 0 : 1;
            check(disjoint == disjointExpected, "a dispatch reading memory the previous unordered one does not write, round " + std::to_string(round + 1) + ": " + std::to_string(disjoint) + " joined a run, expected " + std::to_string(disjointExpected));
        }
        std::printf("dispatch overlap: %s\n", off ? "no unordered dispatch joined a run with APS5_NO_DISPATCH_OVERLAP" : "unordered dispatches of distinct builds ran in one run, results kept");

        LibcRunShutdown_nid_postfix();
        std::puts("dispatch overlap tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}

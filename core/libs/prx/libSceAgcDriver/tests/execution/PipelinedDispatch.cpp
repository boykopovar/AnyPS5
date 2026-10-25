#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
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
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t BufferBytes = Lanes * 4u;
constexpr std::uint32_t DispatchOpcode = 0x15;

constexpr std::size_t BlockBytes = 65536;

std::uint32_t* NewBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    if (block == nullptr) throw std::runtime_error("pipelined dispatch: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, false);
    return block;
}

std::uint32_t* Input = nullptr;
std::uint32_t* Output = nullptr;
std::uint32_t* Scratch = nullptr;
std::uint32_t* Table = nullptr;
std::uint32_t* TargetA = nullptr;
std::uint32_t* TargetB = nullptr;

alignas(256) constexpr std::array<std::uint32_t, 8> Code{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a060481u, 0xe0701000u, 0x80010301u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 9> ScalarCode{0xf4000200u, 0xfa000000u, 0xbf8cc07fu, 0x34020082u, 0x7e040208u, 0x4a040481u, 0xe0701000u, 0x80010201u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 11> TableCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x34020082u, 0xe0301000u, 0x80020201u, 0xbf8c3f70u, 0x4a060481u, 0xe0701000u, 0x80010301u, 0xbf810000u};

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("pipelined dispatch: " + what);
}

template <typename Ready>
void WaitUntil(Ready ready, const std::string& what) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!ready()) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("pipelined dispatch: timed out waiting for " + what);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::array<std::uint32_t, 7> FillPacket(std::uint32_t value, const void* destination, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(destination);
    return {0xc0055000u, 2u << 29u, value, 0u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), bytes};
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::vector<std::uint32_t> Round(const void* destination, std::uint32_t value, const void* code) {
    const auto fill = FillPacket(value, destination, BufferBytes);
    std::vector<std::uint32_t> words(fill.begin(), fill.end());
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code);
    const auto input = BufferDescriptor(Input, BufferBytes);
    const auto output = BufferDescriptor(Output, BufferBytes);
    const std::vector<std::uint32_t> commands{
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u),
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 8u << 1u,
        0xc0087600u, 0x240u, input[0], input[1], input[2], input[3], output[0], output[1], output[2], output[3],
        0xc0031500u, 1u, 1u, 1u, 0x8041u,
    };
    words.insert(words.end(), commands.begin(), commands.end());
    return words;
}

std::vector<std::uint32_t> TableFills(const void* target, std::uint32_t value) {
    const auto targetFill = FillPacket(value, target, BufferBytes);
    const auto tableFill = FillPacket(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(target)), Table, 4u);
    std::vector<std::uint32_t> words(targetFill.begin(), targetFill.end());
    words.insert(words.end(), tableFill.begin(), tableFill.end());
    return words;
}

std::vector<std::uint32_t> TableDispatch(const void* code) {
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code);
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(Table);
    const auto output = BufferDescriptor(Output, BufferBytes);
    const std::vector<std::uint32_t> commands{
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u),
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 8u << 1u,
        0xc0087600u, 0x240u, static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u), 0u, 0u, output[0], output[1], output[2], output[3],
        0xc0031500u, 1u, 1u, 1u, 0x8041u,
    };
    return commands;
}

void Submit(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "1");
#else
    setenv("APS5_PIPELINED_DRAWS", "1", 1);
#endif
    try {
        {
            const auto probe = OpenVulkanTestDevice();
            if (!probe) return VulkanTestSkipped;
        }
        const char* dispatchSwitch = std::getenv("APS5_PIPELINE_DISPATCH");
        const bool pipelinedDispatch = dispatchSwitch != nullptr && dispatchSwitch[0] != '\0' && dispatchSwitch[0] != '0';

        Shader shader{};
        shader.file_header = 0x34333231;
        shader.version = 0x18;
        shader.header_size = sizeof(Shader);
        shader.shader_size = sizeof(Code);
        shader.code = Code.data();
        AgcDriverRegisterShader_nid_postfix(&shader);
        Shader scalarShader{};
        scalarShader.file_header = 0x34333231;
        scalarShader.version = 0x18;
        scalarShader.header_size = sizeof(Shader);
        scalarShader.shader_size = sizeof(ScalarCode);
        scalarShader.code = ScalarCode.data();
        AgcDriverRegisterShader_nid_postfix(&scalarShader);
        Shader tableShader{};
        tableShader.file_header = 0x34333231;
        tableShader.version = 0x18;
        tableShader.header_size = sizeof(Shader);
        tableShader.shader_size = sizeof(TableCode);
        tableShader.code = TableCode.data();
        AgcDriverRegisterShader_nid_postfix(&tableShader);

        Input = NewBlock();
        Output = NewBlock();
        Scratch = NewBlock();
        Table = NewBlock();
        TargetA = NewBlock();
        TargetB = NewBlock();
        check((reinterpret_cast<std::uintptr_t>(TargetA) >> 32u) == (reinterpret_cast<std::uintptr_t>(TargetB) >> 32u), "the two targets do not share their high address dword");
        const auto initialDescriptor = BufferDescriptor(TargetA, BufferBytes);
        std::copy(initialDescriptor.begin(), initialDescriptor.end(), Table);
        const std::array<std::uint32_t, 5> values{0x0a0b0c0du, 0x11112222u, 0x33334444u, 0x55556666u, 0x77778888u};
        for (std::size_t round = 0; round < values.size(); ++round) {
            const auto value = values[round];
            const auto drainsBefore = DrawPipeline::DrainRequestsByOpcode(DispatchOpcode);
            auto words = Round(Input, value, Code.data());
            Submit(words);
            AgcDriverWaitIdle_nid_postfix();
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output), BufferBytes);
            const auto drains = DrawPipeline::DrainRequestsByOpcode(DispatchOpcode) - drainsBefore;
            for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
                check(Input[lane] == value, "the immediate fill did not land in lane " + std::to_string(lane));
                check(Output[lane] == value + 1u, "round " + std::to_string(round) + ": lane " + std::to_string(lane) + " of the output is " + std::to_string(Output[lane]) + ", expected " + std::to_string(value + 1u) + " (the dispatch did not see the fill before it)");
            }
            std::printf("pipelined dispatch: input fill round %zu, fill 0x%08x, dispatch drains %llu\n", round, value, static_cast<unsigned long long>(drains));
            if (!pipelinedDispatch) check(drains >= 1, "a dispatch did not drain the pipeline, and APS5_PIPELINE_DISPATCH is off");
            if (pipelinedDispatch) check(drains <= 1, "a dispatch drained the pipeline more than once (" + std::to_string(drains) + ")");
        }
        for (std::size_t round = 0; round < 3; ++round) {
            const auto value = 0xc0de0000u + static_cast<std::uint32_t>(round);
            const auto drainsBefore = DrawPipeline::DrainRequestsByOpcode(DispatchOpcode);
            auto words = Round(Scratch, value, Code.data());
            Submit(words);
            AgcDriverWaitIdle_nid_postfix();
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Scratch), BufferBytes);
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output), BufferBytes);
            const auto drains = DrawPipeline::DrainRequestsByOpcode(DispatchOpcode) - drainsBefore;
            for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
                check(Scratch[lane] == value, "the scratch fill did not land in lane " + std::to_string(lane));
                check(Output[lane] == Input[lane] + 1u, "scratch round " + std::to_string(round) + ": lane " + std::to_string(lane) + " of the output is " + std::to_string(Output[lane]) + ", expected " + std::to_string(Input[lane] + 1u));
            }
            std::printf("pipelined dispatch: scratch fill round %zu, fill 0x%08x, dispatch drains %llu\n", round, value, static_cast<unsigned long long>(drains));
            if (!pipelinedDispatch) check(drains >= 1, "a dispatch did not drain the pipeline, and APS5_PIPELINE_DISPATCH is off");
            if (pipelinedDispatch) check(drains == 0, "a dispatch that reads nothing pending drained the pipeline (" + std::to_string(drains) + ")");
        }
        const std::array<std::uint32_t, 4> tableValues{0x6a6a0001u, 0x6b6b0002u, 0x6c6c0003u, 0x6d6d0004u};
        for (std::size_t round = 0; round < tableValues.size(); ++round) {
            auto* target = round % 2 == 0 ? TargetA : TargetB;
            const auto value = tableValues[round];
            std::atomic<bool> released{false};
            struct Release {
                std::atomic<bool>& flag;
                ~Release() { flag.store(true, std::memory_order_release); }
            } release{released};
            WaitUntil([] { return DrawPipeline::QueuedItems() == 0; }, "the pipeline to empty before descriptor round " + std::to_string(round));
            const auto drainsBefore = DrawPipeline::DrainRequestsByOpcode(DispatchOpcode);
            DrawPipeline::Queue0().Enqueue([&released] {
                while (!released.load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }, {});
            auto fills = TableFills(target, value);
            Submit(fills);
            WaitUntil([] { return DrawPipeline::QueuedItems() >= 4; }, "the target and table fills queued behind the gate");
            auto dispatch = TableDispatch(TableCode.data());
            Submit(dispatch);
            WaitUntil([&drainsBefore] { return DrawPipeline::QueuedItems() >= 5 || DrawPipeline::DrainRequestsByOpcode(DispatchOpcode) > drainsBefore; }, "the dispatch handled while the table fill is pending");
            released.store(true, std::memory_order_release);
            AgcDriverWaitIdle_nid_postfix();
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output), BufferBytes);
            const auto drains = DrawPipeline::DrainRequestsByOpcode(DispatchOpcode) - drainsBefore;
            for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
                check(Output[lane] == value + 1u, "descriptor round " + std::to_string(round) + ": lane " + std::to_string(lane) + " of the output is " + std::to_string(Output[lane]) + ", expected " + std::to_string(value + 1u) + " (the dispatch read the buffer the table named before the pending fill of the table)");
            }
            std::printf("pipelined dispatch: descriptor round %zu, fill 0x%08x, dispatch drains %llu\n", round, value, static_cast<unsigned long long>(drains));
            if (!pipelinedDispatch) check(drains >= 1, "a dispatch did not drain the pipeline, and APS5_PIPELINE_DISPATCH is off");
            if (pipelinedDispatch) check(drains == 1, "a dispatch whose table entry is pending a write drained " + std::to_string(drains) + " times, expected once");
        }
        const std::array<std::uint32_t, 5> scalarValues{0x13572468u, 0x2468ace0u, 0x0f1e2d3cu, 0x9abcdef0u, 0x31415926u};
        for (std::size_t round = 0; round < scalarValues.size(); ++round) {
            const auto value = scalarValues[round];
            const auto drainsBefore = DrawPipeline::DrainRequestsByOpcode(DispatchOpcode);
            auto words = Round(Input, value, ScalarCode.data());
            Submit(words);
            AgcDriverWaitIdle_nid_postfix();
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output), BufferBytes);
            const auto drains = DrawPipeline::DrainRequestsByOpcode(DispatchOpcode) - drainsBefore;
            for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
                check(Output[lane] == value + 1u, "scalar round " + std::to_string(round) + ": lane " + std::to_string(lane) + " of the output is " + std::to_string(Output[lane]) + ", expected " + std::to_string(value + 1u) + " (a stale input)");
            }
            std::printf("pipelined dispatch: scalar read round %zu, fill 0x%08x, dispatch drains %llu\n", round, value, static_cast<unsigned long long>(drains));
            if (!pipelinedDispatch) check(drains >= 1, "a dispatch did not drain the pipeline, and APS5_PIPELINE_DISPATCH is off");
            if (pipelinedDispatch) check(drains <= 1, "a dispatch drained the pipeline more than once (" + std::to_string(drains) + ")");
        }
        LibcRunShutdown_nid_postfix();
        std::puts("pipelined dispatch tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}

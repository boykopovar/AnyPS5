#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "DepthFastClearHarness.hpp"
#include "TargetAddressWindow.hpp"
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
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::DriverDetail::DrawPipeline;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t BufferBytes = Lanes * 4u;
constexpr std::uint32_t DispatchOpcode = 0x15;

constexpr std::size_t BlockBytes = 65536;

std::uint32_t* NewBlock(bool gpu = false) {
#ifdef _WIN32
    auto* block = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    if (block == nullptr) throw std::runtime_error("pipelined dispatch: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, gpu);
    return block;
}

std::uint32_t* Input = nullptr;
std::uint32_t* Output = nullptr;
std::uint32_t* Scratch = nullptr;
std::uint32_t* Table = nullptr;
std::uint32_t* TargetA = nullptr;
std::uint32_t* TargetB = nullptr;

void NewTargetPair() {
    constexpr std::size_t regionBytes = 3u * BlockBytes;
#ifdef _WIN32
    auto* region = static_cast<std::uint32_t*>(VirtualAlloc(nullptr, regionBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* region = static_cast<std::uint32_t*>(std::aligned_alloc(BlockBytes, regionBytes));
#endif
    if (region == nullptr) throw std::runtime_error("pipelined dispatch: cannot allocate contiguous target blocks");
    std::memset(region, 0, regionBytes);
    GuestAllocations::Mutation().Add(region, regionBytes, true, true, false);
    const auto indices = AgcDriver::Tests::TargetAddressWindowPair(reinterpret_cast<std::uintptr_t>(region), BlockBytes);
    constexpr auto wordsPerBlock = BlockBytes / sizeof(std::uint32_t);
    TargetA = region + indices[0] * wordsPerBlock;
    TargetB = region + indices[1] * wordsPerBlock;
}

alignas(256) constexpr std::array<std::uint32_t, 8> Code{0x34020082u, 0xe0301000u, 0x80000201u, 0xbf8c3f70u, 0x4a060481u, 0xe0701000u, 0x80010301u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 9> ScalarCode{0xf4000200u, 0xfa000000u, 0xbf8cc07fu, 0x34020082u, 0x7e040208u, 0x4a040481u, 0xe0701000u, 0x80010201u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 11> TableCode{0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0x34020082u, 0xe0301000u, 0x80020201u, 0xbf8c3f70u, 0x4a060481u, 0xe0701000u, 0x80010301u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 16> HtileMaskCode{0xd7460004u, 0x04010c0cu, 0xe00c2000u, 0x80000004u, 0xbf8c3f70u, 0xd7710000u, 0x00120008u, 0xd7710001u, 0x00160209u, 0xd7710002u, 0x001a040au, 0xd7710003u, 0x001e060bu, 0xe01c2000u, 0x80000004u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 23> KeyFillCode{
    0xbfa00001u, 0xd7460004u, 0x04010c09u, 0x7e000204u, 0x7e020205u, 0x7e040206u, 0x7e060207u, 0x7da80808u,
    0xbf88000du, 0x360a0883u, 0xbe84047eu, 0xbe86047eu, 0x7e100505u, 0x7da40a08u, 0xbefc0308u, 0x7e0c8700u,
    0x8a867e06u, 0xbefe0406u, 0xbf85fff9u, 0xbefe0404u, 0xe0102000u, 0x80000604u, 0xbf810000u};

alignas(256) constexpr std::array<std::uint32_t, 15> PlaneClearCode{0xd7460004u, 0x0401060cu, 0xf4280204u, 0xfa000000u, 0xd7460005u, 0x0405060du, 0x7e0c020eu, 0xbf8cc07fu, 0x7e000208u, 0x7e020209u, 0x7e04020au, 0x7e06020bu, 0xf0200f28u, 0x00000004u, 0xbf810000u};

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

std::vector<std::uint32_t> TableDispatch(const void* code, std::uint32_t unusedWord = 0u) {
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code);
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(Table);
    const auto output = BufferDescriptor(Output, BufferBytes);
    const std::vector<std::uint32_t> commands{
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u),
        0xc0037600u, 0x207u, Lanes, 1u, 1u,
        0xc0017600u, 0x213u, 8u << 1u,
        0xc0087600u, 0x240u, static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u), unusedWord, 0u, output[0], output[1], output[2], output[3],
        0xc0031500u, 1u, 1u, 1u, 0x8041u,
    };
    return commands;
}

std::vector<std::uint32_t> ComputeDispatch(const void* code, const std::array<std::uint32_t, 3>& threads, std::uint32_t resources, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& groups) {
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code);
    std::vector<std::uint32_t> words{
        0xc0027600u, 0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u), static_cast<std::uint32_t>(codeAddress >> 40u),
        0xc0037600u, 0x207u, threads[0], threads[1], threads[2],
        0xc0017600u, 0x213u, resources,
        0xc0007600u | (static_cast<std::uint32_t>(userData.size()) << 16u), 0x240u,
    };
    words.insert(words.end(), userData.begin(), userData.end());
    words.insert(words.end(), {0xc0031500u, groups[0], groups[1], groups[2], 0x41u});
    return words;
}

std::array<std::uint32_t, 8> PlaneDescriptor(const void* plane) {
    constexpr std::uint32_t format32Float = 22;
    constexpr std::uint32_t type2DArray = 13;
    constexpr auto width = DepthFastClearHarness::Width;
    constexpr auto height = DepthFastClearHarness::Height;
    const auto address = reinterpret_cast<std::uintptr_t>(plane);
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format32Float << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u),
        0xfacu | (type2DArray << 28u),
        0u, 0u, 0u, 0u,
    };
}

Shader ComputeShader(const void* code, std::uint32_t bytes) {
    Shader shader{};
    shader.file_header = 0x34333231;
    shader.version = 0x18;
    shader.header_size = sizeof(Shader);
    shader.shader_size = bytes;
    shader.code = code;
    return shader;
}

void Submit(std::vector<std::uint32_t>& words) {
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "submission failed");
}

void Settle(std::span<const std::uint32_t> words) {
    std::vector<std::uint32_t> copy(words.begin(), words.end());
    Submit(copy);
    AgcDriverWaitIdle_nid_postfix();
}

std::uint64_t DrainRequests() {
    std::uint64_t total = 0;
    for (std::size_t reason = 0; reason < static_cast<std::size_t>(DrawPipeline::DrainReason::Count); ++reason) total += DrawPipeline::DrainRequestsByReason(static_cast<DrawPipeline::DrainReason>(reason));
    return total;
}

struct Handling {
    std::uint64_t drains = 0;
    std::uint64_t packetDrains = 0;
    bool queued = false;
};

Handling SubmitBehindHeldCommitter(std::vector<std::uint32_t>& words, const void* written, std::size_t bytes, const std::string& what) {
    WaitUntil([] { return DrawPipeline::QueuedItems() == 0; }, "the pipeline to empty before " + what);
    const auto released = std::make_shared<std::atomic<bool>>(false);
    struct Release {
        std::atomic<bool>& flag;
        ~Release() { flag.store(true, std::memory_order_release); }
    } release{*released};
    const auto drainsBefore = DrainRequests();
    const auto packetDrainsBefore = DrawPipeline::DrainRequestsByOpcode(DispatchOpcode);
    DrawPipeline::Queue0().Enqueue([released] {
        while (!released->load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }, {});
    Submit(words);
    const auto address = reinterpret_cast<std::uintptr_t>(written);
    Handling handling;
    WaitUntil([&] {
        handling = {DrainRequests() - drainsBefore, DrawPipeline::DrainRequestsByOpcode(DispatchOpcode) - packetDrainsBefore, DrawPipeline::Queue0().Overlaps(address, bytes)};
        return handling.drains != 0 || handling.queued;
    }, what + " to drain the held draw pipeline or queue behind it");
    released->store(true, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    return handling;
}

std::string Describe(const Handling& handling) {
    return std::to_string(handling.drains) + " drains (" + std::to_string(handling.packetDrains) + " at the dispatch packet), " + (handling.queued ? "queued" : "not queued");
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
        const bool dispatchMisses = pipelinedDispatch && std::getenv("APS5_PIPELINE_DISPATCH_MISSES") != nullptr;

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
        NewTargetPair();
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
            const auto captureDrainsBefore = DrawPipeline::DrainRequestsByReason(DrawPipeline::DrainReason::Capture);
            DrawPipeline::Queue0().Enqueue([&released] {
                while (!released.load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }, {});
            auto fills = TableFills(target, value);
            Submit(fills);
            WaitUntil([] { return DrawPipeline::QueuedItems() >= 4; }, "the target and table fills queued behind the gate");
            auto dispatch = TableDispatch(TableCode.data());
            Submit(dispatch);
            WaitUntil([&drainsBefore, &captureDrainsBefore] { return DrawPipeline::QueuedItems() >= 5 || DrawPipeline::DrainRequestsByOpcode(DispatchOpcode) > drainsBefore || DrawPipeline::DrainRequestsByReason(DrawPipeline::DrainReason::Capture) > captureDrainsBefore; }, "the dispatch handled while the table fill is pending");
            released.store(true, std::memory_order_release);
            AgcDriverWaitIdle_nid_postfix();
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output), BufferBytes);
            const auto drains = DrawPipeline::DrainRequestsByOpcode(DispatchOpcode) - drainsBefore + DrawPipeline::DrainRequestsByReason(DrawPipeline::DrainReason::Capture) - captureDrainsBefore;
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
        std::uint32_t missKey = 0;
        const std::array<std::uint32_t, 3> missValues{0x7a7a0001u, 0x7b7b0002u, 0x7c7c0003u};
        for (std::size_t round = 0; round < missValues.size(); ++round) {
            const auto current = missValues[round];
            const auto fresh = current ^ 0x00ff00ffu;
            auto targets = TableFills(TargetA, current);
            const auto freshFill = FillPacket(fresh, TargetB, BufferBytes);
            targets.insert(targets.end(), freshFill.begin(), freshFill.end());
            Settle(targets);

            const auto scratchFill = FillPacket(current, Scratch, BufferBytes);
            std::vector<std::uint32_t> clear(scratchFill.begin(), scratchFill.end());
            const auto clearDispatch = TableDispatch(TableCode.data(), ++missKey);
            clear.insert(clear.end(), clearDispatch.begin(), clearDispatch.end());
            const auto clearHandling = SubmitBehindHeldCommitter(clear, Output, BufferBytes, "a missed dispatch whose reads are clear of the queued write");
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Scratch), BufferBytes);
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output), BufferBytes);
            for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
                check(Scratch[lane] == current, "missed round " + std::to_string(round) + ": the queued scratch fill did not land in lane " + std::to_string(lane));
                check(Output[lane] == current + 1u, "missed round " + std::to_string(round) + ": lane " + std::to_string(lane) + " of the output is " + std::to_string(Output[lane]) + ", expected " + std::to_string(current + 1u) + " (the dispatch whose reads are clear of the queued write)");
            }
            std::printf("pipelined dispatch: missed dispatch round %zu, reads clear of the queued write: %s\n", round, Describe(clearHandling).c_str());
            if (dispatchMisses) check(clearHandling.queued && clearHandling.drains == 0, "a missed plain dispatch whose reads are clear of the queued write did not queue behind it: " + Describe(clearHandling));
            if (!dispatchMisses) check(!clearHandling.queued && clearHandling.drains >= 1, "a missed dispatch did not drain the pipeline, and APS5_PIPELINE_DISPATCH_MISSES is off: " + Describe(clearHandling));

            const auto tableFill = FillPacket(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(TargetB)), Table, 4u);
            std::vector<std::uint32_t> overlap(tableFill.begin(), tableFill.end());
            const auto overlapDispatch = TableDispatch(TableCode.data(), ++missKey);
            overlap.insert(overlap.end(), overlapDispatch.begin(), overlapDispatch.end());
            const auto overlapHandling = SubmitBehindHeldCommitter(overlap, Output, BufferBytes, "a missed dispatch whose table has a queued write");
            AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output), BufferBytes);
            for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
                check(Output[lane] == fresh + 1u, "missed round " + std::to_string(round) + ": lane " + std::to_string(lane) + " of the output is " + std::to_string(Output[lane]) + ", expected " + std::to_string(fresh + 1u) + " (the dispatch read its table before the queued write of the table)");
            }
            std::printf("pipelined dispatch: missed dispatch round %zu, table under a queued write: %s\n", round, Describe(overlapHandling).c_str());
            check(!overlapHandling.queued && overlapHandling.drains >= 1, "a missed plain dispatch whose table has a queued write did not drain the pipeline: " + Describe(overlapHandling));
        }

        Shader htileShader = ComputeShader(HtileMaskCode.data(), sizeof(HtileMaskCode));
        AgcDriverRegisterShader_nid_postfix(&htileShader);
        Shader keyFillShader = ComputeShader(KeyFillCode.data(), sizeof(KeyFillCode));
        AgcDriverRegisterShader_nid_postfix(&keyFillShader);
        Shader planeClearShader = ComputeShader(PlaneClearCode.data(), sizeof(PlaneClearCode));
        AgcDriverRegisterShader_nid_postfix(&planeClearShader);
        const auto expectInline = [&](std::size_t round) { return round == 0 || dispatchMisses || !pipelinedDispatch; };

        auto* htile = NewBlock();
        constexpr std::uint32_t htileRecords = 64;
        constexpr std::uint32_t htileValue = 0xfffc0000u;
        constexpr std::uint32_t htileKeep = 0x0000fff0u;
        const auto htileAddress = reinterpret_cast<std::uintptr_t>(htile);
        const std::array<std::uint32_t, 12> htileData{static_cast<std::uint32_t>(htileAddress), (16u << 16u) | (static_cast<std::uint32_t>(htileAddress >> 32u) & 0xffffu), htileRecords, 0x0004bfacu, htileValue, htileValue, htileValue, htileValue, htileKeep, htileKeep, htileKeep, htileKeep};
        const std::array<std::uint32_t, 3> htileSeeds{0x12345678u, 0x9abcdef0u, 0x0f1e2d3cu};
        for (std::size_t round = 0; round < htileSeeds.size(); ++round) {
            const auto seed = htileSeeds[round];
            Settle(FillPacket(seed, htile, htileRecords * 32u));
            auto words = ComputeDispatch(HtileMaskCode.data(), {64u, 1u, 1u}, (12u << 1u) | 0x80u, htileData, {1u, 1u, 1u});
            const auto handling = SubmitBehindHeldCommitter(words, htile, htileRecords * 16u, "an HTILE mask dispatch");
            AgcDriver::GuestMemory::FlushGpuWrites(htileAddress, htileRecords * 32u);
            for (std::uint32_t word = 0; word < htileRecords * 8u; ++word) {
                const auto expected = word < htileRecords * 4u ? (seed & htileKeep) | htileValue : seed;
                check(htile[word] == expected, "HTILE round " + std::to_string(round) + ": word " + std::to_string(word) + " is " + std::to_string(htile[word]) + ", expected " + std::to_string(expected));
            }
            std::printf("pipelined dispatch: HTILE mask round %zu: %s\n", round, Describe(handling).c_str());
            if (expectInline(round)) check(!handling.queued && handling.drains >= 1, "an HTILE mask dispatch did not keep the inline path in round " + std::to_string(round) + ": " + Describe(handling));
        }

        auto* keys = NewBlock();
        constexpr std::uint32_t keyDwords = 1024;
        constexpr std::uint32_t keyCode = 0x40404040u;
        const auto keysAddress = reinterpret_cast<std::uintptr_t>(keys);
        const std::array<std::uint32_t, 9> keyData{static_cast<std::uint32_t>(keysAddress), (static_cast<std::uint32_t>(keysAddress >> 32u) & 0xffffu) | (4u << 16u), keyDwords, 0x00014004u, keyCode, keyCode, keyCode, keyCode, keyDwords};
        for (std::size_t round = 0; round < 3; ++round) {
            Settle(FillPacket(0u, keys, keyDwords * 8u));
            auto words = ComputeDispatch(KeyFillCode.data(), {64u, 1u, 1u}, (9u << 1u) | 0x80u, keyData, {keyDwords / 64u, 1u, 1u});
            const auto handling = SubmitBehindHeldCommitter(words, keys, keyDwords * 4u, "a key fill dispatch");
            AgcDriver::GuestMemory::FlushGpuWrites(keysAddress, keyDwords * 8u);
            for (std::uint32_t word = 0; word < keyDwords * 2u; ++word) {
                const auto expected = word < keyDwords ? keyCode : 0u;
                check(keys[word] == expected, "key fill round " + std::to_string(round) + ": dword " + std::to_string(word) + " is " + std::to_string(keys[word]) + ", expected " + std::to_string(expected));
            }
            std::printf("pipelined dispatch: key fill round %zu: %s\n", round, Describe(handling).c_str());
            if (expectInline(round)) check(!handling.queued && handling.drains >= 1, "a key fill dispatch did not keep the inline path in round " + std::to_string(round) + ": " + Describe(handling));
        }

        auto* plane = NewBlock(true);
        auto* color = NewBlock(true);
        const auto planeAddress = reinterpret_cast<std::uintptr_t>(plane);
        Settle(FillPacket(0x3f800000u, color, 16u));
        auto* const driverRecorder = AgcDriver::Graphics::Recorder::Active();
        check(driverRecorder != nullptr, "the driver's device has no active recorder");
        const auto depthDevice = OpenVulkanTestDevice();
        check(depthDevice != nullptr, "the depth surface device did not open");
        AgcDriver::VulkanDeviceTestAccess::ContextOf(*depthDevice).recorder->Activate();
        const auto depthShaders = DepthFastClearHarness::Compile(*depthDevice);
        const DepthFastClearHarness::Surface surface{planeAddress, 0, 0, false, VK_FORMAT_D32_SFLOAT};
        check(DepthFastClearHarness::Draw(*depthDevice, depthShaders, surface, {.depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}) == DepthFastClearHarness::Covered, "the depth draw did not cover the plane");
        driverRecorder->Activate();
        const auto planeView = PlaneDescriptor(plane);
        const auto colorBuffer = BufferDescriptor(color, 16u);
        std::array<std::uint32_t, 12> planeData{};
        std::copy(planeView.begin(), planeView.end(), planeData.begin());
        std::copy(colorBuffer.begin(), colorBuffer.end(), planeData.begin() + planeView.size());
        const auto planeBytes = static_cast<std::size_t>(AgcDriver::Graphics::DepthSliceBytes({DepthFastClearHarness::Width, DepthFastClearHarness::Height}, 4u));
        for (std::size_t round = 0; round < 3; ++round) {
            check(AgcDriver::Graphics::DepthSurfaceAt(planeAddress), "depth plane round " + std::to_string(round) + ": the plane is no longer a depth surface");
            const auto before = AgcDriver::Graphics::DepthClearCounts();
            auto words = ComputeDispatch(PlaneClearCode.data(), {8u, 8u, 1u}, (12u << 1u) | 0x80u | 0x100u | 0x200u | (1u << 11u), planeData, {DepthFastClearHarness::Width / 8u, DepthFastClearHarness::Height / 8u, 1u});
            const auto handling = SubmitBehindHeldCommitter(words, plane, planeBytes, "a depth plane clear dispatch");
            const auto matched = AgcDriver::Graphics::DepthClearCounts().matched - before.matched;
            std::printf("pipelined dispatch: depth plane clear round %zu: %s, %llu depth plane clears matched\n", round, Describe(handling).c_str(), static_cast<unsigned long long>(matched));
            if (expectInline(round)) {
                check(!handling.queued && handling.drains >= 1, "a depth plane clear dispatch did not keep the inline path in round " + std::to_string(round) + ": " + Describe(handling));
                check(matched == 1u, "depth plane round " + std::to_string(round) + ": the inline path matched " + std::to_string(matched) + " depth plane clears, expected 1");
            }
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

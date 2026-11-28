#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::uint32_t Lanes = 64;
constexpr std::uint32_t Groups = 7;
constexpr std::uint32_t UserData = 14;
constexpr std::uint32_t TableOffset = 0x270;
constexpr std::uint32_t MaxCalls = 8;
constexpr std::uint32_t SetShReg = 0x76u;
constexpr std::uint32_t DispatchDirect = 0x15u;
constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t CallerOffset = 0x0000;
constexpr std::size_t CalleeOffset = 0x1000;
constexpr std::size_t InputOffset = 0x2000;
constexpr std::size_t OutputOffset = 0x3000;
constexpr std::size_t TableBase = 0x4000;
constexpr std::size_t UserBlockOffset = 0x5000;

struct Callee {
    std::vector<std::uint32_t> code;
};

const std::array<Callee, 3>& Callees() {
    static const std::array<Callee, 3> callees{{
        {{0xbf068000u, 0x4a020285u, 0xbe80200eu}},
        {{0x4a0202c0u, 0xbe80200eu}},
        {{0x34020281u, 0xbe80200eu}},
    }};
    return callees;
}

std::vector<std::uint32_t> LoopCaller() {
    std::vector<std::uint32_t> code{0x8f15860eu, 0x4a060015u, 0x34040682u, 0xe0301000u, 0x80000102u, 0xbf8c3f70u, 0xbeaa047eu, 0xbe960380u, 0x36080087u, 0x4a080881u};
    const auto loop = static_cast<std::int32_t>(code.size());
    code.insert(code.end(), {0x8f148416u, 0xf4080406u, 0xfa000270u, 0xbf8cc07fu, 0xf4280208u, 0x28000000u, 0xbf8cc07fu, 0xbe8e0308u, 0xbe8f0309u, 0xbe8e210eu, 0x80168116u, 0x7da20816u});
    code.push_back(0xbf890000u | (static_cast<std::uint32_t>(loop - static_cast<std::int32_t>(code.size() + 1u)) & 0xffffu));
    code.insert(code.end(), {0xbefe042au, 0xe0701000u, 0x80010102u, 0xbf810000u});
    return code;
}

std::uint32_t Served(std::uint32_t value, std::uint32_t callee) {
    switch (callee % 3u) {
        case 0: return value + 5u;
        case 1: return value + 64u;
        default: return value << 1u;
    }
}

std::uint32_t Expected(std::uint32_t input, std::uint32_t index) {
    auto value = input;
    const auto calls = (index % Lanes & (MaxCalls - 1u)) + 1u;
    for (std::uint32_t call = 0; call < calls; ++call) value = Served(value, call);
    return value;
}

std::uint64_t Address(const void* data) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data)); }

std::uint8_t* NewBlock() {
#ifdef _WIN32
    auto* block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
    Require(block != nullptr, "split table call driver: cannot allocate a guest block");
    std::memset(block, 0, BlockBytes);
    GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    return block;
}

std::array<std::uint32_t, 4> BufferDescriptor(std::uint64_t address, std::uint32_t bytes) {
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

void SetRegister(std::vector<std::uint32_t>& words, std::uint32_t offset, std::uint32_t value) {
    words.push_back(0xc0010000u | (SetShReg << 8u));
    words.push_back(offset);
    words.push_back(value);
}

void SubmitDispatch(std::uint64_t code, const std::vector<std::uint32_t>& userData) {
    std::vector<std::uint32_t> packet;
    SetRegister(packet, 0x20c, static_cast<std::uint32_t>(code >> 8u));
    SetRegister(packet, 0x20d, static_cast<std::uint32_t>(code >> 40u) & 0xffu);
    SetRegister(packet, 0x212, 13u);
    SetRegister(packet, 0x207, Lanes);
    SetRegister(packet, 0x208, 1u);
    SetRegister(packet, 0x209, 1u);
    SetRegister(packet, 0x213, (static_cast<std::uint32_t>(userData.size()) << 1u) | (1u << 7u));
    for (std::uint32_t i = 0; i < userData.size(); ++i) SetRegister(packet, 0x240 + i, userData[i]);
    packet.insert(packet.end(), {0xc0000000u | (3u << 16u) | (DispatchDirect << 8u), Groups, 1u, 1u, 0x41u});
    Packet submission{packet.data(), static_cast<std::uint32_t>(packet.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&submission) == 0, "split table call driver: the dispatch submission failed");
}

void Dispatch(std::uint64_t code, const std::vector<std::uint32_t>& userData) {
    SubmitDispatch(code, userData);
    AgcDriverWaitIdle_nid_postfix();
}

void DispatchBehindHeldCommitter(std::uint64_t code, const std::vector<std::uint32_t>& userData, std::uint64_t* table, std::uint64_t target) {
    using AgcDriver::DriverDetail::DrawPipeline;
    const auto released = std::make_shared<std::atomic<bool>>(false);
    struct Release {
        std::shared_ptr<std::atomic<bool>> flag;
        ~Release() { flag->store(true, std::memory_order_release); }
    } release{released};
    const auto drainsBefore = DrawPipeline::DrainRequestsByOpcode(DispatchDirect);
    const auto tableAddress = Address(table);
    DrawPipeline::Queue0().Enqueue([released, table, target] {
        while (!released->load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        for (std::uint32_t entry = 2; entry < Lanes; entry += 3u) table[entry * 2u] = target;
    }, {{tableAddress, tableAddress + Lanes * 16u}});
    SubmitDispatch(code, userData);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (DrawPipeline::DrainRequestsByOpcode(DispatchDirect) == drainsBefore && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool drained = DrawPipeline::DrainRequestsByOpcode(DispatchDirect) != drainsBefore;
    released->store(true, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    Require(drained, "split table call driver: the split dispatch recorded without draining the draw pipeline queued ahead of it");
}

}

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc == 2 ? argv[1] : "";
        Require(argc <= 2 && (argc == 1 || mode == "--cpu-rounds" || mode == "--queued"), "usage: [--cpu-rounds|--queued]");
        const bool queued = mode == "--queued";
        setenv("APS5_SPLIT_TABLE_CALLS", "1", 1);
        if (mode == "--cpu-rounds") setenv("APS5_SPLIT_CPU_ROUNDS", "1", 1);
        if (queued) {
            setenv("APS5_PIPELINED_DRAWS", "1", 1);
            for (const char* name : {"APS5_NO_PIPELINE_DISPATCH", "APS5_DRAW_DRAIN", "APS5_DRAIN_ALL", "APS5_LOCKED_DRAW_PREPARE"}) unsetenv(name);
        }
        if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
        if (queued) {
            Require(AgcDriver::DriverDetail::DrawPipeline::Depth() != 0, "split table call driver: the draw pipeline is off");
            Require(!AgcDriver::Graphics::RuntimeSwitches::Set(AgcDriver::Graphics::RuntimeSwitch::NoPipelineDispatch), "split table call driver: pipelined dispatch is off");
        }
        auto* block = NewBlock();
        const auto caller = LoopCaller();
        std::memcpy(block + CallerOffset, caller.data(), caller.size() * sizeof(std::uint32_t));
        std::array<std::uint64_t, 3> targets{};
        const std::array<std::uint32_t, 2> staleCode{0x4a020290u, 0xbe80200eu};
        std::memcpy(block + CalleeOffset + Callees().size() * 0x100u, staleCode.data(), sizeof(staleCode));
        const auto stale = Address(block + CalleeOffset + Callees().size() * 0x100u);
        for (std::size_t index = 0; index < Callees().size(); ++index) {
            const auto offset = CalleeOffset + index * 0x100u;
            std::memcpy(block + offset, Callees()[index].code.data(), Callees()[index].code.size() * sizeof(std::uint32_t));
            targets[index] = Address(block + offset);
        }
        auto* input = reinterpret_cast<std::uint32_t*>(block + InputOffset);
        auto* output = reinterpret_cast<std::uint32_t*>(block + OutputOffset);
        for (std::uint32_t index = 0; index < Lanes * Groups; ++index) {
            input[index] = 0x9e3779b9u * (index + 1u);
            output[index] = 0xdeadbeefu;
        }
        auto* table = reinterpret_cast<std::uint64_t*>(block + TableBase);
        for (std::uint32_t entry = 0; entry < Lanes; ++entry) table[entry * 2u] = queued && entry % 3u == 2u ? stale : targets[entry % 3u];
        const auto tableDescriptor = BufferDescriptor(Address(table), Lanes * 16u);
        std::memcpy(block + UserBlockOffset + TableOffset, tableDescriptor.data(), sizeof(tableDescriptor));
        std::vector<std::uint32_t> userData(UserData, 0u);
        const auto inputDescriptor = BufferDescriptor(Address(input), Lanes * Groups * 4u);
        const auto outputDescriptor = BufferDescriptor(Address(output), Lanes * Groups * 4u);
        std::copy(inputDescriptor.begin(), inputDescriptor.end(), userData.begin());
        std::copy(outputDescriptor.begin(), outputDescriptor.end(), userData.begin() + 4);
        const auto userBlock = Address(block + UserBlockOffset);
        userData[12] = static_cast<std::uint32_t>(userBlock);
        userData[13] = static_cast<std::uint32_t>(userBlock >> 32u);
        if (queued) DispatchBehindHeldCommitter(Address(block + CallerOffset), userData, table, targets[2]);
        else Dispatch(Address(block + CallerOffset), userData);
        AgcDriver::GuestMemory::Read(Address(output), std::as_writable_bytes(std::span(output, Lanes * Groups)));
        for (std::uint32_t index = 0; index < Lanes * Groups; ++index) {
            if (output[index] != Expected(input[index], index)) {
                char text[160];
                std::snprintf(text, sizeof(text), "split table call driver: invocation %u holds 0x%08x, expected 0x%08x", index, output[index], Expected(input[index], index));
                throw std::runtime_error(text);
            }
        }
        AgcDriverShutdown_nid_postfix();
        std::puts("split table call driver test passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}

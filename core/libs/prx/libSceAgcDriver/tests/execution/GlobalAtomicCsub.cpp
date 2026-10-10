#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using Testing::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
constexpr std::uint32_t Written = 3;
constexpr std::uint32_t SharedOffset = Threads * 4u;
constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t SharedPartial = 0x800186a0u;
constexpr std::uint32_t SharedExhausted = 1000u;
constexpr std::size_t BlockBytes = 4096;
constexpr std::uint8_t Fill = 0xcd;
alignas(256) std::array<std::uint32_t, Threads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 25> Code{
    0x34020084, 0x34060082, 0xe0301000, 0x80000a01, 0xe0301004, 0x80000b01, 0xe0301008, 0x80000c01,
    0x7e0e02ff, 0x00000100, 0xbf8c3f70, 0xdcd18000, 0x14080a03, 0xdcd18000, 0x15080b07, 0xdcd18004,
    0x16080c07, 0xbf8c3f70, 0xe0701000, 0x80011401, 0xe0701004, 0x80011501, 0xe0701008, 0x80011601,
    0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> NoReturnCode{
    0xdcd08000, 0x00080a03, 0xbf810000,
};

alignas(256) constexpr std::array<std::uint32_t, 3> FlatSegmentCode{
    0xdcd10000, 0x147f0a03, 0xbf810000,
};

struct Row {
    std::uint32_t memory;
    std::uint32_t subtrahend;
};

constexpr std::array<Row, 16> Edges{{
    {0u, 0u}, {0u, 1u}, {1u, 0u}, {1u, 1u},
    {5u, 7u}, {7u, 5u}, {0xffffffffu, 1u}, {1u, 0xffffffffu},
    {0x80000000u, 0x7fffffffu}, {0x7fffffffu, 0x80000000u}, {0xffffffffu, 0xffffffffu}, {0x80000000u, 0x80000000u},
    {0x80000001u, 0x80000000u}, {0u, 0xffffffffu}, {0xffffffffu, 0u}, {0x12345678u, 0x12345679u},
}};

Row RowOf(std::uint32_t tid) {
    if (tid < Edges.size()) return Edges[tid];
    const std::uint32_t memory = (tid + 1u) * 0x9e3779b9u;
    const std::uint32_t subtrahend = (tid + 1u) * 0x85ebca6bu;
    return {memory, (tid & 3u) == 0u ? subtrahend >> 8u : subtrahend};
}

std::uint32_t SharedPartialStep(std::uint32_t tid) { return tid + 1u; }
std::uint32_t SharedExhaustedStep(std::uint32_t tid) { return tid + 100u; }

std::uint32_t SubtractClamped(std::uint32_t memory, std::uint32_t subtrahend) {
    return memory >= subtrahend ? memory - subtrahend : 0u;
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "global atomic csub: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

void Put(std::vector<std::uint8_t>& image, std::uint32_t offset, std::uint32_t value) {
    for (std::uint32_t byte = 0; byte < 4u; ++byte) image.at(offset + byte) = static_cast<std::uint8_t>(value >> (byte * 8u));
}

std::vector<std::uint8_t> Initial() {
    std::vector<std::uint8_t> image(BlockBytes, Fill);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Put(image, tid * 4u, RowOf(tid).memory);
    }
    Put(image, SharedOffset, SharedPartial);
    Put(image, SharedOffset + 4u, SharedExhausted);
    return image;
}

std::vector<std::uint8_t> Expected() {
    auto image = Initial();
    std::uint32_t partial = SharedPartial;
    std::uint32_t exhausted = SharedExhausted;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const Row row = RowOf(tid);
        Put(image, tid * 4u, SubtractClamped(row.memory, row.subtrahend));
        partial = SubtractClamped(partial, SharedPartialStep(tid));
        exhausted = SubtractClamped(exhausted, SharedExhaustedStep(tid));
    }
    Require(partial != 0u && exhausted == 0u, "global atomic csub: the shared dwords do not cover both outcomes");
    Put(image, SharedOffset, partial);
    Put(image, SharedOffset + 4u, exhausted);
    return image;
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::vector<std::uint32_t> UserData(const std::uint8_t* base) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
    std::vector<std::uint32_t> userData(10, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    userData[8] = static_cast<std::uint32_t>(address);
    userData[9] = static_cast<std::uint32_t>(address >> 32u);
    return userData;
}

ShaderRecompiler::RecompileResult Recompile(const AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t waveSize, std::span<const std::uint32_t> userData) {
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void CheckChain(const std::string& where, std::uint32_t column, std::uint32_t initial, std::uint32_t (*step)(std::uint32_t)) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> order;
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        order.emplace_back(Output[tid * Results + column], tid);
    }
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::uint32_t running = initial;
    for (const auto& [returned, tid] : order) {
        Require(returned == running, where + " returned " + Hex(returned) + " to thread " + std::to_string(tid) + ", expected " + Hex(running));
        running = SubtractClamped(running, step(tid));
    }
}

void Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t waveSize) {
    const auto initial = Initial();
    std::memcpy(guest.Data(), initial.data(), BlockBytes);
    Input.fill(0u);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        Input[tid * Inputs] = RowOf(tid).subtrahend;
        Input[tid * Inputs + 1u] = SharedPartialStep(tid);
        Input[tid * Inputs + 2u] = SharedExhaustedStep(tid);
    }
    Output.fill(Untouched);
    const auto userData = UserData(guest.Data());
    const std::span<const std::uint32_t> code(Code);
    const auto result = Recompile(device, code, waveSize, userData);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();

    const auto wave = "global atomic csub: wave" + std::to_string(waveSize) + " ";
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const Row row = RowOf(tid);
        const auto returned = Output[tid * Results];
        Require(returned == row.memory, wave + "thread " + std::to_string(tid) + " returned " + Hex(returned) + ", expected " + Hex(row.memory));
        for (std::uint32_t j = Written; j < Results; ++j) {
            Require(Output[tid * Results + j] == Untouched, wave + "thread " + std::to_string(tid) + " result " + std::to_string(j) + " was written");
        }
    }
    CheckChain(wave + "dword shared by all lanes", 1u, SharedPartial, SharedPartialStep);
    CheckChain(wave + "dword exhausted by all lanes", 2u, SharedExhausted, SharedExhaustedStep);
    const auto expected = Expected();
    for (std::uint32_t offset = 0; offset < BlockBytes; ++offset) {
        const auto actual = guest.Data()[offset];
        Require(actual == expected[offset], wave + "byte " + std::to_string(offset) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected[offset]));
    }
}

void CheckRejected(const AgcDriver::VulkanDevice& device, GuestBlock& guest, std::span<const std::uint32_t> code, const std::string& reason) {
    const auto userData = UserData(guest.Data());
    std::string failure;
    try {
        static_cast<void>(Recompile(device, code, 32, userData));
    } catch (const std::exception& error) {
        failure = error.what();
    }
    Require(failure.find(reason) != std::string::npos, "global atomic csub: expected '" + reason + "', got '" + failure + "'");
}

const Testing::Case wave32{"GlobalAtomicCsub_Wave32_ReturnsOldValuesAndStoresClampedDifferences", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock guest;
    Run(device, guest, 32);
}};

const Testing::Case wave64{"GlobalAtomicCsub_Wave64_ReturnsOldValuesAndStoresClampedDifferences", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock guest;
    Run(device, guest, 64);
}};

const Testing::Case withoutGlc{"GlobalAtomicCsub_WithoutGlc_IsRejected", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock guest;
    CheckRejected(device, guest, NoReturnCode, "global_atomic_csub without glc is not supported");
}};

const Testing::Case flatSegment{"GlobalAtomicCsub_FlatSegment_IsRejected", [] {
    auto& device = SharedVulkanTestDevice();
    GuestBlock guest;
    CheckRejected(device, guest, FlatSegmentCode, "global_atomic_csub is available only in the global segment");
}};

} // namespace

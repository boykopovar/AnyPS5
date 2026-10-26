#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Words = 64;
constexpr std::uint32_t Bytes = Words * 4u;
constexpr std::uint32_t Constant = 0x00010000u;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;
constexpr std::size_t Piece = 65536;
constexpr std::size_t BlockBytes = 5 * Piece;

alignas(256) std::array<std::uint32_t, 8> Table{};

constexpr std::uint32_t Vop2(std::uint32_t opcode, std::uint32_t destination, std::uint32_t source0, std::uint32_t source1) {
    return (opcode << 25u) | (destination << 17u) | (source1 << 9u) | source0;
}

constexpr std::uint32_t BufferOperands = (0x80u << 24u) | (1u << 16u) | (2u << 8u) | 1u;

alignas(256) constexpr std::array<std::uint32_t, 15> Code{
    Vop2(0x1au, 1u, 0x82u, 0u),
    0xf4080100u, 0xfa000000u,
    0xbf8cc07fu,
    0xe0301000u, BufferOperands,
    0xbf8c3f70u,
    0xf4080100u, 0xfa000010u,
    0xbf8cc07fu,
    Vop2(0x25u, 2u, 0xffu, 2u), Constant,
    0xe0701000u, BufferOperands,
    0xbf810000u,
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, Piece));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
#else
        constexpr std::uintptr_t alignment = Piece;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "dispatch move: cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        munmap(block, BlockBytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint32_t* InputA() const { return reinterpret_cast<std::uint32_t*>(block); }
    std::uint32_t* InputB() const { return reinterpret_cast<std::uint32_t*>(block + Piece + 12); }
    std::uint32_t* InputC() const { return reinterpret_cast<std::uint32_t*>(block + 2 * Piece + 32); }
    std::uint32_t* OutputO() const { return reinterpret_cast<std::uint32_t*>(block + 3 * Piece); }
    std::uint32_t* OutputP() const { return reinterpret_cast<std::uint32_t*>(block + 4 * Piece); }

private:
    std::uint8_t* block = nullptr;
};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

void Fill(std::uint32_t* where, std::uint32_t first) {
    for (std::uint32_t index = 0; index < Words; ++index) where[index] = first + index;
}

void Reset(std::uint32_t* output) {
    std::fill(output, output + Words, Sentinel);
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, const std::uint32_t* input, std::uint32_t inputBytes, const std::uint32_t* output) {
    const auto in = BufferDescriptor(input, inputBytes);
    const auto out = BufferDescriptor(output, Bytes);
    std::copy(in.begin(), in.end(), Table.begin());
    std::copy(out.begin(), out.end(), Table.begin() + 4);
    const auto table = reinterpret_cast<std::uintptr_t>(Table.data());
    const std::vector<std::uint32_t> userData{static_cast<std::uint32_t>(table), static_cast<std::uint32_t>(table >> 32u)};
    const std::span<const std::uint32_t> code(Code);
    const std::span<const std::uint32_t> tableWords(Table);
    const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}, {table, std::as_bytes(tableWords)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& compiled, const std::uint32_t* input, std::uint32_t inputBytes, const std::uint32_t* output, bool wait) {
    auto result = compiled;
    std::size_t elements = 0;
    for (auto& binding : result.bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
        Require(binding.guestDescriptor.size() == static_cast<std::size_t>(binding.count) * 4u && binding.bufferWritten.size() == binding.count, "dispatch move: the program's buffer bindings have another shape");
        for (std::uint32_t element = 0; element < binding.count; ++element, ++elements) {
            const auto words = binding.bufferWritten[element] ? BufferDescriptor(output, Bytes) : BufferDescriptor(input, inputBytes);
            std::copy(words.begin(), words.end(), binding.guestDescriptor.begin() + static_cast<std::ptrdiff_t>(element) * 4);
        }
    }
    Require(elements == 2, "dispatch move: the program does not bind an input and an output");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    if (wait) device.WaitIdle();
}

void Expect(const std::uint32_t* input, const std::uint32_t* output, std::uint32_t added, const std::string& what) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = input[tid] + added;
        Require(output[tid] == expected, "dispatch move: " + what + ": lane " + std::to_string(tid) + " is " + Hex(output[tid]) + ", expected " + Hex(expected));
    }
}

struct Step {
    std::uint64_t checked, exact, moved, refused, rebuilt, kept, served;
};

Step Since(const AgcDriver::VulkanDevice::DispatchMoveCount& before) {
    const auto now = AgcDriver::VulkanDevice::DispatchMoveCounts();
    return {now.checked - before.checked, now.exact - before.exact, now.moved - before.moved, now.refused - before.refused, now.rebuilt - before.rebuilt, now.kept - before.kept, now.served - before.served};
}

void Print(const char* what, const Step& step) {
    std::printf("dispatch move: %s: checked %llu, exact %llu, moved %llu, refused %llu\n", what, static_cast<unsigned long long>(step.checked), static_cast<unsigned long long>(step.exact), static_cast<unsigned long long>(step.moved), static_cast<unsigned long long>(step.refused));
}

bool RunTests(AgcDriver::VulkanDevice& device, const GuestBlock& guest) {
    Fill(guest.InputA(), 0xa000u);
    Fill(guest.InputB(), 0xb000u);
    Fill(guest.InputC(), 0xc000u);

    const auto compiled = Compile(device, guest.InputA(), Bytes, guest.OutputO());
    Reset(guest.OutputO());
    Run(device, compiled, guest.InputA(), Bytes, guest.OutputO(), true);
    Expect(guest.InputA(), guest.OutputO(), Constant, "the first dispatch");
    const auto start = AgcDriver::VulkanDevice::DispatchMoveCounts();

    Reset(guest.OutputO());
    Run(device, compiled, guest.InputB(), Bytes, guest.OutputO(), true);
    Expect(guest.InputB(), guest.OutputO(), Constant, "an input moved to another block");
    Reset(guest.OutputO());
    Run(device, compiled, guest.InputA(), Bytes, guest.OutputO(), true);
    Expect(guest.InputA(), guest.OutputO(), Constant, "the input back where the template was built");
    Reset(guest.OutputO());
    Run(device, compiled, guest.InputC(), Bytes, guest.OutputO(), true);
    Expect(guest.InputC(), guest.OutputO(), Constant, "an input moved to a third block");
    const auto repeats = Since(start);
    Print("an exact repeat between two moved ones", repeats);
    const bool templates = repeats.checked != 0;
    if (templates) {
        Require(repeats.exact == 1 && repeats.moved == 2 && repeats.refused == 0, "dispatch move: the repeats were not served as one exact hit and two moved ones");
    } else {
        std::puts("dispatch move: the device kept no template (no host imports); the moved-buffer path is not exercised");
    }

    Reset(guest.OutputO());
    Run(device, compiled, guest.InputB(), 192, guest.OutputO(), true);
    Expect(guest.InputB(), guest.OutputO(), Constant, "an input moved and shortened");

    std::array<std::uint32_t, Words> changed{};
    for (std::uint32_t index = 0; index < Words; ++index) changed[index] = 0xd000u + index * 3u;
    AgcDriver::GuestMemory::Write(reinterpret_cast<std::uintptr_t>(guest.InputB()), std::as_bytes(std::span(changed)));
    Reset(guest.OutputO());
    Run(device, compiled, guest.InputB(), Bytes, guest.OutputO(), true);
    Expect(changed.data(), guest.OutputO(), Constant, "an input moved to a range the driver stored over");

    const auto refusals = AgcDriver::VulkanDevice::DispatchMoveCounts();
    for (std::uint32_t index = 0; index < Words; ++index) guest.OutputO()[index] = 0x7000u + index;
    Run(device, compiled, guest.OutputO() + Threads, Bytes / 2, guest.OutputO(), true);
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = 0x7000u + Threads + tid + Constant;
        Require(guest.OutputO()[tid] == expected, "dispatch move: an input moved into the unwritten half of the dispatch's own output: lane " + std::to_string(tid) + " is " + Hex(guest.OutputO()[tid]) + ", expected " + Hex(expected));
    }

    Reset(guest.OutputO());
    Reset(guest.OutputP());
    Run(device, compiled, guest.InputA(), Bytes, guest.OutputP(), false);
    Run(device, compiled, guest.OutputP(), Bytes, guest.OutputO(), true);
    Expect(guest.InputA(), guest.OutputO(), 2u * Constant, "an input moved onto a range a recorded dispatch still writes");
    const auto refused = Since(refusals);
    Print("two inputs moved onto written ranges", refused);
    if (templates) {
        Require(refused.refused == 2, "dispatch move: the inputs moved onto written ranges were not both refused");
        Require(refused.rebuilt == 2 && refused.kept == 2 && refused.served == 0, "dispatch move: the two refusals were not two rebuilds inserted beside the template");
    }

    const auto afterRefusals = AgcDriver::VulkanDevice::DispatchMoveCounts();
    Reset(guest.OutputO());
    Run(device, compiled, guest.InputA(), Bytes, guest.OutputO(), true);
    Expect(guest.InputA(), guest.OutputO(), Constant, "the input moved back after the refusals");
    const auto back = Since(afterRefusals);
    Print("the input back after the refusals", back);
    if (templates) Require(back.checked == 1 && back.exact == 1 && back.moved == 0 && back.refused == 0 && back.rebuilt == 0, "dispatch move: the input back at the template's address was not an exact hit after the refusals");

    for (int round = 0; round < 2; ++round) {
        const auto alternating = AgcDriver::VulkanDevice::DispatchMoveCounts();
        Reset(guest.OutputO());
        Reset(guest.OutputP());
        Run(device, compiled, guest.InputA(), Bytes, guest.OutputP(), false);
        Run(device, compiled, guest.OutputP(), Bytes, guest.OutputO(), true);
        Expect(guest.InputA(), guest.OutputO(), 2u * Constant, "an input refused again while A alternates");
        Reset(guest.OutputO());
        Run(device, compiled, guest.InputA(), Bytes, guest.OutputO(), true);
        Expect(guest.InputA(), guest.OutputO(), Constant, "the input at A after the refused one");
        const auto step = Since(alternating);
        Print("A alternating with a refused input", step);
        if (templates) Require(step.refused == 1 && step.served == 1 && step.rebuilt == 0 && step.exact == 2 && step.moved == 0, "dispatch move: a refused input rebuilt, or evicted the template of A, while A alternated with it");
    }
    const auto total = AgcDriver::VulkanDevice::DispatchMoveCounts();
    std::printf("dispatch move: in total: checked %llu, exact %llu, moved %llu, refused %llu, dropped by stage A %llu\n", static_cast<unsigned long long>(total.checked), static_cast<unsigned long long>(total.exact), static_cast<unsigned long long>(total.moved), static_cast<unsigned long long>(total.refused), static_cast<unsigned long long>(total.filtered));
    return templates;
}

}

int main() {
    try {
        GuestBlock guest;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!RunTests(*device, guest)) return VulkanTestSkipped;
        std::puts("dispatch move tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

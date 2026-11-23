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
constexpr std::size_t Moved = 2u * 1024u * 1024u;
constexpr std::size_t Bytes = Moved;
constexpr std::uint32_t Constant = 0x00010000u;
constexpr std::uint32_t Sentinel = 0xdeadbeefu;

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

class GuestBuffer {
public:
    GuestBuffer() = default;

    void Create() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(Moved, Moved));
        if (block != nullptr) GuestArena::GuestArenaCommit_nid_postfix(block, Moved, PAGE_READWRITE, Moved);
#else
        void* mapped = mmap(nullptr, Moved + Moved, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + Moved - 1) & ~(static_cast<std::uintptr_t>(Moved) - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + Moved != begin + Moved + Moved) munmap(reinterpret_cast<void*>(aligned + Moved), begin + Moved + Moved - aligned - Moved);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, Moved);
        }
#endif
        Require(block != nullptr, "dispatch rebind: cannot allocate the guest buffer");
        GuestAllocations::Mutation().Add(block, Moved, true, true, true);
    }

    ~GuestBuffer() {
        if (block == nullptr) return;
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestArena::GuestArenaReset_nid_postfix(block, Moved);
        GuestArena::GuestArenaRelease_nid_postfix(block, Moved);
#else
        munmap(block, Moved);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, Moved);
#endif
    }

    GuestBuffer(const GuestBuffer&) = delete;
    GuestBuffer& operator=(const GuestBuffer&) = delete;

    std::uint32_t* Words() const { return reinterpret_cast<std::uint32_t*>(block); }

    void Reregister() {
        GuestAllocations::Mutation().Remove(block);
        GuestAllocations::Mutation().Add(block, Moved, true, true, true);
    }

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

void Run(AgcDriver::VulkanDevice& device, const ShaderRecompiler::RecompileResult& compiled, const std::uint32_t* input, std::uint32_t inputBytes, const std::uint32_t* output) {
    auto result = compiled;
    std::size_t elements = 0;
    for (auto& binding : result.bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
        Require(binding.guestDescriptor.size() == static_cast<std::size_t>(binding.count) * 4u && binding.bufferWritten.size() == binding.count, "dispatch rebind: the program's buffer bindings have another shape");
        for (std::uint32_t element = 0; element < binding.count; ++element, ++elements) {
            const auto words = binding.bufferWritten[element] ? BufferDescriptor(output, Bytes) : BufferDescriptor(input, inputBytes);
            std::copy(words.begin(), words.end(), binding.guestDescriptor.begin() + static_cast<std::ptrdiff_t>(element) * 4);
        }
    }
    Require(elements == 2, "dispatch rebind: the program does not bind an input and an output");
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(Code.data()));
    device.WaitIdle();
}

void Expect(const std::uint32_t* input, const std::uint32_t* output, const std::string& what) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        const auto expected = input[tid] + Constant;
        Require(output[tid] == expected, "dispatch rebind: " + what + ": lane " + std::to_string(tid) + " is " + Hex(output[tid]) + ", expected " + Hex(expected));
    }
}

struct Step {
    std::uint64_t checked, exact, moved, refused, rebuilt, kept, served, inPlace;
};

Step Since(const AgcDriver::VulkanDevice::DispatchMoveCount& before) {
    const auto now = AgcDriver::VulkanDevice::DispatchMoveCounts();
    return {now.checked - before.checked, now.exact - before.exact, now.moved - before.moved, now.refused - before.refused, now.rebuilt - before.rebuilt, now.kept - before.kept, now.served - before.served, now.inPlace - before.inPlace};
}

bool RunTests(AgcDriver::VulkanDevice& device, GuestBuffer& first, GuestBuffer& second, GuestBuffer& output) {
    Fill(first.Words(), 0xa000u);
    Fill(second.Words(), 0xb000u);

    const auto compiled = Compile(device, first.Words(), Bytes, output.Words());
    Reset(output.Words());
    Run(device, compiled, first.Words(), Bytes, output.Words());
    Expect(first.Words(), output.Words(), "the first dispatch");

    Reset(output.Words());
    Run(device, compiled, second.Words(), Bytes, output.Words());
    Expect(second.Words(), output.Words(), "the input moved to a second block");
    Reset(output.Words());
    Run(device, compiled, first.Words(), Bytes, output.Words());
    Expect(first.Words(), output.Words(), "the input back where the template was built");

    const auto before = AgcDriver::VulkanDevice::DispatchMoveCounts();
    Reset(output.Words());
    Run(device, compiled, second.Words(), Bytes, output.Words());
    Expect(second.Words(), output.Words(), "the input moved to the imported block");
    const auto moved = Since(before);
    std::printf("dispatch rebind: a moved read-only buffer of %zu bytes: checked %llu, moved %llu, in place %llu, rebuilt %llu\n", Moved, static_cast<unsigned long long>(moved.checked), static_cast<unsigned long long>(moved.moved), static_cast<unsigned long long>(moved.inPlace), static_cast<unsigned long long>(moved.rebuilt));
    if (moved.checked == 0) {
        std::puts("dispatch rebind: the device kept no template (no host imports); the in-place path is not exercised");
        return false;
    }
    Require(moved.moved != 0 && moved.inPlace != 0, "dispatch rebind: the moved read-only buffer was not rebound in place");
    Require(moved.rebuilt == 0 && moved.kept == 0, "dispatch rebind: the in-place move rebuilt the template");

    const auto written = AgcDriver::VulkanDevice::DispatchMoveCounts();
    for (std::uint32_t index = 0; index < Words; ++index) second.Words()[index] = 0xe000u + index;
    Reset(output.Words());
    Run(device, compiled, second.Words(), Bytes, output.Words());
    Expect(second.Words(), output.Words(), "the input written between two dispatches");
    const auto afterWrite = Since(written);
    Require(afterWrite.inPlace != 0, "dispatch rebind: a guest write to the moved range left the in-place path");

    second.Reregister();
    for (std::uint32_t index = 0; index < Words; ++index) second.Words()[index] = 0xf000u + index;
    Reset(output.Words());
    Run(device, compiled, second.Words(), Bytes, output.Words());
    Expect(second.Words(), output.Words(), "the input whose import changed");
    return true;
}

}

int main() {
    try {
        GuestBuffer first;
        GuestBuffer second;
        GuestBuffer output;
        first.Create();
        second.Create();
        output.Create();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!RunTests(*device, first, second, output)) return VulkanTestSkipped;
        std::puts("dispatch rebind tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

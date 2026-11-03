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
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
constexpr std::size_t BlockBytes = 65536;
constexpr std::uint32_t Lanes = 2;
constexpr std::uint32_t LaneBytes = 16;
constexpr std::size_t OutputBytes = 128;
constexpr std::size_t FirstByte = 33;
constexpr std::uint8_t Untouched = 0xa5;
constexpr std::size_t MaxRuntimeWriteCalls = 3;

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, BlockBytes));
#endif
        Require(block != nullptr, "runtime formatted store: cannot allocate guest memory");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint32_t* Descriptor() { return reinterpret_cast<std::uint32_t*>(block); }
    std::uint32_t* Input() { return reinterpret_cast<std::uint32_t*>(block + 256); }
    std::span<std::uint8_t, OutputBytes> Output(bool reference) { return std::span<std::uint8_t, OutputBytes>(block + (reference ? 768 : 512), OutputBytes); }

private:
    std::uint8_t* block = nullptr;
};

struct Format {
    std::uint32_t value;
    std::uint32_t bytes;
};

constexpr std::array<Format, 18> Formats{{
    {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 2}, {11, 2}, {12, 2},
    {13, 2}, {20, 4}, {22, 4}, {36, 4}, {50, 4}, {56, 4}, {71, 8}, {74, 12}, {77, 16},
}};

std::array<std::uint32_t, 4> Descriptor(const void* pointer, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::uint32_t FindFunction(const ShaderRecompiler::RecompileResult& shader, std::string_view expected) {
    const auto words = shader.spirv.Words();
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        Require(count != 0u && count <= words.size() - offset, "runtime formatted store: malformed SPIR-V");
        if ((words[offset] & spv::OpCodeMask) == spv::OpName && count >= 3u && std::string_view(reinterpret_cast<const char*>(&words[offset + 2u])) == expected) return words[offset + 1u];
        offset += count;
    }
    return 0u;
}

std::size_t RuntimeWriteCalls(const ShaderRecompiler::RecompileResult& shader, std::uint32_t components) {
    const auto runtime = FindFunction(shader, "store_formatted_gpu_buffer_x" + std::to_string(components));
    const auto bytes = FindFunction(shader, "write_bda_bytes");
    const auto coherentBytes = FindFunction(shader, "write_bda_bytes_coherent");
    Require(runtime != 0u && (bytes != 0u || coherentBytes != 0u), "runtime formatted store: missing store or byte-write helper");
    const auto words = shader.spirv.Words();
    bool inside = false;
    std::size_t calls = 0;
    for (std::size_t offset = 5; offset < words.size();) {
        const auto count = words[offset] >> spv::WordCountShift;
        Require(count != 0u && count <= words.size() - offset, "runtime formatted store: malformed SPIR-V");
        const auto opcode = words[offset] & spv::OpCodeMask;
        if (opcode == spv::OpFunction && count >= 3u) inside = words[offset + 2u] == runtime;
        if (inside && opcode == spv::OpFunctionCall && count >= 4u &&
            ((bytes != 0u && words[offset + 3u] == bytes) || (coherentBytes != 0u && words[offset + 3u] == coherentBytes))) ++calls;
        if (opcode == spv::OpFunctionEnd) inside = false;
        offset += count;
    }
    return calls;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t components, bool d16, std::uint32_t format = 0u) {
    std::vector<std::uint32_t> code{
        0xe0340000u, 0x80010200u, 0x34020084u, 0xe0381000u, 0x80030401u, 0xbf8c3f70u, 0x7e140502u,
    };
    if (format == 0u) code.push_back(0x7e160503u);
    else code.insert(code.end(), {0xbe8b03ffu, 0x31000facu | (format << 12u)});
    code.insert(code.end(), {0xe0001000u | ((components + (d16 ? 0x83u : 3u)) << 18u), 0x80020401u, 0xbf810000u});
    std::array<std::uint32_t, 16> userData{};
    const auto descriptor = Descriptor(guest.Descriptor(), 8u);
    const auto output = Descriptor(guest.Output(format != 0u).data() + FirstByte, 0u);
    const auto input = Descriptor(guest.Input(), Lanes * LaneBytes);
    std::copy(descriptor.begin(), descriptor.end(), userData.begin() + 4u);
    std::copy(output.begin(), output.end(), userData.begin() + 8u);
    std::copy(input.begin(), input.end(), userData.begin() + 12u);
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0u, {}},
        {32u, 0u, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0u, 0u, 0u, 128u},
    };
    request.useCache = false;
    auto result = ShaderRecompiler::Recompile(request);
    Require((FindFunction(result, "store_formatted_gpu_buffer_x" + std::to_string(components)) != 0u) == (format == 0u), "runtime formatted store: the runtime and constant-format shaders did not take distinct store paths");
    return result;
}

void Run(AgcDriver::VulkanDevice& device, GuestBlock& guest, std::uint32_t components, bool d16) {
    const std::array<std::uint32_t, 8> input = d16
        ? std::array<std::uint32_t, 8>{0xc0003e00u, 0x7e093800u, 0u, 0u, 0x3c00b800u, 0x0001fc00u, 0u, 0u}
        : std::array<std::uint32_t, 8>{0x3fc00000u, 0xc0000000u, 0x3f000000u, 0x7fc12345u, 0xbf000000u, 0x3f800000u, 0xff800000u, 0x00000001u};
    std::copy(input.begin(), input.end(), guest.Input());
    const auto runtime = Compile(device, guest, components, d16);
    const auto writeCalls = RuntimeWriteCalls(runtime, components);
    std::printf("runtime formatted store x%u%s: %zu SPIR-V words, %zu byte-write call sites\n", components, d16 ? " d16" : "", runtime.spirv.Words().size(), writeCalls);
    std::fflush(stdout);
    Require(writeCalls != 0u && writeCalls <= MaxRuntimeWriteCalls, "runtime formatted store: expected one shared byte-write call site per 8/16/32-bit width");
    for (std::size_t index = (components - 1u) % 2u; index < Formats.size(); index += 2u) {
        const auto format = Formats[index];
        const auto reference = Compile(device, guest, components, d16, format.value);
        for (const auto records : {32u, 16u, format.bytes - 1u}) {
            guest.Descriptor()[0] = records;
            guest.Descriptor()[1] = 0x31000facu | (format.value << 12u);
            std::ranges::fill(guest.Output(false), Untouched);
            std::ranges::fill(guest.Output(true), Untouched);
            device.Dispatch(runtime, 1u, 1u, 1u);
            device.WaitIdle();
            device.Dispatch(reference, 1u, 1u, 1u);
            device.WaitIdle();
            const auto context = "runtime formatted store: x" + std::to_string(components) + (d16 ? " d16" : " f32") + " format " + std::to_string(format.value) + " records " + std::to_string(records);
            for (std::size_t byte = 0; byte < OutputBytes; ++byte) {
                const auto actual = guest.Output(false)[byte];
                const auto expected = guest.Output(true)[byte];
                Require(actual == expected, context + " byte " + std::to_string(byte) + " is " + std::to_string(actual) + ", expected " + std::to_string(expected));
                bool writable = false;
                for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
                    const auto offset = lane * LaneBytes;
                    writable = writable || (offset + format.bytes <= records && byte >= FirstByte + offset && byte < FirstByte + offset + format.bytes);
                }
                if (!writable) Require(actual == Untouched, context + " changed guard or out-of-bounds byte " + std::to_string(byte));
            }
            if (records >= format.bytes) Require(guest.Output(true)[FirstByte] != Untouched, context + " reference store wrote no first byte");
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        std::uint32_t components = 4u;
        bool d16 = false;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument(argv[index]);
            if (argument == "--d16") d16 = true;
            else if (argument.size() == 4u && argument.starts_with("--x") && argument[3] >= '1' && argument[3] <= '4') components = static_cast<std::uint32_t>(argument[3] - '0');
            else Require(false, "runtime formatted store: unknown argument");
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestBlock guest;
        Run(*device, guest, components, d16);
        std::printf("runtime formatted store x%u%s: 9 formats, unaligned base, full/record/partial OOB and all output bytes match constant-format stores\n", components, d16 ? " d16" : "");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

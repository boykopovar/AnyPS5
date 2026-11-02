#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>

namespace {
using AgcDriver::Graphics::Require;
constexpr std::uint32_t Threads = 64u;
constexpr std::uint32_t Guard = 0xdeadbeefu;
alignas(256) std::array<std::uint32_t, Threads> Input{};
alignas(256) std::array<std::uint32_t, Threads + 8u> Output{};

std::array<std::uint32_t, 4> Descriptor(const void* bytes, std::uint32_t size, std::uint32_t format) {
    const auto address = reinterpret_cast<std::uintptr_t>(bytes);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), size, format};
}

void Run(AgcDriver::VulkanDevice& device, std::uint32_t wave, std::uint32_t opcode, std::uint32_t bytes, std::uint32_t format, bool store = false) {
    Output.fill(Guard);
    alignas(256) const std::array<std::uint32_t, 11> code{
        0x34020082u, 0xbee80304u, 0xbee90305u, 0xbeea0306u, 0xbeeb0307u,
        store ? 0xe0301000u : opcode, store ? 0x80000201u : 0x801a0201u, 0xbf8c3f70u,
        store ? opcode : 0xe0701000u, store ? 0x801a0201u : 0x80000201u, 0xbf810000u};
    std::array<std::uint32_t, 8> userData{};
    const auto output = Descriptor(Output.data() + 4u, store ? bytes : Threads * 4u, format);
    const auto input = Descriptor(Input.data(), store ? Threads * 4u : bytes, format);
    const auto ordinary = store ? input : output;
    const auto mixed = store ? output : input;
    std::copy(ordinary.begin(), ordinary.end(), userData.begin());
    std::copy(mixed.begin(), mixed.end(), userData.begin() + 4u);
    const std::span<const std::uint32_t> words(code);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(words)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), words, 0u, {}},
        {wave, 0u, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(), {0u, 0u, 0u, 128u}};
    request.useCache = false;
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    for (std::size_t index = 0u; index < 4u; ++index) {
        Require(Output[index] == Guard && Output[Threads + 4u + index] == Guard, "buffer VCC tuple writes passed the output bounds");
    }
    for (std::uint32_t lane = 0u; lane < Threads; ++lane) {
        std::uint32_t expected = lane * 4u < bytes ? Input[lane] : store ? Guard : 0u;
        if (!store && format == 0x31038facu && opcode == 0xe0001000u && lane * 4u < bytes) {
            expected = std::bit_cast<std::uint32_t>(static_cast<float>(Input[lane] & 0xffu) / 255.0f);
        }
        if (Output[lane + 4u] != expected) {
            std::fprintf(stderr, "wave=%u opcode=%08x store=%d bytes=%u format=%08x lane=%u actual=%08x expected=%08x\n", wave, opcode, store, bytes, format, lane, Output[lane + 4u], expected);
        }
        Require(Output[lane + 4u] == expected, "buffer VCC descriptor data differs from its scalar words");
    }
}
}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (std::uint32_t lane = 0u; lane < Threads; ++lane) Input[lane] = 0x3f800000u | (lane * 0x10001u);
        for (const auto wave : {32u, 64u}) {
            for (const auto load : {0xe8b01000u, 0xe0301000u, 0xe0001000u}) {
                Run(*device, wave, load, Threads * 4u, 0x31016facu);
                Run(*device, wave, load, 12u, 0x31016facu);
            }
            Run(*device, wave, 0xe0001000u, Threads * 4u, 0x31038facu);
            for (const auto store : {0xe0701000u, 0xe8b41000u}) {
                Run(*device, wave, store, Threads * 4u, 0x31016facu, true);
                Run(*device, wave, store, 12u, 0x31016facu, true);
            }
        }
        std::puts("buffer mixed SGPR/VCC descriptor loads/stores, both VCC words, wave32/wave64 and output guards pass");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

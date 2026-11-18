#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Threads = 32;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, 6> Data{};
alignas(256) std::array<std::uint32_t, Threads * Results> Output{};

constexpr std::uint64_t OldValue = 0x1122334455667788ull;
constexpr std::uint64_t SwapValue = 0xf00e0d0c0b0a0908ull;
constexpr std::uint64_t CompareValue = OldValue;

struct Form {
    std::uint32_t opcode;
    const char* name;
};

constexpr std::array<Form, 7> Forms{{
    {0x60u, "s_buffer_atomic_swap_x2"},
    {0x61u, "s_buffer_atomic_cmpswap_x2"},
    {0x62u, "s_buffer_atomic_add_x2"},
    {0x63u, "s_buffer_atomic_sub_x2"},
    {0x64u, "s_buffer_atomic_smin_x2"},
    {0x65u, "s_buffer_atomic_umin_x2"},
    {0x66u, "s_buffer_atomic_smax_x2"},
}};

std::vector<std::uint32_t> BuildCode(std::uint32_t opcode) {
    const std::uint32_t atomicWord0 = 0xf4000000u | (opcode << 18u) | (1u << 16u) | (8u << 6u);
    return {
        0x34020084u,
        0xf4240200u, 0xfa000008u,
        0xbf8cc07fu,
        0xf4240280u, 0xfa000010u,
        0xbf8cc07fu,
        atomicWord0, 0xfa000000u,
        0xbf8cc07fu,
        0x7e040208u, 0x7e060209u,
        0xe0701000u, 0x80010201u,
        0xe0701004u, 0x80010301u,
        0xf4240200u, 0xfa000000u,
        0xbf8cc07fu,
        0x7e040208u, 0x7e060209u,
        0xe0701008u, 0x80010201u,
        0xe070100cu, 0x80010301u,
        0xbf810000u,
    };
}

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x01016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto data = BufferDescriptor(Data.data(), static_cast<std::uint32_t>(Data.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(data.begin(), data.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 10240u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::uint64_t Expected(std::uint32_t opcode) {
    const auto old = static_cast<std::int64_t>(OldValue);
    const auto value = static_cast<std::int64_t>(SwapValue);
    switch (opcode) {
        case 0x60u: return SwapValue;
        case 0x61u: return CompareValue == OldValue ? SwapValue : OldValue;
        case 0x62u: return OldValue + SwapValue;
        case 0x63u: return OldValue - SwapValue;
        case 0x64u: return static_cast<std::uint64_t>(std::min(old, value));
        case 0x65u: return std::min(OldValue, SwapValue);
        case 0x66u: return static_cast<std::uint64_t>(std::max(old, value));
        default: throw std::runtime_error("unexpected opcode");
    }
}

void Run(AgcDriver::VulkanDevice& device, Form form) {
    Data[0] = static_cast<std::uint32_t>(OldValue);
    Data[1] = static_cast<std::uint32_t>(OldValue >> 32u);
    Data[2] = static_cast<std::uint32_t>(SwapValue);
    Data[3] = static_cast<std::uint32_t>(SwapValue >> 32u);
    Data[4] = static_cast<std::uint32_t>(CompareValue);
    Data[5] = static_cast<std::uint32_t>(CompareValue >> 32u);
    Output.fill(0xdeadbeefu);
    const auto code = BuildCode(form.opcode);
    const auto result = Compile(device, code);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(Form form) {
    const auto expected = Expected(form.opcode);
    for (std::uint32_t lane = 0; lane < Threads; ++lane) {
        const std::uint32_t* words = &Output[lane * Results];
        Require(words[0] == static_cast<std::uint32_t>(OldValue) && words[1] == static_cast<std::uint32_t>(OldValue >> 32u), std::string(form.name) + ": lane " + std::to_string(lane) + " returned " + Hex(words[1]) + ":" + Hex(words[0]) + ", expected the old value");
        Require(words[2] == static_cast<std::uint32_t>(expected) && words[3] == static_cast<std::uint32_t>(expected >> 32u), std::string(form.name) + ": lane " + std::to_string(lane) + " stored " + Hex(words[3]) + ":" + Hex(words[2]) + ", expected " + Hex(static_cast<std::uint32_t>(expected >> 32u)) + ":" + Hex(static_cast<std::uint32_t>(expected)));
    }
}

}

int main(int argc, char** argv) {
    try {
        std::vector<Form> selected;
        if (argc > 1) {
            for (const Form form : Forms) {
                const std::string name = form.name;
                if (name.find(argv[1]) != std::string::npos) selected.push_back(form);
            }
            if (selected.empty()) {
                std::cerr << "no form matches " << argv[1] << '\n';
                return 1;
            }
        } else {
            selected.assign(Forms.begin(), Forms.end());
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        for (const Form form : selected) {
            Run(*device, form);
            Check(form);
        }
        std::puts("s_buffer_atomic_x2 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

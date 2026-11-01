#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t InputBytes = Lanes * 4u;
constexpr std::uint32_t Fill = 0xdeadbeefu;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format16UInt = 11;
alignas(256) std::array<std::uint32_t, Lanes> Input{};
alignas(256) std::array<std::uint32_t, Lanes * 2u + 8u> Output{};

constexpr std::array<std::uint32_t, 16> LoadCode(std::uint32_t marker) {
    return {0x34020082u, 0xe0001000u, 0x80000201u, 0xe0301000u, 0x80000301u, 0xbf8c0000u,
            0xe0101000u, 0x80000201u, 0xe0701000u, 0x80000301u,
            0xe0701000u, 0x80010201u, 0xe0701080u + marker * 4u, 0x80010301u, 0xbf800000u | marker, 0xbf810000u};
}

enum class Mode { Direct, Captured, Prepared };

alignas(256) constexpr std::array Codes{LoadCode(0u), LoadCode(1u), LoadCode(2u), LoadCode(3u), LoadCode(4u), LoadCode(5u)};

std::array<std::uint32_t, 4> Descriptor(const void* data, std::uint32_t records, std::uint32_t format, std::uint32_t stride = 0u) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    const auto oobSelect = stride != 0u ? 0u : 3u;
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, (0x01016facu & ~(0x7fu << 12u)) | (format << 12u) | (oobSelect << 28u)};
}

constexpr std::array<std::uint32_t, 4> GameNull{0x00000000u, 0x00080050u, 0x00000000u, 0x00005204u};
constexpr std::array<std::uint32_t, 4> ZeroNull{0u, 0u, 0u, 0u};

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

struct Outcome {
    std::uint64_t pipeline = 0;
    std::vector<std::uint32_t> spirv;
};

Outcome Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t marker, const std::array<std::uint32_t, 4>& optional, const std::string& name, std::uint32_t mask, Mode mode) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) Input[lane] = 0x9e3779b9u * (lane + 1u) ^ 0x01234567u;
    Output.fill(Fill);
    std::vector<std::uint32_t> userData(optional.begin(), optional.end());
    const auto output = Descriptor(Output.data(), sizeof(Output), Format32UInt);
    userData.insert(userData.end(), output.begin(), output.end());
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {Lanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = true;
    const auto result = [&] {
        if (mode == Mode::Direct) return ShaderRecompiler::Recompile(request);
        ShaderRecompiler::RequestMemoryView memoryView(request.context.memory);
        const auto runtime = memoryView.MakeRuntime(request.context.userData, request.shader.codeAddress);
        if (mode == Mode::Captured) return *ShaderRecompiler::Recompile(request, *ShaderRecompiler::CaptureResources(request, runtime));
        const auto handle = ShaderRecompiler::PrepareShader(request);
        const auto invocation = ShaderRecompiler::PreparedShaderInvocation::TryCreate(request, handle);
        Require(invocation.has_value(), "the prepared shader refused its own request");
        return *invocation->Materialize(*invocation->Capture(runtime));
    }();
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
    const bool present = optional[2] != 0u && (optional[0] != 0u || (optional[1] & 0xffffu) != 0u);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        Require(Input[lane] == (0x9e3779b9u * (lane + 1u) ^ 0x01234567u), name + ": an absent store changed its backing memory");
        const auto formatted = present ? Input[lane] & mask : 0u;
        const auto raw = present ? Input[lane] : 0u;
        Require(Output[lane] == formatted, name + ": lane " + std::to_string(lane) + " buffer_load_format_x is " + Hex(Output[lane]) + ", expected " + Hex(formatted));
        Require(Output[Lanes + marker + lane] == raw, name + ": lane " + std::to_string(lane) + " buffer_load_dword is " + Hex(Output[Lanes + marker + lane]) + ", expected " + Hex(raw));
    }
    Outcome outcome;
    outcome.pipeline = result.PipelineVariantId();
    outcome.spirv = result.spirv.Copy();
    Require(outcome.pipeline != 0u && !outcome.spirv.empty(), name + ": no specialized module");
    return outcome;
}

void BoundThenNull(AgcDriver::VulkanDevice& device, Mode mode) {
    const auto marker = static_cast<std::uint32_t>(mode) * 2u;
    const auto& code = Codes[marker];
    const auto bound = Run(device, code, marker, Descriptor(Input.data(), InputBytes, Format32UInt), "bound", 0xffffffffu, mode);
    const auto absent = Run(device, code, marker, GameNull, "null V#", 0xffffffffu, mode);
    Require(absent.pipeline == bound.pipeline && absent.spirv == bound.spirv, "a null V# in an optional slot made another module than the bound slot");
    const auto zero = Run(device, code, marker, ZeroNull, "zero V#", 0xffffffffu, mode);
    Require(zero.pipeline == bound.pipeline, "a zero V# made another module than the bound slot");
    auto zeroAddress = Descriptor(nullptr, InputBytes, Format32UInt);
    const auto addressless = Run(device, code, marker, zeroAddress, "zero address with records", 0xffffffffu, mode);
    Require(addressless.pipeline == bound.pipeline, "a zero-address V# did not reuse the bound module");
    auto zeroRecords = Descriptor(Input.data(), 0u, Format16UInt, InputBytes);
    const auto backedNull = Run(device, code, marker, zeroRecords, "zero records with backing memory", 0xffffffffu, mode);
    Require(backedNull.pipeline == bound.pipeline, "a zero-record V# did not reuse the bound module");
    const auto again = Run(device, code, marker, Descriptor(Input.data(), InputBytes, Format32UInt), "bound again", 0xffffffffu, mode);
    Require(again.pipeline == bound.pipeline, "the bound slot changed modules after a null V#");
    const auto reformatted = Run(device, code, marker, Descriptor(Input.data(), 1u, Format16UInt, InputBytes), "bound 16_UINT structured stride 128", 0xffffu, mode);
    Require(reformatted.pipeline != bound.pipeline && reformatted.spirv != bound.spirv, "a bound slot with another format and stride shared the substitute's module");
    const auto absentAfter = Run(device, code, marker, GameNull, "null V# after another format", 0xffffffffu, mode);
    Require(absentAfter.pipeline == bound.pipeline, "a null V# did not keep the first bound slot's substitute");
}

void NullThenBound(AgcDriver::VulkanDevice& device, Mode mode) {
    const auto marker = static_cast<std::uint32_t>(mode) * 2u + 1u;
    const auto& code = Codes[marker];
    const auto first = Run(device, code, marker, GameNull, "null V# before any bound slot", 0xffffffffu, mode);
    const auto bound = Run(device, code, marker, Descriptor(Input.data(), InputBytes, Format32UInt), "bound after null", 0xffffffffu, mode);
    Require(first.pipeline != bound.pipeline, "a slot never seen bound did not specialize as absent");
    const auto repeated = Run(device, code, marker, GameNull, "same null V# after learning", 0xffffffffu, mode);
    Require(repeated.pipeline == bound.pipeline && repeated.spirv == bound.spirv, "the full-result memo retained the null module from before learning");
    const auto learned = Run(device, code, marker, ZeroNull, "zero V# after the slot was bound", 0xffffffffu, mode);
    Require(learned.pipeline == bound.pipeline && learned.spirv == bound.spirv, "a null V# did not take the substitute learned from the bound slot");
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::printf("null buffer substitutes registrationModules=%d\n", device->Target().registrationModules);
        for (const auto mode : {Mode::Direct, Mode::Captured, Mode::Prepared}) {
            if (argc > 1 && ((std::string(argv[1]) == "--captured" && mode != Mode::Captured) || (std::string(argv[1]) == "--prepared" && mode != Mode::Prepared))) continue;
            BoundThenNull(*device, mode);
            NullThenBound(*device, mode);
            std::printf("null buffer substitutes mode=%u passed\n", static_cast<unsigned>(mode));
        }
        std::puts("null buffer substitute tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

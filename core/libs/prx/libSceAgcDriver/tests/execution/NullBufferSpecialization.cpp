#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "VulkanTestDevice.hpp"
#include "ExecutionHelpers.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Lanes = 32;
constexpr std::uint32_t InputBytes = Lanes * 4u;
constexpr std::uint32_t Fill = 0xdeadbeefu;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::uint32_t Format16UInt = 11;
constexpr std::uint32_t Format32Float = 22;
alignas(256) std::array<std::uint32_t, Lanes> Input{};
alignas(256) std::array<std::uint32_t, Lanes * 4u> Output{};

constexpr std::array<std::uint32_t, 16> LoadCode(std::uint32_t marker) {
    return {0x34020082u, 0xe0001000u, 0x80000201u, 0xe0301000u, 0x80000301u, 0xbf8c0000u,
            0xe0101000u, 0x80000201u, 0xe0701000u, 0x80000301u,
            0xe0701000u, 0x80010201u, 0xe0701080u + marker * 4u, 0x80010301u, 0xbf800000u | marker, 0xbf810000u};
}

constexpr std::array<std::uint32_t, 14> VectorCode(std::uint32_t marker) {
    return {0x34020084u, 0xe00c1000u, 0x80000201u, 0xbf8c0000u,
            0xe0701000u, 0x80010201u, 0xe0701004u, 0x80010301u, 0xe0701008u, 0x80010401u, 0xe070100cu, 0x80010501u,
            0xbf800000u | marker, 0xbf810000u};
}

constexpr std::array<std::uint32_t, 9> AtomicCode(std::uint32_t marker) {
    return {0x34020082u, 0x7e040285u, 0xe0c85000u, 0x80000201u, 0xbf8c0000u, 0xe0701000u, 0x80010201u, 0xbf800000u | marker, 0xbf810000u};
}

enum class Mode { Direct, Captured, Prepared };

alignas(256) constexpr std::array Codes{LoadCode(0u), LoadCode(1u), LoadCode(2u), LoadCode(3u), LoadCode(4u), LoadCode(5u)};
alignas(256) constexpr std::array VectorCodes{VectorCode(8u), VectorCode(9u), VectorCode(10u)};
alignas(256) constexpr std::array AtomicCodes{AtomicCode(12u), AtomicCode(13u), AtomicCode(14u)};

std::array<std::uint32_t, 4> Descriptor(const void* data, std::uint32_t records, std::uint32_t format, std::uint32_t stride = 0u) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    const auto oobSelect = stride != 0u ? 0u : 3u;
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, (0x01016facu & ~(0x7fu << 12u)) | (format << 12u) | (oobSelect << 28u)};
}

constexpr std::array<std::uint32_t, 4> GameNull{0x00000000u, 0x00080050u, 0x00000000u, 0x00005204u};
constexpr std::array<std::uint32_t, 4> GameNullTwin{0x00000000u, 0x00100000u, 0x00000000u, (Format32UInt << 12u) | (1u << 9u) | 5u};
constexpr std::array<std::uint32_t, 4> FloatNull{0x00000000u, 0x00080050u, 0x00000000u, (Format32Float << 12u) | (1u << 9u) | 4u};
constexpr std::array<std::uint32_t, 4> ZeroNull{0u, 0u, 0u, 0u};

std::uint32_t Pattern(std::uint32_t lane) {
    return 0x9e3779b9u * (lane + 1u) ^ 0x01234567u;
}

struct Outcome {
    std::uint64_t pipeline = 0;
    std::vector<std::uint32_t> spirv;
};

Outcome Dispatch(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, const std::array<std::uint32_t, 4>& optional, const std::string& name, Mode mode) {
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) Input[lane] = Pattern(lane);
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
    Outcome outcome;
    outcome.pipeline = result.PipelineVariantId();
    outcome.spirv = result.spirv.Copy();
    Require(outcome.pipeline != 0u && !outcome.spirv.empty(), name + ": no specialized module");
    return outcome;
}

Outcome Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t marker, const std::array<std::uint32_t, 4>& optional, const std::string& name, std::uint32_t mask, Mode mode) {
    auto outcome = Dispatch(device, code, optional, name, mode);
    const bool present = optional[2] != 0u && (optional[0] != 0u || (optional[1] & 0xffffu) != 0u);
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        Require(Input[lane] == Pattern(lane), name + ": an absent store changed its backing memory");
        const auto formatted = present ? Input[lane] & mask : 0u;
        const auto raw = present ? Input[lane] : 0u;
        Require(Output[lane] == formatted, name + ": lane " + std::to_string(lane) + " buffer_load_format_x is " + Hex(Output[lane]) + ", expected " + Hex(formatted));
        Require(Output[Lanes + marker + lane] == raw, name + ": lane " + std::to_string(lane) + " buffer_load_dword is " + Hex(Output[Lanes + marker + lane]) + ", expected " + Hex(raw));
    }
    return outcome;
}

void BoundThenNull(AgcDriver::VulkanDevice& device, Mode mode) {
    const auto marker = static_cast<std::uint32_t>(mode) * 2u;
    const auto& code = Codes[marker];
    const auto bound = Run(device, code, marker, Descriptor(Input.data(), InputBytes, Format32UInt), "bound", 0xffffffffu, mode);
    const auto absent = Run(device, code, marker, GameNull, "null V#", 0xffffffffu, mode);
    for (const auto& [descriptor, name] : {std::pair{GameNullTwin, std::string("null V# with other words")}, std::pair{FloatNull, std::string("32_FLOAT null V#")}, std::pair{ZeroNull, std::string("zero V#")}, std::pair{Descriptor(nullptr, InputBytes, Format32UInt), std::string("zero address with records")}, std::pair{Descriptor(Input.data(), 0u, Format16UInt, InputBytes), std::string("zero records with backing memory")}}) {
        const auto other = Run(device, code, marker, descriptor, name, 0xffffffffu, mode);
        Require(other.pipeline == absent.pipeline && other.spirv == absent.spirv, name + " did not specialize as the first null V#");
    }
    const auto again = Run(device, code, marker, Descriptor(Input.data(), InputBytes, Format32UInt), "bound again", 0xffffffffu, mode);
    Require(again.pipeline == bound.pipeline, "the bound slot changed modules after a null V#");
    const auto reformatted = Run(device, code, marker, Descriptor(Input.data(), 1u, Format16UInt, InputBytes), "bound 16_UINT structured stride 128", 0xffffu, mode);
    Require(reformatted.pipeline != bound.pipeline && reformatted.spirv != bound.spirv, "a bound slot with another format and stride shared the first bound slot's module");
    const auto absentAfter = Run(device, code, marker, GameNull, "null V# after another format", 0xffffffffu, mode);
    Require(absentAfter.pipeline == absent.pipeline && absentAfter.spirv == absent.spirv, "a null V# changed modules after the slot was bound with another format");
}

void NullThenBound(AgcDriver::VulkanDevice& device, Mode mode) {
    const auto marker = static_cast<std::uint32_t>(mode) * 2u + 1u;
    const auto& code = Codes[marker];
    const auto first = Run(device, code, marker, GameNull, "null V# before any bound slot", 0xffffffffu, mode);
    static_cast<void>(Run(device, code, marker, Descriptor(Input.data(), InputBytes, Format32UInt), "bound after null", 0xffffffffu, mode));
    const auto repeated = Run(device, code, marker, GameNull, "same null V# after the slot was bound", 0xffffffffu, mode);
    Require(repeated.pipeline == first.pipeline && repeated.spirv == first.spirv, "a null V# changed modules after a dispatch bound the slot");
    const auto zero = Run(device, code, marker, ZeroNull, "zero V# after the slot was bound", 0xffffffffu, mode);
    Require(zero.pipeline == first.pipeline && zero.spirv == first.spirv, "a zero V# did not specialize as the first null V#");
}

void NullFormattedVector(AgcDriver::VulkanDevice& device, Mode mode) {
    const auto& code = VectorCodes[static_cast<std::size_t>(mode)];
    std::vector<Outcome> outcomes;
    const auto expect = [&](const std::array<std::uint32_t, 4>& descriptor, const std::string& name, const std::array<std::uint32_t, 4>& expected) {
        outcomes.push_back(Dispatch(device, code, descriptor, name, mode));
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            Require(Input[lane] == Pattern(lane), name + ": a load changed its backing memory");
            for (std::uint32_t component = 0; component < 4u; ++component) {
                const auto value = Output[lane * 4u + component];
                Require(value == expected[component], name + ": lane " + std::to_string(lane) + " buffer_load_format_xyzw component " + std::to_string(component) + " is " + Hex(value) + ", expected " + Hex(expected[component]));
            }
        }
    };
    expect(GameNull, "8_UINT null V# with DST_SEL X,0,0,1", {0u, 0u, 0u, 0u});
    expect(GameNullTwin, "32_UINT null V# with DST_SEL Y,0,0,1", {0u, 0u, 0u, 0u});
    expect(FloatNull, "32_FLOAT null V# with DST_SEL X,0,0,1", {0u, 0u, 0u, 0u});
    expect(ZeroNull, "zero V#", {0u, 0u, 0u, 0u});
    for (const auto& outcome : outcomes) Require(outcome.pipeline == outcomes.front().pipeline && outcome.spirv == outcomes.front().spirv, "null V#s with other DST_SELs or formats made different xyzw modules");
}

void NullAtomic(AgcDriver::VulkanDevice& device, Mode mode) {
    const auto& code = AtomicCodes[static_cast<std::size_t>(mode)];
    static_cast<void>(Dispatch(device, code, Descriptor(Input.data(), InputBytes, Format32UInt), "bound atomic", mode));
    for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
        Require(Output[lane] == Pattern(lane), "bound atomic: lane " + std::to_string(lane) + " returned " + Hex(Output[lane]) + ", expected " + Hex(Pattern(lane)));
        Require(Input[lane] == Pattern(lane) + 5u, "bound atomic: lane " + std::to_string(lane) + " left " + Hex(Input[lane]) + ", expected " + Hex(Pattern(lane) + 5u));
    }
    for (const auto& [descriptor, name] : {std::pair{Descriptor(Input.data(), 0u, Format32UInt), std::string("zero-record atomic")}, std::pair{GameNull, std::string("null V# atomic")}}) {
        static_cast<void>(Dispatch(device, code, descriptor, name, mode));
        for (std::uint32_t lane = 0; lane < Lanes; ++lane) {
            Require(Output[lane] == 0u, name + ": lane " + std::to_string(lane) + " returned " + Hex(Output[lane]) + ", expected 0");
            Require(Input[lane] == Pattern(lane), name + ": lane " + std::to_string(lane) + " changed its backing memory to " + Hex(Input[lane]));
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::printf("null buffer specialization registrationModules=%d\n", device->Target().registrationModules);
        for (const auto mode : {Mode::Direct, Mode::Captured, Mode::Prepared}) {
            if (argc > 1 && ((std::string(argv[1]) == "--captured" && mode != Mode::Captured) || (std::string(argv[1]) == "--prepared" && mode != Mode::Prepared))) continue;
            BoundThenNull(*device, mode);
            NullThenBound(*device, mode);
            NullFormattedVector(*device, mode);
            NullAtomic(*device, mode);
            std::printf("null buffer specialization mode=%u passed\n", static_cast<unsigned>(mode));
        }
        std::puts("null buffer specialization tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

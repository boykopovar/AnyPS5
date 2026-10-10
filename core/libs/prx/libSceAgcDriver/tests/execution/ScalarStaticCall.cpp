#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t MaxThreads = 64;
constexpr std::uint32_t Inputs = 4;
constexpr std::uint32_t Results = 4;
alignas(256) std::array<std::uint32_t, MaxThreads * Inputs> Input{};
alignas(256) std::array<std::uint32_t, MaxThreads * Results> Output{};

alignas(256) constexpr std::array<std::uint32_t, 21> SwappcForward{0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xbe8a1f00u, 0x800aff0au, 0x00000014u, 0x820b800bu, 0xbe88210au, 0xbf820008u, 0x4a0808ffu, 0x13579bdfu, 0x3a0a0affu, 0xa5a5a5a5u, 0x4a0c0f06u, 0x3a0e0effu, 0x5a5a5a5au, 0xbe802008u, 0xe0781000u, 0x80010401u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 17> CallForward{0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xbb080001u, 0xbf820008u, 0x4a0808ffu, 0x13579bdfu, 0x3a0a0affu, 0xa5a5a5a5u, 0x4a0c0f06u, 0x3a0e0effu, 0x5a5a5a5au, 0xbe802008u, 0xe0781000u, 0x80010401u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 18> CallBackward{0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xbf820008u, 0x4a0808ffu, 0x13579bdfu, 0x3a0a0affu, 0xa5a5a5a5u, 0x4a0c0f06u, 0x3a0e0effu, 0x5a5a5a5au, 0xbe802008u, 0xbb08fff7u, 0xbf820000u, 0xe0781000u, 0x80010401u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 17> CallAfterEnd{0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xbb080003u, 0xe0781000u, 0x80010401u, 0xbf810000u, 0x4a0808ffu, 0x13579bdfu, 0x3a0a0affu, 0xa5a5a5a5u, 0x4a0c0f06u, 0x3a0e0effu, 0x5a5a5a5au, 0xbe802008u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 24> MixedNested{0x34020084u, 0xe0381000u, 0x80000401u, 0xbf8c3f70u, 0xbe8a1f00u, 0x800aff0au, 0x00000014u, 0x820b800bu, 0xbe88210au, 0xbf82000bu, 0x4a0808ffu, 0x13579bdfu, 0xbb0c0001u, 0xbf820006u, 0x3a0a0affu, 0xa5a5a5a5u, 0x4a0c0f06u, 0x3a0e0effu, 0x5a5a5a5au, 0xbe80200cu, 0xbe802008u, 0xe0781000u, 0x80010401u, 0xbf810000u};

constexpr std::array<std::uint32_t, 8> Selectors{0u, 1u, 2u, 3u, 4u, 7u, 0x80000000u, 0xffffffffu};
constexpr const char* Names[Results] = {"add", "xor", "sum", "xor_second"};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::string Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

std::uint32_t Expected(std::uint32_t lane, std::uint32_t result) {
    const auto selector = Input[lane * Inputs];
    const auto value = Input[lane * Inputs + 1u];
    switch (result) {
        case 0: return selector + 0x13579bdfu;
        case 1: return value ^ 0xa5a5a5a5u;
        case 2: return Input[lane * Inputs + 2u] + Input[lane * Inputs + 3u];
        default: return Input[lane * Inputs + 3u] ^ 0x5a5a5a5au;
    }
}

ShaderRecompiler::RecompileResult Compile(std::span<const std::uint32_t> code, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target) {
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {waveSize, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> code, std::uint32_t waveSize, const ShaderRecompiler::SpirvTarget& target) {
    for (std::uint32_t tid = 0; tid < MaxThreads; ++tid) {
        Input[tid * Inputs] = Selectors[tid % Selectors.size()];
        Input[tid * Inputs + 1u] = 0x9e3779b9u * (tid + 1u);
        Input[tid * Inputs + 2u] = 0xffffffffu - tid;
        Input[tid * Inputs + 3u] = 17u * tid + 1u;
    }
    Output.fill(0xdeadbeefu);
    const auto result = Compile(code, waveSize, target);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void Check(const char* variant, std::uint32_t lanes) {
    for (std::uint32_t tid = 0; tid < lanes; ++tid) {
        for (std::uint32_t index = 0; index < Results; ++index) {
            const auto actual = Output[tid * Results + index];
            const auto expected = Expected(tid, index);
            Testing::Require(actual == expected, std::string(variant) + " wave" + std::to_string(lanes) + ": lane " + std::to_string(tid) + " " + Names[index] + " is " + Hex(actual) + ", expected " + Hex(expected));
        }
    }
    for (std::uint32_t index = lanes * Results; index < Output.size(); ++index) {
        Testing::Require(Output[index] == 0xdeadbeefu, std::string(variant) + ": inactive lane wrote output");
    }
}

void RunVariant(std::span<const std::uint32_t> code, const char* name) {
    auto& device = SharedVulkanTestDevice();
    const auto defaultTarget = device.Target();
    const auto wave32Target = device.ComputeTarget(32);
    const bool distinctTarget = defaultTarget.subgroupSize != wave32Target.subgroupSize;
    std::printf("static call targets: default subgroup %u, ComputeTarget(32) subgroup %u\n", defaultTarget.subgroupSize, wave32Target.subgroupSize);
    Run(device, code, 32, wave32Target);
    Check(name, 32);
    Run(device, code, 64, defaultTarget);
    Check(name, 64);
    if (distinctTarget) {
        Run(device, code, 64, wave32Target);
        Check(name, 64);
    }
}

const Testing::Case swappcForward{"ScalarStaticCall_SwappcForward_ReturnsToCaller", [] {
    RunVariant(SwappcForward, "SWAPPC forward");
}};

const Testing::Case callForward{"ScalarStaticCall_CallForward_ReturnsToCaller", [] {
    RunVariant(CallForward, "CALL forward");
}};

const Testing::Case callBackward{"ScalarStaticCall_CallBackward_ReturnsToCaller", [] {
    RunVariant(CallBackward, "CALL backward");
}};

const Testing::Case callAfterEnd{"ScalarStaticCall_CallAfterEndpgm_ReturnsToCaller", [] {
    RunVariant(CallAfterEnd, "CALL after endpgm");
}};

const Testing::Case mixedNested{"ScalarStaticCall_MixedNestedCalls_ReturnToCallers", [] {
    RunVariant(MixedNested, "mixed nested calls");
}};

} // namespace

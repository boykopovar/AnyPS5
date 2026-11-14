#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
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
constexpr std::uint32_t Format32UInt = 20;
alignas(256) std::array<std::uint32_t, Lanes> Input{};
alignas(256) std::array<std::uint32_t, Lanes * 2u + 8u> Output{};
alignas(256) constexpr std::array<std::uint32_t, 16> Code{0x34020082u, 0xe0001000u, 0x80000201u, 0xe0301000u, 0x80000301u, 0xbf8c0000u,
                                                          0xe0101000u, 0x80000201u, 0xe0701000u, 0x80000301u,
                                                          0xe0701000u, 0x80010201u, 0xe0701080u, 0x80010301u, 0xbf800000u, 0xbf810000u};

std::array<std::uint32_t, 4> Descriptor(const void* data, std::uint32_t bytes, std::uint32_t type) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    const auto word3 = (((0x01016facu & ~(0x7fu << 12u)) | (Format32UInt << 12u) | (3u << 28u)) & ~(3u << 30u)) | (type << 30u);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, word3};
}

ShaderRecompiler::RecompileRequest Request(AgcDriver::VulkanDevice& device, std::vector<std::uint32_t>& userData) {
    static const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(Code.data()), std::as_bytes(std::span(Code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Lanes, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(Code.data()), Code, 0, {}},
        {Lanes, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    return request;
}

std::vector<std::uint32_t> UserData(std::uint32_t inputType) {
    std::vector<std::uint32_t> userData;
    const auto input = Descriptor(Input.data(), sizeof(Input), inputType);
    const auto output = Descriptor(Output.data(), sizeof(Output), 0u);
    userData.insert(userData.end(), input.begin(), input.end());
    userData.insert(userData.end(), output.begin(), output.end());
    return userData;
}

void UnsupportedTypeDeclines(AgcDriver::VulkanDevice& device) {
    auto userData = UserData(2u);
    const auto request = Request(device, userData);
    const auto handle = ShaderRecompiler::PrepareShader(request);
    const auto invocation = ShaderRecompiler::PreparedShaderInvocation::TryCreate(request, handle);
    Require(invocation.has_value(), "the prepared shader refused its own request");
    AgcDriver::DriverDetail::HeapStageMaterialized materialized;
    try {
        materialized = AgcDriver::DriverDetail::MaterializeHeapStage(*invocation, userData, request.shader.codeAddress);
    } catch (const std::exception& error) {
        Require(false, std::string("a heap walk that read words it cannot capture threw instead of leaving the stage to the synchronized capture: ") + error.what());
    }
    Require(materialized.result == nullptr, "a heap walk over an unsupported buffer type produced a stage");
    ShaderRecompiler::RequestMemoryView memoryView(request.context.memory);
    const auto runtime = memoryView.MakeRuntime(request.context.userData, request.shader.codeAddress);
    std::string message;
    try {
        static_cast<void>(invocation->Materialize(*invocation->Capture(runtime)));
    } catch (const std::exception& error) {
        message = error.what();
    }
    Require(message.find("buffer descriptor uses an unsupported type") != std::string::npos, "the synchronized capture of an unsupported buffer type did not reject it: " + message);
}

void ValidTypeYieldsStage(AgcDriver::VulkanDevice& device) {
    auto userData = UserData(0u);
    const auto request = Request(device, userData);
    const auto handle = ShaderRecompiler::PrepareShader(request);
    const auto invocation = ShaderRecompiler::PreparedShaderInvocation::TryCreate(request, handle);
    Require(invocation.has_value(), "the prepared shader refused its own request");
    const auto materialized = AgcDriver::DriverDetail::MaterializeHeapStage(*invocation, userData, request.shader.codeAddress);
    Require(materialized.result != nullptr, "a heap walk over valid buffer descriptors declined its stage");
}

void RepeatedWalksYieldStages(AgcDriver::VulkanDevice& device) {
    auto userData = UserData(0u);
    userData[6] -= 16u;
    const auto request = Request(device, userData);
    const auto handle = ShaderRecompiler::PrepareShader(request);
    const auto invocation = ShaderRecompiler::PreparedShaderInvocation::TryCreate(request, handle);
    Require(invocation.has_value(), "the prepared shader refused its own request");
    for (int walk = 0; walk < 3; ++walk) {
        const auto materialized = AgcDriver::DriverDetail::MaterializeHeapStage(*invocation, userData, request.shader.codeAddress);
        Require(materialized.result != nullptr && materialized.words != nullptr, "a repeated heap walk over valid buffer descriptors declined its stage");
    }
}

}

int main() {
    SetTestEnvironment("APS5_PROFILE_DRAW", "1");
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        UnsupportedTypeDeclines(*device);
        ValidTypeYieldsStage(*device);
        RepeatedWalksYieldStages(*device);
        std::puts("heap walk capture failure tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

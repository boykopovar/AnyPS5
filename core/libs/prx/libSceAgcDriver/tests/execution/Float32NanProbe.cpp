#include "prx/libSceAgcDriver/Execution/include/Float32NanProbe.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

alignas(256) std::array<std::uint32_t, 32 * 4> Input{};
alignas(256) std::array<std::uint32_t, 32 * 64> Output{};

std::array<std::uint32_t, 4> BufferDescriptor(const void* data, std::uint32_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(data);
    return {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu), bytes, 0x31016facu};
}

std::size_t Words(ShaderRecompiler::SpirvTarget target, bool guarded, bool ieee) {
    target.float32SingleNans = guarded;
    std::vector<std::uint32_t> userData(8, 0u);
    const auto input = BufferDescriptor(Input.data(), static_cast<std::uint32_t>(Input.size() * 4u));
    const auto output = BufferDescriptor(Output.data(), static_cast<std::uint32_t>(Output.size() * 4u));
    std::copy(input.begin(), input.end(), userData.begin());
    std::copy(output.begin(), output.end(), userData.begin() + 4);
    const auto code = AgcDriver::Float32NanProbeCode();
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{32, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        target,
        {0, 0, 0, 128}
    };
    request.useCache = false;
    if (ieee) request.context.floatMode = ShaderRecompiler::ShaderFloatMode{0xc0u, true, true, false};
    return ShaderRecompiler::Recompile(request).spirv.size();
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto target = device->Target();
        const auto emulated = Words(target, false, false);
        const auto guarded = Words(target, true, false);
        Require(guarded < emulated, "f32 NaN probe: the operand guard emits " + std::to_string(guarded) + " words, the emulated rule " + std::to_string(emulated));
        Require(Words(target, true, true) == Words(target, false, true), "f32 NaN probe: the operand guard changed a program with IEEE_MODE set");
        const auto probe = AgcDriver::ProbeFloat32SingleNans(*device, target);
        Require(probe.compared == 2u * 8u * 32u * 48u, "f32 NaN probe: compared " + std::to_string(probe.compared) + " results");
        Require(!target.float32SingleNans || probe.mismatches == 0u, "f32 NaN probe: the device uses the operand guard with " + std::to_string(probe.mismatches) + " differing results");
        std::printf("f32 NaN probe: %s, %u of %u results differ, %zu words guarded, %zu emulated\n", target.float32SingleNans ? "operand guard" : "emulated", probe.mismatches, probe.compared, guarded, emulated);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

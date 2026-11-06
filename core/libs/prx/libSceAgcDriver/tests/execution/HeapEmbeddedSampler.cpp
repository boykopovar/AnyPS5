#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "Recompiler.hpp"
#include "RuntimeAbi.hpp"
#include "HeapSampleKernel.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::uint32_t BorderClamp = 6u | (6u << 3u) | (6u << 6u);
constexpr std::uint32_t OpaqueWhiteBorder = 2u << 30u;

alignas(256) constexpr std::array<std::uint32_t, 16> ConstantSamplerCode{
    0xbe8803ffu, BorderClamp, 0xbe8903ffu, 0u,           0xbe8a03ffu, 0u,          0xbe8b03ffu, OpaqueWhiteBorder,
    0x7e0002f3u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0xe0780000u, 0x80030000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> WrapSamplerCode{
    0xbe8803ffu, 0u, 0xbe8903ffu, 0u,           0xbe8a03ffu, 0u,          0xbe8b03ffu, OpaqueWhiteBorder,
    0x7e0002f3u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0xe0780000u, 0x80030000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 16> MirrorSamplerCode{
    0xbe8803ffu, 1u | (1u << 3u) | (1u << 6u), 0xbe8903ffu, 0u, 0xbe8a03ffu, 0u,          0xbe8b03ffu, OpaqueWhiteBorder,
    0x7e0002f3u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u, 0xbf8c0000u, 0xe0780000u, 0x80030000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> UserSamplerCode{0x7e0002f3u, 0x7e0202f0u, 0xf09c0f08u, 0x00400000u,
                                                                    0xbf8c0000u, 0xe0780000u, 0x80030000u, 0xbf810000u};
constexpr std::array<std::uint8_t, 4> White{0xffu, 0xffu, 0xffu, 0xffu};

alignas(256) std::array<std::uint8_t, 1024> TextureMemory{};
alignas(4096) std::array<std::array<float, 1024>, 3> Outputs{};

ShaderRecompiler::RecompileResult Compile(AgcDriver::VulkanDevice& device, float* output, std::span<const std::uint32_t> code, bool userSampler) {
    auto userData = SampleUserData(TextureMemory.data(), output);
    if (userSampler) {
        userData[8] = BorderClamp;
        userData[11] = OpaqueWhiteBorder;
    }
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(code)}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{1u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}},
        {32u, 0u, userData, compute, std::nullopt, std::nullopt, memory},
        device.ComputeTarget(32u),
        {0u, 0u, 0u, 128u}};
    request.useCache = false;
    return ShaderRecompiler::Recompile(request);
}

std::size_t EmbeddedElements(const ShaderRecompiler::RecompileResult& result) {
    std::size_t count = 0;
    for (const auto& binding : result.bindings) {
        if (binding.role != ShaderRecompiler::DescriptorRole::GuestSamplers) continue;
        count += static_cast<std::size_t>(std::ranges::count_if(binding.samplerEmbedded, [](std::uint32_t value) { return value != ShaderRecompiler::RuntimeAbi::NoEmbeddedSampler; }));
    }
    return count;
}

bool DecoratesEmbeddedSet(const ShaderRecompiler::RecompileResult& result) {
    const auto& spirv = result.spirv;
    for (std::size_t at = 5u; at < spirv.size();) {
        const auto count = spirv[at] >> 16u;
        if (count == 0u) break;
        if ((spirv[at] & 0xffffu) == 71u && count >= 4u && spirv[at + 2u] == 34u && spirv[at + 3u] == ShaderRecompiler::RuntimeAbi::EmbeddedSamplerSet) return true;
        at += count;
    }
    return false;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const bool heap = std::getenv("APS5_GPU_HEAP") != nullptr;
        if (heap && SkipWithoutDescriptorHeap()) return VulkanTestSkipped;
        const bool embedding = heap;
        FillTexels(TextureMemory, SampleTexel);
        for (auto& output : Outputs) output.fill(-1.0f);
        const auto constant = Compile(*device, Outputs[0].data(), ConstantSamplerCode, false);
        const auto user = Compile(*device, Outputs[1].data(), UserSamplerCode, true);
        const auto wrap = Compile(*device, Outputs[2].data(), WrapSamplerCode, false);
        const auto mirror = Compile(*device, Outputs[2].data(), MirrorSamplerCode, false);
        Require(constant.gpuResolvedImages == heap, "the constant-sampler stage is not in the expected mode");
        Require((EmbeddedElements(constant) != 0u) == embedding, embedding ? "a constant sampler was not embedded" : "a sampler was embedded outside heap mode");
        Require(DecoratesEmbeddedSet(constant) == embedding, "the stage's SPIR-V does not match its embedded samplers");
        Require(EmbeddedElements(user) == 0u && !DecoratesEmbeddedSet(user), "a sampler read from user data was embedded");
        Require((EmbeddedElements(wrap) != 0u) == embedding, "the wrapping constant sampler was not embedded like the clamping one");
        Require(!embedding || wrap.Identity() != constant.Identity(),
                "two compute stages differing only in an embedded sampler share one pipeline identity");
        Require(!embedding || mirror.Identity() != wrap.Identity(),
                "two compute stages whose embedded samplers differ only in their first word share one pipeline identity");
        const auto* existing = AgcDriver::Graphics::BindlessHeap::Existing(device->graphicsContext());
        const auto embeddedBefore = existing != nullptr ? existing->EmbeddedSamplerCount() : 0u;
        device->Dispatch(constant, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(ConstantSamplerCode.data()));
        device->Dispatch(user, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(UserSamplerCode.data()));
        device->Dispatch(wrap, 1u, 1u, 1u, {}, reinterpret_cast<std::uintptr_t>(WrapSamplerCode.data()));
        device->WaitIdle();
        RequireSampled(Outputs[0].data(), White, "the constant sampler's opaque white border", heap);
        RequireSampled(Outputs[1].data(), White, "the user-data sampler's opaque white border", heap);
        RequireSampled(Outputs[2].data(), SampleTexel, "the wrapping constant sampler's texel", heap);
        if (embedding) {
            const bool capped = std::getenv("APS5_EMBEDDED_SAMPLER_CAP") != nullptr;
            const auto* heapObject = AgcDriver::Graphics::BindlessHeap::Existing(device->graphicsContext());
            Require(heapObject != nullptr, "the heap dispatch left no descriptor heap");
            Require(capped ? heapObject->EmbeddedSamplerCount() == 0u : heapObject->EmbeddedSamplerCount() > embeddedBefore,
                    capped ? "a capped device embedded a sampler past its limit" : "the pipeline did not embed the constant sampler");
        }
        const bool capped = std::getenv("APS5_EMBEDDED_SAMPLER_CAP") != nullptr;
        std::cout << "constant samplers sampled " << (!embedding ? "through descriptors" : capped ? "from sampler heap slots past the embedded sampler limit" : "as embedded samplers") << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

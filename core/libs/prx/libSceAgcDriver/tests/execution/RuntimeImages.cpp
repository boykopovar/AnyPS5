#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <span>
#include <string_view>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) std::array<std::array<std::uint32_t, 64>, 2> textures{};
alignas(256) std::array<std::uint32_t, 64> output{};
constexpr std::array<std::uint32_t, 10> code{0x7e3c02ffu, 0x3fa00000u, 0x7e3e02ffu, 0x3f000000u, 0xf09c0108u, 0x00610a1eu, 0x34060082u, 0xe0701000u, 0x80000a03u, 0xbf810000u};
constexpr std::array<std::uint32_t, 18> loopCode{0x7e3c02ffu, 0x3fa00000u, 0x7e3e02ffu, 0x3f000000u, 0xbe940380u, 0x7e180280u, 0xf09c0108u, 0x00610a1eu, 0x4a18190au, 0x063c3cffu, 0x3d000000u, 0x80148114u, 0xbf0a8314u, 0xbf85fff8u, 0x34060082u, 0xe0701000u, 0x80000c03u, 0xbf810000u};

void Run(AgcDriver::VulkanDevice& device, std::span<const std::uint32_t> shaderCode, std::uint32_t iterations, std::uint32_t waveSize) {
    ShaderRecompiler::CompiledShaderArtifact first;
    const std::array formats{20u, 21u, 22u, 34u};
    for (std::uint32_t iteration = 0u; iteration < 17u; ++iteration) {
        const auto format = formats[iteration % formats.size()];
        const bool clamped = (iteration & 4u) != 0u;
        const bool swizzled = (iteration & 8u) != 0u;
        auto& texels = textures[iteration % textures.size()];
        for (std::uint32_t pixel = 0u; pixel < 32u; ++pixel) {
            texels[pixel] = format == 22u ? std::bit_cast<std::uint32_t>(static_cast<float>(pixel + 100u)) : format == 21u ? 0u - pixel - 100u : format == 34u ? (pixel + 100u) | ((pixel + 200u) << 11u) | ((pixel + 300u) << 22u) : pixel + 100u;
        }
        output.fill(0xdeadbeefu);
        const auto bufferAddress = reinterpret_cast<std::uintptr_t>(output.data());
        const auto imageAddress = reinterpret_cast<std::uintptr_t>(texels.data());
        const auto swizzle = swizzled ? (format == 34u ? 0xfadu : 0xfa9u) : 0xfacu;
        std::array<std::uint32_t, 16> userData{static_cast<std::uint32_t>(bufferAddress), static_cast<std::uint32_t>(bufferAddress >> 32u), 256u, 0x31016facu, static_cast<std::uint32_t>(imageAddress >> 8u), static_cast<std::uint32_t>(imageAddress >> 40u) | (format << 20u) | (3u << 30u), 7u, 0x90000000u | swizzle, 0u, 0u, 0u, 0u, clamped ? 2u : 0u, 0u, 0u, 0u};
        if (iteration == 16u) std::fill(userData.begin() + 4u, userData.begin() + 12u, 0u);
        const ShaderRecompiler::ShaderComputeStageInfo compute{{waveSize, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(shaderCode.data()), shaderCode, 0u, {}}, {waveSize, 0u, userData, compute, std::nullopt, std::nullopt, {}}, device.Target(), {0u, 0u, 0u, 128u}};
        const auto shader = ShaderRecompiler::Recompile(request);
        if (first.variantId == 0u) first = shader;
        else Require(shader.cacheHit && shader.variantId == first.variantId, "T#/S# changed the compiled artifact");
        device.Dispatch(shader, 1u, 1u, 1u);
        device.WaitIdle();
        std::uint32_t expected = 0u;
        for (std::uint32_t sample = 0u; sample < iterations; ++sample) {
            const auto pixel = clamped ? 31u : 8u + sample;
            expected += iteration == 16u ? 0u : swizzled ? (format == 34u ? pixel + 200u : format == 22u ? 0x3f800000u : 1u) : format == 34u ? pixel + 100u : texels[pixel];
        }
        for (std::uint32_t lane = 0u; lane < output.size(); ++lane) {
            const auto laneExpected = lane < waveSize ? expected : 0xdeadbeefu;
            Require(output[lane] == laneExpected, "runtime image/sampler selection failed: wave=" + std::to_string(waveSize) + " iterations=" + std::to_string(iterations) + " iteration=" + std::to_string(iteration) + " lane=" + std::to_string(lane) + " value=" + std::to_string(output[lane]) + " expected=" + std::to_string(laneExpected));
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--wave64"), "expected no argument or --wave64");
        const auto waveSize = argc == 2 ? 64u : 32u;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (waveSize == 64u && device->Target().subgroupSize < 32u) {
            std::cout << "wave64 runtime images skipped, subgroup size " << device->Target().subgroupSize << " cannot hold two guest lanes\n";
            return VulkanTestSkipped;
        }
        Run(*device, code, 1u, waveSize);
        Run(*device, loopCode, 3u, waveSize);
        std::cout << "runtime image and sampler tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

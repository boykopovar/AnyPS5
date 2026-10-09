#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <memory>
#include <set>
#include <spirv/unified1/spirv.hpp>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) std::array<std::array<std::uint32_t, 64>, 2> textures{};
alignas(256) std::array<std::uint32_t, 64> output{};
constexpr std::array<std::uint32_t, 10> code{0x7e3c02ffu, 0x3fa00000u, 0x7e3e02ffu, 0x3f000000u, 0xf09c0108u, 0x00610a1eu, 0x34060082u, 0xe0701000u, 0x80000a03u, 0xbf810000u};

void RunLoop(AgcDriver::VulkanDevice& device, std::uint32_t waveSize) {
    alignas(256) static std::array<std::uint32_t, 256> texels{};
    constexpr std::array<std::uint32_t, 12> loopCode{
        0xbe8c0381u, 0x7e040280u, 0x7e020280u, 0xbf0a840cu, 0xbf840006u,
        0x4a04040cu, 0xf0201108u, 0x00010200u, 0x4a020281u, 0x800c810cu, 0xbf82fff8u, 0xbf810000u,
    };
    GuestAllocations::Mutation().Add(texels.data(), sizeof(texels), true, true);
    const auto deregister = [](std::uint32_t* pointer) { GuestAllocations::Mutation().Remove(pointer); };
    const std::unique_ptr<std::uint32_t, decltype(deregister)> registration(texels.data(), deregister);
    texels.fill(0xdeadbeefu);
    const auto imageAddress = reinterpret_cast<std::uintptr_t>(texels.data());
    std::array<std::uint32_t, 16> userData{};
    const std::array<std::uint32_t, 8> image{
        static_cast<std::uint32_t>(imageAddress >> 8u), static_cast<std::uint32_t>(imageAddress >> 40u) | (20u << 20u) | (3u << 30u),
        15u | (3u << 14u), 0x90000facu, 0u, 0u, 0u, 0u,
    };
    std::copy(image.begin(), image.end(), userData.begin() + 4u);
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(loopCode.data()), loopCode, 0u, {}},
        {waveSize, 0u, userData, compute, std::nullopt, std::nullopt, {}}, device.Target(), {0u, 0u, 0u, 128u},
    };
    request.useCache = false;
    const auto shader = ShaderRecompiler::Recompile(request);
    std::set<std::uint32_t> continuations;
    const auto& words = shader.spirv;
    for (std::size_t offset = 5u; offset < words.size(); offset += words[offset] >> 16u) {
        if ((words[offset] & 0xffffu) == spv::OpLoopMerge) continuations.insert(words[offset + 2u]);
    }
    Require(!continuations.empty(), "runtime image loop lost its loop construct");
    std::uint32_t label = 0u;
    for (std::size_t offset = 5u; offset < words.size(); offset += words[offset] >> 16u) {
        const auto opcode = words[offset] & 0xffffu;
        if (opcode == spv::OpLabel) label = words[offset + 1u];
        if (continuations.contains(label)) {
            Require(opcode != spv::OpUnreachable && opcode != spv::OpReturn && opcode != spv::OpReturnValue &&
                opcode != spv::OpKill && opcode != spv::OpTerminateInvocation,
                "runtime image loop continuation includes a terminating path: label=" +
                std::to_string(label) + " opcode=" + std::to_string(opcode));
        }
    }
    device.Dispatch(shader, 1u, 1u, 1u);
    device.WaitIdle();
    AgcDriver::Graphics::StorageTexture::FlushPending(imageAddress, sizeof(texels), nullptr, "test");
    device.WaitIdle();
    constexpr std::array expected{1u, 3u, 6u, 0xdeadbeefu};
    for (std::uint32_t row = 0u; row < expected.size(); ++row) {
        for (std::uint32_t lane = 0u; lane < 64u; ++lane) {
            Require(texels[row * 64u + lane] == expected[row], "runtime image loop did not preserve all three stores and exit: wave=" +
                std::to_string(waveSize) + " row=" + std::to_string(row) + " lane=" + std::to_string(lane) + " actual=" +
                std::to_string(texels[row * 64u + lane]) + " expected=" + std::to_string(expected[row]));
        }
    }
}

void Run(AgcDriver::VulkanDevice& device) {
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
        const ShaderRecompiler::ShaderComputeStageInfo compute{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0u, {}}, {32u, 0u, userData, compute, std::nullopt, std::nullopt, {}}, device.Target(), {0u, 0u, 0u, 128u}};
        const auto shader = ShaderRecompiler::Recompile(request);
        if (first.variantId == 0u) first = shader;
        else Require(shader.cacheHit && shader.variantId == first.variantId, "T#/S# changed the compiled artifact");
        device.Dispatch(shader, 1u, 1u, 1u);
        device.WaitIdle();
        const auto pixel = clamped ? 31u : 8u;
        const auto expected = iteration == 16u ? 0u : swizzled ? (format == 34u ? pixel + 200u : format == 22u ? 0x3f800000u : 1u) : format == 34u ? pixel + 100u : texels[pixel];
        for (std::uint32_t lane = 0u; lane < 32u; ++lane) Require(output[lane] == expected, "runtime image/sampler selection failed: iteration=" + std::to_string(iteration) + " value=" + std::to_string(output[lane]) + " expected=" + std::to_string(expected));
    }
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        RunLoop(*device, 32u);
        RunLoop(*device, 64u);
        Run(*device);
        std::cout << "runtime image and sampler tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

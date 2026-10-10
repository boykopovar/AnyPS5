#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Extent = 64u;
alignas(256) std::array<std::byte, Extent * Extent * 4u> pixels{};
alignas(256) std::array<std::byte, 256> vertices{};
alignas(256) std::array<std::byte, 256> movedVertices{};
constexpr std::array<std::array<float, 4>, 3> positions{{{-1.0f, -1.0f, 0.0f, 1.0f}, {3.0f, -1.0f, 0.0f, 1.0f}, {-1.0f, 3.0f, 0.0f, 1.0f}}};
constexpr std::array<std::uint32_t, 11> vertexCode{0xf4080100u, 0xfa000000u, 0x4a0a0a02u, 0x4a0a0b08u, 0x4a0a0a03u, 0xe00c2000u, 0x80010005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
constexpr std::array<std::uint32_t, 5> pixelCode{0x7e0002f2u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u};

void RequireIncompatibleAbiRejected(AgcDriver::VulkanDevice& device) {
    ShaderRecompiler::RecompileResult incompatible;
    incompatible.runtimeAbiVersion = ShaderRecompiler::RuntimeAbi::Version + 1u;
    const auto rejectAbi = [](auto action) {
        std::string failure;
        try {
            action();
        } catch (const std::runtime_error& error) {
            failure = error.what();
        }
        Testing::Require(!failure.empty(), "driver accepted an incompatible shader runtime ABI");
        Testing::Require(failure.find("incompatible version") != std::string::npos, "unexpected runtime ABI validation error: " + failure);
    };
    rejectAbi([&] { device.PrepareDispatch(incompatible, {}); });
    rejectAbi([&] { device.Dispatch(incompatible, 1u, 1u, 1u); });
    rejectAbi([&] { device.DispatchIndirect(incompatible, 0u); });
    const std::array<AgcDriver::Graphics::CompiledShader, 1> incompatibleShaders{{{ShaderStage::Vertex, &incompatible, 0u}}};
    rejectAbi([&] { device.Draw({}, {}, incompatibleShaders); });
}

void DrawWithRuntimeFetch(AgcDriver::VulkanDevice& device) {
    std::array<std::uint32_t, 4> descriptor{};
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(descriptor.data());
    std::array<std::uint32_t, 4> userData{static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u), 1u, 1u};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{tableAddress, std::as_bytes(std::span(descriptor))}}};
    ShaderRecompiler::ShaderVertexStageInfo vertexInfo{};
    vertexInfo.fetchEmbedded = true;
    vertexInfo.resourcesNum = 1u;
    vertexInfo.resourcesDst[0] = {0, 4, 0, 0};
    ShaderRecompiler::RecompileRequest request{{ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(vertexCode.data()), vertexCode, 0, {}}, {32u, 0u, userData, std::nullopt, std::nullopt, vertexInfo, memory}, device.Target(), {0u, 0u, 0u, 96u}};
    ShaderRecompiler::RecompileResult first;
    struct Case {
        std::uint32_t stride;
        std::uint32_t format;
        bool moved;
        std::uint32_t offset;
    };
    constexpr std::array<Case, 4> cases{{{16u, 77u, false, 1u}, {32u, 77u, true, 0u}, {8u, 71u, false, 1u}, {16u, 71u, true, 0u}}};
    for (const auto& value : cases) {
        auto& storage = value.moved ? movedVertices : vertices;
        storage.fill(std::byte{0});
        userData[2] = value.offset;
        const auto firstRecord = value.offset + userData[3] + 2u;
        for (std::size_t index = 0; index < positions.size(); ++index) {
            auto* destination = storage.data() + (firstRecord + index) * value.stride;
            if (value.format == 77u) std::memcpy(destination, positions[index].data(), 16u);
            else {
                for (std::size_t component = 0; component < 4u; ++component) {
                    const auto scalar = positions[index][component];
                    const std::uint16_t half = scalar == -1.0f ? 0xbc00u : scalar == 3.0f ? 0x4200u : scalar == 1.0f ? 0x3c00u : 0u;
                    std::memcpy(destination + component * 2u, &half, sizeof(half));
                }
            }
        }
        const auto address = reinterpret_cast<std::uintptr_t>(storage.data());
        descriptor = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (value.stride << 16u), 8u, 0x01000facu | (value.format << 12u)};
        request.context.vertex->resources[0].fields = descriptor;
        const auto vertex = ShaderRecompiler::Recompile(request);
        Testing::Require(vertex.vertexInputs.empty() && vertex.vertexAttributes.empty(), "runtime vertex fetch created Vulkan vertex attributes");
        Testing::Require(vertex.vertexOffsetSgpr == -1 && vertex.instanceOffsetSgpr == -1, "runtime vertex fetch requested draw offset folding");
        Testing::Require(vertex.runtimeAbiVersion == ShaderRecompiler::RuntimeAbi::Version, "runtime vertex fetch has an incompatible runtime ABI");
        if (first.spirv.empty()) first = vertex;
        else Testing::Require(vertex.cacheHit && vertex.variantId == first.variantId, "runtime vertex descriptor changed the artifact");
        const auto pushBytes = static_cast<std::uint32_t>(vertex.pushConstants.size());
        ShaderRecompiler::ShaderPixelStageInfo pixelInfo{};
        pixelInfo.wave32 = true;
        pixelInfo.targetOutputMode[0] = 9u;
        pixelInfo.targetExportMapping.fill(0xe4u);
        ShaderRecompiler::RecompileRequest pixelRequest{{ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}}, {32u, 0u, {}, std::nullopt, pixelInfo, std::nullopt, {}}, device.Target(), {0u, 0u, pushBytes, 128u - pushBytes}};
        const auto pixel = ShaderRecompiler::Recompile(pixelRequest);
        const std::array<AgcDriver::Graphics::CompiledShader, 2> shaders{{{ShaderStage::Vertex, &vertex, 0u}, {ShaderStage::Fragment, &pixel, pushBytes}}};
        AgcDriver::Graphics::State state{};
        state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
        state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {Extent, Extent}, VK_FORMAT_R8G8B8A8_UNORM, pixels.size(), 0xe4u};
        state.colors = {state.color};
        state.hasColorTarget = true;
        state.renderExtent = {Extent, Extent};
        state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        state.viewport = {0.0f, static_cast<float>(Extent), static_cast<float>(Extent), -static_cast<float>(Extent), 0.0f, 1.0f};
        state.scissor = {{0, 0}, {Extent, Extent}};
        state.cullMode = VK_CULL_MODE_NONE;
        state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        state.blend.colorWriteMask = 15u;
        state.blends = {state.blend};
        AgcDriver::Pm4::DrawParameters draw{0u, 3u, 0u, 1u, 0u, false};
        draw.firstVertex = 1u;
        draw.firstInstance = 1u;
        pixels.fill(std::byte{0});
        device.Draw(state, draw, shaders);
        device.WaitIdle();
        for (std::size_t index = 0; index < pixels.size(); index += 4u) {
            Testing::Require(pixels[index] == std::byte{255} && pixels[index + 1u] == std::byte{0} && pixels[index + 2u] == std::byte{0} && pixels[index + 3u] == std::byte{255}, "runtime vertex fetch produced the wrong triangle");
        }
    }
}


const Testing::Case incompatibleAbi{"RuntimeAbi_IncompatibleVersion_IsRejectedByEveryEntryPoint", [] {
    RequireIncompatibleAbiRejected(SharedVulkanTestDevice());
}};

const Testing::Case runtimeVertexFetch{"RuntimeVertexFetch_DescriptorVariants_DrawTheTriangleWithOneArtifact", [] {
    DrawWithRuntimeFetch(SharedVulkanTestDevice());
}};

} // namespace

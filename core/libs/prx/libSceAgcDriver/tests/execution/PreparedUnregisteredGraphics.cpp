#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

alignas(256) constexpr std::array<std::uint32_t, 64> VertexCode{0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 64> PixelCode{0xc8020002u, 0xc8060102u, 0xc80a0202u, 0xc80e0302u, 0xf800180fu, 0x03020100u, 0xbf810000u};

void Bind(AgcDriver::QueueState& queue, const void* code, std::uint32_t program, std::uint32_t resources) {
    const auto address = reinterpret_cast<std::uintptr_t>(code);
    queue.shader[program] = static_cast<std::uint32_t>(address >> 8u);
    queue.shader[program + 1u] = static_cast<std::uint32_t>(address >> 40u);
    queue.shader[resources] = 16u << 1u;
}

void Check(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::DriverDetail;
    using namespace ShaderRecompiler;
    AgcDriver::QueueState queue{};
    queue.context[0x8e] = 0xfu;
    queue.context[0x8f] = 0xfu;
    Bind(queue, VertexCode.data(), 0xc8u, 0x8bu);
    Bind(queue, PixelCode.data(), 0x8u, 0xbu);
    DrawDecode draw{};
    draw.state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
    draw.state.stages.vertexWaveSize = 32u;
    draw.state.stages.fragmentWaveSize = 32u;
    draw.pixel.wave32 = true;
    draw.pixel.interpolatorCount = 2;
    draw.pixel.interpolatorSettings[0] = 0x403u;
    draw.pixel.interpolatorSettings[1] = 0x220u;
    draw.pixel.targetOutputMode[0] = 9;
    draw.pixel.targetExportMapping.fill(0xe4u);
    const ShaderRegistry registry;
    DecodeGraphicsPrograms(draw, queue, registry, false, true);
    Require(draw.programs.size() == 2 && draw.roles.back() == ProgramRole::Fragment, "unregistered graphics programs did not decode as a vertex and a fragment stage");
    for (const auto& program : draw.programs) Require(program.snapshot != nullptr && program.snapshot->header.empty() && program.snapshot->prepared->entries.empty(), "an unregistered graphics program was not read as raw code");
    const auto target = device.Target();
    std::vector<LinkedProgram> linked;
    std::vector<MemoryRegion> memory;
    for (std::size_t index = 0; index < draw.programs.size(); ++index) {
        const auto& program = draw.programs[index];
        linked.push_back({draw.roles[index], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
        memory.insert(memory.end(), program.memory.begin(), program.memory.end());
    }
    std::array<std::uint64_t, 2> variants{};
    for (std::size_t index = 0; index < draw.programs.size(); ++index) {
        const auto& program = draw.programs[index];
        const bool pixel = program.binary.stage == ShaderStage::Fragment;
        std::optional<ShaderVertexStageInfo> vertex;
        if (!pixel) vertex = AgcDriver::Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, nullptr, true);
        RecompileRequest request{program.binary, {32u, program.firstUserSgpr, program.userData, {}, pixel ? std::optional(draw.pixel) : std::nullopt, vertex, memory}, target, {0, 0, 0, 128u}, GraphicsCompileContext{program.firstUserSgpr, linked, std::nullopt, std::nullopt, {0, 3, 4, 1}}};
        static_cast<void>(InvocationFor(*program.snapshot, program.codeOffset, request));
        Require(program.snapshot->prepared->entries.size() == 1, "an unregistered graphics program was not prepared at first use");
        variants[index] = GetPreparedArtifact(*program.snapshot->prepared->entries.front().handle).variantId;
        static_cast<void>(InvocationFor(*program.snapshot, program.codeOffset, request));
        Require(program.snapshot->prepared->entries.size() == 1, "a prepared unregistered graphics program was prepared again");
        request.layout.pushConstantOffsetBytes = 4;
        request.layout.pushConstantSizeBytes -= 4;
        static_cast<void>(InvocationFor(*program.snapshot, program.codeOffset, request));
        Require(program.snapshot->prepared->entries.size() == 1, "a relocated push-constant block prepared an unregistered graphics program again");
    }
    ResolvePreparedGraphics(*draw.programs[0].snapshot, draw.programs[1].snapshot, 17, target);
    const auto rectangle = PreparedRectangle(*draw.programs[0].snapshot, variants[0], variants[1]);
    Require(!rectangle.control.spirv.empty() && !rectangle.evaluation.spirv.empty(), "unregistered graphics programs did not prepare rectangle shaders");
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Check(*device);
        std::cout << "prepared unregistered graphics tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

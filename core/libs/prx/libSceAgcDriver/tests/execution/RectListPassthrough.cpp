#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "SpirvBackend/SpirvModule.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include "VulkanTestDevice.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using ShaderRecompiler::ShaderStage;

constexpr std::uint32_t Extent = 64;

alignas(256) std::array<std::byte, Extent * Extent * 4> pixels{};

class Emitter {
public:
    explicit Emitter(std::uint32_t version) : builder(version) {
        builder.EmitCapability(spv::CapabilityShader);
        builder.AddMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
        voidType = type(spv::OpTypeVoid);
        intType = type(spv::OpTypeInt, 32u, 1u);
        floatType = type(spv::OpTypeFloat, 32u);
        vec4Type = type(spv::OpTypeVector, floatType, 4u);
        functionType = type(spv::OpTypeFunction, voidType);
    }

    std::vector<std::uint32_t> Vertex() {
        const auto main = result(spv::OpFunction, voidType, spv::FunctionControlMaskNone, functionType);
        const auto indexPointer = pointer(spv::StorageClassInput, intType);
        const auto vectorPointer = pointer(spv::StorageClassOutput, vec4Type);
        const auto vertexIndex = builder.DefineGlobalVariable(indexPointer, spv::StorageClassInput);
        const auto position = builder.DefineGlobalVariable(vectorPointer, spv::StorageClassOutput);
        const auto identity = builder.DefineGlobalVariable(vectorPointer, spv::StorageClassOutput);
        decorate(vertexIndex, spv::DecorationBuiltIn, spv::BuiltInVertexIndex);
        decorate(position, spv::DecorationBuiltIn, spv::BuiltInPosition);
        decorate(identity, spv::DecorationLocation, 0u);
        builder.EmitEntryPoint(spv::ExecutionModelVertex, main, "main", {vertexIndex, position, identity});
        result(spv::OpLabel);
        const auto index = result(spv::OpLoad, intType, vertexIndex);
        const auto first = result(spv::OpIEqual, type(spv::OpTypeBool), index, constant(intType, 0u));
        const auto second = result(spv::OpIEqual, type(spv::OpTypeBool), index, constant(intType, 1u));
        const auto positionValue = result(spv::OpSelect, vec4Type, first, vector(-1.0f, -1.0f, 0.0f, 1.0f), result(spv::OpSelect, vec4Type, second, vector(3.0f, -1.0f, 0.0f, 1.0f), vector(-1.0f, 3.0f, 0.0f, 1.0f)));
        emit(spv::OpStore, position, positionValue);
        emit(spv::OpStore, identity, vector(1.0f, 0.0f, 0.0f, 1.0f));
        emit(spv::OpReturn);
        emit(spv::OpFunctionEnd);
        return builder.Finalize();
    }

    std::vector<std::uint32_t> Fragment() {
        const auto main = result(spv::OpFunction, voidType, spv::FunctionControlMaskNone, functionType);
        const auto vectorPointerIn = pointer(spv::StorageClassInput, vec4Type);
        const auto vectorPointerOut = pointer(spv::StorageClassOutput, vec4Type);
        const auto parameter = builder.DefineGlobalVariable(vectorPointerIn, spv::StorageClassInput);
        const auto color = builder.DefineGlobalVariable(vectorPointerOut, spv::StorageClassOutput);
        decorate(parameter, spv::DecorationLocation, 1u);
        decorate(color, spv::DecorationLocation, 0u);
        builder.EmitEntryPoint(spv::ExecutionModelFragment, main, "main", {parameter, color});
        builder.AddExecutionMode(main, spv::ExecutionModeOriginUpperLeft);
        result(spv::OpLabel);
        const auto value = result(spv::OpLoad, vec4Type, parameter);
        const auto red = result(spv::OpCompositeExtract, floatType, value, 0u);
        emit(spv::OpStore, color, result(spv::OpCompositeConstruct, vec4Type, red, constant(floatType, std::bit_cast<std::uint32_t>(0.25f)), constant(floatType, std::bit_cast<std::uint32_t>(0.5f)), constant(floatType, std::bit_cast<std::uint32_t>(1.0f))));
        emit(spv::OpReturn);
        emit(spv::OpFunctionEnd);
        return builder.Finalize();
    }

private:
    template <typename... TArgs>
    std::uint32_t type(spv::Op opcode, TArgs... operands) {
        return builder.Type(opcode, operands...);
    }

    std::uint32_t pointer(spv::StorageClass storage, std::uint32_t valueType) { return type(spv::OpTypePointer, storage, valueType); }

    std::uint32_t constant(std::uint32_t constantType, std::uint32_t value) { return builder.Constant(spv::OpConstant, constantType, value); }

    std::uint32_t vector(float x, float y, float z, float w) {
        return builder.Constant(spv::OpConstantComposite, vec4Type, constant(floatType, std::bit_cast<std::uint32_t>(x)), constant(floatType, std::bit_cast<std::uint32_t>(y)), constant(floatType, std::bit_cast<std::uint32_t>(z)), constant(floatType, std::bit_cast<std::uint32_t>(w)));
    }

    template <typename... TArgs>
    std::uint32_t result(spv::Op opcode, std::uint32_t resultType, TArgs... operands) {
        const auto id = builder.AllocateId();
        builder.AddFunction(opcode, resultType, id, operands...);
        return id;
    }

    std::uint32_t result(spv::Op opcode) {
        const auto id = builder.AllocateId();
        builder.AddFunction(opcode, id);
        return id;
    }

    template <typename... TArgs>
    void emit(spv::Op opcode, TArgs... operands) {
        builder.AddFunction(opcode, operands...);
    }

    void decorate(std::uint32_t target, spv::Decoration decoration, std::uint32_t value) {
        builder.AddAnnotation(spv::OpDecorate, target, decoration, value);
    }

    ShaderRecompiler::SpirvModule builder;
    std::uint32_t voidType = 0;
    std::uint32_t intType = 0;
    std::uint32_t floatType = 0;
    std::uint32_t vec4Type = 0;
    std::uint32_t functionType = 0;
};

int Run(AgcDriver::VulkanDevice& device) {
    std::cout << "device: " << device.DeviceName() << "\n";
    auto target = device.Target();
    if (!target.tessellation.has_value() || !TargetHasCapability(target, spv::CapabilityTessellation)) {
        std::cout << "skipped, device has no tessellation\n";
        return VulkanTestSkipped;
    }
    Require(target.spirvVersion >= 0x00010300u, "rect-list pass-through draw needs SPIR-V 1.3");
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult fragment;
    vertex.spirv = ShaderRecompiler::SharedSpirv(Emitter(target.spirvVersion).Vertex());
    fragment.spirv = ShaderRecompiler::SharedSpirv(Emitter(target.spirvVersion).Fragment());
#if ANYPS5_ENABLE_SPIRV_TOOLS
    static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(vertex.spirv, target.vulkanVersion, target.spirvVersion, target.nonConstantImageOffsets, false));
    static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(fragment.spirv, target.vulkanVersion, target.spirvVersion, target.nonConstantImageOffsets, false));
#endif
    vertex.parameterExports = {0u};
    fragment.fragmentParameters = {{1u, 1u, false, false, true}};
    bool refused = false;
    try {
        auto exported = vertex;
        exported.parameterExports = {0u, 1u};
        static_cast<void>(ShaderRecompiler::BuildRectListShaders(exported, fragment, target));
    } catch (const std::exception& error) {
        refused = std::string(error.what()).find("custom per-vertex interpolation of an exported parameter is unsupported") != std::string::npos;
    }
    Require(refused, "a pass-through parameter the vertex stage exports was not refused");
    const auto rectangle = ShaderRecompiler::BuildRectListShaders(vertex, fragment, target);
    const std::array<AgcDriver::Graphics::CompiledShader, 4> shaders{{
        {ShaderStage::Vertex, &vertex, 0u},
        {ShaderStage::TessellationControl, &rectangle.control, 0u},
        {ShaderStage::TessellationEvaluation, &rectangle.evaluation, 0u},
        {ShaderStage::Fragment, &fragment, PixelPushOffset(0u, device.Target())},
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {Extent, Extent}, VK_FORMAT_R8G8B8A8_UNORM, pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.rectList = true;
    state.renderExtent = {Extent, Extent};
    state.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    state.viewport = {0.0f, static_cast<float>(Extent), static_cast<float>(Extent), -static_cast<float>(Extent), 0.0f, 1.0f};
    state.scissor = {{0, 0}, {Extent, Extent}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15u;
    state.blends = {state.blend};
    pixels.fill(std::byte{0});
    device.Draw(state, {0u, 3u, 0u, 1u, 0u, false}, shaders);
    device.WaitIdle();
    std::uint32_t covered = 0;
    std::uint32_t red = 0;
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        if (pixels[offset + 3] == std::byte{0}) continue;
        ++covered;
        red = std::max(red, std::to_integer<std::uint32_t>(pixels[offset]));
    }
    std::cout << "covered " << covered << " of " << Extent * Extent << ", maximum red " << red << "\n";
    Require(covered * 2u > Extent * Extent, "the rect-list draw with a pass-through parameter covered " + std::to_string(covered) + " pixels");
    Require(red == 0u, "a pass-through parameter the vertex stage never writes read " + std::to_string(red) + " instead of zero");
    return 0;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto result = Run(*device);
        if (result != 0) return result;
        std::cout << "rect-list pass-through tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

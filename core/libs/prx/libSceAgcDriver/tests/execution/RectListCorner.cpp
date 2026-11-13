#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Recompiler.hpp"
#include "SpirvBackend/SpirvModule.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include "VulkanTestDevice.hpp"
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
        boolType = type(spv::OpTypeBool);
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
        const auto boolVectorType = type(spv::OpTypeVector, boolType, 4u);
        const auto firstScalar = result(spv::OpIEqual, boolType, index, constant(intType, 0u));
        const auto secondScalar = result(spv::OpIEqual, boolType, index, constant(intType, 1u));
        const auto first = result(spv::OpCompositeConstruct, boolVectorType, firstScalar, firstScalar, firstScalar, firstScalar);
        const auto second = result(spv::OpCompositeConstruct, boolVectorType, secondScalar, secondScalar, secondScalar, secondScalar);
        const auto positionValue = result(spv::OpSelect, vec4Type, first, vector(-1.0f, -1.0f, 0.0f, 1.0f), result(spv::OpSelect, vec4Type, second, vector(1.0f, -1.0f, 0.0f, 1.0f), vector(-1.0f, 1.0f, 0.0f, 1.0f)));
        const auto identityValue = result(spv::OpSelect, vec4Type, first, vector(0.0f, 0.0f, 0.0f, 1.0f), result(spv::OpSelect, vec4Type, second, vector(1.0f, 0.0f, 0.0f, 1.0f), vector(0.0f, 1.0f, 0.0f, 1.0f)));
        emit(spv::OpStore, position, positionValue);
        emit(spv::OpStore, identity, identityValue);
        emit(spv::OpReturn);
        emit(spv::OpFunctionEnd);
        return builder.Finalize();
    }

    std::vector<std::uint32_t> Fragment() {
        const auto main = result(spv::OpFunction, voidType, spv::FunctionControlMaskNone, functionType);
        const auto vectorPointerIn = pointer(spv::StorageClassInput, vec4Type);
        const auto vectorPointerOut = pointer(spv::StorageClassOutput, vec4Type);
        const auto identity = builder.DefineGlobalVariable(vectorPointerIn, spv::StorageClassInput);
        const auto color = builder.DefineGlobalVariable(vectorPointerOut, spv::StorageClassOutput);
        decorate(identity, spv::DecorationLocation, 0u);
        decorate(color, spv::DecorationLocation, 0u);
        builder.EmitEntryPoint(spv::ExecutionModelFragment, main, "main", {identity, color});
        builder.AddExecutionMode(main, spv::ExecutionModeOriginUpperLeft);
        result(spv::OpLabel);
        emit(spv::OpStore, color, result(spv::OpLoad, vec4Type, identity));
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
    std::uint32_t boolType = 0;
    std::uint32_t intType = 0;
    std::uint32_t floatType = 0;
    std::uint32_t vec4Type = 0;
    std::uint32_t functionType = 0;
};

std::uint32_t VertexOf(std::uint32_t x, std::uint32_t y) {
    const auto offset = (static_cast<std::size_t>(y) * Extent + x) * 4u;
    const auto red = std::to_integer<unsigned>(pixels[offset]);
    const auto green = std::to_integer<unsigned>(pixels[offset + 1]);
    const auto alpha = std::to_integer<unsigned>(pixels[offset + 3]);
    Require(alpha >= 128u, "rect-list corner pixel was not drawn");
    return (red >= 128u ? 1u : 0u) + (green >= 128u ? 2u : 0u);
}

int Run(AgcDriver::VulkanDevice& device, std::uint32_t maximumVersion) {
    std::cout << "device: " << device.DeviceName() << "\n";
    auto target = device.Target();
    if (!target.tessellation.has_value() || !TargetHasCapability(target, spv::CapabilityTessellation)) {
        std::cout << "skipped, device has no tessellation\n";
        return VulkanTestSkipped;
    }
    if (target.spirvVersion > maximumVersion) target.spirvVersion = maximumVersion;
    Require(target.spirvVersion >= 0x00010300u, "rect-list corner draw needs SPIR-V 1.3");
    std::cout << "SPIR-V " << (target.spirvVersion >> 16u) << "." << ((target.spirvVersion >> 8u) & 0xffu) << "\n";
    ShaderRecompiler::RecompileResult vertex;
    ShaderRecompiler::RecompileResult fragment;
    vertex.spirv = ShaderRecompiler::SharedSpirv(Emitter(target.spirvVersion).Vertex());
    fragment.spirv = ShaderRecompiler::SharedSpirv(Emitter(target.spirvVersion).Fragment());
#if ANYPS5_ENABLE_SPIRV_TOOLS
    static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(vertex.spirv, target.vulkanVersion, target.spirvVersion, target.nonConstantImageOffsets, false));
    static_cast<void>(ShaderRecompiler::ValidateAndOptimizeSpirv(fragment.spirv, target.vulkanVersion, target.spirvVersion, target.nonConstantImageOffsets, false));
#endif
    vertex.parameterExports = {0u};
    fragment.fragmentParameters = {{0u, 0u, false, false}};
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

    const std::array<std::array<std::uint32_t, 3>, 4> corners{{
        {0u, Extent - 1u, 0u},
        {Extent - 1u, Extent - 1u, 1u},
        {0u, 0u, 2u},
        {Extent - 1u, 0u, 3u},
    }};
    for (const auto& corner : corners) {
        const auto selected = VertexOf(corner[0], corner[1]);
        std::cout << "corner (" << corner[0] << ", " << corner[1] << ") vertex " << selected << "\n";
        Require(selected == corner[2], "rect-list corner selected vertex " + std::to_string(selected) + ", expected " + std::to_string(corner[2]));
    }
    std::uint32_t covered = 0;
    for (std::size_t offset = 3; offset < pixels.size(); offset += 4) {
        if (pixels[offset] != std::byte{0}) ++covered;
    }
    std::cout << "covered " << covered << " of " << Extent * Extent << "\n";
    Require(covered * 5u > Extent * Extent * 4u, "rect-list rectangle covered " + std::to_string(covered) + " pixels");
    return 0;
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        auto result = Run(*device, 0x00010300u);
        if (result != 0) return result;
        if (device->Target().spirvVersion >= 0x00010400u) {
            result = Run(*device, 0x00010400u);
            if (result != 0) return result;
        }
        std::cout << "rect-list corner tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

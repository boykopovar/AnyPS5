#include "Recompiler.hpp"
#include "SpirvBackend/SpirvModule.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include <algorithm>
#include <array>
#include <bit>
#include <set>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {
namespace {

constexpr std::uint32_t ParameterLocations = 32u;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("Barycentric geometry SPIR-V: " + message);
}

struct PerVertexParameter {
    std::uint32_t sourceLocation;
    std::uint32_t location;
};

class BarycentricGeometryEmitter {
public:
    explicit BarycentricGeometryEmitter(std::uint32_t version) : builder(version) {
        builder.AddMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
        voidType = builder.Type(spv::OpTypeVoid);
        uintType = builder.Type(spv::OpTypeInt, 32u, 0u);
        intType = builder.Type(spv::OpTypeInt, 32u, 1u);
        floatType = builder.Type(spv::OpTypeFloat, 32u);
        vec3FloatType = builder.Type(spv::OpTypeVector, floatType, 3u);
        vec4FloatType = builder.Type(spv::OpTypeVector, floatType, 4u);
        vertexArrayType = builder.Type(spv::OpTypeArray, vec4FloatType, constant(uintType, 3u));
        functionType = builder.Type(spv::OpTypeFunction, voidType);
        perVertexType = builder.DecoratedType(spv::OpTypeStruct, {{spv::OpMemberDecorate, {0u, spv::DecorationBuiltIn, spv::BuiltInPosition}}, {spv::OpDecorate, {spv::DecorationBlock}}}, vec4FloatType);
    }

    std::vector<std::uint32_t> Emit(const std::vector<std::uint32_t>& passed, const std::vector<PerVertexParameter>& perVertex, const BarycentricEmulationLayout& layout) {
        builder.EmitCapability(spv::CapabilityShader);
        builder.EmitCapability(spv::CapabilityGeometry);
        main = result(spv::OpFunction, voidType, spv::FunctionControlMaskNone, functionType);
        builder.AddExecutionMode(main, spv::ExecutionModeTriangles);
        builder.AddExecutionMode(main, spv::ExecutionModeInvocations, 1u);
        builder.AddExecutionMode(main, spv::ExecutionModeOutputTriangleStrip);
        builder.AddExecutionMode(main, spv::ExecutionModeOutputVertices, 3u);

        const auto glIn = addInterface(spv::StorageClassInput, builder.Type(spv::OpTypeArray, perVertexType, constant(uintType, 3u)));
        std::set<std::uint32_t> sources(passed.begin(), passed.end());
        for (const auto& parameter : perVertex) sources.insert(parameter.sourceLocation);
        std::array<std::uint32_t, ParameterLocations> inputs {};
        for (const auto location : sources) {
            inputs[location] = addInterface(spv::StorageClassInput, vertexArrayType);
            decorate(inputs[location], spv::DecorationLocation, location);
        }
        const auto position = addInterface(spv::StorageClassOutput, vec4FloatType);
        decorate(position, spv::DecorationBuiltIn, spv::BuiltInPosition);
        std::vector<std::uint32_t> passedOutputs;
        for (const auto location : passed) {
            passedOutputs.push_back(addInterface(spv::StorageClassOutput, vec4FloatType));
            decorate(passedOutputs.back(), spv::DecorationLocation, location);
        }
        const auto barycentricOutput = [&](std::uint32_t location) {
            if (location == BarycentricEmulationLayout::NoLocation) return 0u;
            const auto variable = addInterface(spv::StorageClassOutput, vec3FloatType);
            decorate(variable, spv::DecorationLocation, location);
            return variable;
        };
        const auto smooth = barycentricOutput(layout.smoothLocation);
        const auto linear = barycentricOutput(layout.linearLocation);
        std::vector<std::uint32_t> perVertexOutputs;
        for (const auto& parameter : perVertex) {
            perVertexOutputs.push_back(addInterface(spv::StorageClassOutput, vertexArrayType));
            decorate(perVertexOutputs.back(), spv::DecorationLocation, parameter.location);
        }
        builder.EmitEntryPoint(spv::ExecutionModelGeometry, main, "main", interfaces);
        builder.AddFunction(spv::OpLabel, builder.AllocateId());

        const auto ptrInputVec4 = builder.Type(spv::OpTypePointer, spv::StorageClassInput, vec4FloatType);
        const auto ptrOutputVec4 = builder.Type(spv::OpTypePointer, spv::StorageClassOutput, vec4FloatType);
        const auto zero = constant(floatType, 0u);
        const auto one = constant(floatType, std::bit_cast<std::uint32_t>(1.0f));
        std::array<std::array<std::uint32_t, 3>, ParameterLocations> values {};
        for (const auto location : sources) {
            for (std::uint32_t vertex = 0; vertex < 3u; ++vertex) values[location][vertex] = load(vec4FloatType, result(spv::OpAccessChain, ptrInputVec4, inputs[location], constant(intType, vertex)));
        }
        for (std::uint32_t vertex = 0; vertex < 3u; ++vertex) {
            store(position, load(vec4FloatType, result(spv::OpAccessChain, ptrInputVec4, glIn, constant(intType, vertex), constant(intType, 0u))));
            for (std::size_t i = 0; i < passed.size(); ++i) store(passedOutputs[i], values[passed[i]][vertex]);
            const auto weights = result(spv::OpCompositeConstruct, vec3FloatType, vertex == 0u ? one : zero, vertex == 1u ? one : zero, vertex == 2u ? one : zero);
            if (smooth != 0u) store(smooth, weights);
            if (linear != 0u) store(linear, weights);
            for (std::size_t i = 0; i < perVertex.size(); ++i) {
                for (std::uint32_t source = 0; source < 3u; ++source) store(result(spv::OpAccessChain, ptrOutputVec4, perVertexOutputs[i], constant(intType, source)), values[perVertex[i].sourceLocation][source]);
            }
            builder.AddFunction(spv::OpEmitVertex);
        }
        builder.AddFunction(spv::OpEndPrimitive);
        builder.AddFunction(spv::OpReturn);
        builder.AddFunction(spv::OpFunctionEnd);
        return builder.Finalize();
    }

private:
    SpirvModule builder;
    std::vector<std::uint32_t> interfaces;
    std::uint32_t main = 0;
    std::uint32_t voidType = 0;
    std::uint32_t uintType = 0;
    std::uint32_t intType = 0;
    std::uint32_t floatType = 0;
    std::uint32_t vec3FloatType = 0;
    std::uint32_t vec4FloatType = 0;
    std::uint32_t vertexArrayType = 0;
    std::uint32_t functionType = 0;
    std::uint32_t perVertexType = 0;

    std::uint32_t constant(std::uint32_t type, std::uint32_t value) { return builder.Constant(spv::OpConstant, type, value); }

    template <typename... TArgs>
    std::uint32_t result(spv::Op opcode, std::uint32_t type, TArgs... operands) {
        const auto id = builder.AllocateId();
        builder.AddFunction(opcode, type, id, operands...);
        return id;
    }

    std::uint32_t load(std::uint32_t type, std::uint32_t pointer) { return result(spv::OpLoad, type, pointer); }

    void store(std::uint32_t pointer, std::uint32_t value) { builder.AddFunction(spv::OpStore, pointer, value); }

    void decorate(std::uint32_t target, spv::Decoration decoration, std::uint32_t value) { builder.AddAnnotation(spv::OpDecorate, target, decoration, value); }

    std::uint32_t addInterface(spv::StorageClass storage, std::uint32_t type) {
        const auto variable = builder.DefineGlobalVariable(builder.Type(spv::OpTypePointer, storage, type), storage);
        interfaces.push_back(variable);
        return variable;
    }
};

}

BarycentricEmulationLayout LayoutBarycentricEmulation(std::span<const FragmentParameter> parameters, const BarycentricEmulation& emulation) {
    BarycentricEmulationLayout layout;
    if (!emulation.active) return layout;
    std::array<bool, ParameterLocations> used {};
    for (const auto& parameter : parameters) {
        require(parameter.location < ParameterLocations, "fragment parameter location " + std::to_string(parameter.location) + " is out of range");
        if (!parameter.perVertex) used[parameter.location] = true;
    }
    const auto take = [&](std::uint32_t count) {
        for (std::uint32_t first = 0; first + count <= ParameterLocations; ++first) {
            if (std::any_of(used.begin() + first, used.begin() + first + count, [](bool taken) { return taken; })) continue;
            std::fill(used.begin() + first, used.begin() + first + count, true);
            return first;
        }
        throw std::runtime_error("Barycentric geometry SPIR-V: the emulated barycentric inputs exceed " + std::to_string(ParameterLocations) + " locations");
    };
    if (emulation.smooth) layout.smoothLocation = take(1u);
    if (emulation.linear) layout.linearLocation = take(1u);
    for (const auto& parameter : parameters) {
        if (parameter.perVertex) layout.perVertexLocations.emplace_back(parameter.location, take(3u));
    }
    return layout;
}

RecompileResult BuildBarycentricGeometryShader(const RecompileResult& vertex, const RecompileResult& fragment, const SpirvTarget& target, const std::optional<GeometryStageLimits>& limits) {
    require(fragment.barycentricEmulation.active, "the fragment shader does not emulate barycentrics");
    require(limits.has_value(), "geometry shaders are unavailable");
    require(target.spirvVersion >= 0x00010300u && target.spirvVersion <= 0x00010500u, "unsupported SPIR-V target version");
    require(std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), spv::CapabilityGeometry) != target.supportedCapabilities.end(), "geometry capability is unavailable");
    const auto exported = [&](std::uint32_t location) { return std::find(vertex.parameterExports.begin(), vertex.parameterExports.end(), location) != vertex.parameterExports.end(); };
    const auto layout = LayoutBarycentricEmulation(fragment.fragmentParameters, fragment.barycentricEmulation);
    std::vector<std::uint32_t> passed;
    std::vector<PerVertexParameter> perVertex;
    for (const auto& parameter : fragment.fragmentParameters) {
        require(exported(parameter.sourceLocation), "fragment parameter " + std::to_string(parameter.location) + " is not exported by the vertex shader");
        if (!parameter.perVertex) {
            passed.push_back(parameter.sourceLocation);
            continue;
        }
        const auto relocated = std::find_if(layout.perVertexLocations.begin(), layout.perVertexLocations.end(), [&](const auto& entry) { return entry.first == parameter.location; });
        require(relocated != layout.perVertexLocations.end(), "per-vertex parameter " + std::to_string(parameter.location) + " has no emulated location");
        perVertex.push_back({parameter.sourceLocation, relocated->second});
    }
    const auto barycentrics = (layout.smoothLocation != BarycentricEmulationLayout::NoLocation ? 1u : 0u) + (layout.linearLocation != BarycentricEmulationLayout::NoLocation ? 1u : 0u);
    const auto outputComponents = 4u + 4u * static_cast<std::uint32_t>(passed.size()) + 3u * barycentrics + 12u * static_cast<std::uint32_t>(perVertex.size());
    std::set<std::uint32_t> sources(passed.begin(), passed.end());
    for (const auto& parameter : perVertex) sources.insert(parameter.sourceLocation);
    const auto inputComponents = 4u + 4u * static_cast<std::uint32_t>(sources.size());
    const auto fragmentComponents = 4u * (static_cast<std::uint32_t>(passed.size()) + barycentrics + 3u * static_cast<std::uint32_t>(perVertex.size()));
    require(limits->maxGeometryOutputVertices >= 3u && outputComponents <= limits->maxGeometryOutputComponents && 3u * outputComponents <= limits->maxGeometryTotalOutputComponents && inputComponents <= limits->maxGeometryInputComponents && fragmentComponents <= limits->maxFragmentInputComponents, "the emulated barycentric interface exceeds device limits");
    BarycentricGeometryEmitter emitter(target.spirvVersion);
    RecompileResult result;
    result.spirv = emitter.Emit(passed, perVertex, layout);
#if ANYPS5_ENABLE_SPIRV_TOOLS
    result.spirv = ValidateAndOptimizeSpirv(result.spirv, target.vulkanVersion, target.spirvVersion);
#endif
    return result;
}

}

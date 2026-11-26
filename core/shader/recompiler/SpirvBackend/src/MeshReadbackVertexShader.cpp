#include "Recompiler.hpp"
#include "SpirvBackend/SpirvModule.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include <algorithm>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {
namespace {

constexpr std::uint32_t PushWords = (MeshDrawPushOffsetBytes + MeshDrawPushBytes) / 4u;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("Mesh readback vertex SPIR-V: " + message);
}

class MeshReadbackEmitter {
public:
    explicit MeshReadbackEmitter(std::uint32_t version) : builder(version), listsAllGlobals(version >= 0x00010400u) {
        voidType = builder.Type(spv::OpTypeVoid);
        uintType = builder.Type(spv::OpTypeInt, 32u, 0u);
        intType = builder.Type(spv::OpTypeInt, 32u, 1u);
        ulongType = builder.Type(spv::OpTypeInt, 64u, 0u);
        floatType = builder.Type(spv::OpTypeFloat, 32u);
        vec4FloatType = builder.Type(spv::OpTypeVector, floatType, 4u);
        functionType = builder.Type(spv::OpTypeFunction, voidType);
    }

    std::vector<std::uint32_t> Emit(const std::vector<std::uint32_t>& parameters, std::uint32_t layout) {
        const auto clipDistances = MeshRecordClipDistances(layout);
        const auto cullDistances = MeshRecordCullDistances(layout);
        const bool layer = MeshRecordLayer(layout);
        builder.EmitCapability(spv::CapabilityShader);
        if (clipDistances != 0u) builder.EmitCapability(spv::CapabilityClipDistance);
        if (cullDistances != 0u) builder.EmitCapability(spv::CapabilityCullDistance);
        if (layer) {
            builder.EmitCapability(spv::CapabilityShaderViewportIndexLayerEXT);
            builder.EmitExtension("SPV_EXT_shader_viewport_index_layer");
        }
        builder.EmitCapability(spv::CapabilityInt64);
        builder.EmitCapability(spv::CapabilityPhysicalStorageBufferAddresses);
        builder.EmitExtension("SPV_KHR_physical_storage_buffer");
        builder.AddMemoryModel(spv::AddressingModelPhysicalStorageBuffer64, spv::MemoryModelGLSL450);
        main = result(spv::OpFunction, voidType, spv::FunctionControlMaskNone, functionType);

        const auto wordArray = builder.Type(spv::OpTypeArray, uintType, constant(uintType, PushWords));
        builder.AddAnnotation(spv::OpDecorate, wordArray, spv::DecorationArrayStride, 4u);
        const auto pushBlock = builder.DecoratedType(spv::OpTypeStruct, {{spv::OpMemberDecorate, {0u, spv::DecorationOffset, 0u}}, {spv::OpDecorate, {spv::DecorationBlock}}}, wordArray);
        const auto push = addInterface(spv::StorageClassPushConstant, pushBlock);
        const auto vertexIndex = addInterface(spv::StorageClassInput, intType);
        builder.AddAnnotation(spv::OpDecorate, vertexIndex, spv::DecorationBuiltIn, spv::BuiltInVertexIndex);
        const auto position = addInterface(spv::StorageClassOutput, vec4FloatType);
        builder.AddAnnotation(spv::OpDecorate, position, spv::DecorationBuiltIn, spv::BuiltInPosition);
        std::vector<std::uint32_t> outputs;
        for (const auto location : parameters) {
            outputs.push_back(addInterface(spv::StorageClassOutput, vec4FloatType));
            builder.AddAnnotation(spv::OpDecorate, outputs.back(), spv::DecorationLocation, location);
        }
        const auto distanceOutput = [&](std::uint32_t count, spv::BuiltIn builtin) {
            if (count == 0u) return 0u;
            const auto variable = addInterface(spv::StorageClassOutput, builder.Type(spv::OpTypeArray, floatType, constant(uintType, count)));
            builder.AddAnnotation(spv::OpDecorate, variable, spv::DecorationBuiltIn, builtin);
            return variable;
        };
        const auto clip = distanceOutput(clipDistances, spv::BuiltInClipDistance);
        const auto cull = distanceOutput(cullDistances, spv::BuiltInCullDistance);
        const auto layerOutput = layer ? addInterface(spv::StorageClassOutput, intType) : 0u;
        if (layer) builder.AddAnnotation(spv::OpDecorate, layerOutput, spv::DecorationBuiltIn, spv::BuiltInLayer);
        builder.EmitEntryPoint(spv::ExecutionModelVertex, main, "main", interfaces);
        builder.AddFunction(spv::OpLabel, builder.AllocateId());

        const auto pushWordPointer = builder.Type(spv::OpTypePointer, spv::StorageClassPushConstant, uintType);
        const auto pushWord = [&](std::uint32_t word) { return widen(load(uintType, result(spv::OpAccessChain, pushWordPointer, push, constant(intType, 0u), constant(intType, word)))); };
        const auto arguments = combine(pushWord(MeshDrawPushOffsetBytes / 4u + MeshArgumentAddressDword), pushWord(MeshDrawPushOffsetBytes / 4u + MeshArgumentAddressDword + 1u));
        const auto output = combine(widen(loadAt(uintType, offset(arguments, 4ull * MeshOutputAddressDword))), widen(loadAt(uintType, offset(arguments, 4ull * (MeshOutputAddressDword + 1u)))));
        const auto triangles = offset(output, MeshOutputHeaderBytes);
        const auto corners = widen(loadAt(uintType, output));
        const auto records = result(spv::OpIAdd, ulongType, triangles, result(spv::OpShiftLeftLogical, ulongType, corners, constant(uintType, 2u)));
        const auto corner = widen(result(spv::OpBitcast, uintType, load(intType, vertexIndex)));
        const auto vertex = widen(loadAt(uintType, result(spv::OpIAdd, ulongType, triangles, result(spv::OpShiftLeftLogical, ulongType, corner, constant(uintType, 2u)))));
        const auto parameterCount = static_cast<std::uint32_t>(parameters.size());
        const auto stride = constant64(MeshRecordBytes(parameterCount, layout));
        const auto record = result(spv::OpIAdd, ulongType, records, result(spv::OpIMul, ulongType, vertex, stride));
        store(position, loadAt(vec4FloatType, record));
        for (std::size_t i = 0; i < outputs.size(); ++i) store(outputs[i], loadAt(vec4FloatType, offset(record, 16ull * (1u + i))));
        const auto extras = MeshRecordExtraOffset(parameterCount);
        const auto outputPointer = builder.Type(spv::OpTypePointer, spv::StorageClassOutput, floatType);
        for (std::uint32_t i = 0; i < clipDistances; ++i) store(result(spv::OpAccessChain, outputPointer, clip, constant(intType, i)), loadAt(floatType, offset(record, extras + 4ull * i)));
        for (std::uint32_t i = 0; i < cullDistances; ++i) store(result(spv::OpAccessChain, outputPointer, cull, constant(intType, i)), loadAt(floatType, offset(record, extras + 4ull * (clipDistances + i))));
        if (layer) {
            const auto size = MeshRecordPrimitiveSize(layout);
            const auto primitive = result(spv::OpUDiv, ulongType, corner, constant64(size));
            const auto provokingCorner = result(spv::OpIAdd, ulongType, result(spv::OpIMul, ulongType, primitive, constant64(size)), constant64(std::min(MeshRecordProvokingVertex(layout), size - 1u)));
            const auto provoking = widen(loadAt(uintType, result(spv::OpIAdd, ulongType, triangles, result(spv::OpShiftLeftLogical, ulongType, provokingCorner, constant(uintType, 2u)))));
            const auto provokingRecord = result(spv::OpIAdd, ulongType, records, result(spv::OpIMul, ulongType, provoking, stride));
            store(layerOutput, result(spv::OpBitcast, intType, loadAt(uintType, offset(provokingRecord, extras + 4ull * (clipDistances + cullDistances)))));
        }
        builder.AddFunction(spv::OpReturn);
        builder.AddFunction(spv::OpFunctionEnd);
        return builder.Finalize();
    }

private:
    SpirvModule builder;
    bool listsAllGlobals;
    std::vector<std::uint32_t> interfaces;
    std::uint32_t main = 0;
    std::uint32_t voidType = 0;
    std::uint32_t uintType = 0;
    std::uint32_t intType = 0;
    std::uint32_t ulongType = 0;
    std::uint32_t floatType = 0;
    std::uint32_t vec4FloatType = 0;
    std::uint32_t functionType = 0;

    std::uint32_t constant(std::uint32_t type, std::uint32_t value) { return builder.Constant(spv::OpConstant, type, value); }

    std::uint32_t constant64(std::uint64_t value) { return builder.Constant(spv::OpConstant, ulongType, static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32u)); }

    template <typename... TArgs>
    std::uint32_t result(spv::Op opcode, std::uint32_t type, TArgs... operands) {
        const auto id = builder.AllocateId();
        builder.AddFunction(opcode, type, id, operands...);
        return id;
    }

    std::uint32_t load(std::uint32_t type, std::uint32_t pointer) { return result(spv::OpLoad, type, pointer); }

    std::uint32_t loadAt(std::uint32_t type, std::uint32_t address) {
        const auto pointer = result(spv::OpConvertUToPtr, builder.Type(spv::OpTypePointer, spv::StorageClassPhysicalStorageBuffer, type), address);
        return result(spv::OpLoad, type, pointer, spv::MemoryAccessAlignedMask, 4u);
    }

    std::uint32_t widen(std::uint32_t value) { return result(spv::OpUConvert, ulongType, value); }

    std::uint32_t combine(std::uint32_t low, std::uint32_t high) { return result(spv::OpBitwiseOr, ulongType, low, result(spv::OpShiftLeftLogical, ulongType, high, constant(uintType, 32u))); }

    std::uint32_t offset(std::uint32_t address, std::uint64_t bytes) { return result(spv::OpIAdd, ulongType, address, constant64(bytes)); }

    void store(std::uint32_t pointer, std::uint32_t value) { builder.AddFunction(spv::OpStore, pointer, value); }

    std::uint32_t addInterface(spv::StorageClass storage, std::uint32_t type) {
        const auto variable = builder.DefineGlobalVariable(builder.Type(spv::OpTypePointer, storage, type), storage);
        if (listsAllGlobals || storage != spv::StorageClassPushConstant) interfaces.push_back(variable);
        return variable;
    }
};

}

RecompileResult BuildMeshReadbackVertexShader(const RecompileResult& mesh, std::uint32_t vulkanVersion) {
    const auto& words = mesh.spirv.Words();
    require(words.size() >= 5u && words[1] >= 0x00010300u && words[1] <= 0x00010500u, "unsupported SPIR-V version of the mesh program");
    MeshReadbackEmitter emitter(words[1]);
    RecompileResult result;
    result.variantId = mesh.variantId;
    result.runtimeAbiVersion = mesh.runtimeAbiVersion;
    result.spirv = emitter.Emit(mesh.parameterExports, mesh.meshRecordLayout);
#if ANYPS5_ENABLE_SPIRV_TOOLS
    result.spirv = ValidateAndOptimizeSpirv(result.spirv, vulkanVersion, words[1]);
#else
    static_cast<void>(vulkanVersion);
#endif
    return result;
}

}

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

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("Tessellation capture SPIR-V: " + message);
}

class TessellationCaptureEmitter {
public:
    explicit TessellationCaptureEmitter(std::uint32_t version) : builder(version), version(version) {
        builder.AddMemoryModel(spv::AddressingModelPhysicalStorageBuffer64, spv::MemoryModelGLSL450);
        voidType = builder.Type(spv::OpTypeVoid);
        boolType = builder.Type(spv::OpTypeBool);
        uintType = builder.Type(spv::OpTypeInt, 32u, 0u);
        ulongType = builder.Type(spv::OpTypeInt, 64u, 0u);
        uvec4Type = builder.Type(spv::OpTypeVector, uintType, 4u);
        functionType = builder.Type(spv::OpTypeFunction, voidType);
        bufferPointer = builder.Type(spv::OpTypePointer, spv::StorageClassPhysicalStorageBuffer, uintType);
    }

    std::vector<std::uint32_t> Emit(std::uint32_t itemDwords, std::uint32_t primitivesPerGroup) {
        builder.EmitCapability(spv::CapabilityShader);
        builder.EmitCapability(spv::CapabilityGeometry);
        builder.EmitCapability(spv::CapabilityInt64);
        builder.EmitCapability(spv::CapabilityPhysicalStorageBufferAddresses);
        builder.EmitExtension("SPV_KHR_physical_storage_buffer");
        main = result(spv::OpFunction, voidType, spv::FunctionControlMaskNone, functionType);
        builder.AddExecutionMode(main, spv::ExecutionModeTriangles);
        builder.AddExecutionMode(main, spv::ExecutionModeInvocations, 1u);
        builder.AddExecutionMode(main, spv::ExecutionModeOutputPoints);
        builder.AddExecutionMode(main, spv::ExecutionModeOutputVertices, 1u);

        const auto locations = (itemDwords + 3u) / 4u;
        const auto itemType = builder.Type(spv::OpTypeArray, uvec4Type, constant(locations));
        const auto items = addInterface(spv::StorageClassInput, builder.Type(spv::OpTypeArray, itemType, constant(3u)));
        builder.AddAnnotation(spv::OpDecorate, items, spv::DecorationLocation, 0u);
        const auto wordArray = builder.DecoratedType(spv::OpTypeArray, {{spv::OpDecorate, {spv::DecorationArrayStride, 4u}}}, uintType, constant(PushWords));
        const auto pushBlock = builder.DecoratedType(spv::OpTypeStruct, {{spv::OpMemberDecorate, {0u, spv::DecorationOffset, 0u}}, {spv::OpDecorate, {spv::DecorationBlock}}}, wordArray);
        push = builder.DefineGlobalVariable(builder.Type(spv::OpTypePointer, spv::StorageClassPushConstant, pushBlock), spv::StorageClassPushConstant);
        if (version >= 0x00010400u) interfaces.push_back(push);
        builder.EmitEntryPoint(spv::ExecutionModelGeometry, main, "main", interfaces);
        builder.AddFunction(spv::OpLabel, builder.AllocateId());

        const auto itemBase = address(MeshCapturedItemAddressDword);
        const auto arguments = address(TessellationCaptureArgumentAddressDword);
        const auto capacity = result(spv::OpLoad, uintType, offset(arguments, MeshCapturedCapacityDword), spv::MemoryAccessAlignedMask, 4u);
        const auto index = atomicAdd(offset(arguments, MeshCapturedCounterDword), constant(1u));
        const auto fits = result(spv::OpULessThan, boolType, index, capacity);
        const auto stored = builder.AllocateId();
        const auto merge = builder.AllocateId();
        builder.AddFunction(spv::OpSelectionMerge, merge, spv::SelectionControlMaskNone);
        builder.AddFunction(spv::OpBranchConditional, fits, stored, merge);
        builder.AddFunction(spv::OpLabel, stored);
        const auto uvec4Input = builder.Type(spv::OpTypePointer, spv::StorageClassInput, uvec4Type);
        const auto first = result(spv::OpIMul, uintType, index, constant(3u * itemDwords));
        for (std::uint32_t vertex = 0; vertex < 3u; ++vertex) {
            for (std::uint32_t location = 0; location < locations; ++location) {
                const auto value = result(spv::OpLoad, uvec4Type, result(spv::OpAccessChain, uvec4Input, items, constant(vertex), constant(location)));
                for (std::uint32_t component = 0; component < 4u && location * 4u + component < itemDwords; ++component) {
                    const auto word = result(spv::OpIAdd, uintType, first, constant(vertex * itemDwords + location * 4u + component));
                    builder.AddFunction(spv::OpStore, pointer(itemBase, word), result(spv::OpCompositeExtract, uintType, value, component), spv::MemoryAccessAlignedMask, 4u);
                }
            }
        }
        static_cast<void>(atomicAdd(offset(arguments, MeshArgumentIndexCountDword), constant(3u)));
        const auto leads = result(spv::OpIEqual, boolType, result(spv::OpUMod, uintType, index, constant(primitivesPerGroup)), constant(0u));
        const auto opens = builder.AllocateId();
        const auto opened = builder.AllocateId();
        builder.AddFunction(spv::OpSelectionMerge, opened, spv::SelectionControlMaskNone);
        builder.AddFunction(spv::OpBranchConditional, leads, opens, opened);
        builder.AddFunction(spv::OpLabel, opens);
        static_cast<void>(atomicAdd(offset(arguments, 0u), constant(1u)));
        builder.AddFunction(spv::OpBranch, opened);
        builder.AddFunction(spv::OpLabel, opened);
        builder.AddFunction(spv::OpBranch, merge);
        builder.AddFunction(spv::OpLabel, merge);
        builder.AddFunction(spv::OpReturn);
        builder.AddFunction(spv::OpFunctionEnd);
        return builder.Finalize();
    }

private:
    static constexpr std::uint32_t PushWords = (MeshDrawPushOffsetBytes + MeshDrawPushBytes) / 4u;

    SpirvModule builder;
    std::uint32_t version;
    std::vector<std::uint32_t> interfaces;
    std::uint32_t main = 0;
    std::uint32_t voidType = 0;
    std::uint32_t boolType = 0;
    std::uint32_t uintType = 0;
    std::uint32_t ulongType = 0;
    std::uint32_t uvec4Type = 0;
    std::uint32_t functionType = 0;
    std::uint32_t bufferPointer = 0;
    std::uint32_t push = 0;

    std::uint32_t constant(std::uint32_t value) { return builder.Constant(spv::OpConstant, uintType, value); }

    template <typename... TArgs>
    std::uint32_t result(spv::Op opcode, std::uint32_t type, TArgs... operands) {
        const auto id = builder.AllocateId();
        builder.AddFunction(opcode, type, id, operands...);
        return id;
    }

    std::uint32_t pushWord(std::uint32_t dword) {
        const auto element = result(spv::OpAccessChain, builder.Type(spv::OpTypePointer, spv::StorageClassPushConstant, uintType), push, constant(0u), constant(MeshDrawPushOffsetBytes / 4u + dword));
        return result(spv::OpLoad, uintType, element);
    }

    std::uint32_t address(std::uint32_t dword) {
        const auto low = result(spv::OpUConvert, ulongType, pushWord(dword));
        const auto high = result(spv::OpUConvert, ulongType, pushWord(dword + 1u));
        return result(spv::OpBitwiseOr, ulongType, low, result(spv::OpShiftLeftLogical, ulongType, high, constant(32u)));
    }

    std::uint32_t offset(std::uint32_t base, std::uint32_t dword) {
        return result(spv::OpConvertUToPtr, bufferPointer, result(spv::OpIAdd, ulongType, base, builder.Constant(spv::OpConstant, ulongType, 4u * dword, 0u)));
    }

    std::uint32_t pointer(std::uint32_t base, std::uint32_t word) {
        const auto bytes = result(spv::OpShiftLeftLogical, ulongType, result(spv::OpUConvert, ulongType, word), constant(2u));
        return result(spv::OpConvertUToPtr, bufferPointer, result(spv::OpIAdd, ulongType, base, bytes));
    }

    std::uint32_t atomicAdd(std::uint32_t target, std::uint32_t value) {
        return result(spv::OpAtomicIAdd, uintType, target, constant(spv::ScopeDevice), constant(spv::MemorySemanticsMaskNone), value);
    }

    std::uint32_t addInterface(spv::StorageClass storage, std::uint32_t type) {
        const auto variable = builder.DefineGlobalVariable(builder.Type(spv::OpTypePointer, storage, type), storage);
        interfaces.push_back(variable);
        return variable;
    }
};

}

RecompileResult BuildTessellationCaptureGeometryShader(const MeshConfiguration& geometry, const SpirvTarget& target, const std::optional<GeometryStageLimits>& limits) {
    require(limits.has_value(), "geometry shaders are unavailable");
    require(target.spirvVersion >= 0x00010300u && target.spirvVersion <= 0x00010500u, "unsupported SPIR-V target version");
    for (const auto capability : {spv::CapabilityGeometry, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses}) {
        require(std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), static_cast<std::uint32_t>(capability)) != target.supportedCapabilities.end(), "SPIR-V capability " + std::to_string(capability) + " is unavailable");
    }
    require(std::find(target.supportedExtensions.begin(), target.supportedExtensions.end(), "SPV_KHR_physical_storage_buffer") != target.supportedExtensions.end(), "SPV_KHR_physical_storage_buffer is unavailable");
    require(geometry.esgsItemSize != 0u && geometry.primitivesPerGroup != 0u, "the geometry subgroup has no ESGS item or primitive");
    const auto locations = (geometry.esgsItemSize + 3u) / 4u;
    require(4u * locations <= limits->maxGeometryInputComponents, "the ESGS item exceeds the geometry input limit");
    TessellationCaptureEmitter emitter(target.spirvVersion);
    RecompileResult result;
    result.spirv = emitter.Emit(geometry.esgsItemSize, geometry.primitivesPerGroup);
#if ANYPS5_ENABLE_SPIRV_TOOLS
    result.spirv = ValidateAndOptimizeSpirv(result.spirv, target.vulkanVersion, target.spirvVersion);
#endif
    return result;
}

}

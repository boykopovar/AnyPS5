#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SHADERSTAGEINPUTINFO_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_INCLUDE_OPTIMIZATION_SHADERSTAGEINPUTINFO_HPP

#include "IntermediateRepresentation/IrMetadata.hpp"
#include "Recompiler.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

struct ShaderBufferResource {
    std::array<std::uint32_t, 4> fields{};

    void UpdateAddress48(std::uint64_t gpuAddr) {
        fields[0] = static_cast<std::uint32_t>(gpuAddr);
        fields[1] = (fields[1] & 0xffff0000u) | (static_cast<std::uint32_t>(gpuAddr >> 32u) & 0x0000ffffu);
    }

    [[nodiscard]] std::uint16_t Stride() const { return (fields[1] >> 16u) & 0x3FFFu; }
    [[nodiscard]] bool SwizzleEnabled() const { return ((fields[1] >> 31u) & 0x1u) == 1u; }
    [[nodiscard]] std::uint32_t NumRecords() const { return fields[2]; }
    [[nodiscard]] std::uint64_t GetSize() const { return Stride() == 0 ? NumRecords() : static_cast<std::uint64_t>(Stride()) * NumRecords(); }
    [[nodiscard]] std::uint8_t DstSelX() const { return (fields[3] >> 0u) & 0x7u; }
    [[nodiscard]] std::uint8_t DstSelY() const { return (fields[3] >> 3u) & 0x7u; }
    [[nodiscard]] std::uint8_t DstSelZ() const { return (fields[3] >> 6u) & 0x7u; }
    [[nodiscard]] std::uint8_t DstSelW() const { return (fields[3] >> 9u) & 0x7u; }
    [[nodiscard]] std::uint32_t DstSelXY() const { return (fields[3] >> 0u) & 0x3Fu; }
    [[nodiscard]] std::uint32_t DstSelXYZ() const { return (fields[3] >> 0u) & 0x1FFu; }
    [[nodiscard]] std::uint32_t DstSelXYZW() const { return (fields[3] >> 0u) & 0xFFFu; }
    [[nodiscard]] bool AddTid() const { return ((fields[3] >> 23u) & 0x1u) == 1u; }
    [[nodiscard]] std::uint8_t IndexStride() const { return (fields[3] >> 21u) & 0x3u; }
    [[nodiscard]] std::uint32_t PackedStride() const { return Stride() | (static_cast<std::uint32_t>(SwizzleEnabled()) << 14u) | (static_cast<std::uint32_t>(IndexStride()) << 16u) | (static_cast<std::uint32_t>(AddTid()) << 20u); }
    [[nodiscard]] std::uint64_t Base48() const { return (fields[0] | (static_cast<std::uint64_t>(fields[1]) << 32u)) & 0xFFFFFFFFFFFFull; }
    [[nodiscard]] std::uint8_t RawFormat() const { return (fields[3] >> 12u) & 0x7Fu; }
    [[nodiscard]] IrBufferFormat Format() const { return static_cast<IrBufferFormat>(RawFormat()); }
    [[nodiscard]] std::uint8_t OutOfBounds() const { return (fields[3] >> 28u) & 0x3u; }
    [[nodiscard]] std::uint8_t Type() const { return (fields[3] >> 30u) & 0x3u; }
};

struct ShaderVertexInputBuffer {
    static constexpr int MaxAttributes = 32;

    std::uint64_t addr = 0;
    std::uint32_t stride = 0;
    std::uint32_t numRecords = 0;
    std::uint32_t fetchIndex = 0;
    int attrNum = 0;
    int attrIndices[MaxAttributes] = {0};
    std::uint32_t attrOffsets[MaxAttributes] = {0};
};

struct ShaderVertexDestination {
    int registerStart = 0;
    int registersNum = 0;
    int attrId = -1;
    std::uint32_t fetchIndex = 0;
};

struct ShaderStageRuntime {
    const CompiledShaderInfo* program = nullptr;
    ResourceSnapshot resources;

    [[nodiscard]] explicit operator bool() const {
        throw std::runtime_error("shader input helper not implemented");
    }
};

struct ShaderClipSpaceTransform {
    float scale[2] = {};
    float offset[2] = {};
    float halfExtent[2] = {};
    bool enabled = false;
};

struct ShaderWorkgroupInputInfo {
    std::uint32_t threadsNum[3] = {0, 0, 0};
    std::uint32_t ldsSizeDwords = 0;
    std::uint32_t scratchSizeDwords = 0;
    std::uint32_t hostSubgroupSize = 64;
    std::uint32_t waveSize = 64;
};

struct ShaderMeshInputInfo: ShaderWorkgroupInputInfo {
    std::uint32_t inputPrimitive = 0;
    std::uint32_t primitivesPerGroup = 0;
    std::uint32_t verticesPerGroup = 0;
    std::uint32_t maxVertices = 0;
    std::uint32_t maxPrimitives = 0;
    std::uint32_t provokingVertex = 0;
    std::uint32_t esgsItemSize = 0;

    [[nodiscard]] std::uint32_t InputPrimitiveSize() const {
        return inputPrimitive == 1u ? 1u : inputPrimitive == 2u ? 2u : 3u;
    }
    [[nodiscard]] std::uint32_t InputPrimitiveStep() const {
        return inputPrimitive == 5u || inputPrimitive == 6u ? 1u : InputPrimitiveSize();
    }
    [[nodiscard]] std::uint32_t InputPrimitiveCount(std::uint32_t vertices) const {
        return vertices < InputPrimitiveSize() ? 0u : (vertices - InputPrimitiveSize()) / InputPrimitiveStep() + 1u;
    }
    [[nodiscard]] std::uint32_t InputVertexCount(std::uint32_t primitives) const {
        return primitives == 0u ? 0u : (primitives - 1u) * InputPrimitiveStep() + InputPrimitiveSize();
    }
};

struct ShaderTessellationInputInfo {
    std::uint32_t inputControlPoints = 0;
    std::uint32_t outputControlPoints = 0;
    std::uint32_t lsStride = 0;
    std::uint32_t hsStride = 0;
    std::uint32_t domain = 0;
    std::uint32_t partitioning = 0;
    std::uint32_t outputTopology = 0;
};

struct ShaderVertexInputInfo {
    static constexpr int MaxResources = 32;

    ShaderBufferResource resources[MaxResources];
    ShaderVertexDestination resourcesDst[MaxResources];
    ShaderVertexInputBuffer buffers[MaxResources];
    ShaderStageRuntime stage;
    IrShaderStage logicalStage = IrShaderStage::Vertex;
    int resourcesNum = 0;
    int fetchAttribReg = 0;
    int fetchBufferReg = 0;
    int buffersNum = 0;
    std::uint32_t scratchSizeDwords = 0;
    std::uint32_t paClVsOutCntl = 0;
    ShaderClipSpaceTransform clipSpace;
    ShaderMeshInputInfo mesh;
    ShaderTessellationInputInfo tess;
    bool fetchExternal = false;
    bool fetchEmbedded = false;
};

struct ShaderComputeInputInfo: ShaderWorkgroupInputInfo {
    bool groupId[3] = {false, false, false};
    bool partialGroups = false;
    int threadIdsNum = 0;
    int workgroupRegister = 0;
    bool tgSizeEn = false;
    ShaderStageRuntime stage;
};

struct ShaderPixelInputInfo {
    std::uint32_t interpolatorSettings[32] = {0};
    std::uint32_t inputNum = 0;
    std::uint32_t customInterpolationMask = 0;
    static constexpr std::uint32_t NoPixelInputVgpr = std::numeric_limits<std::uint32_t>::max();
    std::array<std::uint32_t, 16> psInputVgpr = [] {
        std::array<std::uint32_t, 16> vgprs{};
        vgprs.fill(NoPixelInputVgpr);
        return vgprs;
    }();
    std::uint8_t targetOutputMode[8] = {};
    std::uint32_t scratchSizeDwords = 0;
    bool psPosX = false;
    bool psPosY = false;
    bool psPosZ = false;
    bool psPosW = false;
    bool psFrontFace = false;
    bool psAncillary = false;
    bool psNoPerspective = false;
    bool psPixelKillEnable = false;
    bool psDepthExportEnable = false;
    bool psSampleMaskExportEnable = false;
    bool psSampleShading = false;
    bool psEarlyZ = false;
    bool psExecuteOnNoop = false;
    ConservativeZExport psConservativeZExport = ConservativeZExport::AnyZ;
    bool psOrderedPixelShader = false;
    ShaderStageRuntime stage;

    bool HasPositionInput() const {
        return psPosX || psPosY || psPosZ || psPosW;
    }

    [[nodiscard]] bool InputIsDefault(std::uint32_t input) const {
        return input < inputNum && input < 32u && (interpolatorSettings[input] & 0x420u) == 0x20u;
    }

    [[nodiscard]] bool InputIsPassthrough(std::uint32_t input) const {
        return input < inputNum && input < 32u && (interpolatorSettings[input] & 0x420u) == 0x420u;
    }

    [[nodiscard]] bool InputIsCustom(std::uint32_t input) const {
        return input < 32u && ((customInterpolationMask & (1u << input)) != 0u || InputIsPassthrough(input));
    }

    [[nodiscard]] bool InputIsFlat(std::uint32_t input) const {
        return input < inputNum && input < 32u && (interpolatorSettings[input] & 0x400u) != 0u && !InputIsCustom(input);
    }

    [[nodiscard]] std::uint32_t InputSlot(std::uint32_t input) const {
        return input < 32u ? interpolatorSettings[input] & 0x1fu : input;
    }

    [[nodiscard]] std::uint32_t InputDefaultBits(std::uint32_t input, std::uint32_t component) const {
        const auto value = input < 32u ? (interpolatorSettings[input] >> 8u) & 0x3u : 0u;
        const bool one = component == 3u ? (value & 0x1u) != 0u : (value & 0x2u) != 0u;
        return one ? 0x3f800000u : 0u;
    }

    [[nodiscard]] bool InputIsFp16(std::uint32_t input) const {
        return input < inputNum && input < 32u && (interpolatorSettings[input] & 0x80000u) != 0u;
    }

    [[nodiscard]] bool InputHalfIsDefault(std::uint32_t input, bool high) const {
        if (!high) {
            return InputIsDefault(input);
        }
        return input < inputNum && input < 32u && (interpolatorSettings[input] & 0x100000u) != 0u;
    }

    [[nodiscard]] std::uint32_t InputHalfDefaultBits(std::uint32_t input, std::uint32_t component, bool high) const {
        if (!high) {
            return InputDefaultBits(input, component);
        }
        const auto value = input < 32u ? (interpolatorSettings[input] >> 21u) & 0x3u : 0u;
        const bool one = component == 3u ? (value & 0x1u) != 0u : (value & 0x2u) != 0u;
        return one ? 0x3f800000u : 0u;
    }

    [[nodiscard]] bool InputIsLinear(std::uint32_t input, std::uint32_t linearInputs, std::uint32_t perspectiveInputs) const {
        const auto bit = input < 32u ? 1u << input : 0u;
        if ((linearInputs & perspectiveInputs & bit) != 0u) {
            throw std::runtime_error("pixel input " + std::to_string(input) + " is interpolated through both a perspective and a linear I/J pair");
        }
        return (linearInputs & bit) != 0u || ((perspectiveInputs & bit) == 0u && psNoPerspective);
    }
};

struct ShaderStageInputInfo {
    const ShaderVertexInputInfo* vertex = nullptr;
    const ShaderPixelInputInfo* pixel = nullptr;
    const ShaderComputeInputInfo* compute = nullptr;
};

}

#endif

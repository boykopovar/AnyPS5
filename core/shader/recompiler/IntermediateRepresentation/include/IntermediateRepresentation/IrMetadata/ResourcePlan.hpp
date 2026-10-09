#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_RESOURCEPLAN_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_RESOURCEPLAN_HPP

#include "IntermediateRepresentation/IrBlock.hpp"
#include "IntermediateRepresentation/IrValue.hpp"
#include "IntermediateRepresentation/IrMetadata/ControlFlowInfo.hpp"
#include "IntermediateRepresentation/IrMetadata/DescriptorBinding.hpp"
#include "IntermediateRepresentation/IrMetadata/ResourceInfo.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderInfo.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderStage.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace ShaderRecompiler {

struct DescriptorValue {
    std::array<std::uint32_t, 8> dwords = {};
    std::uint32_t dwordCount = 0;

    bool operator==(const DescriptorValue& other) const = default;
};

enum class UniformFillKind { None, Buffer, Image };

struct UniformFill {
    UniformFillKind kind = UniformFillKind::None;
    std::uint32_t resource = 0;
    std::array<std::uint32_t, 3> groupStride {};
    std::uint32_t words = 0;
    std::uint32_t value = 0;

    bool operator==(const UniformFill& other) const = default;
};

struct ResourceSnapshot {
    std::vector<DescriptorValue> buffers;
    std::vector<DescriptorValue> images;
    std::vector<DescriptorValue> samplers;
    std::vector<std::uint32_t> flattenedSrt;
    std::vector<std::uint32_t> userData;
    UniformFill uniformFill;
    std::vector<SrtReadPoison> srtPoison;
};

struct UniformFillPlan {
    UniformFill fill;
    std::array<IrValue*, 4> values{};
};

inline constexpr std::uint32_t NativePushConstantSize = sizeof(PushData);

struct IrResourcePlan {
    IrShaderStage stage = IrShaderStage::Unknown;
    std::uint64_t shaderHash = 0;
    std::uint32_t userDataBase = 0;
    std::uint32_t userDataCount = 64;
    std::uint32_t srgbDecodeFormats = 0;
    std::vector<std::unique_ptr<IrValue>> valueStorage;
    std::vector<std::unique_ptr<IrBlock>> blockStorage;
    std::vector<MemoryInfo> memoryInfo;
    std::vector<DescriptorSource> descriptorSources;
    std::vector<ResourceBlock> controlFlow;
    std::vector<std::uint32_t> materializationSources;
    std::vector<SrtRead> srtReads;
    std::vector<std::uint32_t> guardedSrtSlots;
    std::uint32_t srtGuardOffset = 0;
    std::vector<std::uint8_t> cleanFlatSlots;
    // One byte per srtReads slot, 1 when the CPU walk never consumes the slot's value (see
    // Detail::ComputePureFlatSlots): a driver may reuse a capture whose words differ only there.
    std::vector<std::uint8_t> pureFlatSlots;
    bool requiresSpecializationMemory = false;
    bool srtPlanComplete = false;
    bool resourceTrackingComplete = false;
    ShaderInfo info;
    UniformFillPlan uniformFill;
};

}

#endif

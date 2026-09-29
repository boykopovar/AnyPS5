#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_DESCRIPTORBINDING_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_DESCRIPTORBINDING_HPP

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace ShaderRecompiler {

inline constexpr std::uint32_t FirstImageBinding = 1u;
inline constexpr std::uint32_t FirstComparisonImageBinding = 22u;
inline constexpr std::uint32_t FirstStorageImageBinding = 29u;
inline constexpr std::uint32_t ImageBindingCount = 43u;

enum class DescriptorBindingKind : std::uint32_t {
    Buffers = 0u,
    Samplers = FirstImageBinding + ImageBindingCount,
    Gds,
    BdaPagetable,
    FaultBuffer,
    FlattenedSrt,
    ShaderData,
    Count,
};

struct PushData {
    static constexpr std::uint32_t DwordCount = 32;
    static constexpr std::uint32_t MeshDrawDwordCount = 6;
    static constexpr std::uint32_t NoStart = std::numeric_limits<std::uint32_t>::max();
    std::array<std::uint32_t, DwordCount> dwords {};

    [[nodiscard]] static bool CanFit(std::uint32_t start, std::uint32_t size) {
        return size != 0u && start <= DwordCount && size <= DwordCount - start;
    }
    [[nodiscard]] static std::uint32_t StartFor(std::uint32_t cursor, std::uint32_t size) {
        return CanFit(cursor, size) ? cursor : NoStart;
    }
};

struct IrDescriptorBinding {
    DescriptorBindingKind kind = DescriptorBindingKind::Buffers;
    std::vector<std::uint32_t> resources;

    bool operator==(const IrDescriptorBinding& other) const = default;
};

struct IrBindingLayout {
    std::uint32_t pushDataStartDword = PushData::NoStart;
    std::uint32_t memoryOffsetDword = 0;
    std::uint32_t memoryOffsetCount = 0;
    // Three dwords after the memory offsets hold the partial-group dispatch size (ShaderInfo::
    // dispatchThreadLimit).
    bool dispatchThreadLimit = false;
    std::vector<std::uint32_t> userDataRegisters;
    std::vector<IrDescriptorBinding> descriptors;

    [[nodiscard]] std::uint32_t DispatchThreadLimitDword() const {
        return memoryOffsetDword + (memoryOffsetCount + 3u) / 4u;
    }
    [[nodiscard]] std::uint32_t ShaderDataDwords() const {
        return DispatchThreadLimitDword() + (dispatchThreadLimit ? 3u : 0u);
    }
    [[nodiscard]] bool UsesPushData() const {
        return pushDataStartDword != PushData::NoStart;
    }
    void AdvancePushData(std::uint32_t& cursor) const {
        if (UsesPushData()) {
            cursor = pushDataStartDword + ShaderDataDwords();
        }
    }

    bool operator==(const IrBindingLayout& other) const = default;
};

}

#endif

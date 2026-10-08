#ifndef CORE_SHADER_RECOMPILER_PIPELINESPECIALIZATION_HPP
#define CORE_SHADER_RECOMPILER_PIPELINESPECIALIZATION_HPP

#include <cstdint>

namespace ShaderRecompiler {

struct PipelineSpecializationConstant {
    std::uint32_t id;
    std::uint32_t value;

    bool operator==(const PipelineSpecializationConstant&) const = default;
};

namespace PipelineSpecialization {

inline constexpr std::uint32_t CompareBase = 49152u;
inline constexpr std::uint32_t CompareWords = 6u;
inline constexpr std::uint32_t BufferBase = 0u;
inline constexpr std::uint32_t BufferWords = 4u;
inline constexpr std::uint32_t ImageBase = 1024u;
inline constexpr std::uint32_t ImageWords = 5u;
inline constexpr std::uint32_t VertexBase = 4096u;
inline constexpr std::uint32_t VertexWords = 6u;
inline constexpr std::uint32_t PushDataOffset = 8188u;
inline constexpr std::uint32_t ExportBase = 8192u;
inline constexpr std::uint32_t ExportPackingBase = 8224u;
inline constexpr std::uint32_t DualSourceBlend = 8232u;
inline constexpr std::uint32_t HeapCountBase = 8256u;
inline constexpr std::uint32_t MipCountBase = 8320u;
inline constexpr std::uint32_t DescriptorIndexBase = 16384u;
inline constexpr std::uint32_t DescriptorIndexStride = 128u;
inline constexpr std::uint32_t ImageModeBase = 32768u;
inline constexpr std::uint32_t ImageModeStride = 256u;

inline constexpr std::uint32_t DescriptorIndex(std::uint32_t binding, std::uint32_t element) {
    return DescriptorIndexBase + binding * DescriptorIndexStride + element;
}

}

}

#endif

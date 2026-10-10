#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_RESOURCEINFO_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_RESOURCEINFO_HPP

#include "RdnaDecoder/RdnaInstruction.hpp"
#include <cstdint>

namespace ShaderRecompiler {

enum class ResourceKind {
    None,
    ScalarBuffer,
    ScalarAddress,
    Buffer,
    Flat,
    Global,
    Scratch,
    Lds,
    Gds,
    Image,
    Sampler
};

struct MemoryInfo {
    ResourceKind kind = ResourceKind::None;
    std::uint32_t resource = 0;
    std::uint32_t sampler = 0;
    std::uint32_t offset = 0;
    std::uint32_t secondaryOffset = 0;
    std::uint32_t dmask = 0;
    std::uint32_t dataDwords = 1;
    std::uint32_t dataBits = 32;
    std::uint32_t componentIndex = 0;
    std::uint32_t componentCount = 1;
    std::uint32_t dataFormat = 0;
    std::uint32_t numberFormat = 0;
    std::uint32_t imageSampleFlags = 0;
    RdnaImageDimension imageDimension = RdnaImageDimension::Unknown;
    std::uint32_t imageAddressComponents = 0;
    bool addressIsFull = false;
    bool dataSigned = false;
    bool typed = false;
    bool formatted = false;
    bool d16 = false;
    bool imageHasMip = false;
    bool imageR128 = false;
    bool imagePacked = false;
    std::uint32_t imageByElements = 0;
    bool idxen = false;
    bool offen = false;
    bool planningOnly = false;
    bool coherent = false;
    bool gpuDescriptor = false;
    bool flushDenormals = false;

    bool operator==(const MemoryInfo& other) const = default;
};

enum class ExportTargetKind { Unknown, Null, Position, Primitive, Parameter, Mrt, MrtZ };

struct ExportInfo {
    ExportTargetKind kind = ExportTargetKind::Unknown;
    std::uint32_t target = 0;
    std::uint32_t index = 0;
    std::uint32_t en = 0;
    bool done = false;
    bool compr = false;
    bool vm = false;

    bool operator==(const ExportInfo& other) const = default;
};

}

#endif

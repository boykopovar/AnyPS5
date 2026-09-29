#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_STAGEIO_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRMETADATA_STAGEIO_HPP

#include <cstdint>
#include <limits>
#include <string>

namespace ShaderRecompiler {

enum class TessellationAttribute {
    LocalOutput,
    ControlInput,
    ControlOutput,
    EvaluationInput,
    PatchOutput,
    Factor
};

enum class StageInputKind {
    VertexIndex,
    InvocationId,
    PrimitiveId,
    TessCoord,
    InstanceIndex,
    FragCoord,
    FrontFacing,
    PackedAncillary,
    Layer,
    SampleId,
    BaryCoordSmooth,
    BaryCoordNoPerspective,
    WorkgroupId,
    LocalInvocationId,
    LocalInvocationIndex,
    GlobalInvocationId,
    // Not an input: a partial-group dispatch's size in threads, from shader data
    // (IrBindingLayout::DispatchThreadLimitDword).
    DispatchThreadLimit,
    Parameter,
};

enum class StageOutputKind {
    Position,
    Parameter,
    Mrt,
    Depth,
    SampleMask,
    PointSize,
    ClipDistance,
    CullDistance,
    Layer,
    ViewportIndex
};

struct PositionExportComponent {
    std::uint32_t clipDistance = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t cullDistance = std::numeric_limits<std::uint32_t>::max();
    bool pointSize = false;
    bool layer = false;
    bool viewport = false;
};

struct StageInput {
    StageInputKind kind = StageInputKind::VertexIndex;
    std::uint32_t location = 0;
    std::uint32_t componentCount = 1;
    std::string debugName;
    bool perVertex = false;

    bool operator==(const StageInput& other) const = default;
};

struct StageOutput {
    StageOutputKind kind = StageOutputKind::Parameter;
    std::uint32_t index = 0;
    std::uint32_t location = 0;
    std::string debugName;

    bool operator==(const StageOutput& other) const = default;
};

}

#endif

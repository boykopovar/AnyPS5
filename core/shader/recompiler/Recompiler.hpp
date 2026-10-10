#ifndef CORE_SHADER_RECOMPILIER_INCLUDE_SHADER_RECOMPILIER_RECOMPILER_HPP
#define CORE_SHADER_RECOMPILIER_INCLUDE_SHADER_RECOMPILIER_RECOMPILER_HPP

#include "RuntimeAbi.hpp"
#include "PipelineSpecialization.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <string_view>
#include <vector>

namespace ShaderRecompiler {

enum class ShaderStage {
    Compute,
    Vertex,
    TessellationControl,
    TessellationEvaluation,
    Geometry,
    Fragment,
    Local,
    Mesh
};

struct MemoryRegion {
    std::uint64_t guestAddress;
    std::span<const std::byte> bytes;
};

struct ShaderBinary {
    ShaderStage stage;
    std::uint64_t codeAddress;
    std::span<const std::uint32_t> code;
    std::uint64_t headerAddress;
    std::span<const std::byte> header;
};

struct ShaderComputeStageInfo {
    std::array<std::uint32_t, 3> numThreads;
    std::uint32_t ldsSizeDwords;
    std::array<bool, 3> groupIdEnable;
    bool tgSizeEnable;
    std::uint32_t threadIdComponentCount;
    std::array<std::uint32_t, 3> partialThreads;
    std::uint32_t scratchDwords = 0;

    [[nodiscard]] bool PartialGroups() const {
        return partialThreads != std::array<std::uint32_t, 3>{};
    }
};

enum class PixelInput : std::uint32_t {
    PerspectiveSample,
    PerspectiveCenter,
    PerspectiveCentroid,
    PerspectivePullModel,
    LinearSample,
    LinearCenter,
    LinearCentroid,
    LineStipple,
    PositionX,
    PositionY,
    PositionZ,
    PositionW,
    FrontFace,
    Ancillary,
    SampleCoverage,
    PositionFixedPoint,
    Count
};

constexpr std::uint32_t PixelInputBit(PixelInput input) {
    return 1u << static_cast<std::uint32_t>(input);
}

constexpr std::uint32_t PixelInputVgprCount(PixelInput input) {
    switch (input) {
    case PixelInput::PerspectiveSample:
    case PixelInput::PerspectiveCenter:
    case PixelInput::PerspectiveCentroid:
    case PixelInput::LinearSample:
    case PixelInput::LinearCenter:
    case PixelInput::LinearCentroid:
        return 2u;
    case PixelInput::PerspectivePullModel:
        return 3u;
    default:
        return 1u;
    }
}

constexpr std::uint32_t PixelInputVgpr(std::uint32_t inputAddr, PixelInput input) {
    std::uint32_t vgpr = 0;
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(input); ++i) {
        if ((inputAddr & (1u << i)) != 0u) vgpr += PixelInputVgprCount(static_cast<PixelInput>(i));
    }
    return vgpr;
}

enum class ConservativeZExport : std::uint8_t {
    AnyZ,
    LessThanZ,
    GreaterThanZ
};

enum class ColorExportPacking : std::uint8_t {
    None,
    Unorm10_11_11
};

struct ShaderPixelStageInfo {
    std::uint32_t interpolatorCount;
    std::array<std::uint32_t, 32> interpolatorSettings;
    bool wave32;
    std::uint32_t inputAddr;
    bool hasPerspectiveCenterVgpr;
    bool perspectiveCentroid;
    bool posX;
    bool posY;
    bool posZ;
    bool posW;
    bool frontFace;
    bool ancillary;
    bool sampleShading;
    bool noPerspective;
    bool linearCentroid;
    bool pixelKillEnable;
    bool depthExportEnable;
    bool sampleMaskExportEnable;
    bool earlyZ;
    bool executeOnNoop;
    ConservativeZExport conservativeZExport;
    bool orderedPixelShader;
    std::array<std::uint8_t, 8> targetOutputMode;
    std::array<std::uint8_t, 8> targetExportMapping;
    std::array<ColorExportPacking, 8> targetExportPacking;
    bool dualSourceBlend;
};

struct ShaderVertexBufferResource {
    std::array<std::uint32_t, 4> fields;
};

struct ShaderVertexResourceDestination {
    std::int32_t registerStart;
    std::int32_t registersNum;
    std::int32_t attrId;
    std::uint32_t fetchIndex;
};

struct ShaderVertexStageInfo {
    static constexpr std::uint32_t MaxResources = 32;
    std::array<ShaderVertexBufferResource, MaxResources> resources;
    std::array<ShaderVertexResourceDestination, MaxResources> resourcesDst;
    std::uint32_t resourcesNum;
    std::uint32_t fetchAttribReg;
    std::uint32_t fetchBufferReg;
    bool fetchEmbedded;
};

struct ShaderFloatMode {
    std::uint32_t floatMode = 0;
    bool dx10Clamp = false;
    bool ieeeMode = false;
    bool fp16Overflow = false;

    bool operator==(const ShaderFloatMode&) const = default;
};

inline constexpr std::uint32_t InterpolationQuiet = 1u;
inline constexpr std::uint32_t InterpolationFlush32 = 2u;
inline constexpr std::uint32_t InterpolationFlush16 = 4u;

struct GuestContext {
    std::uint32_t waveSize;
    std::uint32_t userDataBaseRegister;
    std::span<const std::uint32_t> userData;
    std::optional<ShaderComputeStageInfo> compute;
    std::optional<ShaderPixelStageInfo> pixel;
    std::optional<ShaderVertexStageInfo> vertex;
    std::span<const MemoryRegion> memory;
    std::optional<ShaderFloatMode> floatMode;
};

struct MeshTargetLimits {
    std::array<std::uint32_t, 3> maxWorkgroupSize;
    std::uint32_t maxWorkgroupInvocations;
    std::uint32_t maxSharedMemoryBytes;
    std::uint32_t maxOutputVertices;
    std::uint32_t maxOutputPrimitives;
    std::uint32_t maxOutputComponents;
    std::uint32_t maxOutputMemoryBytes;
    std::uint32_t outputPerVertexGranularity;
    std::uint32_t outputPerPrimitiveGranularity;
};

struct TessellationTargetLimits {
    std::uint32_t maxPatchSize;
    std::uint32_t maxControlPerVertexInputComponents;
    std::uint32_t maxControlPerVertexOutputComponents;
    std::uint32_t maxControlPerPatchOutputComponents;
    std::uint32_t maxControlTotalOutputComponents;
    std::uint32_t maxEvaluationInputComponents;
    std::uint32_t maxEvaluationOutputComponents;
};

struct SpirvTarget {
    std::uint32_t vulkanVersion;
    std::uint32_t spirvVersion;
    std::uint32_t subgroupSize;
    std::uint32_t bdaAbiVersion;
    std::span<const std::uint32_t> supportedCapabilities;
    std::span<const std::string_view> supportedExtensions;
    bool fragmentShaderBarycentricEnabled;
    std::array<std::uint32_t, 3> maxWorkgroupSize;
    std::uint32_t maxWorkgroupInvocations;
    std::uint32_t maxWorkgroupSharedMemoryBytes;
    std::optional<MeshTargetLimits> mesh;
    std::optional<TessellationTargetLimits> tessellation;
    bool nonConstantImageOffsets = false;
    std::uint32_t srgbDecodeFormats = 0;
    bool narrowSubgroupClock = false;
    bool fixedPushSlots = false;
};

struct BindingLayout {
    std::uint32_t descriptorSet;
    std::uint32_t firstBinding;
    std::uint32_t pushConstantOffsetBytes;
    std::uint32_t pushConstantSizeBytes;
};

enum class ProgramRole {
    Main,
    GeometryBack,
    Local,
    Hull,
    Domain,
    Fragment
};

struct LinkedProgram {
    ProgramRole role;
    ShaderBinary binary;
    std::uint32_t userDataBaseRegister;
    std::uint32_t firstUserSgpr;
    std::span<const std::uint32_t> userData;
};

struct MeshConfiguration {
    std::uint32_t inputPrimitive;
    std::uint32_t primitivesPerGroup;
    std::uint32_t verticesPerGroup;
    std::uint32_t maxVertices;
    std::uint32_t maxPrimitives;
    std::uint32_t threadsPerGroup;
    std::uint32_t ldsSizeDwords;
    std::uint32_t provokingVertex;
    std::uint32_t esgsItemSize = 0;
};

struct TessellationConfiguration {
    std::uint32_t inputControlPoints;
    std::uint32_t outputControlPoints;
    std::uint32_t domain;
    std::uint32_t partitioning;
    std::uint32_t outputTopology;
};

inline constexpr std::uint32_t MeshDrawPushOffsetBytes = 104;
inline constexpr std::uint32_t MeshDrawPushBytes = 24;
inline constexpr std::uint32_t MeshArgumentAddressDword = 4;
inline constexpr std::uint32_t MeshArgumentIndexCountDword = 3;
inline constexpr std::uint32_t MeshArgumentFirstIndexDword = 4;
inline constexpr std::uint32_t MeshArgumentBytes = 20;
inline constexpr std::uint32_t MeshIndexBufferUserWord = 4;
inline constexpr std::uint32_t WorkgroupMemoryDescriptorSet = 1;

struct GraphicsDrawParameters {
    std::uint64_t indexAddress;
    std::uint32_t indexCount;
    std::uint32_t indexElementBytes;
    std::uint32_t instanceCount;
};

struct GraphicsCompileContext {
    std::uint32_t firstUserSgpr;
    std::span<const LinkedProgram> linkedPrograms;
    std::optional<MeshConfiguration> mesh;
    std::optional<TessellationConfiguration> tessellation;
    GraphicsDrawParameters draw;
};

struct RecompileRequest {
    ShaderBinary shader;
    GuestContext context;
    SpirvTarget target;
    BindingLayout layout;
    std::optional<GraphicsCompileContext> graphics;
    bool useCache = true;
};

enum class DescriptorKind {
    UniformBuffer,
    StorageBuffer,
    UniformTexelBuffer,
    StorageTexelBuffer,
    SampledImage,
    StorageImage,
    Sampler
};

enum class DescriptorImageShape {
    Image1D,
    Image2D,
    Image2DArray,
    ImageCube,
    Image3D,
    Image1DArray
};

enum class DescriptorRole {
    GuestBuffers,
    GuestImages,
    GuestSamplers,
    Gds,
    BdaPagetable,
    FaultBuffer,
    FlattenedSrt,
    ShaderData,
    ImageTable,
    SamplerTable,
    ImageTableMap
};

struct DescriptorBinding {
    DescriptorKind kind;
    DescriptorRole role;
    std::uint32_t descriptorSet;
    std::uint32_t binding;
    std::uint32_t count;
    std::vector<std::uint32_t> guestDescriptor;
    bool readOnly = false;
    std::optional<DescriptorImageShape> imageShape;
    std::vector<bool> samplerDepthCompare;
    // Guest image elements the shader stores to (or updates atomically); the others are only read.
    std::vector<bool> imageWritten;
    std::vector<bool> imageDepthCompare;
    std::vector<bool> imageAtomic;
    std::vector<bool> imageAtomic64;
    std::vector<DescriptorImageShape> imageShapes;
    // Guest buffer elements the shader updates atomically (one entry per element of a GuestBuffers
    // binding, empty otherwise). An atomic on a host-imported range is a serialized PCIe round trip
    // (~0.4-0.5 us each on NVIDIA), so a driver may keep these elements in device-local memory.
    std::vector<bool> bufferAtomic;
    // Guest buffer elements the shader may store to through this V# (any store or atomic in the
    // program, whatever its offset), one entry per element of a GuestBuffers binding, empty
    // otherwise. A false entry is proved: every access of that element is a load. A driver may then
    // skip the write-back and the pending-write note for the element; an element beyond the vector
    // (a producer that does not fill it) must be treated as written.
    std::vector<bool> bufferWritten;
    std::vector<bool> samplerUnnormalized;
    std::vector<bool> imageUnnormalized;
    std::vector<std::uint32_t> imageSamplers;
    std::vector<bool> bufferRead;
};

struct ImageTableEntryPoison {
    std::array<std::uint32_t, 8> words{};
    std::uint32_t dwordCount = 0;
    std::uint32_t resource = 0;
    std::uint32_t reason = 0;
};

struct VertexAttribute {
    std::uint32_t location;
    std::uint32_t components;
    ShaderVertexBufferResource resource;
    std::uint32_t fetchIndex;
    std::uint32_t formatComponents = 0;
};

struct FragmentParameter {
    std::uint32_t location;
    std::uint32_t sourceLocation;
    bool flat;
    bool perVertex;
    bool custom = false;
};

struct BarycentricEmulation {
    bool active = false;
    bool smooth = false;
    bool linear = false;
};

struct BarycentricEmulationLayout {
    static constexpr std::uint32_t NoLocation = 0xffffffffu;
    std::uint32_t smoothLocation = NoLocation;
    std::uint32_t linearLocation = NoLocation;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> perVertexLocations;
};

[[nodiscard]] BarycentricEmulationLayout LayoutBarycentricEmulation(std::span<const FragmentParameter> parameters, const BarycentricEmulation& emulation);

// Compiled SPIR-V shared between a cached variant and every result materialized from it: results
// are copied per dispatch and draw, so the words are reference counted and only duplicated when a
// holder writes to them (tests and tools patch modules in place). Reads look like a vector.
// The non-const data(), operator[], begin() and end() count as writes: on a shared holder (every
// cached result) they clone the module, so a consumer that only reads takes the result by const
// reference, or the per-dispatch copy this class removes comes back without a compiler hint.
class SharedSpirv {
public:
    SharedSpirv() = default;
    SharedSpirv(std::vector<std::uint32_t> words) : words(std::make_shared<std::vector<std::uint32_t>>(std::move(words))) {}
    SharedSpirv& operator=(std::vector<std::uint32_t> other) {
        words = std::make_shared<std::vector<std::uint32_t>>(std::move(other));
        return *this;
    }

    [[nodiscard]] const std::vector<std::uint32_t>& Words() const { return words ? *words : Empty(); }
    operator const std::vector<std::uint32_t>&() const { return Words(); }
    [[nodiscard]] std::size_t size() const { return Words().size(); }
    [[nodiscard]] bool empty() const { return Words().empty(); }
    [[nodiscard]] const std::uint32_t* data() const { return Words().data(); }
    [[nodiscard]] std::uint32_t* data() { return Mutable().data(); }
    [[nodiscard]] const std::uint32_t& operator[](std::size_t index) const { return Words()[index]; }
    [[nodiscard]] std::uint32_t& operator[](std::size_t index) { return Mutable()[index]; }
    [[nodiscard]] std::vector<std::uint32_t>::const_iterator begin() const { return Words().begin(); }
    [[nodiscard]] std::vector<std::uint32_t>::const_iterator end() const { return Words().end(); }
    [[nodiscard]] std::vector<std::uint32_t>::iterator begin() { return Mutable().begin(); }
    [[nodiscard]] std::vector<std::uint32_t>::iterator end() { return Mutable().end(); }
    void resize(std::size_t count) { Mutable().resize(count); }
    std::vector<std::uint32_t>::iterator insert(std::vector<std::uint32_t>::const_iterator where, std::initializer_list<std::uint32_t> values) { return Mutable().insert(where, values); }
    friend bool operator==(const SharedSpirv& left, const SharedSpirv& right) { return left.words == right.words || left.Words() == right.Words(); }

private:
    static const std::vector<std::uint32_t>& Empty() {
        static const std::vector<std::uint32_t> empty;
        return empty;
    }
    // Copy on write: a holder whose words are shared gets its own copy before the first write.
    std::vector<std::uint32_t>& Mutable() {
        if (words == nullptr || words.use_count() != 1) words = std::make_shared<std::vector<std::uint32_t>>(Words());
        return *words;
    }

    std::shared_ptr<std::vector<std::uint32_t>> words;
};

struct VertexInput {
    std::uint32_t location;
    std::uint32_t components;
    std::uint32_t fetchIndex;
    std::uint32_t outputMask = 0;

    bool operator==(const VertexInput& other) const = default;
};

struct VertexInputPatch {
    std::uint32_t location;
    std::uint32_t word;
    std::array<std::uint32_t, 3> values;

    bool operator==(const VertexInputPatch& other) const = default;
};

struct CompiledShaderArtifact {
    std::vector<VertexInputPatch> vertexInputPatches;
    SharedSpirv spirv;
    std::uint32_t memoryOffsetDword = 0;
    std::uint32_t shaderDataDwords = 0;
    std::uint32_t imageMetadataDword = 0;
    std::uint32_t runtimeImageCount = 0;
    std::vector<std::uint32_t> runtimeImageResources;
    std::uint32_t bdaAbiVersion = 0;
    std::uint32_t runtimeAbiVersion = RuntimeAbi::Version;
    std::vector<VertexInput> vertexInputs;
    std::int32_t vertexOffsetSgpr = -1;
    std::int32_t instanceOffsetSgpr = -1;
    bool vertexOffsetShared = false;
    bool instanceOffsetShared = false;
    bool vertexOffsetConflict = false;
    bool instanceOffsetConflict = false;
    std::uint32_t hostSubgroupSize = 0;
    std::vector<std::uint32_t> parameterExports;
    std::vector<FragmentParameter> fragmentParameters;
    BarycentricEmulation barycentricEmulation;
    std::uint64_t variantId = 0;
};

struct ShaderInvocation {
    std::vector<PipelineSpecializationConstant> specialization;
    std::uint64_t specializationId = 0;
    std::vector<DescriptorBinding> bindings;
    std::vector<std::byte> pushConstants;
    std::vector<VertexAttribute> vertexAttributes;
    std::uint32_t poisonedSrtReads = 0;
    std::vector<ImageTableEntryPoison> imageTablePoison;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> imageTableRanges;
    std::vector<std::vector<std::pair<std::uint64_t, std::uint64_t>>> imageTableReadRanges;
    std::uint64_t imageTableShader = 0;
    std::uint32_t imageTableFaults = 0;
};

struct RecompileResult : CompiledShaderArtifact, ShaderInvocation {
    bool cacheHit = false;
    std::uint32_t workgroupMemoryDwords = 0;
    [[nodiscard]] std::uint64_t PipelineVariantId() const { return specializationId != 0 ? specializationId : variantId; }
};

[[nodiscard]] RecompileResult Recompile(const RecompileRequest& request);

struct ResourceCapture;
[[nodiscard]] std::shared_ptr<const RecompileResult> Recompile(const RecompileRequest& request, const ResourceCapture& capture, bool* memoHit = nullptr);

// Debug aid (see DebugProbe in Translation/TranslationContext.hpp): the APS5_PROBE register probe is
// only applied while a driver has it active, so it can be limited to one dispatch; the recompile
// cache keys on it.
void SetDebugProbeActive(bool active);
[[nodiscard]] bool DebugProbeActive();
[[nodiscard]] bool RayTracingStrict();
[[nodiscard]] bool RayTracingMiss();

struct RectListShaders {
    RecompileResult control;
    RecompileResult evaluation;
};

[[nodiscard]] RectListShaders BuildRectListShaders(const RecompileResult& vertex, const RecompileResult& fragment, const SpirvTarget& target);

struct GeometryStageLimits {
    std::uint32_t maxGeometryInputComponents;
    std::uint32_t maxGeometryOutputComponents;
    std::uint32_t maxGeometryOutputVertices;
    std::uint32_t maxGeometryTotalOutputComponents;
    std::uint32_t maxFragmentInputComponents;
};

[[nodiscard]] RecompileResult BuildBarycentricGeometryShader(const RecompileResult& vertex, const RecompileResult& fragment, const SpirvTarget& target, const std::optional<GeometryStageLimits>& limits);

}

#endif

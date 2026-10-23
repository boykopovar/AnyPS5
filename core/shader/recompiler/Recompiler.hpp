#ifndef CORE_SHADER_RECOMPILIER_INCLUDE_SHADER_RECOMPILIER_RECOMPILER_HPP
#define CORE_SHADER_RECOMPILIER_INCLUDE_SHADER_RECOMPILIER_RECOMPILER_HPP

#include "RuntimeAbi.hpp"
#include "PipelineSpecialization.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#include <algorithm>
#include <array>
#include <atomic>
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
    Unorm10_11_11,
    Float11_11_10
};

enum class RasterPrimitive : std::uint8_t {
    Point,
    Line,
    Triangle,
    Rectangle
};

enum class PixelPositionLocation : std::uint8_t {
    Center,
    Centroid,
    Sample,
    UpperLeft
};

constexpr float SampleLocationOffset(std::uint8_t location, std::uint32_t axis) {
    const auto nibble = (static_cast<std::uint32_t>(location) >> (4u * axis)) & 0xfu;
    return static_cast<float>(static_cast<std::int32_t>(nibble & 7u) - static_cast<std::int32_t>(nibble & 8u)) / 16.0f;
}

namespace ReversedBlendCode {

inline constexpr std::uint32_t Enable = 1u;
inline constexpr std::uint32_t ColorShift = 8u;
inline constexpr std::uint32_t SecondShift = 16u;
inline constexpr std::uint32_t RouteBits = 2u;
inline constexpr std::uint32_t RouteMask = 3u;
inline constexpr std::uint32_t Routes = 0xffu;
inline constexpr std::uint32_t Own = 0u;
inline constexpr std::uint32_t Component0 = 1u;
inline constexpr std::uint32_t Zero = 2u;
inline constexpr std::uint32_t One = 3u;

}

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
    bool frontFaceAllBits;
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
    bool alphaToCoverage;
    bool stencilRefExportEnable;
    bool stencilOperationValueExport = false;
    RasterPrimitive rasterPrimitive = RasterPrimitive::Triangle;
    bool layerExported = false;
    bool ditheredAlphaToCoverage = false;
    std::uint8_t quadPixelMask = 0xf;
    std::array<std::uint16_t, 4> quadSampleMask{0xffffu, 0xffffu, 0xffffu, 0xffffu};
    std::uint32_t reversedBlend = 0;
    bool iteratesSamples = false;
    PixelPositionLocation positionLocation = PixelPositionLocation::Center;
    std::array<std::uint8_t, 32> sampleLocations{};
    std::array<std::uint32_t, 4> sampleOrder{};
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
    std::uint32_t paClVsOutCntl = 0;
    std::uint32_t depthRemapMode = 0;
    bool pointList = false;
    float pointSize = 1.0f;
    float pointSizeMin = 0.0f;
    float pointSizeMax = 0.0f;
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
inline constexpr std::uint32_t InterpolationRound16Shift = 3u;

struct GuestContext {
    std::uint32_t waveSize;
    std::uint32_t userDataBaseRegister;
    std::span<const std::uint32_t> userData;
    std::optional<ShaderComputeStageInfo> compute;
    std::optional<ShaderPixelStageInfo> pixel;
    std::optional<ShaderVertexStageInfo> vertex;
    std::span<const MemoryRegion> memory;
    std::optional<ShaderFloatMode> floatMode;
    std::span<const std::uint32_t> linkedCallFallbacks;
    bool convertedSamples = false;
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
    bool float64DenormPreserve = false;
    bool registrationModules = false;
    std::uint32_t registrationWordLimit = 200000u;
    bool vertexSubgroups = true;
    bool float16DenormPreserve = false;
    bool spannedWaves = false;
    bool robustBufferAccess2 = false;
    bool depthRangeRemap = false;
    bool float32FlushToZero = false;
    bool waveLdsMemoryBarriers = false;
    bool float32DenormPreserve = false;
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
    std::uint32_t outputPrimitiveSize = 3;
};

struct TessellationConfiguration {
    std::uint32_t inputControlPoints;
    std::uint32_t outputControlPoints;
    std::uint32_t domain;
    std::uint32_t partitioning;
    std::uint32_t outputTopology;
    std::uint32_t patchesPerGroup = 256;
    std::uint32_t primitiveGroupSize = 0;
    bool breakAtInstance = false;
    std::uint32_t instancePatches = 0;
    bool drawParameters = false;
    bool mergedHull = false;
};

inline constexpr std::uint32_t MeshDrawPushOffsetBytes = 104;
inline constexpr std::uint32_t MeshDrawPushBytes = 24;
inline constexpr std::uint32_t TessellationLevelPushOffsetBytes = 120;
inline constexpr std::uint32_t TessellationLevelPushBytes = 8;
inline constexpr std::uint32_t DepthRemapPushOffsetBytes = 104;
inline constexpr std::uint32_t DepthRemapPushBytes = 8;
inline constexpr std::uint32_t MeshArgumentAddressDword = 4;
inline constexpr std::uint32_t MeshArgumentIndexCountDword = 3;
inline constexpr std::uint32_t MeshArgumentFirstIndexDword = 4;
inline constexpr std::uint32_t MeshArgumentBytes = 20;
inline constexpr std::uint32_t MeshIndexRestartTable = 0x100;
inline constexpr std::uint32_t MeshRestartLengthDword = MeshArgumentBytes / 4;
inline constexpr std::uint32_t MeshRestartTableDword = MeshRestartLengthDword + 1;
inline constexpr std::uint32_t MeshIndexBufferUserWord = 4;
inline constexpr std::uint32_t WorkgroupMemoryDescriptorSet = 1;

struct GraphicsDrawParameters {
    std::uint64_t indexAddress;
    std::uint32_t indexCount;
    std::uint32_t indexElementBytes;
    std::uint32_t instanceCount;
};

struct FragmentParameter {
    std::uint32_t location;
    std::uint32_t sourceLocation;
    bool flat;
    bool perVertex;
    bool custom = false;
    bool flatP10 = false;
    bool vertexOrder = false;
    bool flatP20 = false;
    bool operator==(const FragmentParameter&) const = default;
};

struct BarycentricEmulation {
    bool active = false;
    bool smooth = false;
    bool linear = false;
    bool smoothOffset = false;
    bool operator==(const BarycentricEmulation&) const = default;
};

struct MeshBarycentricLayout {
    BarycentricEmulation emulation;
    std::vector<FragmentParameter> parameters;
    bool operator==(const MeshBarycentricLayout&) const = default;
};

struct GraphicsCompileContext {
    std::uint32_t firstUserSgpr;
    std::span<const LinkedProgram> linkedPrograms;
    std::optional<MeshConfiguration> mesh;
    std::optional<TessellationConfiguration> tessellation;
    GraphicsDrawParameters draw;
    std::optional<MeshBarycentricLayout> meshBarycentrics;
};

struct RecompileRequest {
    ShaderBinary shader;
    GuestContext context;
    SpirvTarget target;
    BindingLayout layout;
    std::optional<GraphicsCompileContext> graphics;
    bool useCache = true;
};

inline std::uint32_t TessellationInstancePatches(const RecompileRequest& request) {
    if (!request.graphics || !request.graphics->tessellation || request.graphics->draw.instanceCount <= 1u || request.graphics->tessellation->breakAtInstance) return 0u;
    return request.graphics->draw.indexCount / request.graphics->tessellation->inputControlPoints;
}

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
    Image1DArray,
    Image2DMultisample,
    Image2DMultisampleArray
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
    ImageTableMap,
    ImageStorageTable
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
    std::vector<bool> samplerIntegerBorder;
    std::vector<bool> samplerCoordinatesIgnored;
    std::vector<bool> imageUnnormalized;
    std::vector<bool> imageUnnormalizedBaseLevel;
    std::vector<std::uint32_t> imageSamplers;
    std::vector<bool> bufferRead;
    std::vector<std::uint32_t> bufferExtent;
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

struct BarycentricEmulationLayout {
    static constexpr std::uint32_t NoLocation = 0xffffffffu;
    std::uint32_t smoothLocation = NoLocation;
    std::uint32_t linearLocation = NoLocation;
    std::uint32_t positionsLocation = NoLocation;
    std::uint32_t reciprocalWLocation = NoLocation;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> perVertexLocations;
};

[[nodiscard]] BarycentricEmulationLayout LayoutBarycentricEmulation(std::span<const FragmentParameter> parameters, const BarycentricEmulation& emulation, bool meshPositions = false);

struct EmulatedPerVertexParameter {
    std::uint32_t sourceLocation;
    std::uint32_t location;
};

struct EmulatedInterface {
    std::vector<std::uint32_t> passed;
    std::vector<bool> passedFlat;
    std::vector<EmulatedPerVertexParameter> perVertex;
    BarycentricEmulationLayout layout;
    std::uint32_t barycentrics = 0;
    std::uint32_t sources = 0;
};

[[nodiscard]] EmulatedInterface DescribeEmulatedInterface(std::span<const FragmentParameter> parameters, const BarycentricEmulation& emulation, bool meshPositions = false);

// A 128-bit hash of a module's words (SharedSpirv::ContentHash), zero for an empty module. The
// mixing is pure integer arithmetic over the words in order, so the value is the same on every run,
// platform and compiler: a pipeline journal names modules by it across runs.
struct SpirvContentHash {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    bool operator==(const SpirvContentHash&) const = default;
};

inline std::uint64_t HashSpirvWords(std::span<const std::uint32_t> words, std::uint64_t seed) {
    constexpr std::uint64_t multiplier = 0x9e3779b97f4a7c15ull;
    std::uint64_t hash = seed ^ (static_cast<std::uint64_t>(words.size()) * multiplier);
    const auto mix = [&](std::uint64_t chunk) {
        chunk *= 0xff51afd7ed558ccdull;
        chunk ^= chunk >> 32u;
        hash = (hash ^ chunk) * multiplier;
        hash = (hash << 27u) | (hash >> 37u);
    };
    std::size_t index = 0;
    for (; index + 2 <= words.size(); index += 2) mix(words[index] | (static_cast<std::uint64_t>(words[index + 1]) << 32u));
    if (index < words.size()) mix(words[index] ^ 0x8000000000000000ull);
    hash ^= hash >> 33u;
    hash *= 0xff51afd7ed558ccdull;
    hash ^= hash >> 33u;
    hash *= 0xc4ceb9fe1a85ec53ull;
    hash ^= hash >> 33u;
    return hash;
}

// Compiled SPIR-V shared between a cached variant and every result materialized from it: results
// are copied per dispatch and draw, so the words are reference counted and only duplicated when a
// holder writes to them (tests and tools patch modules in place). Reads look like a vector.
// The non-const data(), operator[], begin() and end() count as writes: on a shared holder (every
// cached result) they clone the module, so a consumer that only reads takes the result by const
// reference, or the per-dispatch copy this class removes comes back without a compiler hint.
// ContentHash is computed once per shared holder, on first use, and dropped by a write.
class SharedSpirv {
public:
    SharedSpirv() = default;
    SharedSpirv(std::vector<std::uint32_t> words) : holder(std::make_shared<Holder>(std::move(words), nullptr, std::span<const std::uint32_t>{})) {}
    SharedSpirv(std::shared_ptr<const void> owner, std::span<const std::uint32_t> view) : holder(std::make_shared<Holder>(std::vector<std::uint32_t>{}, std::move(owner), view)) {}
    SharedSpirv& operator=(std::vector<std::uint32_t> other) {
        holder = std::make_shared<Holder>(std::move(other), nullptr, std::span<const std::uint32_t>{});
        return *this;
    }

    [[nodiscard]] std::span<const std::uint32_t> Words() const { return holder ? holder->Words() : std::span<const std::uint32_t>{}; }
    [[nodiscard]] SpirvContentHash ContentHash() const {
        if (holder == nullptr) return {};
        if (holder->hashed.load(std::memory_order_acquire)) return {holder->hashLow.load(std::memory_order_relaxed), holder->hashHigh.load(std::memory_order_relaxed)};
        const auto words = holder->Words();
        if (words.empty()) return {};
        const SpirvContentHash hash{HashSpirvWords(words, 0x5eed5a1d00000001ull), HashSpirvWords(words, 0x5eed5a1d00000002ull)};
        holder->hashLow.store(hash.low, std::memory_order_relaxed);
        holder->hashHigh.store(hash.high, std::memory_order_relaxed);
        holder->hashed.store(true, std::memory_order_release);
        return hash;
    }
    [[nodiscard]] std::vector<std::uint32_t> Copy() const { return {Words().begin(), Words().end()}; }
    [[nodiscard]] std::size_t size() const { return Words().size(); }
    [[nodiscard]] bool empty() const { return Words().empty(); }
    [[nodiscard]] const std::uint32_t* data() const { return Words().data(); }
    [[nodiscard]] std::uint32_t* data() { return Mutable().data(); }
    [[nodiscard]] const std::uint32_t& operator[](std::size_t index) const { return Words()[index]; }
    [[nodiscard]] std::uint32_t& operator[](std::size_t index) { return Mutable()[index]; }
    [[nodiscard]] std::span<const std::uint32_t>::iterator begin() const { return Words().begin(); }
    [[nodiscard]] std::span<const std::uint32_t>::iterator end() const { return Words().end(); }
    [[nodiscard]] std::vector<std::uint32_t>::iterator begin() { return Mutable().begin(); }
    [[nodiscard]] std::vector<std::uint32_t>::iterator end() { return Mutable().end(); }
    void resize(std::size_t count) { Mutable().resize(count); }
    std::vector<std::uint32_t>::iterator insert(std::vector<std::uint32_t>::const_iterator where, std::initializer_list<std::uint32_t> values) { return Mutable().insert(where, values); }
    friend bool operator==(const SharedSpirv& left, const SharedSpirv& right) { return std::ranges::equal(left.Words(), right.Words()); }

private:
    struct Holder {
        Holder(std::vector<std::uint32_t> words, std::shared_ptr<const void> owner, std::span<const std::uint32_t> view) : words(std::move(words)), owner(std::move(owner)), view(view) {}
        [[nodiscard]] std::span<const std::uint32_t> Words() const { return owner ? view : std::span<const std::uint32_t>(words); }
        std::vector<std::uint32_t> words;
        std::shared_ptr<const void> owner;
        std::span<const std::uint32_t> view;
        mutable std::atomic<std::uint64_t> hashLow{0};
        mutable std::atomic<std::uint64_t> hashHigh{0};
        mutable std::atomic<bool> hashed{false};
    };

    std::vector<std::uint32_t>& Mutable() {
        if (holder == nullptr || holder.use_count() != 1 || holder->owner != nullptr) {
            holder = std::make_shared<Holder>(Copy(), nullptr, std::span<const std::uint32_t>{});
        } else {
            holder->hashed.store(false, std::memory_order_relaxed);
        }
        return holder->words;
    }

    std::shared_ptr<Holder> holder;
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
    bool registrationModule = false;
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
    bool fullSubgroups = false;
    std::vector<std::uint32_t> parameterExports;
    std::vector<FragmentParameter> fragmentParameters;
    BarycentricEmulation barycentricEmulation;
    std::optional<MeshBarycentricLayout> meshBarycentrics;
    std::vector<std::uint32_t> stencilReferences;
    bool stencilReplaySafe = false;
    std::uint32_t stencilReferenceDword = 0xffffffffu;
    std::uint64_t variantId = 0;
};

struct ShaderInvocation {
    std::vector<PipelineSpecializationConstant> specialization;
    std::uint64_t specializationId = 0;
    std::vector<DescriptorBinding> bindings;
    std::vector<std::byte> pushConstants;
    bool readsDepthRemapPush = false;
    std::vector<VertexAttribute> vertexAttributes;
    std::uint64_t userDataRegisterReads = 0;
    std::uint32_t poisonedSrtReads = 0;
    std::vector<ImageTableEntryPoison> imageTablePoison;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> imageTableRanges;
    std::vector<std::vector<std::pair<std::uint64_t, std::uint64_t>>> imageTableReadRanges;
    std::uint64_t imageTableShader = 0;
    std::uint32_t imageTableFaults = 0;
};

// What a Vulkan pipeline built from a result depends on besides the fixed-function state and the
// descriptor set layouts: the module words, the specialization constants handed to the driver, the
// subgroup width the module was laid out for, the workgroup memory slice and the bindings' shape.
// Two results with equal identities build equal pipelines, in this run or another: unlike the
// process-lifetime variant ids, the identity is the same on every run, so the pipeline stores key on
// it and a pipeline journal can name a stage across runs. Empty for a result without a module.
struct PipelineIdentity {
    std::uint64_t low = 0;
    std::uint64_t high = 0;
    [[nodiscard]] bool Empty() const { return low == 0 && high == 0; }
    bool operator==(const PipelineIdentity&) const = default;
    auto operator<=>(const PipelineIdentity&) const = default;
};

struct ResultKeyMemo {
    mutable AtomicSharedPtr<const std::vector<std::uint32_t>> words;
    ResultKeyMemo() = default;
    ResultKeyMemo(const ResultKeyMemo&) {}
    ResultKeyMemo& operator=(const ResultKeyMemo&) {
        words.store(nullptr, std::memory_order_relaxed);
        return *this;
    }
};

struct RecompileResult : CompiledShaderArtifact, ShaderInvocation {
    ResultKeyMemo drawKeyMemo;
    bool cacheHit = false;
    std::uint32_t workgroupMemoryDwords = 0;
    std::vector<std::uint32_t> shaderDataUserWords;
    [[nodiscard]] std::uint64_t PipelineVariantId() const { return specializationId != 0 ? specializationId : variantId; }
    [[nodiscard]] PipelineIdentity Identity() const {
        const auto hash = spirv.ContentHash();
        if (hash == SpirvContentHash{}) return {};
        PipelineIdentity identity{hash.low, hash.high};
        const auto mix = [&](std::uint64_t value) {
            identity.low = (identity.low ^ value) * 0x9e3779b97f4a7c15ull;
            identity.low ^= identity.low >> 29u;
            identity.high = (identity.high ^ (value + 0x7f4a7c159e3779b9ull)) * 0xc2b2ae3d27d4eb4full;
            identity.high ^= identity.high >> 31u;
        };
        mix(specialization.size());
        for (const auto& constant : specialization) mix(constant.id | (static_cast<std::uint64_t>(constant.value) << 32u));
        mix(hostSubgroupSize | (fullSubgroups ? 0x10000u : 0u) | (static_cast<std::uint64_t>(workgroupMemoryDwords) << 32u));
        mix(bindings.size());
        for (const auto& binding : bindings) mix(binding.descriptorSet | (static_cast<std::uint64_t>(binding.binding) << 16u) | (static_cast<std::uint64_t>(binding.kind) << 40u) | (static_cast<std::uint64_t>(binding.count) << 48u));
        if (identity.Empty()) identity.low = 1;
        return identity;
    }
};

[[nodiscard]] std::optional<MeshBarycentricLayout> MeshBarycentricLayoutOf(const RecompileResult& fragment);

struct MeshOutputUse {
    std::uint32_t flatParameters = 0;
    std::uint32_t perVertexParameters = 0;
    std::uint32_t clipVaryings = 0;
    std::uint32_t clipDistances = 0;
    std::uint32_t cullDistances = 0;
    bool layer = false;
};

struct MeshOutputBudget {
    std::uint32_t vertexLocations = 0;
    std::uint32_t primitiveLocations = 0;
    std::uint64_t components = 0;
    std::uint64_t bytes = 0;
};

[[nodiscard]] MeshOutputBudget MeshBarycentricOutputBudget(const MeshConfiguration& mesh, const MeshTargetLimits& limits, const MeshOutputUse& use);

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
    RecompileResult lastVertexControl;
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

struct BarycentricTessellationShaders {
    RecompileResult control;
    RecompileResult evaluation;
};

[[nodiscard]] BarycentricTessellationShaders BuildBarycentricTessellationShaders(const RecompileResult& vertex, const RecompileResult& fragment, const SpirvTarget& target, std::uint32_t maxFragmentInputComponents, std::uint32_t provokingVertex);

}

#endif

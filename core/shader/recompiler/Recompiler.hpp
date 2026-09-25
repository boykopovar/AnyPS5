#ifndef CORE_SHADER_RECOMPILIER_INCLUDE_SHADER_RECOMPILIER_RECOMPILER_HPP
#define CORE_SHADER_RECOMPILIER_INCLUDE_SHADER_RECOMPILIER_RECOMPILER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
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
};

struct ShaderPixelStageInfo {
    std::uint32_t interpolatorCount;
    std::array<std::uint32_t, 32> interpolatorSettings;
    bool wave32;
    std::uint32_t perspectiveCenterVgpr;
    bool hasPerspectiveCenterVgpr;
    bool posX;
    bool posY;
    bool posZ;
    bool posW;
    bool frontFace;
    bool ancillary;
    bool sampleShading;
    bool noPerspective;
    bool pixelKillEnable;
    bool depthExportEnable;
    bool sampleMaskExportEnable;
    bool earlyZ;
    bool executeOnNoop;
    std::array<std::uint8_t, 8> targetOutputMode;
    std::array<std::uint8_t, 8> targetExportMapping;
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

struct GuestContext {
    std::uint32_t waveSize;
    std::uint32_t userDataBaseRegister;
    std::span<const std::uint32_t> userData;
    std::optional<ShaderComputeStageInfo> compute;
    std::optional<ShaderPixelStageInfo> pixel;
    std::optional<ShaderVertexStageInfo> vertex;
    std::span<const MemoryRegion> memory;
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
};

struct TessellationConfiguration {
    std::uint32_t inputControlPoints;
    std::uint32_t outputControlPoints;
    std::uint32_t domain;
    std::uint32_t partitioning;
    std::uint32_t outputTopology;
};

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
    Image3D
};

enum class DescriptorRole {
    GuestBuffers,
    GuestImages,
    GuestSamplers,
    Gds,
    BdaPagetable,
    FaultBuffer,
    FlattenedSrt,
    ShaderData
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
};

struct VertexAttribute {
    std::uint32_t location;
    std::uint32_t components;
    ShaderVertexBufferResource resource;
    std::uint32_t fetchIndex;
};

struct FragmentParameter {
    std::uint32_t location;
    std::uint32_t sourceLocation;
    bool flat;
    bool perVertex;
};

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

struct RecompileResult {
    SharedSpirv spirv;
    std::vector<DescriptorBinding> bindings;
    std::vector<std::byte> pushConstants;
    std::uint32_t bdaAbiVersion = 0;
    std::vector<VertexAttribute> vertexAttributes;
    std::int32_t vertexOffsetSgpr = -1;
    std::int32_t instanceOffsetSgpr = -1;
    std::vector<std::uint32_t> parameterExports;
    std::vector<FragmentParameter> fragmentParameters;
    bool cacheHit = false;
    // Identifies the compiled variant the result came from: equal ids mean identical SPIR-V and
    // bindings, so drivers can reuse pipeline objects. Zero when unknown.
    std::uint64_t variantId = 0;
};

[[nodiscard]] RecompileResult Recompile(const RecompileRequest& request);

// The resource plan, snapshot and specialization a driver captured for the request (see
// CaptureResources in Optimization/ResourceProgram.hpp): this overload reuses them instead of
// materializing the request's memory regions again, and is otherwise Recompile(request).
struct ResourceCapture;
[[nodiscard]] RecompileResult Recompile(const RecompileRequest& request, const ResourceCapture& capture);

// Debug aid (see DebugProbe in Translation/TranslationContext.hpp): the APS5_PROBE register probe is
// only applied while a driver has it active, so it can be limited to one dispatch; the recompile
// cache keys on it.
void SetDebugProbeActive(bool active);
[[nodiscard]] bool DebugProbeActive();

struct RectListShaders {
    RecompileResult control;
    RecompileResult evaluation;
};

[[nodiscard]] RectListShaders BuildRectListShaders(const RecompileResult& vertex, const RecompileResult& fragment, const SpirvTarget& target);

}

#endif

#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "ShaderCacheDirectory.hpp"
#include "ShaderDiskCache.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <system_error>
#include <thread>
#include <type_traits>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace AgcDriver::Graphics {

namespace {

static_assert(sizeof(VkPipelineColorBlendAttachmentState) == 32, "VkPipelineColorBlendAttachmentState changed: update the recipe codec");
static_assert(sizeof(VkStencilOpState) == 28, "VkStencilOpState changed: update the recipe codec");
static_assert(sizeof(VkVertexInputBindingDescription) == 12, "VkVertexInputBindingDescription changed: update the recipe codec");
static_assert(sizeof(VkVertexInputAttributeDescription) == 16, "VkVertexInputAttributeDescription changed: update the recipe codec");
#if defined(__linux__) && defined(__x86_64__) && defined(__GLIBCXX__)
static_assert(sizeof(State) == 832, "State changed: update CanonicalPipelineState and the recipe codec");
static_assert(sizeof(ShaderStages) == 112, "ShaderStages changed: update the recipe codec");
static_assert(sizeof(ShaderRecompiler::MeshConfiguration) == 40, "MeshConfiguration changed: update the recipe codec");
static_assert(sizeof(ShaderRecompiler::TessellationConfiguration) == 40, "TessellationConfiguration changed: update the recipe codec");
#endif

constexpr std::uint32_t FileMagic = 0x4c4a5041u;
// Bumped when the recipe codec, CanonicalPipelineState or the store key change in a way the
// layout guards above do not catch (a field renumbered or reinterpreted at the same size).
constexpr std::uint32_t FileFormat = 9u;
constexpr std::uint32_t ModuleRecord = 1u;
constexpr std::uint32_t GraphicsRecord = 2u;
constexpr std::uint32_t ComputeRecord = 3u;
constexpr std::uint64_t RecordAlignment = 8u;

struct FileHeader {
    std::uint32_t magic;
    std::uint32_t format;
    std::uint32_t runtimeAbi;
    std::uint32_t reserved;
    std::uint64_t sourceVersion;
    std::uint64_t device;
};
static_assert(sizeof(FileHeader) == 32 && std::is_trivially_copyable_v<FileHeader>);

struct RecordHeader {
    std::uint32_t kind;
    std::uint32_t reserved;
    std::uint64_t payloadBytes;
    std::uint64_t payloadHash;
};
static_assert(sizeof(RecordHeader) == 24 && std::is_trivially_copyable_v<RecordHeader>);

class Writer {
public:
    explicit Writer(std::vector<std::byte>& out) : out(out) {}

    template<typename TValue>
    void Raw(const TValue& value) requires std::is_trivially_copyable_v<TValue> {
        const auto offset = out.size();
        out.resize(offset + sizeof(value));
        std::memcpy(out.data() + offset, &value, sizeof(value));
    }

    void Bool(bool value) { Raw<std::uint8_t>(value ? 1u : 0u); }
    void U32(std::uint32_t value) { Raw(value); }
    void U64(std::uint64_t value) { Raw(value); }

    template<typename TValue>
    void Raws(std::span<const TValue> values) requires std::is_trivially_copyable_v<TValue> {
        U64(values.size());
        for (const auto& value : values) Raw(value);
    }

private:
    std::vector<std::byte>& out;
};

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes) : bytes(bytes) {}

    [[nodiscard]] bool Ok() const { return ok; }
    [[nodiscard]] bool Done() const { return ok && position == bytes.size(); }
    void Fail() { ok = false; }

    template<typename TValue>
    [[nodiscard]] TValue Raw() requires std::is_trivially_copyable_v<TValue> {
        TValue value{};
        if (!ok || bytes.size() - position < sizeof(value)) {
            ok = false;
            return value;
        }
        std::memcpy(&value, bytes.data() + position, sizeof(value));
        position += sizeof(value);
        return value;
    }

    [[nodiscard]] bool Bool() {
        const auto value = Raw<std::uint8_t>();
        if (value > 1u) ok = false;
        return ok && value == 1u;
    }

    [[nodiscard]] std::uint32_t U32() { return Raw<std::uint32_t>(); }
    [[nodiscard]] std::uint64_t U64() { return Raw<std::uint64_t>(); }

    template<typename TValue>
    void Raws(std::vector<TValue>& values, std::size_t limit) requires std::is_trivially_copyable_v<TValue> {
        const auto count = U64();
        if (!ok || count > limit || count > (bytes.size() - position) / sizeof(TValue)) {
            ok = false;
            values.clear();
            return;
        }
        values.resize(static_cast<std::size_t>(count));
        for (auto& value : values) value = Raw<TValue>();
    }

private:
    std::span<const std::byte> bytes;
    std::size_t position = 0;
    bool ok = true;
};

template<typename TValue>
void encodeOptional(Writer& writer, const std::optional<TValue>& value, void (*encode)(Writer&, const TValue&)) {
    writer.Bool(value.has_value());
    if (value) encode(writer, *value);
}

void encodeMesh(Writer& writer, const ShaderRecompiler::MeshConfiguration& mesh) {
    for (const auto value : {mesh.inputPrimitive, mesh.primitivesPerGroup, mesh.verticesPerGroup, mesh.maxVertices, mesh.maxPrimitives, mesh.threadsPerGroup, mesh.ldsSizeDwords, mesh.provokingVertex, mesh.esgsItemSize, mesh.outputPrimitiveSize}) writer.U32(value);
}

void decodeMesh(Reader& reader, ShaderRecompiler::MeshConfiguration& mesh) {
    for (auto* value : {&mesh.inputPrimitive, &mesh.primitivesPerGroup, &mesh.verticesPerGroup, &mesh.maxVertices, &mesh.maxPrimitives, &mesh.threadsPerGroup, &mesh.ldsSizeDwords, &mesh.provokingVertex, &mesh.esgsItemSize, &mesh.outputPrimitiveSize}) *value = reader.U32();
}

void encodeTessellation(Writer& writer, const ShaderRecompiler::TessellationConfiguration& tessellation) {
    for (const auto value : {tessellation.inputControlPoints, tessellation.outputControlPoints, tessellation.domain, tessellation.partitioning, tessellation.outputTopology, tessellation.patchesPerGroup, tessellation.primitiveGroupSize}) writer.U32(value);
    writer.Bool(tessellation.breakAtInstance);
    writer.U32(tessellation.instancePatches);
    writer.Bool(tessellation.drawParameters);
}

void decodeTessellation(Reader& reader, ShaderRecompiler::TessellationConfiguration& tessellation) {
    for (auto* value : {&tessellation.inputControlPoints, &tessellation.outputControlPoints, &tessellation.domain, &tessellation.partitioning, &tessellation.outputTopology, &tessellation.patchesPerGroup, &tessellation.primitiveGroupSize}) *value = reader.U32();
    tessellation.breakAtInstance = reader.Bool();
    tessellation.instancePatches = reader.U32();
    tessellation.drawParameters = reader.Bool();
}

void encodeState(Writer& writer, const State& state) {
    writer.U32(static_cast<std::uint32_t>(state.stages.path));
    writer.U32(state.stages.vertexWaveSize);
    writer.U32(state.stages.fragmentWaveSize);
    writer.U32(state.meshLayerBase);
    writer.U32(state.meshLayerCount);
    encodeOptional(writer, state.stages.mesh, encodeMesh);
    encodeOptional(writer, state.stages.tessellation, encodeTessellation);
    writer.Bool(state.depth.has_value());
    if (state.depth) {
        writer.U32(static_cast<std::uint32_t>(state.depth->format));
        writer.U32(state.depth->samples);
    }
    writer.Bool(state.depthTest);
    writer.Bool(state.depthWrite);
    writer.U32(static_cast<std::uint32_t>(state.depthCompare));
    writer.Bool(state.depthBoundsTest);
    writer.Bool(state.depthBias);
    writer.Raw(state.depthBiasClamp);
    writer.Bool(state.depthBiasPerFace);
    writer.Bool(state.stencilTest);
    writer.Raw(state.stencilFront);
    writer.Raw(state.stencilBack);
    writer.Bool(state.stencilReplayReference.has_value());
    writer.U64(state.colors.size());
    for (const auto& color : state.colors) {
        writer.U32(static_cast<std::uint32_t>(color.format));
        writer.U32(color.exportIndex);
        writer.Raw(color.componentMapping);
        writer.Raw(color.partialWriteMask);
    }
    writer.Raws(std::span<const VkPipelineColorBlendAttachmentState>(state.blends));
    for (const auto value : state.blendConstants) writer.Raw(value);
    writer.Bool(state.hasColorTarget);
    writer.Bool(state.rectList);
    writer.Bool(state.dualSourceBlend);
    writer.U32(state.reversedBlend);
    writer.U32(static_cast<std::uint32_t>(state.topology));
    writer.Bool(state.primitiveRestart);
    writer.Bool(state.negativeOneToOne);
    writer.Bool(state.depthClamp);
    writer.U32(static_cast<std::uint32_t>(state.conservativeRasterization));
    writer.U32(state.cullMode);
    writer.U32(static_cast<std::uint32_t>(state.polygonMode));
    writer.U32(static_cast<std::uint32_t>(state.frontFace));
    writer.U32(static_cast<std::uint32_t>(state.provokingVertexMode));
    writer.U32(state.samples);
    for (const auto value : state.sampleLocations) writer.Raw(value);
    writer.Bool(state.customSampleLocations);
    writer.Bool(state.sampleLocationGrid);
    writer.Bool(state.perSampleShading);
    writer.Bool(state.mergedCoverage);
    writer.Bool(state.truncatedCoverage);
    for (const auto mask : state.quadSampleMask) writer.Raw(mask);
    writer.U32(state.depthRemap.mode);
    writer.Bool(state.vertexPoints);
    writer.Raw(state.pointSize);
    writer.Raw(state.pointSizeMin);
    writer.Raw(state.pointSizeMax);
}

void decodeState(Reader& reader, State& state) {
    state = State{};
    state.stages.path = static_cast<ShaderPath>(reader.U32());
    state.stages.vertexWaveSize = reader.U32();
    state.stages.fragmentWaveSize = reader.U32();
    state.meshLayerBase = reader.U32();
    state.meshLayerCount = reader.U32();
    if (reader.Bool()) decodeMesh(reader, state.stages.mesh.emplace());
    if (reader.Bool()) decodeTessellation(reader, state.stages.tessellation.emplace());
    if (reader.Bool()) {
        auto& depth = state.depth.emplace();
        depth.format = static_cast<VkFormat>(reader.U32());
        depth.samples = reader.U32();
    }
    state.depthTest = reader.Bool();
    state.depthWrite = reader.Bool();
    state.depthCompare = static_cast<VkCompareOp>(reader.U32());
    state.depthBoundsTest = reader.Bool();
    state.depthBias = reader.Bool();
    state.depthBiasClamp = reader.Raw<float>();
    state.depthBiasPerFace = reader.Bool();
    state.stencilTest = reader.Bool();
    state.stencilFront = reader.Raw<VkStencilOpState>();
    state.stencilBack = reader.Raw<VkStencilOpState>();
    if (reader.Bool()) state.stencilReplayReference = 0u;
    const auto colors = reader.U64();
    if (!reader.Ok() || colors > 8u) {
        reader.Fail();
        state = State{};
        return;
    }
    for (std::uint64_t index = 0; index < colors; ++index) {
        ColorTarget color{};
        color.format = static_cast<VkFormat>(reader.U32());
        color.exportIndex = reader.U32();
        color.componentMapping = reader.Raw<std::uint8_t>();
        color.partialWriteMask = reader.Raw<std::uint16_t>();
        state.colors.push_back(color);
    }
    reader.Raws(state.blends, 8u);
    for (auto& value : state.blendConstants) value = reader.Raw<float>();
    state.hasColorTarget = reader.Bool();
    state.rectList = reader.Bool();
    state.dualSourceBlend = reader.Bool();
    state.reversedBlend = reader.U32();
    state.topology = static_cast<VkPrimitiveTopology>(reader.U32());
    state.primitiveRestart = reader.Bool();
    state.negativeOneToOne = reader.Bool();
    state.depthClamp = reader.Bool();
    state.conservativeRasterization = static_cast<VkConservativeRasterizationModeEXT>(reader.U32());
    state.cullMode = reader.U32();
    state.polygonMode = static_cast<VkPolygonMode>(reader.U32());
    state.frontFace = static_cast<VkFrontFace>(reader.U32());
    state.provokingVertexMode = static_cast<VkProvokingVertexModeEXT>(reader.U32());
    state.samples = reader.U32();
    for (auto& value : state.sampleLocations) value = reader.Raw<std::uint8_t>();
    state.customSampleLocations = reader.Bool();
    state.sampleLocationGrid = reader.Bool();
    state.perSampleShading = reader.Bool();
    state.mergedCoverage = reader.Bool();
    state.truncatedCoverage = reader.Bool();
    for (auto& mask : state.quadSampleMask) mask = reader.Raw<std::uint16_t>();
    state.depthRemap.mode = reader.U32();
    state.vertexPoints = reader.Bool();
    state.pointSize = reader.Raw<float>();
    state.pointSizeMin = reader.Raw<float>();
    state.pointSizeMax = reader.Raw<float>();
}

void encodeRecipe(Writer& writer, const GraphicsPipelineRecipe& recipe) {
    writer.U32(static_cast<std::uint32_t>(recipe.attachmentLayout));
    writer.U64(recipe.stages.size());
    for (const auto& stage : recipe.stages) {
        writer.U32(static_cast<std::uint32_t>(stage.stage));
        writer.U64(stage.identity.low);
        writer.U64(stage.identity.high);
        writer.U32(stage.pushConstantOffset);
        writer.Bool(stage.generated);
    }
    writer.Raws(std::span<const VkVertexInputBindingDescription>(recipe.vertexInput.bindings));
    writer.Raws(std::span<const VkVertexInputAttributeDescription>(recipe.vertexInput.attributes));
    writer.Raws(std::span<const std::uint32_t>(recipe.mainLayoutKey));
    writer.Raws(std::span<const std::uint32_t>(recipe.fragmentLayoutKey));
    writer.Bool(recipe.splitSets);
    encodeState(writer, recipe.state);
}

bool decodeRecipe(Reader& reader, GraphicsPipelineRecipe& recipe) {
    recipe = GraphicsPipelineRecipe{};
    recipe.attachmentLayout = static_cast<VkImageLayout>(reader.U32());
    const auto stages = reader.U64();
    if (!reader.Ok() || stages > 8u) return false;
    for (std::uint64_t index = 0; index < stages; ++index) {
        GraphicsPipelineRecipe::Stage stage{};
        stage.stage = static_cast<ShaderRecompiler::ShaderStage>(reader.U32());
        stage.identity.low = reader.U64();
        stage.identity.high = reader.U64();
        stage.pushConstantOffset = reader.U32();
        stage.generated = reader.Bool();
        recipe.stages.push_back(stage);
    }
    reader.Raws(recipe.vertexInput.bindings, 64u);
    reader.Raws(recipe.vertexInput.attributes, 64u);
    reader.Raws(recipe.mainLayoutKey, 4096u);
    reader.Raws(recipe.fragmentLayoutKey, 4096u);
    recipe.splitSets = reader.Bool();
    decodeState(reader, recipe.state);
    return reader.Done();
}

std::uint64_t padded(std::uint64_t bytes) {
    return (bytes + RecordAlignment - 1u) / RecordAlignment * RecordAlignment;
}

bool profiling() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

thread_local bool replayingHere = false;

std::uint64_t configured(const char* name, std::uint64_t fallback) {
    const char* text = std::getenv(name);
    if (text == nullptr || *text == '\0') return fallback;
    char* end = nullptr;
    const auto value = std::strtoull(text, &end, 10);
    if (end == text || *end != '\0') throw std::runtime_error(std::string("AGC graphics: ") + name + " must be a decimal integer");
    return value;
}

// Held while the journal is read, cut or appended: another process sharing the directory (a
// second run of the same install) takes the same lock file, so its appends and this run's never
// interleave and neither cuts a record the other is writing. Taken before the journal's mutex,
// never while holding it.
class FileLock {
public:
    explicit FileLock(const std::filesystem::path& path) {
#ifdef _WIN32
        handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return;
        OVERLAPPED overlapped{};
        if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped)) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
#else
        descriptor = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        if (descriptor >= 0 && ::flock(descriptor, LOCK_EX) != 0) {
            ::close(descriptor);
            descriptor = -1;
        }
#endif
    }

    ~FileLock() {
#ifdef _WIN32
        if (handle == INVALID_HANDLE_VALUE) return;
        OVERLAPPED overlapped{};
        UnlockFileEx(handle, 0, 1, 0, &overlapped);
        CloseHandle(handle);
#else
        if (descriptor < 0) return;
        ::flock(descriptor, LOCK_UN);
        ::close(descriptor);
#endif
    }

    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

#ifdef _WIN32
    bool Held() const { return handle != INVALID_HANDLE_VALUE; }
#else
    bool Held() const { return descriptor >= 0; }
#endif

private:
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int descriptor = -1;
#endif
};

// The end of the last whole record in [from, size) of the file: records another process appended
// after this one loaded the file are kept, the torn tail of a record a crashed run left is cut.
std::uint64_t validTail(const std::filesystem::path& path, std::uint64_t from, std::uint64_t size);

struct LoadedModule {
    ShaderRecompiler::ShaderStage stage;
    std::shared_ptr<const ShaderRecompiler::RecompileResult> result;
};

struct LoadedEntry {
    std::uint32_t kind;
    GraphicsPipelineRecipe graphics;
    ShaderRecompiler::PipelineIdentity compute;
    bool pushConstants = false;
};

struct LoadedJournal {
    std::map<ShaderRecompiler::PipelineIdentity, LoadedModule> modules;
    std::vector<LoadedEntry> entries;
    std::set<ShaderRecompiler::PipelineIdentity> indexModules;
    std::set<std::uint64_t> indexRecords;
    std::uint64_t validBytes = 0;
    std::uint64_t bytes = 0;
    std::uint64_t dropped = 0;
    bool headerValid = false;
};

class Journal {
public:
    static Journal& Instance() {
        static auto* journal = Create();
        return *journal;
    }

    static Journal* Existing() { return created().load(std::memory_order_acquire); }

    [[nodiscard]] bool Enabled() const { return !path.empty() && !mismatched.load(std::memory_order_relaxed); }
    [[nodiscard]] const std::filesystem::path& Path() const { return path; }

    // The journal belongs to one device per process: the first device to replay binds its
    // fingerprint, the file header names it and a file written by another device starts over. A
    // later device with another fingerprint (the window's device is the headless device's GPU, so a
    // run does not see one) turns the journal off for the rest of the run.
    bool Bind(std::uint64_t fingerprint) {
        std::lock_guard lock(mutex);
        if (!bound.load(std::memory_order_relaxed)) {
            device.store(fingerprint, std::memory_order_relaxed);
            bound.store(true, std::memory_order_release);
            return true;
        }
        if (device.load(std::memory_order_relaxed) == fingerprint) return true;
        if (!mismatched.exchange(true)) std::fprintf(stderr, "[pipeline-journal] %s belongs to another device; the journal is off for this run\n", path.string().c_str());
        return false;
    }

    // Reads the whole file (modules keep views into the buffer, which lives as long as they do),
    // fills the dedupe index, and remembers how many bytes parsed so a torn tail is cut before the
    // next append.
    LoadedJournal Load() {
        LoadedJournal loaded;
        if (!Enabled() || !bound.load(std::memory_order_acquire)) return loaded;
        const auto started = std::chrono::steady_clock::now();
        auto file = std::make_shared<std::vector<std::byte>>();
        bool read = false;
        {
            FileLock fileLock(lockPath);
            read = ShaderRecompiler::ReadWholeFile(path, *file);
        }
        if (read) parse(*file, file, loaded, true);
        const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        {
            std::lock_guard lock(mutex);
            std::error_code error;
            if (!indexed && (read || !std::filesystem::exists(path, error))) {
                indexed = true;
                validBytes = loaded.validBytes;
                headerValid = loaded.headerValid;
                writtenModules = loaded.indexModules;
                writtenRecords = loaded.indexRecords;
            }
            counters.loadedRecipes.store(loaded.entries.size(), std::memory_order_relaxed);
            counters.loadedModules.store(loaded.modules.size(), std::memory_order_relaxed);
            counters.loadedBytes.store(loaded.bytes, std::memory_order_relaxed);
            counters.droppedRecords.store(loaded.dropped, std::memory_order_relaxed);
            loadMs.store(ms, std::memory_order_relaxed);
        }
        if (loaded.bytes != 0 || loaded.dropped != 0) std::fprintf(stderr, "[pipeline-journal] loaded %zu pipelines and %zu modules (%.1f MiB) from %s in %.0f ms%s\n", loaded.entries.size(), loaded.modules.size(), static_cast<double>(loaded.bytes) / (1024.0 * 1024.0), path.string().c_str(), ms, !loaded.headerValid ? "; written by another build or device, starting over" : loaded.dropped != 0 ? "; the file's tail was unreadable and is dropped" : "");
        return loaded;
    }

    void RecordGraphics(const GraphicsPipelineRecipe& recipe, std::span<const CompiledShader> shaders) {
        if (!Enabled() || !bound.load(std::memory_order_acquire)) return;
        Pending pending;
        pending.kind = GraphicsRecord;
        for (std::size_t index = 0; index < shaders.size() && index < recipe.stages.size(); ++index) {
            Require(shaders[index].program != nullptr, "a recorded pipeline stage has no program");
            pending.modules.push_back({recipe.stages[index].identity, recipe.stages[index].stage, std::make_shared<const ShaderRecompiler::RecompileResult>(*shaders[index].program)});
        }
        Writer writer(pending.payload);
        encodeRecipe(writer, recipe);
        enqueue(std::move(pending));
    }

    void RecordCompute(const ShaderRecompiler::RecompileResult& shader, bool pushConstants) {
        if (!Enabled() || !bound.load(std::memory_order_acquire)) return;
        const auto identity = shader.Identity();
        if (identity.Empty()) return;
        Pending pending;
        pending.kind = ComputeRecord;
        pending.modules.push_back({identity, ShaderRecompiler::ShaderStage::Compute, std::make_shared<const ShaderRecompiler::RecompileResult>(shader)});
        Writer writer(pending.payload);
        writer.U64(identity.low);
        writer.U64(identity.high);
        writer.Bool(pushConstants);
        enqueue(std::move(pending));
    }

    void Flush() {
        std::unique_lock lock(mutex);
        idle.wait(lock, [&] { return queue.empty() && !writing; });
    }

    PipelineJournalCounters Totals() const {
        PipelineJournalCounters totals;
        totals.loadedRecipes = counters.loadedRecipes.load(std::memory_order_relaxed);
        totals.loadedModules = counters.loadedModules.load(std::memory_order_relaxed);
        totals.loadedBytes = counters.loadedBytes.load(std::memory_order_relaxed);
        totals.droppedRecords = counters.droppedRecords.load(std::memory_order_relaxed);
        totals.loadMs = loadMs.load(std::memory_order_relaxed);
        totals.recordedGraphics = counters.recordedGraphics.load(std::memory_order_relaxed);
        totals.recordedCompute = counters.recordedCompute.load(std::memory_order_relaxed);
        totals.recordedModules = counters.recordedModules.load(std::memory_order_relaxed);
        totals.writtenBytes = counters.writtenBytes.load(std::memory_order_relaxed);
        totals.writeFailures = counters.writeFailures.load(std::memory_order_relaxed);
        return totals;
    }

    struct Counters {
        std::atomic<std::uint64_t> loadedRecipes{0}, loadedModules{0}, loadedBytes{0}, droppedRecords{0};
        std::atomic<std::uint64_t> recordedGraphics{0}, recordedCompute{0}, recordedModules{0}, writtenBytes{0}, writeFailures{0};
    };

private:
    struct PendingModule {
        ShaderRecompiler::PipelineIdentity identity;
        ShaderRecompiler::ShaderStage stage;
        std::shared_ptr<const ShaderRecompiler::RecompileResult> result;
    };

    struct Pending {
        std::uint32_t kind = 0;
        std::vector<PendingModule> modules;
        std::vector<std::byte> payload;
    };

    static std::atomic<Journal*>& created() {
        static std::atomic<Journal*> pointer{nullptr};
        return pointer;
    }

    static Journal* Create() {
        auto* journal = new Journal();
        created().store(journal, std::memory_order_release);
        return journal;
    }

    Journal() {
        const char* off = std::getenv("APS5_NO_PIPELINE_JOURNAL");
        if ((off != nullptr && std::strcmp(off, "0") != 0) || !ShaderRecompiler::ShaderDiskCache::Enabled()) return;
        auto directory = ShaderRecompiler::ShaderDiskCache::EntryDirectory();
#ifdef _WIN32
        std::error_code error;
        const auto native = ShaderRecompiler::NativeCachePath_nid_no_patch(directory, error);
        if (error) throw std::filesystem::filesystem_error("cannot resolve the pipeline journal directory", directory, error);
        directory = native;
#endif
        path = directory / "pipelines.journal";
        lockPath = directory / "pipelines.journal.lock";
        maxBytes = configured("APS5_PIPELINE_JOURNAL_MAX_MIB", 1024u) << 20u;
        std::thread([this] { run(); }).detach();
    }

    // Parses the file: with `loaded` set every record is decoded, otherwise only the index (the
    // module identities and record hashes) is built. Stops at the first record that does not
    // parse; the bytes before it stay valid.
    void parse(const std::vector<std::byte>& file, const std::shared_ptr<const std::vector<std::byte>>& owner, LoadedJournal& loaded, bool decode) {
        std::uint64_t valid = 0;
        bool header = false;
        std::uint64_t dropped = 0;
        const auto bytes = std::span<const std::byte>(file);
        if (bytes.size() >= sizeof(FileHeader)) {
            FileHeader fileHeader;
            std::memcpy(&fileHeader, bytes.data(), sizeof(fileHeader));
            header = fileHeader.magic == FileMagic && fileHeader.format == FileFormat && fileHeader.runtimeAbi == ShaderRecompiler::RuntimeAbi::Version && fileHeader.sourceVersion == ShaderRecompiler::ShaderDiskCache::SourceVersion() && fileHeader.device == device.load(std::memory_order_relaxed);
        }
        if (header) {
            valid = padded(sizeof(FileHeader));
            std::uint64_t cursor = valid;
            while (cursor + sizeof(RecordHeader) <= bytes.size()) {
                RecordHeader record;
                std::memcpy(&record, bytes.data() + cursor, sizeof(record));
                const auto payloadOffset = cursor + sizeof(RecordHeader);
                if (record.payloadBytes > bytes.size() - payloadOffset) break;
                const auto end = padded(payloadOffset + record.payloadBytes);
                if (end > bytes.size()) break;
                const auto payload = bytes.subspan(static_cast<std::size_t>(payloadOffset), static_cast<std::size_t>(record.payloadBytes));
                if (ShaderRecompiler::HashBytes(payload) != record.payloadHash) break;
                if (record.kind == ModuleRecord) {
                    if (payload.size() < 20u) break;
                    ShaderRecompiler::PipelineIdentity identity;
                    std::memcpy(&identity.low, payload.data(), sizeof(identity.low));
                    std::memcpy(&identity.high, payload.data() + 8, sizeof(identity.high));
                    std::uint32_t stage = 0;
                    std::memcpy(&stage, payload.data() + 16, sizeof(stage));
                    bool decoded = !decode;
                    if (decode && !loaded.indexModules.contains(identity)) {
                        auto result = std::make_shared<ShaderRecompiler::RecompileResult>();
                        decoded = ShaderRecompiler::ShaderDiskCache::DecodeResult(payload.subspan(20), *result, owner) && result->Identity() == identity;
                        if (decoded) loaded.modules.emplace(identity, LoadedModule{static_cast<ShaderRecompiler::ShaderStage>(stage), std::move(result)});
                        else ++dropped;
                    }
                    if (decoded) loaded.indexModules.insert(identity);
                } else if (record.kind == GraphicsRecord || record.kind == ComputeRecord) {
                    if (decode && !loaded.indexRecords.contains(record.payloadHash)) {
                        LoadedEntry entry;
                        entry.kind = record.kind;
                        bool decoded = false;
                        if (record.kind == GraphicsRecord) {
                            Reader reader(payload);
                            decoded = decodeRecipe(reader, entry.graphics);
                        } else {
                            Reader reader(payload);
                            entry.compute.low = reader.U64();
                            entry.compute.high = reader.U64();
                            entry.pushConstants = reader.Bool();
                            decoded = reader.Done();
                        }
                        if (decoded) loaded.entries.push_back(std::move(entry));
                        else ++dropped;
                    }
                    loaded.indexRecords.insert(record.payloadHash);
                } else {
                    break;
                }
                valid = end;
                cursor = end;
            }
        }
        if (valid < bytes.size()) ++dropped;
        loaded.validBytes = valid;
        loaded.bytes = bytes.size();
        loaded.dropped = dropped;
        loaded.headerValid = header;
    }

    // The index the writer dedupes against, replaced from the file: what it holds and where it
    // ends. Under the file lock. False when the file exists but could not be read, so nothing is
    // written until it can be (the next record tries again) and the file is never started over for
    // a read failure.
    bool indexLocked() {
        std::vector<std::byte> file;
        LoadedJournal index;
        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        if (exists && !ShaderRecompiler::ReadWholeFile(path, file)) return false;
        if (exists) parse(file, nullptr, index, false);
        std::lock_guard lock(mutex);
        indexed = true;
        validBytes = index.validBytes;
        headerValid = index.headerValid;
        writtenModules = std::move(index.indexModules);
        writtenRecords = std::move(index.indexRecords);
        return true;
    }

    bool ensureIndexed() {
        {
            std::lock_guard lock(mutex);
            if (indexed) return true;
        }
        FileLock fileLock(lockPath);
        {
            std::lock_guard lock(mutex);
            if (indexed) return true;
        }
        return indexLocked();
    }

    void enqueue(Pending pending) {
        {
            std::lock_guard lock(mutex);
            queue.push_back(std::move(pending));
        }
        wake.notify_one();
    }

    void run() {
        for (;;) {
            Pending pending;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [&] { return !queue.empty(); });
                pending = std::move(queue.front());
                queue.pop_front();
                writing = true;
            }
            try {
                write(pending);
            } catch (const std::exception& error) {
                counters.writeFailures.fetch_add(1, std::memory_order_relaxed);
                static std::atomic<std::uint64_t> reported{0};
                if (reported.fetch_add(1) < 4) std::fprintf(stderr, "[pipeline-journal] cannot write %s: %s\n", path.string().c_str(), error.what());
            }
            {
                std::lock_guard lock(mutex);
                writing = false;
                if (queue.empty()) idle.notify_all();
            }
        }
    }

    // One record per module the file lacks, then the pipeline record; a record whose payload the
    // file already holds is skipped. The file is opened for appending per batch, after the torn
    // tail of a crashed run (bytes past the last parsed record) was cut.
    void write(const Pending& pending) {
        if (!ensureIndexed()) throw std::runtime_error("the journal could not be read");
        for (unsigned attempt = 0; attempt < 2; ++attempt) {
            if (append(pending)) return;
        }
        throw std::runtime_error("the journal keeps changing under this run");
    }

    // One attempt: false when the file was cut or replaced by someone else since it was indexed
    // (it is indexed again, and the batch is rebuilt against that index).
    bool append(const Pending& pending) {
        const auto recordHash = ShaderRecompiler::HashBytes(pending.payload);
        std::vector<std::byte> batch;
        std::vector<ShaderRecompiler::PipelineIdentity> newModules;
        {
            std::lock_guard lock(mutex);
            if (writtenRecords.contains(recordHash)) return true;
            for (const auto& module : pending.modules) {
                if (!writtenModules.contains(module.identity) && std::ranges::find(newModules, module.identity) == newModules.end()) newModules.push_back(module.identity);
            }
        }
        const auto appendRecord = [&](std::uint32_t kind, std::span<const std::byte> payload) {
            const RecordHeader header{kind, 0u, payload.size(), ShaderRecompiler::HashBytes(payload)};
            Writer writer(batch);
            writer.Raw(header);
            batch.insert(batch.end(), payload.begin(), payload.end());
            batch.resize(static_cast<std::size_t>(padded(batch.size())));
        };
        for (const auto& identity : newModules) {
            const auto module = std::ranges::find_if(pending.modules, [&](const PendingModule& candidate) { return candidate.identity == identity; });
            std::vector<std::byte> payload;
            Writer writer(payload);
            writer.U64(identity.low);
            writer.U64(identity.high);
            writer.U32(static_cast<std::uint32_t>(module->stage));
            ShaderRecompiler::ShaderDiskCache::EncodeResult(*module->result, payload);
            appendRecord(ModuleRecord, payload);
        }
        appendRecord(pending.kind, pending.payload);
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        FileLock fileLock(lockPath);
        if (!fileLock.Held()) {
            static std::atomic<bool> reported{false};
            if (!reported.exchange(true)) std::fprintf(stderr, "[pipeline-journal] cannot lock %s; pipelines are not recorded while it cannot be locked\n", lockPath.string().c_str());
            counters.writeFailures.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        bool fresh = false;
        {
            std::lock_guard lock(mutex);
            fresh = !headerValid;
            if (!fresh) {
                const auto size = std::filesystem::file_size(path, error);
                if (error) fresh = true;
                else if (size > validBytes) {
                    const auto tail = validTail(path, validBytes, size);
                    if (tail < size) std::filesystem::resize_file(path, tail, error);
                    if (error) fresh = true;
                    else validBytes = tail;
                }
            }
            if (!fresh && validBytes + batch.size() > maxBytes) {
                static std::atomic<bool> reported{false};
                if (!reported.exchange(true)) std::fprintf(stderr, "[pipeline-journal] %s reached APS5_PIPELINE_JOURNAL_MAX_MIB; later pipelines are not recorded\n", path.string().c_str());
                counters.writeFailures.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        }
        if (!fresh) {
            const auto size = std::filesystem::file_size(path, error);
            if (!error && size < validBytes) {
                if (!indexLocked()) throw std::runtime_error("the journal could not be read");
                return false;
            }
        }
        std::vector<std::byte> prefix;
        if (fresh) {
            const FileHeader header{FileMagic, FileFormat, ShaderRecompiler::RuntimeAbi::Version, 0u, ShaderRecompiler::ShaderDiskCache::SourceVersion(), device.load(std::memory_order_relaxed)};
            Writer writer(prefix);
            writer.Raw(header);
            prefix.resize(static_cast<std::size_t>(padded(prefix.size())));
        }
        std::ofstream file(path, fresh ? std::ios::binary | std::ios::trunc : std::ios::binary | std::ios::app);
        if (!file) throw std::runtime_error("cannot open the journal for writing");
        if (!prefix.empty()) file.write(reinterpret_cast<const char*>(prefix.data()), static_cast<std::streamsize>(prefix.size()));
        file.write(reinterpret_cast<const char*>(batch.data()), static_cast<std::streamsize>(batch.size()));
        file.flush();
        if (file.fail()) throw std::runtime_error("the journal write failed");
        std::lock_guard lock(mutex);
        headerValid = true;
        validBytes = (fresh ? padded(sizeof(FileHeader)) : validBytes) + batch.size();
        for (const auto& identity : newModules) writtenModules.insert(identity);
        writtenRecords.insert(recordHash);
        counters.recordedModules.fetch_add(newModules.size(), std::memory_order_relaxed);
        if (pending.kind == GraphicsRecord) counters.recordedGraphics.fetch_add(1, std::memory_order_relaxed);
        else counters.recordedCompute.fetch_add(1, std::memory_order_relaxed);
        counters.writtenBytes.fetch_add(prefix.size() + batch.size(), std::memory_order_relaxed);
        return true;
    }

    std::filesystem::path path;
    std::filesystem::path lockPath;
    std::uint64_t maxBytes = 0;
    std::atomic<std::uint64_t> device{0};
    std::atomic<bool> bound{false};
    std::atomic<bool> mismatched{false};
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable idle;
    std::deque<Pending> queue;
    bool writing = false;
    bool indexed = false;
    bool headerValid = false;
    std::uint64_t validBytes = 0;
    std::set<ShaderRecompiler::PipelineIdentity> writtenModules;
    std::set<std::uint64_t> writtenRecords;
    Counters counters;
    std::atomic<double> loadMs{0};
};

std::uint64_t validTail(const std::filesystem::path& path, std::uint64_t from, std::uint64_t size) {
    if (size <= from) return from;
    std::ifstream file(path, std::ios::binary);
    if (!file) return from;
    file.seekg(static_cast<std::streamoff>(from));
    std::vector<std::byte> bytes(static_cast<std::size_t>(size - from));
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (static_cast<std::uint64_t>(file.gcount()) != bytes.size()) return from;
    const auto span = std::span<const std::byte>(bytes);
    std::uint64_t cursor = 0;
    std::uint64_t valid = 0;
    while (cursor + sizeof(RecordHeader) <= span.size()) {
        RecordHeader record;
        std::memcpy(&record, span.data() + cursor, sizeof(record));
        const auto payloadOffset = cursor + sizeof(RecordHeader);
        if (record.kind == 0 || record.kind > ComputeRecord || record.payloadBytes > span.size() - payloadOffset) break;
        const auto end = padded(payloadOffset + record.payloadBytes);
        if (end > span.size()) break;
        if (ShaderRecompiler::HashBytes(span.subspan(static_cast<std::size_t>(payloadOffset), static_cast<std::size_t>(record.payloadBytes))) != record.payloadHash) break;
        valid = end;
        cursor = end;
    }
    return from + valid;
}

struct ReplayCounters {
    std::atomic<std::uint64_t> graphics{0}, compute{0}, present{0}, skipped{0}, rejected{0}, failed{0};
    std::atomic<std::uint32_t> threads{0};
    std::atomic<double> ms{0};
};

ReplayCounters& replayCounters() {
    static ReplayCounters counters;
    return counters;
}

// Builds the journal's pipelines for one device on a pool: the first thread loads the file, every
// thread then takes the next entry. A pipeline the store already holds counts as present; an entry
// whose modules are missing or whose set layouts differ from what its modules describe is rejected.
class Replay : public PipelineJournalReplay {
public:
    // The context keeps the device's buffer pool: a store entry is alive for a draw only while its
    // pool is the draw's (the pool tells device instances apart), so the entries built here must
    // name it. The pool is released with the replay, before the device's teardown resets it.
    Replay(Context context, PipelineJournalHooks hooks, unsigned threads) : context(std::move(context)), hooks(std::move(hooks)), active(threads) {
        started = std::chrono::steady_clock::now();
        replayCounters().threads.store(threads, std::memory_order_relaxed);
        workers.reserve(threads);
        for (unsigned index = 0; index < threads; ++index) workers.emplace_back([this] { run(); });
    }

    ~Replay() override {
        Cancel();
        Wait();
    }

    void Cancel() override { cancelled.store(true, std::memory_order_relaxed); }

    void Wait() override {
        for (auto& worker : workers) {
            if (worker.joinable()) worker.join();
        }
        workers.clear();
    }

    [[nodiscard]] bool Done() const override { return finished.load(std::memory_order_acquire); }

private:
    void run() {
        std::call_once(loadOnce, [&] {
            try {
                auto journal = Journal::Instance().Load();
                const auto limit = configured("APS5_PIPELINE_JOURNAL_ENTRIES", 1024u);
                if (journal.entries.size() > limit) journal.entries.resize(static_cast<std::size_t>(limit));
                loaded = std::move(journal);
            } catch (const std::exception& error) {
                std::fprintf(stderr, "[pipeline-journal] cannot load %s: %s\n", Journal::Instance().Path().string().c_str(), error.what());
            }
        });
        for (;;) {
            if (cancelled.load(std::memory_order_relaxed)) break;
            const auto index = cursor.fetch_add(1, std::memory_order_relaxed);
            if (index >= loaded.entries.size()) break;
            try {
                replay(loaded.entries[index]);
            } catch (const std::exception& error) {
                replayCounters().failed.fetch_add(1, std::memory_order_relaxed);
                static std::atomic<std::uint64_t> reported{0};
                if (reported.fetch_add(1) < 8) std::fprintf(stderr, "[pipeline-journal] replay failed: %s\n", error.what());
            }
        }
        if (active.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            loaded = LoadedJournal{};
            const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            replayCounters().ms.store(ms, std::memory_order_relaxed);
            finished.store(true, std::memory_order_release);
            ReportPipelineJournal();
        }
    }

    void replay(const LoadedEntry& entry) {
        struct Replaying {
            Replaying() { replayingHere = true; }
            ~Replaying() { replayingHere = false; }
        } replaying;
        auto& counters = replayCounters();
        if (entry.kind == ComputeRecord) {
            const auto module = loaded.modules.find(entry.compute);
            if (module == loaded.modules.end() || module->second.stage != ShaderRecompiler::ShaderStage::Compute || !hooks.compute) {
                counters.rejected.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            (hooks.compute(*module->second.result, entry.pushConstants) ? counters.compute : counters.present).fetch_add(1, std::memory_order_relaxed);
            return;
        }
        std::vector<CompiledShader> shaders;
        for (const auto& stage : entry.graphics.stages) {
            const auto module = loaded.modules.find(stage.identity);
            if (module == loaded.modules.end() || module->second.stage != stage.stage) {
                counters.rejected.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            shaders.push_back({stage.stage, module->second.result.get(), stage.pushConstantOffset});
        }
        if (shaders.empty() || context.descriptorCache == nullptr) {
            counters.rejected.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const auto store = PipelineStoreTotals();
        if (store.cached >= store.bound) {
            counters.skipped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const auto layouts = DescribeSetLayouts(context, shaders);
        if (!std::ranges::equal(layouts.mainKey, entry.graphics.mainLayoutKey) || !std::ranges::equal(layouts.fragmentKey, entry.graphics.fragmentLayoutKey) || layouts.layouts.split != entry.graphics.splitSets) {
            counters.rejected.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        bool created = false;
        static_cast<void>(CachedPipeline(context, entry.graphics.state, entry.graphics.vertexInput, layouts.View(), shaders, entry.graphics.attachmentLayout, &created));
        (created ? counters.graphics : counters.present).fetch_add(1, std::memory_order_relaxed);
    }

    Context context;
    PipelineJournalHooks hooks;
    std::vector<std::thread> workers;
    std::once_flag loadOnce;
    LoadedJournal loaded;
    std::atomic<std::size_t> cursor{0};
    std::atomic<unsigned> active{0};
    std::atomic<bool> finished{false};
    std::atomic<bool> cancelled{false};
    std::chrono::steady_clock::time_point started;
};

}

State CanonicalPipelineState(const State& state) {
    State canonical{};
    canonical.stages.path = state.stages.path;
    canonical.stages.vertexWaveSize = state.stages.vertexWaveSize;
    canonical.stages.fragmentWaveSize = state.stages.fragmentWaveSize;
    canonical.meshLayerBase = state.meshLayerBase;
    canonical.meshLayerCount = state.meshLayerCount;
    canonical.stages.mesh = state.stages.mesh;
    canonical.stages.tessellation = state.stages.tessellation;
    if (state.depth) {
        DepthTarget depth{};
        depth.format = state.depth->format;
        depth.samples = state.depth->samples;
        canonical.depth = depth;
    }
    canonical.depthTest = state.depthTest;
    canonical.depthWrite = state.depthWrite;
    canonical.depthCompare = state.depthCompare;
    canonical.depthBoundsTest = state.depthBoundsTest;
    canonical.depthBias = state.depthBias;
    canonical.depthBiasClamp = state.depthBiasClamp;
    canonical.depthBiasPerFace = state.depthBiasPerFace;
    canonical.stencilTest = state.stencilTest;
    canonical.stencilFront = state.stencilFront;
    canonical.stencilBack = state.stencilBack;
    if (state.stencilReplayReference) canonical.stencilReplayReference = 0u;
    for (const auto& color : state.colors) {
        ColorTarget canonicalColor{};
        canonicalColor.format = color.format;
        canonicalColor.exportIndex = color.exportIndex;
        canonicalColor.componentMapping = color.componentMapping;
        canonicalColor.partialWriteMask = color.partialWriteMask;
        canonical.colors.push_back(canonicalColor);
    }
    canonical.blends = state.blends;
    canonical.blendConstants = state.blendConstants;
    canonical.hasColorTarget = state.hasColorTarget;
    canonical.rectList = state.rectList;
    canonical.dualSourceBlend = state.dualSourceBlend;
    canonical.reversedBlend = state.reversedBlend;
    canonical.topology = state.topology;
    canonical.primitiveRestart = state.primitiveRestart;
    canonical.negativeOneToOne = state.negativeOneToOne;
    canonical.depthClamp = state.depthClamp;
    canonical.conservativeRasterization = state.conservativeRasterization;
    canonical.cullMode = state.cullMode;
    canonical.polygonMode = state.polygonMode;
    canonical.frontFace = state.frontFace;
    canonical.provokingVertexMode = state.provokingVertexMode;
    canonical.samples = state.samples;
    canonical.sampleLocations = state.sampleLocations;
    canonical.customSampleLocations = state.customSampleLocations;
    canonical.sampleLocationGrid = state.sampleLocationGrid;
    canonical.perSampleShading = state.perSampleShading;
    canonical.mergedCoverage = state.mergedCoverage;
    canonical.truncatedCoverage = state.truncatedCoverage;
    canonical.quadSampleMask = state.quadSampleMask;
    canonical.depthRemap.mode = state.depthRemap.mode;
    canonical.vertexPoints = state.vertexPoints;
    canonical.pointSize = state.pointSize;
    canonical.pointSizeMin = state.pointSizeMin;
    canonical.pointSizeMax = state.pointSizeMax;
    return canonical;
}

bool GeneratedPipelineStage(const State& state, ShaderRecompiler::ShaderStage stage) {
    using Stage = ShaderRecompiler::ShaderStage;
    return (state.rectList && (stage == Stage::TessellationControl || stage == Stage::TessellationEvaluation)) || stage == Stage::Geometry;
}

GraphicsPipelineRecipe MakeGraphicsPipelineRecipe(const State& state, const VertexInputLayout& vertexInput, const PipelineSetLayouts& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout) {
    GraphicsPipelineRecipe recipe;
    recipe.attachmentLayout = attachmentLayout;
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        recipe.stages.push_back({shader.stage, shader.program->Identity(), shader.pushConstantOffset, GeneratedPipelineStage(state, shader.stage)});
    }
    recipe.vertexInput = vertexInput;
    recipe.mainLayoutKey.assign(resources.mainKey.begin(), resources.mainKey.end());
    recipe.fragmentLayoutKey.assign(resources.fragmentKey.begin(), resources.fragmentKey.end());
    recipe.splitSets = resources.split;
    recipe.state = CanonicalPipelineState(state);
    return recipe;
}

std::vector<std::byte> EncodeGraphicsPipelineRecipe(const GraphicsPipelineRecipe& recipe) {
    std::vector<std::byte> bytes;
    Writer writer(bytes);
    encodeRecipe(writer, recipe);
    return bytes;
}

bool DecodeGraphicsPipelineRecipe(std::span<const std::byte> bytes, GraphicsPipelineRecipe& recipe) {
    Reader reader(bytes);
    return decodeRecipe(reader, recipe);
}

std::uint64_t PipelineSwitch(const char* name, std::uint64_t fallback) {
    return configured(name, fallback);
}

bool PipelineJournalEnabled() {
    return Journal::Instance().Enabled();
}

bool PipelineJournalReplaying() {
    return replayingHere;
}

std::filesystem::path PipelineJournalPath() {
    return Journal::Instance().Path();
}

PipelineJournalCounters PipelineJournalTotals() {
    auto totals = Journal::Instance().Totals();
    const auto& replay = replayCounters();
    totals.replayedGraphics = replay.graphics.load(std::memory_order_relaxed);
    totals.replayedCompute = replay.compute.load(std::memory_order_relaxed);
    totals.replayPresent = replay.present.load(std::memory_order_relaxed);
    totals.replaySkipped = replay.skipped.load(std::memory_order_relaxed);
    totals.replayRejected = replay.rejected.load(std::memory_order_relaxed);
    totals.replayFailed = replay.failed.load(std::memory_order_relaxed);
    totals.replayMs = replay.ms.load(std::memory_order_relaxed);
    totals.replayThreads = replay.threads.load(std::memory_order_relaxed);
    return totals;
}

void RecordGraphicsPipeline(const GraphicsPipelineRecipe& recipe, std::span<const CompiledShader> shaders) {
    Journal::Instance().RecordGraphics(recipe, shaders);
}

void RecordComputePipeline(const ShaderRecompiler::RecompileResult& shader, bool pushConstants) {
    Journal::Instance().RecordCompute(shader, pushConstants);
}

void FlushPipelineJournal() {
    auto* journal = Journal::Existing();
    if (journal != nullptr && journal->Enabled()) journal->Flush();
}

void ReportPipelineJournal() {
    if (!profiling() || !Journal::Instance().Enabled()) return;
    const auto totals = PipelineJournalTotals();
    AgcDriver::ProfilePrint_nid_no_patch("[pipeline-journal] loaded %llu pipelines, %llu modules (%.1f MiB, %.0f ms); replay on %u threads: %llu graphics and %llu compute built, %llu present, %llu skipped (store full), %llu rejected, %llu failed in %.0f ms; recorded this run: %llu graphics, %llu compute, %llu modules (%.1f MiB), %llu write failures\n",
                                         static_cast<unsigned long long>(totals.loadedRecipes), static_cast<unsigned long long>(totals.loadedModules), static_cast<double>(totals.loadedBytes) / (1024.0 * 1024.0), totals.loadMs, totals.replayThreads,
                                         static_cast<unsigned long long>(totals.replayedGraphics), static_cast<unsigned long long>(totals.replayedCompute), static_cast<unsigned long long>(totals.replayPresent), static_cast<unsigned long long>(totals.replaySkipped), static_cast<unsigned long long>(totals.replayRejected), static_cast<unsigned long long>(totals.replayFailed), totals.replayMs,
                                         static_cast<unsigned long long>(totals.recordedGraphics), static_cast<unsigned long long>(totals.recordedCompute), static_cast<unsigned long long>(totals.recordedModules), static_cast<double>(totals.writtenBytes) / (1024.0 * 1024.0), static_cast<unsigned long long>(totals.writeFailures));
}

std::uint64_t PipelineJournalDeviceFingerprint(const VkPhysicalDeviceProperties& properties) {
    std::array<std::uint32_t, 7> words{properties.vendorID, properties.deviceID, properties.driverVersion, 0u, 0u, 0u, 0u};
    static_assert(VK_UUID_SIZE == 16);
    std::memcpy(words.data() + 3, properties.pipelineCacheUUID, VK_UUID_SIZE);
    return ShaderRecompiler::HashSpirvWords(words, 0x6a6f75726e616c01ull);
}

std::shared_ptr<PipelineJournalReplay> StartPipelineJournalReplay(const Context& context, std::uint64_t deviceFingerprint, PipelineJournalHooks hooks) {
    if (!Journal::Instance().Enabled() || !Journal::Instance().Bind(deviceFingerprint)) return nullptr;
    const auto threads = static_cast<unsigned>(configured("APS5_PIPELINE_JOURNAL_THREADS", std::max(1u, std::thread::hardware_concurrency() / 2u)));
    if (threads == 0) return nullptr;
    return std::make_shared<Replay>(context, std::move(hooks), threads);
}

}

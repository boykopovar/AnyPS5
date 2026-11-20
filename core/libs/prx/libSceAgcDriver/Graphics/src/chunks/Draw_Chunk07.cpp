#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libc/include/RunReport.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GpuColorTransfer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexPlanScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/StagedMerge.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/TracyZone.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <optional>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <set>
#include <unordered_map>
#include <type_traits>
#include <utility>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
std::optional<State> maskedState(const State& state, std::uint32_t fragmentOutputs) {
    std::optional<State> masked;
    for (std::size_t index = 0; index < state.blends.size(); ++index) {
        if ((index < 32 && ((fragmentOutputs >> index) & 1u) != 0) || state.blends[index].colorWriteMask == 0) continue;
        if (!masked.has_value()) masked = state;
        masked->blends[index].colorWriteMask = 0;
    }
    return masked;
}

std::optional<DepthSampleLocations> passDepthLocations(const Context& context, const State& state, const Pipeline& pipeline) {
    if (!state.depth || state.depth->samples <= 1u || !state.customSampleLocations || !pipeline.SampleLocationsEnabled()) return std::nullopt;
    auto locations = DepthSurfaceSampleLocations(context, *state.depth);
    locations.grid = pipeline.HostSampleLocationGrid();
    locations.locations = SampleLocations(state);
    return locations;
}

// Debug aid: APS5_DUMP_TARGETS=<n> saves the first n renders of every color target as a raw file
// (u32 width, u32 height, u32 VkFormat, then tightly packed rows).
int DumpTargetLimit() {
    static const int dumpLimit = [] { const char* text = std::getenv("APS5_DUMP_TARGETS"); return text ? std::atoi(text) : 0; }();
    return dumpLimit;
}

// A draw whose targets are all resident is recorded into the device's open batch like a dispatch:
// the CPU does not wait for it, and the drain before it goes away (queue order and the barriers
// keep it behind earlier recorded work); see `recorded` in Draw for the buffers that still need a
// synchronous draw. Other draws keep their own synchronous batch. Debug aid: APS5_SYNC_DRAWS=1
// makes every draw synchronous.
bool RecordDraws() {
    static const bool recordDraws = std::getenv("APS5_SYNC_DRAWS") == nullptr;
    return recordDraws;
}

}

std::optional<std::string> KnownValidationFailure(const Context& context, std::span<const CompiledShader> shaders, const State& state) {
    auto& key = validationKeyScratch();
    if (!ValidationKey(context, shaders, state, key)) return std::nullopt;
    std::lock_guard lock(validationMutex());
    const auto& failures = validationFailures();
    if (const auto found = failures.find(key); found != failures.end()) return found->second;
    return std::nullopt;
}

namespace {

struct TargetBinding {
    ColorTarget color;
    bool gpuTiling = false;
    std::shared_ptr<StorageTexture> resident;
    bool proxied = false;
    std::unique_ptr<Buffer> transfer;
    bool banded = false;
    VkRect2D band{};
    std::size_t bandOffset = 0;
    std::uint32_t bandPitch = 0;
    std::unique_ptr<Buffer> tiled;
    std::unique_ptr<DeviceBuffer> tiledDevice;
    std::unique_ptr<DeviceBuffer> linearDevice;
    std::vector<std::byte> original;
    TileMipLayout mip{};
    std::unique_ptr<RenderTarget> target;
    bool multisampled = false;
    VkImage multisampleImage = VK_NULL_HANDLE;
    std::shared_ptr<void> multisampleOwner;
    std::shared_ptr<StagedTemporary> stagedTemporary;
    VkImageView stagedTarget = VK_NULL_HANDLE;
    std::unique_ptr<Buffer> dump;
};

struct PreparedDraw {
    explicit PreparedDraw(bool profile) : timer(profile) {}
    PreparedDraw(const PreparedDraw&) = delete;
    PreparedDraw& operator=(const PreparedDraw&) = delete;
    std::shared_ptr<const DrawRecipe>* recipeOut = nullptr;
    const DrawTraceContext* trace = nullptr;
    std::uint64_t shape = 0;
    const std::vector<std::uint32_t>* contentKey = nullptr;
    bool passAttachments = false;
    bool attachmentsFromMemo = false;
    std::vector<std::uint32_t> stencilReferences;
    std::optional<PerformanceTimer> timing;
    DrawTimer timer;
    DrawOutcome outcome;
    const ShaderResources::BuildTiming* built = nullptr;
    double waitedBefore = 0;
    double ownWaitedMs = 0;
    DrawInputs inputs;
    const Pm4::DrawParameters::IndirectDraw* args = nullptr;
    int dumpLimit = 0;
    std::vector<TargetBinding> targets;
    std::vector<VkImageView> targetViews;
    Recorder* recorder = nullptr;
    ResolvedResources resolved;
    IndirectRecord indirect;
    std::vector<Pm4::DrawArguments> records;
    bool completion = false;
    bool listed = false;
    bool recorded = false;
    bool lean = false;
    bool multisampled = false;
    std::optional<State> masked;
    std::vector<std::pair<std::uint32_t, std::shared_ptr<Pipeline>>> stencilReplay;
    std::shared_ptr<Pipeline> pipeline;
    std::vector<std::shared_ptr<StorageTexture>> owners;
    std::vector<std::shared_ptr<void>> framebufferOwners;
    std::shared_ptr<Framebuffer> framebuffer;
    std::vector<SampleOrderTarget> reordered;
    SampleOrderPass reorderPass;
    std::optional<DepthSampleLocations> depthLocations;
    std::shared_ptr<std::shared_ptr<const void>> spaceKeep;
};

void keepHeldSpace(PreparedDraw& prepared) {
    const auto& resources = prepared.resolved.resources;
    if (prepared.spaceKeep != nullptr || prepared.recorder == nullptr || resources == nullptr || !resources->SpaceReuse() || !resources->SpaceHeld()) return;
    prepared.spaceKeep = reserveSpaceKeep(*prepared.recorder, *resources);
}

bool ShapeMemoEnabled() {
    static const bool enabled = std::getenv("APS5_NO_SHAPE_MEMO") == nullptr;
    return enabled;
}

bool VerifyShapeMemo() {
    static const bool verify = std::getenv("APS5_VERIFY_SHAPE_MEMO") != nullptr;
    return verify;
}

void noteShapeMemoVerified(bool pipeline) {
    static std::atomic<std::uint64_t> targets{0};
    static std::atomic<std::uint64_t> pipelines{0};
    static std::atomic<std::int64_t> last{0};
    (pipeline ? pipelines : targets).fetch_add(1, std::memory_order_relaxed);
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto previous = last.load(std::memory_order_relaxed);
    if (now - previous < 10 || !last.compare_exchange_strong(previous, now)) return;
    std::fprintf(stderr, "[shape-memo] verified %llu targets and %llu pipelines\n", static_cast<unsigned long long>(targets.load()), static_cast<unsigned long long>(pipelines.load()));
}

struct ShapeChoice {
    VkImageLayout attachmentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::uint32_t fragmentOutputs = 0;
    std::vector<std::uint64_t> stages;
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    std::vector<std::uint32_t> mainKey;
    std::vector<std::uint32_t> fragmentKey;
    std::uint64_t hash = 0;
    std::weak_ptr<Pipeline> pipeline;
};

struct ShapeEntry {
    VkDevice device = VK_NULL_HANDLE;
    std::vector<std::weak_ptr<StorageTexture>> targets;
    std::vector<ShapeChoice> choices;
};

struct ShapeMemoStorage {};
constexpr std::size_t ShapeMemoEntries = 4096;
constexpr std::size_t ShapeMemoChoices = 4;

std::unordered_map<std::uint64_t, ShapeEntry>& shapeMemo() {
    return HostThreadLocal<std::unordered_map<std::uint64_t, ShapeEntry>, ShapeMemoStorage>();
}

const ShapeEntry* findShape(const Context& context, std::uint64_t shape) {
    if (shape == 0 || !ShapeMemoEnabled()) return nullptr;
    const auto& entries = shapeMemo();
    const auto found = entries.find(shape);
    return found == entries.end() || found->second.device != context.device ? nullptr : &found->second;
}

std::shared_ptr<StorageTexture> shapeTarget(const Context& context, std::uint64_t shape, std::size_t index) {
    const auto* entry = findShape(context, shape);
    return entry == nullptr || index >= entry->targets.size() ? nullptr : entry->targets[index].lock();
}

bool sameStages(std::span<const std::uint64_t> kept, std::span<const CompiledShader> shaders) {
    if (kept.size() != shaders.size() * 4u + 1u) return false;
    std::size_t at = 0;
    for (const auto& shader : shaders) {
        const auto identity = shader.program->Identity();
        if (identity.Empty() || kept[at] != static_cast<std::uint64_t>(shader.stage) || kept[at + 1] != identity.low || kept[at + 2] != identity.high || kept[at + 3] != KeyedPushOffset(shader)) return false;
        at += 4;
    }
    return kept[at] == PushConstantStages(shaders);
}

bool sameChoice(const ShapeChoice& choice, const VertexInputLayout& input, const PipelineSetLayouts& layouts, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout, std::uint32_t fragmentOutputs) {
    const auto sameBinding = [](const VkVertexInputBindingDescription& a, const VkVertexInputBindingDescription& b) { return a.binding == b.binding && a.stride == b.stride && a.inputRate == b.inputRate; };
    const auto sameAttribute = [](const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b) { return a.location == b.location && a.binding == b.binding && a.format == b.format && a.offset == b.offset; };
    return choice.attachmentLayout == attachmentLayout && choice.fragmentOutputs == fragmentOutputs && std::ranges::equal(choice.bindings, input.bindings, sameBinding) && std::ranges::equal(choice.attributes, input.attributes, sameAttribute) && std::ranges::equal(choice.mainKey, layouts.mainKey) && std::ranges::equal(choice.fragmentKey, layouts.fragmentKey) && sameStages(choice.stages, shaders);
}

std::shared_ptr<Pipeline> shapePipeline(const Context& context, std::uint64_t shape, const VertexInputLayout& input, const PipelineSetLayouts& layouts, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout, std::uint32_t fragmentOutputs) {
    const auto* entry = findShape(context, shape);
    if (entry == nullptr) return nullptr;
    for (const auto& choice : entry->choices) {
        if (!sameChoice(choice, input, layouts, shaders, attachmentLayout, fragmentOutputs)) continue;
        const auto hash = choice.hash;
        const auto expected = choice.pipeline;
        return TouchCachedPipeline(context, hash, expected);
    }
    return nullptr;
}

void rememberShape(const Context& context, std::uint64_t shape, std::span<const TargetBinding> targets, const std::shared_ptr<Pipeline>* pipeline, std::uint64_t hash, const VertexInputLayout& input, const PipelineSetLayouts& layouts, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout, std::uint32_t fragmentOutputs) {
    if (shape == 0 || !ShapeMemoEnabled()) return;
    auto& entries = shapeMemo();
    if (entries.size() >= ShapeMemoEntries && !entries.contains(shape)) entries.clear();
    auto& entry = entries[shape];
    if (entry.device != context.device) {
        entry = {};
        entry.device = context.device;
    }
    entry.targets.resize(targets.size());
    for (std::size_t index = 0; index < targets.size(); ++index) {
        const auto& resident = targets[index].resident;
        if (resident == nullptr || targets[index].color.arrayLastSlice != 0) entry.targets[index].reset();
        else if (entry.targets[index].owner_before(resident) || resident.owner_before(entry.targets[index])) entry.targets[index] = resident;
    }
    if (pipeline == nullptr || hash == 0) return;
    auto& choices = entry.choices;
    const auto kept = std::ranges::find_if(choices, [&](const ShapeChoice& choice) { return sameChoice(choice, input, layouts, shaders, attachmentLayout, fragmentOutputs); });
    if (kept != choices.end()) {
        kept->hash = hash;
        kept->pipeline = *pipeline;
        std::rotate(choices.begin(), kept, std::next(kept));
        return;
    }
    ShapeChoice choice;
    choice.attachmentLayout = attachmentLayout;
    choice.fragmentOutputs = fragmentOutputs;
    for (const auto& shader : shaders) {
        const auto identity = shader.program->Identity();
        if (identity.Empty()) return;
        choice.stages.insert(choice.stages.end(), {static_cast<std::uint64_t>(shader.stage), identity.low, identity.high, KeyedPushOffset(shader)});
    }
    choice.stages.push_back(PushConstantStages(shaders));
    choice.bindings = input.bindings;
    choice.attributes = input.attributes;
    choice.mainKey.assign(layouts.mainKey.begin(), layouts.mainKey.end());
    choice.fragmentKey.assign(layouts.fragmentKey.begin(), layouts.fragmentKey.end());
    choice.hash = hash;
    choice.pipeline = *pipeline;
    if (choices.size() >= ShapeMemoChoices) choices.pop_back();
    choices.insert(choices.begin(), std::move(choice));
}

bool sameColorTarget(const ColorTarget& a, const ColorTarget& b) {
    return a.address == b.address && a.extent.width == b.extent.width && a.extent.height == b.extent.height && a.format == b.format && a.bytes == b.bytes && a.componentMapping == b.componentMapping && a.tileMode == b.tileMode && a.elementBytes == b.elementBytes && a.packing == b.packing && a.dccAddress == b.dccAddress && a.chainDccAddress == b.chainDccAddress && a.dccAlphaOnMsb == b.dccAlphaOnMsb && a.dccPipeAligned == b.dccPipeAligned && a.cmaskAddress == b.cmaskAddress && a.cmaskBytes == b.cmaskBytes && a.cmaskFastClear == b.cmaskFastClear && a.eqaaFragments == b.eqaaFragments && a.surfaceAddress == b.surfaceAddress && a.surfaceExtent.width == b.surfaceExtent.width && a.surfaceExtent.height == b.surfaceExtent.height && a.mipCount == b.mipCount && a.mip == b.mip && a.mipTail == b.mipTail && a.clearWords == b.clearWords && a.slot == b.slot && a.depth == b.depth && a.depthSlice == b.depthSlice && a.arrayAddress == b.arrayAddress && a.arrayLastSlice == b.arrayLastSlice && a.arraySlice == b.arraySlice && a.layers == b.layers && a.exportIndex == b.exportIndex && a.uintExport == b.uintExport && a.volume == b.volume && a.pipeBankXor == b.pipeBankXor && a.samples == b.samples && a.coverageSamples == b.coverageSamples && a.dccControl == b.dccControl && a.fmaskAddress == b.fmaskAddress && a.fmaskPipeBankXor == b.fmaskPipeBankXor && a.partialWriteMask == b.partialWriteMask && a.staged == b.staged && a.stagedMask == b.stagedMask && a.stagedFormat == b.stagedFormat;
}

bool sameDepthTarget(const DepthTarget& a, const DepthTarget& b) {
    return a.address == b.address && a.stencilAddress == b.stencilAddress && a.extent.width == b.extent.width && a.extent.height == b.extent.height && a.format == b.format && std::bit_cast<std::uint32_t>(a.clearDepth) == std::bit_cast<std::uint32_t>(b.clearDepth) && a.clearStencil == b.clearStencil && a.htileAddress == b.htileAddress && a.htileStencil == b.htileStencil && a.samples == b.samples && a.depthSwizzle == b.depthSwizzle && a.pipeBankXor == b.pipeBankXor && a.firstSlice == b.firstSlice && a.lastSlice == b.lastSlice && a.iterate256 == b.iterate256 && a.htilePipeAligned == b.htilePipeAligned && a.htileVrsEncoding == b.htileVrsEncoding && a.htileDepthOnly == b.htileDepthOnly && a.gcn == b.gcn;
}

struct AttachmentMemo {
    bool valid = false;
    VkDevice device = VK_NULL_HANDLE;
    std::uint64_t recorder = 0;
    std::uint64_t generation = 0;
    std::uint64_t epoch = 0;
    std::uint64_t proof = 0;
    std::uint64_t pending = 0;
    std::uint64_t depthSerial = 0;
    std::uint64_t depthReleased = 0;
    std::vector<ColorTarget> colors;
    std::vector<std::weak_ptr<StorageTexture>> residents;
    std::vector<std::uint64_t> versions;
    std::vector<bool> gpuTiling;
    std::vector<bool> proxied;
    std::vector<VkImageView> views;
    std::optional<DepthTarget> depth;
    bool depthWrite = false;
    bool stencilTest = false;
    bool stencilWritten = false;
    VkImageView depthView = VK_NULL_HANDLE;
};

struct AttachmentMemoStorage {};

AttachmentMemo& attachmentMemo() {
    return HostThreadLocal<AttachmentMemo, AttachmentMemoStorage>();
}

bool PassAttachmentMemoEnabled() {
    return !RuntimeSwitches::Set(RuntimeSwitch::NoPassAttachmentMemo) && ShaderResources::EpochProofs();
}

bool VerifyPassAttachments() {
    static const bool verify = std::getenv("APS5_VERIFY_PASS_ATTACHMENTS") != nullptr;
    return verify;
}

enum class AttachmentEvent : std::size_t { Hit, NoMemo, Pass, Serials, Targets, Images, Depth, Stored, Ineligible, Verified, Mismatch, Count };

void noteAttachments(AttachmentEvent event) {
    static const bool stats = std::getenv("APS5_PIPELINE_STATS") != nullptr || std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!stats) return;
    static std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(AttachmentEvent::Count)> counts{};
    static std::atomic<std::int64_t> reportedAt{0};
    counts[static_cast<std::size_t>(event)].fetch_add(1, std::memory_order_relaxed);
    const auto tenSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 10;
    if (reportedAt.exchange(tenSeconds, std::memory_order_relaxed) == tenSeconds) return;
    std::array<unsigned long long, static_cast<std::size_t>(AttachmentEvent::Count)> taken{};
    for (std::size_t index = 0; index < taken.size(); ++index) taken[index] = counts[index].exchange(0, std::memory_order_relaxed);
    AgcDriver::ProfilePrint_nid_no_patch("[pass-attachments] 10 s: %llu hits; misses: %llu no memo, %llu pass, %llu serials, %llu targets, %llu images, %llu depth; %llu stored, %llu ineligible; verify %llu checked, %llu differed\n", taken[0], taken[1], taken[2], taken[3], taken[4], taken[5], taken[6], taken[7], taken[8], taken[9], taken[10]);
}

bool memoAttachments(const Context& context, const State& state, PreparedDraw& prepared, bool fill) {
    const auto& memo = attachmentMemo();
    const auto miss = [](AttachmentEvent why) { noteAttachments(why); return false; };
    if (!memo.valid || memo.device != context.device) return miss(AttachmentEvent::NoMemo);
    const auto* recorder = Recorder::Active();
    if (recorder == nullptr || recorder->Id() != memo.recorder || !recorder->RenderPassOpen() || recorder->RenderPassGeneration() != memo.generation) return miss(AttachmentEvent::Pass);
    if (GuestMemory::CollectEpoch() != memo.epoch || GuestMemory::ProofSerial() != memo.proof || StorageTexture::PendingSerial() != memo.pending) return miss(AttachmentEvent::Serials);
    if (state.colors.size() != memo.colors.size()) return miss(AttachmentEvent::Targets);
    for (std::size_t index = 0; index < state.colors.size(); ++index) {
        if (!sameColorTarget(state.colors[index], memo.colors[index])) return miss(AttachmentEvent::Targets);
    }
    if (state.depth.has_value() != memo.depth.has_value()) return miss(AttachmentEvent::Depth);
    if (state.depth && overwrittenStencil(state)) return miss(AttachmentEvent::Depth);
    if (state.depth) {
        if (!sameDepthTarget(*state.depth, *memo.depth) || state.depthWrite != memo.depthWrite || state.stencilTest != memo.stencilTest || writesStencilTarget(state) != memo.stencilWritten || DepthSurfacesSerial() != memo.depthSerial || DepthSurfacesReleased() != memo.depthReleased) return miss(AttachmentEvent::Depth);
    }
    auto& targets = prepared.targets;
    for (std::size_t index = 0; index < memo.residents.size(); ++index) {
        auto resident = memo.residents[index].lock();
        if (resident == nullptr || resident->Version() != memo.versions[index] || !StorageImageCached(context, resident.get()) || !StorageImageServesKeys(*resident, SurfaceForTarget(state.colors[index]).dccAddress)) {
            for (std::size_t filled = 0; filled < index; ++filled) targets[filled].resident.reset();
            return miss(AttachmentEvent::Images);
        }
        if (fill) targets[index].resident = std::move(resident);
    }
    if (!fill) return true;
    noteAttachments(AttachmentEvent::Hit);
    auto& targetViews = prepared.targetViews;
    for (std::size_t index = 0; index < state.colors.size(); ++index) {
        auto& binding = targets[index];
        binding.color = state.colors[index];
        binding.gpuTiling = memo.gpuTiling[index];
        binding.proxied = memo.proxied[index];
        targetViews.push_back(memo.views[index]);
    }
    if (state.depth) targetViews.push_back(memo.depthView);
    return true;
}

void checkAttachments(const State& state, const PreparedDraw& prepared) {
    const auto& memo = attachmentMemo();
    bool same = prepared.targetViews.size() == memo.views.size() + (memo.depth ? 1u : 0u);
    for (std::size_t index = 0; same && index < memo.views.size(); ++index) same = prepared.targets[index].resident == memo.residents[index].lock() && prepared.targetViews[index] == memo.views[index];
    if (same && state.depth) same = prepared.targetViews.back() == memo.depthView;
    noteAttachments(same ? AttachmentEvent::Verified : AttachmentEvent::Mismatch);
    if (!same) {
        static std::atomic<int> reports{0};
        if (reports.fetch_add(1, std::memory_order_relaxed) < 20) std::fprintf(stderr, "[pass-attachments] verify: the memo's attachments differ from the full lookup (target 0x%llx, depth 0x%llx)\n", static_cast<unsigned long long>(state.hasColorTarget ? state.color.address : 0), static_cast<unsigned long long>(state.depth ? state.depth->address : 0));
    }
}

void rememberAttachments(const Context& context, const State& state, const PreparedDraw& prepared) {
    auto& memo = attachmentMemo();
    const auto* recorder = prepared.recorder;
    const auto epoch = GuestMemory::CollectEpoch();
    const bool eligible = epoch != 0 && recorder != nullptr && recorder->RenderPassOpen() && prepared.dumpLimit == 0 && (!state.depth || state.depth->samples == 1u) && prepared.targetViews.size() == prepared.targets.size() + (state.depth ? 1u : 0u) && std::ranges::all_of(prepared.targets, [](const TargetBinding& binding) { return !binding.multisampled && binding.resident != nullptr && binding.target == nullptr && binding.stagedTemporary == nullptr && binding.dump == nullptr && !binding.color.staged && binding.color.partialWriteMask == 0; });
    if (!eligible) {
        memo.valid = false;
        noteAttachments(AttachmentEvent::Ineligible);
        return;
    }
    memo.valid = true;
    memo.device = context.device;
    memo.recorder = recorder->Id();
    memo.generation = recorder->RenderPassGeneration();
    memo.epoch = epoch;
    memo.proof = GuestMemory::ProofSerial();
    memo.pending = StorageTexture::PendingSerial();
    memo.depthSerial = DepthSurfacesSerial();
    memo.depthReleased = DepthSurfacesReleased();
    memo.colors.assign(state.colors.begin(), state.colors.end());
    memo.residents.clear();
    memo.versions.clear();
    memo.gpuTiling.clear();
    memo.proxied.clear();
    memo.views.clear();
    for (std::size_t index = 0; index < prepared.targets.size(); ++index) {
        const auto& binding = prepared.targets[index];
        memo.residents.emplace_back(binding.resident);
        memo.versions.push_back(binding.resident->Version());
        memo.gpuTiling.push_back(binding.gpuTiling);
        memo.proxied.push_back(binding.proxied);
        memo.views.push_back(prepared.targetViews[index]);
    }
    memo.depth = state.depth;
    memo.depthWrite = state.depthWrite;
    memo.stencilTest = state.stencilTest;
    memo.stencilWritten = state.depth ? writesStencilTarget(state) : false;
    memo.depthView = state.depth ? prepared.targetViews.back() : VK_NULL_HANDLE;
    noteAttachments(AttachmentEvent::Stored);
}

bool storesIntoResident(const ShaderResources& resources, const StorageTexture& resident) {
    return std::ranges::any_of(resources.StorageImages(), [&](const std::pair<VkImage, bool>& image) { return image.first == resident.Image() && image.second; });
}

bool VerifyTargetMemo() {
    static const bool verify = std::getenv("APS5_VERIFY_TARGET_MEMO") != nullptr;
    return verify;
}

bool TargetMemoEnabled() {
    static const bool cached = std::getenv("APS5_NO_TEXTURE_CACHE") == nullptr;
    return cached && !RuntimeSwitches::Set(RuntimeSwitch::NoTargetMemo) && ShaderResources::EpochProofs();
}

struct TargetProof {
    std::uint64_t epoch = 0;
    std::uint64_t proofs = 0;
    std::uint64_t pending = 0;
    std::uint64_t unwatch = 0;
    std::uint64_t allocations = 0;
    std::uint64_t surfaces = 0;
    std::uint64_t released = 0;
    std::uint64_t claims = 0;
    const Recorder* recorder = nullptr;
    VkDevice device = VK_NULL_HANDLE;
    bool operator==(const TargetProof&) const = default;
    const char* MovedBefore(const TargetProof& after) const {
        if (epoch != after.epoch) return "epoch";
        if (surfaces != after.surfaces) return "surfaces";
        return released != after.released ? "released" : nullptr;
    }
};

TargetProof currentTargetProof(const Context& context) {
    return {GuestMemory::CollectEpoch(), GuestMemory::ProofSerial(), StorageTexture::PendingSerial(), GuestMemory::UnwatchSerial(), GuestAllocations::GuestAllocationsGeneration_nid_postfix(), DepthSurfacesSerial(), DepthSurfacesReleased(), MultisampleClaimSerial(), Recorder::Active(), context.device};
}


struct DepthFootprint {
    std::uint64_t address = 0;
    std::uint64_t stencilAddress = 0;
    std::uint64_t htileAddress = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t samples = 0;
    bool d16 = false;
    bool sliced = false;
    bool operator==(const DepthFootprint&) const = default;
};

std::optional<DepthFootprint> depthFootprint(const std::optional<DepthTarget>& depth) {
    if (!depth) return std::nullopt;
    return DepthFootprint{depth->address, depth->stencilAddress, depth->htileAddress, depth->extent.width, depth->extent.height, depth->samples,
        depth->format == VK_FORMAT_D16_UNORM || depth->format == VK_FORMAT_D16_UNORM_S8_UINT, depth->firstSlice != 0u || depth->lastSlice != 0u};
}

struct ColorMemo {
    struct Attachment {
        std::weak_ptr<StorageTexture> resident;
        bool proxied = false;
        VkImageView view = VK_NULL_HANDLE;
    };
    bool valid = false;
    TargetProof proof;
    std::vector<ColorTarget> descriptors;
    std::vector<Attachment> attachments;
    std::optional<DepthFootprint> clearOfDepth;
};

struct DepthMemo {
    bool valid = false;
    TargetProof proof;
    std::optional<DepthTarget> descriptor;
};

struct TargetMemo {
    ColorMemo colors;
    DepthMemo depth;
    void Invalidate() {
        colors.valid = false;
        depth.valid = false;
    }
};

struct TargetMemoStorage {};
TargetMemo& ThreadTargetMemo() {
    return HostThreadLocal<TargetMemo, TargetMemoStorage>();
}
thread_local std::uint64_t targetMemoBuilds = 0;
std::atomic<const TargetMemo*> targetMemoOwner{nullptr};

constexpr std::array<const char*, static_cast<std::size_t>(TargetMemoEvent::Count)> TargetMemoEventNames{
    "lookup", "hit", "overlap cached", "overlap computed", "accept", "accept without depth", "depth hit", "avoided refresh lookups",
    "invalid", "thread changed", "descriptor", "resident gone", "resident evicted", "depth overlap", "metadata",
    "epoch", "proofs", "pending", "unwatch", "allocations", "surfaces", "released", "claims", "recorder", "device",
    "post-depth invalidated", "depth fallback", "admission", "recipe invalidated", "build", "verify checked"
};

std::array<ProfileCounter, static_cast<std::size_t>(TargetMemoEvent::Count)>& targetMemoEvents() {
    static std::array<ProfileCounter, static_cast<std::size_t>(TargetMemoEvent::Count)> events;
    return events;
}

const bool targetMemoProfiled = std::getenv("APS5_PROFILE_TARGET_MEMO") != nullptr || std::getenv("APS5_PROFILE_DRAW") != nullptr;

void noteTargetMemo(TargetMemoEvent event, std::uint64_t count = 1) {
    if (targetMemoProfiled) targetMemoEvents()[static_cast<std::size_t>(event)].Add(count);
#ifdef TRACY_ENABLE
    static std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(TargetMemoEvent::Count)> counts{};
    const auto index = static_cast<std::size_t>(event);
    TracyCPlotI(TargetMemoEventNames[index], static_cast<std::int64_t>(counts[index].fetch_add(count, std::memory_order_relaxed) + count));
#endif
}

void reportTargetMemo() {
    using Clock = std::chrono::steady_clock;
    static std::atomic<std::int64_t> reported{Clock::now().time_since_epoch().count()};
    static std::array<std::uint64_t, static_cast<std::size_t>(TargetMemoEvent::Count)> previous{};
    const auto now = Clock::now().time_since_epoch().count();
    auto last = reported.load(std::memory_order_relaxed);
    if (Clock::duration(now - last) < std::chrono::seconds(10) || !reported.compare_exchange_strong(last, now, std::memory_order_relaxed)) return;
    std::string line;
    for (std::size_t index = 0; index < previous.size(); ++index) {
        const auto total = targetMemoEvents()[index].Sum();
        const auto count = total - previous[index];
        previous[index] = total;
        if (count == 0) continue;
        line += (line.empty() ? " " : ", ") + std::string(TargetMemoEventNames[index]) + " " + std::to_string(count);
    }
    AgcDriver::ProfilePrint_nid_no_patch("[target-memo] (10 s, all threads):%s\n", line.empty() ? " none" : line.c_str());
}

bool colorProofMatches(const TargetProof& before, const TargetProof& after) {
    if (before == after) return true;
    const auto note = [&](bool changed, TargetMemoEvent event) { if (changed) noteTargetMemo(event); };
    note(before.epoch != after.epoch, TargetMemoEvent::Epoch);
    note(before.proofs != after.proofs, TargetMemoEvent::Proofs);
    note(before.pending != after.pending, TargetMemoEvent::Pending);
    note(before.unwatch != after.unwatch, TargetMemoEvent::Unwatch);
    note(before.allocations != after.allocations, TargetMemoEvent::Allocations);
    note(before.surfaces != after.surfaces, TargetMemoEvent::Surfaces);
    note(before.released != after.released, TargetMemoEvent::Released);
    note(before.claims != after.claims, TargetMemoEvent::Claims);
    note(before.recorder != after.recorder, TargetMemoEvent::Recorder);
    note(before.device != after.device, TargetMemoEvent::Device);
    return false;
}


bool ownsTargetMemo(TargetMemo& memo) {
    if (targetMemoOwner.load(std::memory_order_relaxed) == &memo) return true;
    targetMemoOwner.store(&memo, std::memory_order_relaxed);
    if (memo.colors.valid || memo.depth.valid) noteTargetMemo(TargetMemoEvent::ThreadChanged);
    memo.Invalidate();
    return false;
}

void dropTargetMemo() {
    if (targetMemoOwner.load(std::memory_order_relaxed) == nullptr) return;
    ThreadTargetMemo().Invalidate();
    targetMemoOwner.store(nullptr, std::memory_order_relaxed);
}

bool colorOverlapsDepth(const State& state, const TargetBinding& binding) {
    if (!state.depth) return false;
    const auto& color = binding.color;
    const auto& resident = *binding.resident;
    const auto& descriptor = resident.Descriptor();
    return DepthTargetOverlaps(*state.depth, resident.Descriptor().baseAddress, resident.GuestBytes()) ||
        (descriptor.dccAddress != 0 && DepthTargetOverlaps(*state.depth, descriptor.dccAddress, std::max<std::uint64_t>(resident.GuestBytes() / 256u, 1u))) ||
        (color.dccAddress != 0 && DepthTargetOverlaps(*state.depth, color.dccAddress, std::max<std::size_t>(colorKeyCount(color, color.bytes), 1u))) ||
        (color.chainDccAddress != 0 && DepthTargetOverlaps(*state.depth, color.chainDccAddress, std::max<std::size_t>(colorKeyCount(color, color.bytes), 1u))) ||
        (color.cmaskAddress != 0 && DepthTargetOverlaps(*state.depth, color.cmaskAddress, color.cmaskBytes));
}

bool metadataProved(const ColorTarget& color) {
    const auto keys = std::max<std::size_t>(colorKeyCount(color, color.bytes), 1u);
    const bool proved = (color.dccAddress == 0 || GuestMemory::CollectWrites(color.dccAddress, keys) != 0) &&
        (color.chainDccAddress == 0 || GuestMemory::CollectWrites(color.chainDccAddress, keys) != 0) &&
        (color.cmaskAddress == 0 || cmaskProved(color.cmaskAddress, color.cmaskBytes));
    if (!proved) noteTargetMemo(TargetMemoEvent::Metadata);
    return proved;
}

}
}
}
}
}
}

#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DirectDraws.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRecords.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/LeanDraw.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DrawWriteRanges.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/SrtWalker.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>
#include <span>
#include <utility>

namespace AgcDriver::DriverDetail {

namespace {

using DirectDraws::Counter;
using DirectDraws::Step;
using Role = ShaderRecompiler::ProgramRole;
using Stage = ShaderRecompiler::ShaderStage;

constexpr std::size_t InvocationSlots = 1024;
constexpr std::size_t WideInvocationSlots = 16384;
constexpr std::uint64_t WalkPageBytes = 0x1000u;

std::size_t mixedSlot(std::uint64_t first, std::uint64_t second, std::size_t slots) {
    auto mixed = first * 0x9e3779b97f4a7c15ull ^ second;
    mixed ^= mixed >> 29u;
    return static_cast<std::size_t>(mixed % slots);
}

void noteUnsettled(const AddressRanges& ranges, const Graphics::Recorder::WriteRanges* before) {
    static const bool on = std::getenv("APS5_PIPELINE_STATS") != nullptr;
    if (!on) return;
    static std::atomic<std::uint64_t> queued{0}, recorded{0}, stamped{0}, total{0};
    if (DrawPipeline::Active() && DrawPipeline::Queue0().Busy() && DrawPipeline::Queue0().OverlapsAny(ranges)) queued.fetch_add(1, std::memory_order_relaxed);
    else if (!Graphics::Recorder::ReadsSettled(ranges, before)) recorded.fetch_add(1, std::memory_order_relaxed);
    else stamped.fetch_add(1, std::memory_order_relaxed);
    if ((total.fetch_add(1, std::memory_order_relaxed) + 1u) % 4096u == 0u) std::fprintf(stderr, "[direct-unsettled] cumulative: %llu over a write queued in the draw pipeline, %llu over a recorder write not settled, %llu with a newer memory stamp; %llu ranges under a newer block stamp settled by their pages\n", static_cast<unsigned long long>(queued.load(std::memory_order_relaxed)), static_cast<unsigned long long>(recorded.load(std::memory_order_relaxed)), static_cast<unsigned long long>(stamped.load(std::memory_order_relaxed)), static_cast<unsigned long long>(PreciseSettles.load(std::memory_order_relaxed)));
}

bool DirectShape(const LeanDrawJob& job) {
    const auto& decode = *job.shape->decode;
    const auto& state = decode.state;
    if (job.parameters.indirect || state.quadList || state.rectList || state.stages.path != Graphics::ShaderPath::Vertex || state.stages.mesh || state.stages.tessellation || state.stages.tessellationGeometry || state.tessellationCapture != nullptr || !Graphics::DrawsRecordable()) return false;
    if (decode.programs.empty() || decode.programs.size() > DirectClassKey::MaxStages || decode.roles.size() != decode.programs.size()) return false;
    for (std::size_t i = 0; i < decode.programs.size(); ++i) {
        const auto& program = decode.programs[i];
        const auto stage = program.binary.stage;
        if ((stage != Stage::Vertex && stage != Stage::Fragment) || decode.roles[i] != (stage == Stage::Fragment ? Role::Fragment : Role::Main) || SkippedShader(program.binary.codeAddress)) return false;
    }
    return true;
}

struct InvocationSlot {
    std::shared_ptr<const ShaderSnapshot> snapshot;
    std::size_t codeOffset = 0;
    std::shared_ptr<const ShaderRecompiler::SourceHandle> handle;
    bool convertedSamples = false;
    bool layerRowSamples = false;
};

struct InvocationCache {
    std::array<InvocationSlot, InvocationSlots> slots;
    std::vector<std::uint64_t> key;
    std::vector<InvocationSlot> wide;

    InvocationSlot& For(const DrawProgram& program) {
        const auto snapshot = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(program.snapshot.get()));
        if (Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::InvocationSlotsWide)) {
            if (wide.empty()) wide.resize(WideInvocationSlots);
            return wide[mixedSlot(snapshot, program.codeOffset, WideInvocationSlots)];
        }
        if (!wide.empty()) {
            wide.clear();
            wide.shrink_to_fit();
        }
        return slots[mixedSlot(snapshot, program.codeOffset, InvocationSlots)];
    }
};

struct InvocationCacheStorage {};

bool InvocationCacheable(const DrawProgram& program, const ShaderRecompiler::RecompileRequest& request) {
    const auto stage = request.shader.stage;
    if (stage == Stage::Compute || (ShaderRecompiler::GcnGuestShaders() && stage == Stage::Vertex)) return false;
    return (stage != Stage::Vertex && stage != Stage::Fragment) || !request.shader.header.empty() || !program.snapshot->userDataTailRegister;
}

struct DirectReads {
    std::vector<std::pair<std::uint64_t, std::uint32_t>> words;
    std::uint64_t page = ~std::uint64_t{0};
    bool failed = false;

    void Reset() {
        words.clear();
        page = ~std::uint64_t{0};
        failed = false;
    }
};

struct DirectReadsStorage {};

bool readDirectWord(void* context, std::uint64_t address, std::uint32_t* value) {
    auto& reads = *static_cast<DirectReads*>(context);
    const auto page = address & ~(WalkPageBytes - 1u);
    const bool crossesPage = (address & (WalkPageBytes - 1u)) > WalkPageBytes - sizeof(*value);
    if (crossesPage || page != reads.page) {
        if (!crossesPage && GuestMemory::Accessible(reinterpret_cast<const void*>(page), WalkPageBytes)) {
            reads.page = page;
        } else if (!GuestMemory::Accessible(reinterpret_cast<const void*>(address), sizeof(*value))) {
            reads.failed = true;
            return false;
        }
    }
    std::memcpy(value, reinterpret_cast<const void*>(address), sizeof(*value));
    reads.words.emplace_back(address, *value);
    return true;
}

bool directAccessible(void* context, std::uint64_t address, std::uint64_t bytes) {
    if (GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes))) return true;
    static_cast<DirectReads*>(context)->failed = true;
    return false;
}

bool directViewsCurrent(void*, std::span<const ShaderRecompiler::RegisteredImageView> views) {
    return CaptureRegisteredViewsCurrent(views);
}

std::shared_ptr<const ShaderRecompiler::ResourceCapture> walkDirect(const ShaderRecompiler::PreparedShaderInvocation& invocation, DirectReads& reads) {
    const auto& request = invocation.Request();
    ShaderRecompiler::SrtRuntime runtime;
    runtime.userData = request.context.userData;
    runtime.shaderBase = request.shader.codeAddress;
    runtime.userContext = &reads;
    runtime.readMemory = &readDirectWord;
    runtime.readSpecializationMemory = &readDirectWord;
    runtime.accessible = &directAccessible;
    ShaderMemory::SetImageViewHooks(runtime);
    runtime.snapshotCurrent = &directViewsCurrent;
    return invocation.Capture(runtime);
}

bool readRanges(DirectReads& reads, AddressRanges& ranges, std::vector<std::uint32_t>& values) {
    auto& words = reads.words;
    std::sort(words.begin(), words.end());
    words.erase(std::unique(words.begin(), words.end()), words.end());
    ranges.clear();
    values.clear();
    for (std::size_t index = 0; index < words.size(); ++index) {
        const auto [address, value] = words[index];
        if (index != 0 && words[index - 1].first == address) return false;
        if (!ranges.empty() && ranges.back().second == address) ranges.back().second += sizeof(std::uint32_t);
        else ranges.emplace_back(address, address + sizeof(std::uint32_t));
        values.push_back(value);
    }
    return true;
}

bool imageTables(const ShaderRecompiler::ResourceSnapshot& snapshot) {
    return !snapshot.tables.tables.empty() || !snapshot.tables.nested.empty() || !snapshot.tables.ranges.empty();
}

struct StageFrontSlot {
    std::uint64_t variant = 0;
    std::uint64_t key = 0;
    std::uint64_t clears = 0;
    std::shared_ptr<const DirectStage> stage;
};

struct StageFront {
    std::array<StageFrontSlot, DirectStageStore::FrontSlots> slots;
};

struct StageFrontStorage {};

struct ClassFrontSlot {
    std::uint64_t probe = 0;
    std::uint64_t clears = 0;
    std::shared_ptr<const DirectClass> entry;
};

struct ClassFront {
    std::array<ClassFrontSlot, DirectClassStore::FrontSlots> slots;
};

struct ClassFrontStorage {};

bool ExactClassKeys() {
    static const bool exact = std::getenv("APS5_DIRECT_DRAWS_EXACT_KEY") != nullptr;
    return exact;
}

bool readsSettled(const AddressRanges& ranges, const DirectStages& resolved, const Graphics::Recorder::WriteRanges* pending, const std::optional<DrawPipeline::Watch>& queued) {
    const bool fast = Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::FastSettle);
    return RangesSettled(ranges, resolved.before.get(), resolved.generation, fast ? pending : nullptr) && (fast || Graphics::Recorder::ReadsSettled(ranges, pending)) && !(queued.has_value() && DrawPipeline::Queue0().WrittenSince(*queued, ranges));
}

constexpr std::uint64_t SettleMergeGap = 4096;

const AddressRanges& mergedRanges(const AddressRanges& ranges) {
    struct MergedStorage {};
    auto& merged = HostThreadLocal<AddressRanges, MergedStorage>();
    merged.assign(ranges.begin(), ranges.end());
    for (std::size_t at = 1; at < merged.size(); ++at) {
        const auto value = merged[at];
        std::size_t to = at;
        for (; to > 0 && merged[to - 1].first > value.first; --to) merged[to] = merged[to - 1];
        merged[to] = value;
    }
    std::size_t kept = 0;
    for (std::size_t at = 1; at < merged.size(); ++at) {
        if (merged[at].first <= merged[kept].second + SettleMergeGap) merged[kept].second = std::max(merged[kept].second, merged[at].second);
        else merged[++kept] = merged[at];
    }
    if (!merged.empty()) merged.resize(kept + 1);
    return merged;
}

Counter BufferRefusal(Graphics::ShaderResources::PreparedBuffers::Refusal refusal) {
    using Refusal = Graphics::ShaderResources::PreparedBuffers::Refusal;
    switch (refusal) {
    case Refusal::Inaccessible: return Counter::BuffersInaccessible;
    case Refusal::UploadLimit: return Counter::BuffersUploadLimit;
    case Refusal::Budget: return Counter::BuffersBudget;
    default: return Counter::BuffersClassification;
    }
}

void prepareBuffers(const LeanDrawJob& job, const DirectStages& resolved, DrawRecordHit& hit) {
    auto plan = hit.record->commit->bufferPlan.load(std::memory_order_acquire);
    if (plan == nullptr) {
        DirectDraws::Add(Counter::BuffersNoPlan);
        return;
    }
    auto& prepared = hit.preparedBuffers.emplace();
    prepared.generation = resolved.generation;
    const auto pending = Graphics::Recorder::PendingWriteSnapshot();
    std::optional<DrawPipeline::Watch> queued;
    if (DrawPipeline::Active()) queued = DrawPipeline::Queue0().WatchWrites();
    bool done = false;
    try {
        done = Graphics::ShaderResources::PrepareDirectBuffers(*plan, hit.stages, &job.shape->decode->state.color, prepared);
    } catch (const std::exception&) {
        prepared.refusal = Graphics::ShaderResources::PreparedBuffers::Refusal::Classification;
    }
    if (!done) {
        DirectDraws::Add(BufferRefusal(prepared.refusal));
        hit.preparedBuffers.reset();
        return;
    }
    struct ReadStorage {};
    auto& read = HostThreadLocal<AddressRanges, ReadStorage>();
    read.clear();
    for (const auto& item : prepared.selected) {
        if (item.begin != 0) read.emplace_back(item.begin, item.begin + item.bytes);
    }
    bool check = !read.empty();
    bool mergedRefused = false;
    if (check && read.size() > 2 && Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::MergedSettle)) {
        struct RecheckStorage {};
        auto& recheckOrdinal = HostThreadLocal<std::uint64_t, RecheckStorage>();
        const auto recheckEvery = DirectDraws::BufferVerifyEvery();
        const bool recheck = recheckEvery != 0 && ++recheckOrdinal % recheckEvery == 0;
        const bool separateRefused = recheck && !readsSettled(read, resolved, pending.get(), queued);
        const auto& merged = mergedRanges(read);
        DirectDraws::Add(Counter::SettleRangesBefore, read.size());
        DirectDraws::Add(Counter::SettleRangesAfter, merged.size());
        if (readsSettled(merged, resolved, pending.get(), queued)) {
            DirectDraws::Add(Counter::MergedSettlesPassed);
            if (recheck) DirectDraws::Add(Counter::MergedSettlesRechecked);
            if (separateRefused) {
                DirectDraws::Add(Counter::MergedSettlesRecheckRefused);
            } else {
                check = false;
                prepared.shortcuts |= Graphics::ShaderResources::PreparedBuffers::MergedSettle;
            }
        } else {
            mergedRefused = true;
            DirectDraws::Add(Counter::MergedSettlesRefused);
        }
    }
    if (check && !readsSettled(read, resolved, pending.get(), queued)) {
        DirectDraws::Add(Counter::BuffersUnsettled);
        hit.preparedBuffers.reset();
        return;
    }
    if (check && mergedRefused) DirectDraws::Add(Counter::MergedSettlesRecovered);
    prepared.plan = std::move(plan);
    DirectDraws::Add(Counter::BuffersPrepared);
    DirectDraws::Add(Counter::BuffersBlobBytes, prepared.blob.size());
    if (Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::BufferBlobAppend)) DirectDraws::Add(Counter::BlobAppends);
}

Counter InputRefusal(Graphics::PreparedInputs::Refusal refusal) {
    using Refusal = Graphics::PreparedInputs::Refusal;
    switch (refusal) {
    case Refusal::NoRecipe: return Counter::InputsNoRecipe;
    case Refusal::NotInPlace: return Counter::InputsNotInPlace;
    case Refusal::Inaccessible: return Counter::InputsInaccessible;
    case Refusal::Thrown: return Counter::InputsThrown;
    default: return Counter::InputsShape;
    }
}

void prepareInputs(const LeanDrawJob& job, const LeanAnchor& anchor, const DirectStages& resolved, DrawRecordHit& hit) {
    auto recipe = hit.record->commit->inputsRecipe.load(std::memory_order_acquire);
    if (recipe == nullptr) {
        DirectDraws::Add(Counter::InputsNoRecipe);
        return;
    }
    auto& prepared = hit.preparedInputs.emplace();
    const auto pending = Graphics::Recorder::PendingWriteSnapshot();
    std::optional<DrawPipeline::Watch> queued;
    if (DrawPipeline::Active()) queued = DrawPipeline::Queue0().WatchWrites();
    bool done = false;
    try {
        done = anchor.device->PrepareDrawInputs(job.shape->decode->state, hit.parameters, hit.stages, std::move(recipe), prepared);
    } catch (const std::exception&) {
        prepared.refusal = Graphics::PreparedInputs::Refusal::Thrown;
    }
    if (!done) {
        DirectDraws::Add(InputRefusal(prepared.refusal));
        hit.preparedInputs.reset();
        return;
    }
    struct ReadStorage {};
    auto& read = HostThreadLocal<AddressRanges, ReadStorage>();
    read.clear();
    if (hit.parameters.indexed) read.emplace_back(prepared.ranges.front().begin, prepared.ranges.front().end);
    if (!read.empty() && !readsSettled(read, resolved, pending.get(), queued)) {
        DirectDraws::Add(Counter::InputsUnsettled);
        hit.preparedInputs.reset();
        return;
    }
    DirectDraws::Add(Counter::InputsPrepared);
}

}

std::size_t DirectStageStore::KeyHash::operator()(const std::pair<std::uint64_t, std::uint64_t>& key) const {
    return static_cast<std::size_t>(key.first * 0x9e3779b97f4a7c15ull ^ key.second);
}

std::shared_ptr<const DirectStage> DirectStageStore::Find(std::uint64_t variant, std::uint64_t key) const {
    auto& slot = HostThreadLocal<StageFront, StageFrontStorage>().slots[mixedSlot(variant, key, FrontSlots)];
    if (slot.stage != nullptr && slot.variant == variant && slot.key == key && slot.clears == clears.load(std::memory_order_acquire)) return slot.stage;
    std::lock_guard lock(mutex);
    const auto found = stages.find({variant, key});
    if (found == stages.end()) return nullptr;
    slot = {variant, key, clears.load(std::memory_order_relaxed), found->second};
    return found->second;
}

std::shared_ptr<const DirectStage> DirectStageStore::Install(std::shared_ptr<const DirectStage> stage, std::uint64_t tag, bool& installed) {
    installed = false;
    bool overflowed = false;
    {
        std::lock_guard lock(mutex);
        if (!DirectStores::Get().Current(tag)) return stage;
        const std::pair key{stage->variant, stage->key};
        if (const auto found = stages.find(key); found != stages.end()) return found->second;
        if (stages.size() >= Bound) {
            DirectDraws::Add(Counter::StagesEvicted, stages.size());
            stages.clear();
            clears.fetch_add(1, std::memory_order_release);
            overflowed = true;
        }
        stages.emplace(key, stage);
        installed = true;
        DirectDraws::NoteStages(stages.size());
    }
    if (overflowed) DirectStores::Get().classes.Evict();
    return stage;
}

void DirectStageStore::Clear() {
    std::lock_guard lock(mutex);
    stages.clear();
    clears.fetch_add(1, std::memory_order_release);
    DirectDraws::NoteStages(0);
}

std::size_t DirectStageStore::Size() const {
    std::lock_guard lock(mutex);
    return stages.size();
}

DirectClassKey DirectClassKey::For(const LeanDrawJob& job, const DirectStages& resolved) {
    const auto& parameters = job.parameters;
    DirectClassKey key;
    key.shape = job.shapeKey;
    const auto layout = HashDrawRecordWords(resolved.vertexLayout, {});
    key.layout = layout[0] ^ (layout[1] * 0x9e3779b97f4a7c15ull);
    key.indexSize = parameters.indexSize;
    key.flags = parameters.flags;
    key.indexed = parameters.indexed;
    key.count = static_cast<std::uint32_t>(resolved.stages.size());
    for (std::size_t i = 0; i < resolved.stages.size(); ++i) key.stages[i] = resolved.stages[i].shared.get();
    if (!ExactClassKeys()) return key;
    key.indexAddress = parameters.indexAddress;
    key.indexCount = parameters.indexCount;
    key.instanceCount = parameters.instanceCount;
    key.firstVertex = parameters.firstVertex;
    key.firstInstance = parameters.firstInstance;
    return key;
}

std::uint64_t DirectClassKey::Probe() const {
    std::uint64_t probe = 0xcbf29ce484222325ull;
    const auto mix = [&](std::uint64_t value) { probe = (probe ^ value) * 0x100000001b3ull; };
    mix(shape);
    mix(layout);
    mix(indexAddress);
    mix((static_cast<std::uint64_t>(indexCount) << 32u) | indexSize);
    mix((static_cast<std::uint64_t>(instanceCount) << 32u) | flags);
    mix((static_cast<std::uint64_t>(firstVertex) << 32u) | firstInstance);
    mix((static_cast<std::uint64_t>(count) << 1u) | (indexed ? 1u : 0u));
    for (std::uint32_t i = 0; i < count && i < MaxStages; ++i) mix(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(stages[i])));
    return probe ^ (probe >> 29u);
}

std::size_t DirectClass::Bytes() const {
    return sizeof(DirectClass) + (record != nullptr ? record->Bytes() : 0);
}

std::shared_ptr<const DirectClass> DirectClassStore::Find(const DirectClassKey& key, std::uint64_t probe) const {
    auto& slot = HostThreadLocal<ClassFront, ClassFrontStorage>().slots[static_cast<std::size_t>(probe % FrontSlots)];
    if (slot.entry != nullptr && slot.probe == probe && slot.clears == clears.load(std::memory_order_acquire) && slot.entry->key == key) return slot.entry;
    std::lock_guard lock(mutex);
    const auto found = classes.find(key);
    if (found == classes.end()) return nullptr;
    slot = {probe, clears.load(std::memory_order_relaxed), found->second};
    return found->second;
}

std::shared_ptr<const DirectClass> DirectClassStore::Install(std::shared_ptr<const DirectClass> entry, std::uint64_t tag, bool& installed) {
    installed = false;
    std::lock_guard lock(mutex);
    if (!DirectStores::Get().Current(tag)) return entry;
    if (const auto found = classes.find(entry->key); found != classes.end()) return found->second;
    if (classes.size() >= Bound) {
        DirectDraws::Add(Counter::ClassesEvicted, kept);
        classes.clear();
        kept = 0;
        bytes = 0;
        clears.fetch_add(1, std::memory_order_release);
    }
    if (entry->record != nullptr) ++kept;
    bytes += entry->Bytes();
    classes.emplace(entry->key, entry);
    installed = true;
    note();
    return entry;
}

bool DirectClassStore::Refuse(const DirectClass& found, Counter reason) {
    auto refused = std::make_shared<DirectClass>();
    refused->key = found.key;
    refused->probe = found.probe;
    refused->stages = found.stages;
    refused->refusal = reason;
    std::lock_guard lock(mutex);
    const auto held = classes.find(found.key);
    if (held == classes.end() || held->second.get() != &found) return false;
    if (found.record != nullptr) --kept;
    bytes = bytes - found.Bytes() + refused->Bytes();
    held->second = std::move(refused);
    clears.fetch_add(1, std::memory_order_release);
    note();
    return true;
}

void DirectClassStore::Evict() {
    std::lock_guard lock(mutex);
    DirectDraws::Add(Counter::ClassesEvicted, kept);
    classes.clear();
    kept = 0;
    bytes = 0;
    clears.fetch_add(1, std::memory_order_release);
    note();
}

void DirectClassStore::Clear() {
    std::lock_guard lock(mutex);
    classes.clear();
    kept = 0;
    bytes = 0;
    clears.fetch_add(1, std::memory_order_release);
    note();
}

std::size_t DirectClassStore::Size() const {
    std::lock_guard lock(mutex);
    return classes.size();
}

void DirectClassStore::note() const {
    DirectDraws::NoteClasses(kept, bytes);
}

void RefuseDirectClass(const std::shared_ptr<const void>& owner) {
    if (owner == nullptr) return;
    const auto& found = *static_cast<const DirectClass*>(owner.get());
    DirectDraws::Add(Counter::CommitDemoted);
    if (DirectStores::Get().classes.Refuse(found, Counter::RefusedVerify)) DirectDraws::Add(Counter::RefusedVerify);
}

DirectStores& DirectStores::Get() {
    static auto* stores = new DirectStores;
    return *stores;
}

std::uint64_t DirectStores::Follow(std::uint64_t serial) {
    struct Seen {
        std::uint64_t generation = 0;
        std::uint64_t deviceSerial = 0;
        std::uint64_t epoch = 0;
    };
    struct SeenStorage {};
    auto& seen = HostThreadLocal<Seen, SeenStorage>();
    const auto generation = Graphics::RuntimeSwitches::Generation();
    const auto current = epoch.load(std::memory_order_acquire);
    if (seen.epoch == current && seen.generation == generation && seen.deviceSerial == serial) return current;
    std::lock_guard lock(mutex);
    auto live = Graphics::RuntimeSwitches::Snapshot();
    for (const auto id : {Graphics::RuntimeSwitch::DirectDraws, Graphics::RuntimeSwitch::DirectRetry, Graphics::RuntimeSwitch::PipelineDispatchMisses, Graphics::RuntimeSwitch::DirectPassTargets, Graphics::RuntimeSwitch::DirectBuffers, Graphics::RuntimeSwitch::DirectInputs, Graphics::RuntimeSwitch::FastSettle, Graphics::RuntimeSwitch::PreciseSettle, Graphics::RuntimeSwitch::ImportSnapshots, Graphics::RuntimeSwitch::MergedSettle, Graphics::RuntimeSwitch::UnlockedStamps, Graphics::RuntimeSwitch::WriteSnapshotFronts, Graphics::RuntimeSwitch::PendingFronts, Graphics::RuntimeSwitch::BufferBlobAppend, Graphics::RuntimeSwitch::PreparedCapacity, Graphics::RuntimeSwitch::InvocationSlotsWide}) live.Assign(id, false);
    if (!tagged || deviceSerial != serial || !(live == switches)) {
        tagged = true;
        deviceSerial = serial;
        switches = live;
        epoch.fetch_add(1, std::memory_order_acq_rel);
        stages.Clear();
        classes.Clear();
    }
    seen = {generation, serial, epoch.load(std::memory_order_relaxed)};
    return seen.epoch;
}

void DirectStepClock::Lap(Step step) {
    const auto now = std::chrono::steady_clock::now();
    const auto index = static_cast<std::size_t>(step);
    nanoseconds[index] += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - mark).count());
    reached[index] = true;
    mark = now;
}

void DirectStepClock::Report() const {
    for (std::size_t index = 0; index < nanoseconds.size(); ++index) {
        if (reached[index]) DirectDraws::AddStep(static_cast<Step>(index), nanoseconds[index]);
    }
}

namespace {

struct RefreshedDecode {
    std::shared_ptr<const DrawDecode> source;
    std::optional<DrawDecode> decode;
};
struct RefreshedDecodeStorage {};

std::shared_ptr<const DrawDecode> refreshedDecode(const std::shared_ptr<const DrawDecode>& from, const DrawUserWords& words) {
    auto& scratch = HostThreadLocal<RefreshedDecode, RefreshedDecodeStorage>();
    if (scratch.source != from) {
        if (scratch.decode.has_value()) *scratch.decode = *from;
        else scratch.decode.emplace(*from);
        scratch.source = from;
    }
    for (auto& program : scratch.decode->programs) {
        ReadUserWordsFrom(words, program);
        if (program.merged) InitializeMergedFrom(words, program, program.mergedPointer, program.mergedPointerRequired);
    }
    return std::shared_ptr<const DrawDecode>(std::shared_ptr<void>(), &*scratch.decode);
}

}

std::optional<Counter> Driver::resolveDirectStages(const LeanDrawJob& job, const LeanAnchor& anchor, DirectStages& resolved, DirectStepClock* clock) {
    DrawPipeline::FollowEpoch(job.epoch);
    DrawPipeline::Active() = true;
    GuestMemory::SetCurrentPacket(job.opcode, job.queue);
    if (clock != nullptr) clock->Start();
    const auto lap = [&](Step step) {
        if (clock != nullptr) clock->Lap(step);
    };
    if (!DirectShape(job)) return Counter::FallbackShape;
    auto& stores = DirectStores::Get();
    resolved.tag = stores.Follow(job.deviceSerial);
    const auto& shape = *job.shape;
    const auto& device = *anchor.device;
    resolved.decode = Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::LeanPrepare) ? refreshedDecode(shape.decode, job.words) : LeanRefreshedDecode(*shape.decode, job.words);
    const auto& decode = *resolved.decode;
    const auto& graphics = decode.state;
    const auto& programs = decode.programs;
    const auto& roles = decode.roles;
    const auto count = programs.size();
    resolved.memory.clear();
    AppendProgramMemory(resolved.memory, programs);
    resolved.linked.clear();
    for (std::size_t i = 0; i < count; ++i) {
        const auto& program = programs[i];
        resolved.linked.push_back({roles[i], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
    }
    resolved.vertexInfos.assign(count, std::nullopt);
    resolved.decodeReads.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& program = programs[i];
        auto& reads = resolved.decodeReads[i];
        reads.clear();
        if (program.binary.stage == Stage::Fragment) continue;
        auto& info = resolved.vertexInfos[i];
        info = DecodeOrderedVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, reads, 8u - program.firstUserSgpr);
        info->paClVsOutCntl = PositionExportLayout(program.snapshot.get(), program.codeOffset, program.binary.code, shape.reg207, program.tailContinuation.get()) | graphics.layerExports | graphics.viewportExports;
        info->depthRemapMode = graphics.depthRemap.mode;
        Graphics::ApplyPointSize(graphics, *info);
    }
    resolved.before = Graphics::Recorder::PendingWriteSnapshot();
    resolved.generation = GuestMemory::TrackerGeneration();
    resolved.stages.resize(count);
    resolved.regions.clear();
    lap(Step::Request);
    auto& cache = HostThreadLocal<InvocationCache, InvocationCacheStorage>();
    auto& reads = HostThreadLocal<DirectReads, DirectReadsStorage>();
    const bool libraries = device.GraphicsPipelineLibraries();
    const bool hasFragment = std::ranges::any_of(programs, [](const DrawProgram& value) { return value.binary.stage == Stage::Fragment; });
    const ShaderRecompiler::RecompileResult* previous = nullptr;
    std::uint32_t cursor = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& program = programs[i];
        auto& stage = resolved.stages[i];
        cursor = Graphics::StagePushOffset(cursor, program.binary.stage, libraries);
        stage.program = i;
        stage.stage = program.binary.stage;
        stage.requestOffset = cursor;
        stage.pushOffset = 0;
        stage.shared.reset();
        stage.capture.reset();
        const auto pixel = program.binary.stage == Stage::Fragment && previous != nullptr ? Graphics::DefaultUnexportedInputs(decode.pixel, previous->parameterExports) : decode.pixel;
        const auto request = [&] { return drawStageRequest(program, graphics, pixel, resolved.vertexInfos[i], resolved.memory, resolved.linked, device, cursor, job.parameters, std::nullopt, hasFragment); };
        auto built = request();
        lap(Step::Request);

        std::optional<ShaderRecompiler::PreparedShaderInvocation> invocation;
        InvocationSlot* slot = nullptr;
        try {
            if (InvocationCacheable(program, built)) slot = &cache.For(program);
            if (slot != nullptr && slot->handle != nullptr && slot->snapshot.get() == program.snapshot.get() && slot->codeOffset == program.codeOffset) {
                built.context.convertedSamples = slot->convertedSamples;
                built.context.layerRowSamples = slot->layerRowSamples;
                ShaderRecompiler::BuildPreparedShaderKey(built, cache.key);
                built.shader.code = ShaderRecompiler::GetPreparedCode(*slot->handle);
                invocation = ShaderRecompiler::PreparedShaderInvocation::TryCreate(built, slot->handle, cache.key);
            }
            if (!invocation) {
                const auto fresh = request();
                invocation.emplace(InvocationFor(*program.snapshot, program.codeOffset, fresh));
                DirectDraws::Add(Counter::InvocationsRebuilt);
                if (slot != nullptr) {
                    const auto& context = invocation->Request().context;
                    *slot = {program.snapshot, program.codeOffset, invocation->Handle(), context.convertedSamples, context.layerRowSamples};
                }
            }
        } catch (const std::exception&) {
            if (slot != nullptr) slot->handle.reset();
            return Counter::FallbackInvocation;
        }
        lap(Step::Invocation);

        reads.Reset();
        try {
            stage.capture = walkDirect(*invocation, reads);
        } catch (const ShaderRecompiler::ConvertedSampleRequired&) {
            if (slot != nullptr) slot->handle.reset();
            return Counter::FallbackSampleRetry;
        } catch (const ShaderRecompiler::LayerRowSamplesRequired&) {
            if (slot != nullptr) slot->handle.reset();
            return Counter::FallbackSampleRetry;
        } catch (const std::exception&) {
            return Counter::FallbackWalk;
        }
        if (reads.failed || stage.capture == nullptr) return Counter::FallbackWalk;
        const auto& snapshot = stage.capture->snapshot;
        if (imageTables(snapshot)) return Counter::FallbackTables;
        if (!readRanges(reads, stage.ranges, stage.values)) return Counter::FallbackTornReads;
        if (!CaptureRegisteredViewsCurrent(snapshot.registeredImages)) return Counter::FallbackViews;
        if (!RangesSettled(stage.ranges, resolved.before.get(), resolved.generation)) {
            noteUnsettled(stage.ranges, resolved.before.get());
            return Counter::FallbackUnsettled;
        }
        GuardRanges(stage.ranges);
        lap(Step::Walk);

        const auto key = invocation->SharedKey(*stage.capture);
        if (!key) return Counter::FallbackWalk;
        const auto variant = invocation->VariantId();
        stage.shared = stores.stages.Find(variant, *key);
        if (stage.shared == nullptr) {
            ShaderRecompiler::SharedStage shared;
            try {
                shared = invocation->ShareStage(*stage.capture);
            } catch (const std::exception&) {
                return Counter::FallbackNoPatch;
            }
            if (!shared) return Counter::FallbackNoPatch;
            auto made = std::make_shared<DirectStage>();
            made->variant = variant;
            made->key = *key;
            made->addressed = std::ranges::any_of(shared.donor->bindings, [](const ShaderRecompiler::DescriptorBinding& binding) { return binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable; });
            made->donor = std::move(shared.donor);
            made->patch = std::move(shared.patch);
            bool installed = false;
            stage.shared = stores.stages.Install(std::move(made), resolved.tag, installed);
            if (installed) DirectDraws::Add(Counter::StagesBuilt);
        }
        const auto& donor = *stage.shared->donor;
        if (stage.shared->addressed) return Counter::FallbackAddressed;
        if (snapshot.srtPoison.size() + snapshot.nullRootReads != donor.poisonedSrtReads) return Counter::FallbackPoisoned;
        lap(Step::Key);

        if (!invocation->FillSharedSide(*stage.capture, donor, *stage.shared->patch, stage.side)) return Counter::FallbackSide;
        const auto pushBudget = Graphics::StagePushBudget(Graphics::StagePushLimit(graphics.stages, cursor, graphics.depthRemap.mode != 0u), cursor, program.binary.stage, libraries, hasFragment);
        if (donor.pushConstants.size() > pushBudget) return Counter::FallbackPush;
        stage.pushOffset = donor.pushConstants.empty() ? 0u : cursor;
        cursor += static_cast<std::uint32_t>(donor.pushConstants.size());
        previous = &donor;
        lap(Step::Side);
    }
    DrawVertexLayout(resolved.vertexInfos, resolved.vertexLayout, resolved.vertexInputs);
    auto& regions = resolved.regions;
    regions.assign(resolved.memory.begin(), resolved.memory.end());
    for (const auto& decodeReads : resolved.decodeReads) {
        for (const auto& read : decodeReads) regions.push_back({read.address, read.Bytes()});
    }
    for (const auto& stage : resolved.stages) {
        std::size_t at = 0;
        for (const auto& [begin, end] : stage.ranges) {
            const auto words = static_cast<std::size_t>((end - begin) / sizeof(std::uint32_t));
            regions.push_back({begin, std::as_bytes(std::span(stage.values.data() + at, words))});
            at += words;
        }
    }
    lap(Step::Checks);
    return std::nullopt;
}

std::optional<Counter> Driver::assembleDirectHit(const LeanDrawJob& job, DirectStages& resolved, DrawRecordHit& hit) {
    const auto count = resolved.stages.size();
    hit.sides.resize(count);
    hit.captures.resize(count);
    hit.stages.clear();
    hit.stages.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto& stage = resolved.stages[i];
        hit.sides[i] = std::move(stage.side);
        hit.captures[i] = stage.capture;
        hit.stages.push_back({stage.stage, stage.shared->donor.get(), stage.pushOffset, &hit.captures[i]->snapshot.registeredImages, &hit.sides[i]});
    }
    hit.parameters = job.parameters;
    try {
        FoldDrawOffsets(*resolved.stages.front().shared->donor, resolved.decode->programs.front(), hit.parameters);
    } catch (const std::exception&) {
        return Counter::FallbackOverflow;
    }
    hit.reads = QueuedDrawReadRanges(hit.parameters, hit.stages);
    runCaptureCheckHook();
    if (capturedMemoryChanged(resolved.regions)) return Counter::FallbackMemoryChanged;
    if (recordQueuedLabelsAfterCapture(job.queue, resolved.regions)) return Counter::FallbackLabels;
    return std::nullopt;
}

const DrawRecordSink* Driver::finishDirectAnswer(const LeanDrawJob& job, const LeanAnchor& anchor, const DirectStages& resolved, const DrawRecordHit& hit, DrawRecordView& answer) {
    bool answered = false;
    try {
        answer.Capture(hit.stages, hit.parameters);
        answer.reads = hit.reads;
        answer.contentKey = anchor.device->DrawContentKey(resolved.decode->state, hit.parameters, hit.stages);
        answered = true;
    } catch (const std::exception&) {
    }
    struct SinkStorage {};
    auto& sink = HostThreadLocal<DrawRecordSink, SinkStorage>();
    sink.Reset();
    sink.verify = true;
    struct SinkScope {
        ~SinkScope() { DrawRecordSinkSlot::Set(nullptr); }
    } scope;
    if (answered) DrawRecordSinkSlot::Set(&sink);
    finishLeanDraw(job, anchor);
    DrawRecordSinkSlot::Set(nullptr);
    return answered ? &sink : nullptr;
}

void Driver::buildDirectClass(const LeanDrawJob& job, const LeanAnchor& anchor, const DirectClassKey& key, std::uint64_t probe, const DirectStages& resolved, const DrawRecordHit& hit) {
    DrawRecordView answer;
    const auto* today = finishDirectAnswer(job, anchor, resolved, hit, answer);
    if (today == nullptr) return;
    const auto& sink = *today;
    auto entry = std::make_shared<DirectClass>();
    entry->key = key;
    entry->probe = probe;
    for (std::size_t i = 0; i < resolved.stages.size(); ++i) entry->stages[i] = resolved.stages[i].shared;
    if (!sink.queued) {
        entry->refusal = Counter::RefusedNotQueued;
    } else if (sink.writes != job.shape->writes) {
        entry->refusal = Counter::RefusedWrites;
    } else {
        auto record = DrawRecord::Build(DrawRecordKey::For(job.shapeKey, job.parameters), job.shape->shared_from_this(), job.deviceSerial, Graphics::Recorder::Presents(), sink);
        record->contentKey.clear();
        record->vertexLayout = resolved.vertexLayout;
        answer.writes = record->writes;
        answer.fragmentOutputs = record->fragmentOutputs;
        if (compareRecordAnswer(job, *record, answer, sink, &DirectDraws::NoteBuildDifference, &DirectDraws::NoteAllowed)) entry->refusal = Counter::RefusedDiffers;
        else entry->record = std::move(record);
    }
    const auto outcome = entry->record != nullptr ? Counter::ClassesBuilt : entry->refusal;
    bool installed = false;
    DirectStores::Get().classes.Install(std::move(entry), resolved.tag, installed);
    if (installed) DirectDraws::Add(outcome);
}

void Driver::verifyDirectHit(const LeanDrawJob& job, const LeanAnchor& anchor, const DirectClass& found, const DirectStages& resolved, const DrawRecordHit& hit) {
    DrawRecordView answer;
    const auto* today = finishDirectAnswer(job, anchor, resolved, hit, answer);
    if (today == nullptr) return;
    DirectDraws::Add(Counter::VerifyChecked);
    DrawRecords::NoteChecked(DrawRecords::Item::Answer);
    const auto& record = *found.record;
    answer.writes = record.writes;
    answer.fragmentOutputs = record.fragmentOutputs;
    if (!compareRecordAnswer(job, record, answer, *today, &DirectDraws::NoteDifference, &DirectDraws::NoteAllowed)) return;
    DirectDraws::Add(Counter::VerifyDiffering);
    if (DirectStores::Get().classes.Refuse(found, Counter::RefusedVerify)) DirectDraws::Add(Counter::RefusedVerify);
}

void Driver::finishDirectDraw(const LeanDrawJob& job) {
    const auto& anchor = leanDraws.AnchorAt(job.anchor);
    DirectDraws::Add(Counter::Draws);
    struct ResolvedStorage {};
    auto& resolved = HostThreadLocal<DirectStages, ResolvedStorage>();
    struct SampleStorage {};
    auto& sampled = HostThreadLocal<std::uint64_t, SampleStorage>();
    std::optional<DirectStepClock> clock;
    if (sampled++ % DirectDraws::StepSampleEvery == 0) clock.emplace();
    auto* timer = clock.has_value() ? &*clock : nullptr;
    const auto lap = [&](Step step) {
        if (timer != nullptr) timer->Lap(step);
    };
    std::optional<Counter> reason;
    DirectClassKey key;
    std::uint64_t probe = 0;
    std::shared_ptr<const DirectClass> found;
    std::shared_ptr<DrawRecordHit> hit;
    try {
        reason = resolveDirectStages(job, anchor, resolved, timer);
        for (std::uint32_t again = 0; again < 2u && reason == Counter::FallbackUnsettled && Graphics::RuntimeSwitches::Set(Graphics::RuntimeSwitch::DirectRetry); ++again) {
            DirectDraws::Add(Counter::WalksRetried);
            reason = resolveDirectStages(job, anchor, resolved, timer);
        }
        if (!reason) {
            key = DirectClassKey::For(job, resolved);
            probe = key.Probe();
            found = DirectStores::Get().classes.Find(key, probe);
            if (found != nullptr && found->record == nullptr) reason = Counter::FallbackClassRefused;
            else if (found != nullptr && found->record->vertexLayout != resolved.vertexLayout) found = nullptr;
            lap(Step::Class);
        }
        if (!reason) {
            hit = std::make_shared<DrawRecordHit>();
            reason = assembleDirectHit(job, resolved, *hit);
            lap(Step::Checks);
        }
        if (!reason && found == nullptr) reason = Counter::FallbackClassNew;
        else if (!reason && (job.recordFlags & DrawRecordFlags::VerifyDue) != 0) reason = Counter::FallbackVerify;
    } catch (const std::exception&) {
        reason = Counter::FallbackWalk;
    }
    if (!reason) {
        hit->record = found->record;
        hit->owner = found;
        hit->direct = true;
        if ((job.recordFlags & DrawRecordFlags::DirectBuffers) != 0) {
            prepareBuffers(job, resolved, *hit);
            lap(Step::Buffers);
        }
        if ((job.recordFlags & DrawRecordFlags::DirectInputs) != 0) {
            prepareInputs(job, anchor, resolved, *hit);
            lap(Step::Inputs);
        }
        commitRecordDraw(job, anchor, std::move(hit));
        lap(Step::HandOff);
        if (timer != nullptr) timer->Report();
        DirectDraws::Add(Counter::Direct);
        return;
    }
    if (timer != nullptr) timer->Report();
    DirectDraws::Add(*reason);
    if (*reason == Counter::FallbackClassNew) {
        buildDirectClass(job, anchor, key, probe, resolved, *hit);
        return;
    }
    if (*reason == Counter::FallbackVerify) {
        verifyDirectHit(job, anchor, *found, resolved, *hit);
        return;
    }
    finishLeanDraw(job, anchor);
}

}

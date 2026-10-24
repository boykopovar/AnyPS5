#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace AgcDriver::DriverDetail {

namespace {

std::atomic<std::uint64_t>& heapStages() {
    static std::atomic<std::uint64_t> count{0};
    return count;
}

const char* resultDifference(const ShaderRecompiler::RecompileResult& left, const ShaderRecompiler::RecompileResult& right,
                             std::size_t& index) {
    index = 0;
    if (left.PipelineVariantId() != right.PipelineVariantId()) return "variant";
    if (left.pushConstants != right.pushConstants) return "push constants";
    if (left.bindings.size() != right.bindings.size()) return "binding count";
    if (left.specialization.size() != right.specialization.size()) return "specialization count";
    for (index = 0; index < left.bindings.size(); ++index) {
        const auto& a = left.bindings[index];
        const auto& b = right.bindings[index];
        if (a.role != b.role || a.kind != b.kind || a.binding != b.binding || a.count != b.count) return "binding layout";
        if (a.guestDescriptor != b.guestDescriptor) return "binding words";
    }
    for (index = 0; index < left.specialization.size(); ++index) {
        if (left.specialization[index].id != right.specialization[index].id ||
            left.specialization[index].value != right.specialization[index].value)
            return "specialization";
    }
    index = 0;
    if (left.vertexAttributes.size() != right.vertexAttributes.size()) return "attribute count";
    for (index = 0; index < left.vertexAttributes.size(); ++index) {
        const auto& a = left.vertexAttributes[index];
        const auto& b = right.vertexAttributes[index];
        const bool sameAttribute = a.location == b.location && a.components == b.components && a.resource.fields == b.resource.fields &&
                                   a.fetchIndex == b.fetchIndex && a.formatComponents == b.formatComponents;
        if (!sameAttribute) return "attribute";
    }
    return nullptr;
}

bool sameResult(const ShaderRecompiler::RecompileResult& left, const ShaderRecompiler::RecompileResult& right) {
    std::size_t index = 0;
    return resultDifference(left, right, index) == nullptr;
}

void reportHeapMismatch(const char* check, const char* what, const ShaderRecompiler::RecompileResult& result, std::size_t index) {
    static std::atomic<std::uint32_t> printed{0};
    if (printed.fetch_add(1, std::memory_order_relaxed) >= 64u) return;
    const int role = index < result.bindings.size() ? static_cast<int>(result.bindings[index].role) : -1;
    const int kind = index < result.bindings.size() ? static_cast<int>(result.bindings[index].kind) : -1;
    std::fprintf(stderr, "[heap-verify] %s: %s differ at %zu (role %d kind %d, template %llu, variant 0x%llx)\n", check, what, index, role,
                 kind, static_cast<unsigned long long>(result.heapTemplateId), static_cast<unsigned long long>(result.PipelineVariantId()));
    std::fflush(stderr);
}

struct VerifyCounts {
    std::atomic<std::uint64_t> verified{0};
    std::atomic<std::uint64_t> differ{0};
    std::atomic<std::uint64_t> notComparable{0};
    std::atomic<std::uint64_t> accessor{0};
    std::atomic<std::uint64_t> accessorDiffer{0};
    std::atomic<std::uint64_t> ring{0};
    std::atomic<std::uint64_t> ringDiffer{0};
};

unsigned long long loaded(const std::atomic<std::uint64_t>& counter) {
    return static_cast<unsigned long long>(counter.load());
}

void reportVerifyCounts(const VerifyCounts& c) {
    if (c.verified.load() != 0u || c.notComparable.load() != 0u) {
        std::fprintf(
            stderr,
            "[heap-verify] %llu heap stages compared with a full capture, %llu differed, %llu not comparable (the full capture could not read every descriptor)\n",
            loaded(c.verified), loaded(c.differ), loaded(c.notComparable));
    }
    if (c.accessor.load() != 0u || c.ring.load() != 0u) {
        std::fprintf(stderr, "[heap-verify] %llu accessor checks, %llu differed; %llu ring hits checked, %llu differed\n",
                     loaded(c.accessor), loaded(c.accessorDiffer), loaded(c.ring), loaded(c.ringDiffer));
    }
}

VerifyCounts& verifyCounts() {
    static VerifyCounts counts;
    static std::once_flag registered;
    std::call_once(registered, [] { std::atexit([] { reportVerifyCounts(verifyCounts()); }); });
    return counts;
}

}

bool HeapRefused() {
    return Graphics::BindlessHeap::NearlyFull();
}

namespace {

bool& heapDrawOff() {
    struct DrawOffStorage {};
    return HostThreadLocal<bool, DrawOffStorage>();
}

}

HeapDrawOff::HeapDrawOff() : previous(heapDrawOff()) {
    heapDrawOff() = true;
}

HeapDrawOff::~HeapDrawOff() {
    heapDrawOff() = previous;
}

bool HeapFragmentsEnabled() {
    return Graphics::DescriptorHeapEnabled() && !HeapRefused() && !heapDrawOff();
}

bool HeapPreRasterStage(ShaderRecompiler::ShaderStage stage) {
    using Stage = ShaderRecompiler::ShaderStage;
    return stage == Stage::Vertex || stage == Stage::Local || stage == Stage::TessellationControl ||
           stage == Stage::TessellationEvaluation || stage == Stage::Mesh;
}

Graphics::HeapStageReason HeapStageIneligibility(ShaderRecompiler::ShaderStage stage, const ShaderRecompiler::IrResourcePlan* plan,
                                                 bool heapVertex) {
    using Reason = Graphics::HeapStageReason;
    if (stage != ShaderRecompiler::ShaderStage::Fragment && !HeapPreRasterStage(stage)) return Reason::StageKind;
    if (plan == nullptr) return Reason::NoPlan;
    if (stage != ShaderRecompiler::ShaderStage::Fragment && !heapVertex) return Reason::FragmentOffHeap;
    if (plan->stage == ShaderRecompiler::IrShaderStage::Unknown || plan->stage == ShaderRecompiler::IrShaderStage::Fetch)
        return Reason::PlanStage;
    if (!ShaderRecompiler::HeapFragmentEligible(*plan)) return Reason::UnresolvedImageSource;
    return Reason::Heap;
}

bool HeapStageEligible(ShaderRecompiler::ShaderStage stage, const ShaderRecompiler::IrResourcePlan* plan, bool heapVertex) {
    return HeapStageIneligibility(stage, plan, heapVertex) == Graphics::HeapStageReason::Heap;
}

namespace {

std::unordered_set<std::uint64_t>& refusedPairs() {
    struct PairsStorage {};
    return HostThreadLocal<std::unordered_set<std::uint64_t>, PairsStorage>();
}

}

bool HeapPairRefused(std::uint64_t pair) {
    return refusedPairs().contains(pair);
}

void RefuseHeapPair(std::uint64_t pair) {
    auto& refused = refusedPairs();
    if (refused.size() >= 65536u) refused.clear();
    refused.insert(pair);
}

bool HeapStageMappable(const ShaderRecompiler::RecompileResult& result, bool workgroupAddress) {
    struct MappableStorage {};
    auto& known = HostThreadLocal<std::unordered_map<std::uint64_t, bool>, MappableStorage>();
    const auto key = result.variantId * 2u + (workgroupAddress ? 1u : 0u);
    if (const auto found = known.find(key); found != known.end()) return found->second;
    if (known.size() >= 65536u) known.clear();
    const bool mappable = Graphics::DescriptorHeapMappable(std::span<const std::uint32_t>(result.spirv.data(), result.spirv.size()),
                                                           workgroupAddress, result.dataAddressPush, result.functionLdsBuffer);
    known.emplace(key, mappable);
    return mappable;
}

namespace {

struct HeapFragmentKey {
    const void* snapshot;
    std::size_t codeOffset;
    bool operator==(const HeapFragmentKey&) const = default;
};

struct HeapFragmentKeyHash {
    std::size_t operator()(const HeapFragmentKey& key) const {
        return std::hash<const void*>{}(key.snapshot) ^ (key.codeOffset * 0x9e3779b97f4a7c15ull);
    }
};

std::unordered_map<HeapFragmentKey, bool, HeapFragmentKeyHash>& heapFragments() {
    struct FragmentsStorage {};
    auto& value = HostThreadLocal<std::unordered_map<HeapFragmentKey, bool, HeapFragmentKeyHash>, FragmentsStorage>();
    return value;
}

}

std::optional<bool> HeapFragmentKnown(const void* snapshot, std::size_t codeOffset) {
    const auto& known = heapFragments();
    const auto found = known.find({snapshot, codeOffset});
    return found == known.end() ? std::nullopt : std::optional(found->second);
}

void NoteHeapFragment(const void* snapshot, std::size_t codeOffset, bool eligible) {
    heapFragments()[{snapshot, codeOffset}] = eligible;
}

bool SampleHeapStageVerify() {
    static const std::uint64_t every = [] {
        const char* text = std::getenv("APS5_VERIFY_GPU_HEAP");
        if (text == nullptr) return std::uint64_t{0};
        const auto value = std::strtoull(text, nullptr, 10);
        return value == 0 ? std::uint64_t{1} : static_cast<std::uint64_t>(value);
    }();
    if (every == 0) return false;
    thread_local std::uint64_t calls = 0;
    return calls++ % every == 0;
}

namespace {

struct HeapWalkEntry {
    std::uint64_t variant = 0;
    std::uint64_t shaderBase = 0;
    std::uint32_t pushOffset = 0;
    std::vector<std::uint32_t> userData;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    std::vector<std::uint32_t> values;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> accessible;
    ShaderRecompiler::SrtUserDataUse userDataUse;
    std::shared_ptr<const ShaderRecompiler::ResourceCapture> capture;
    std::shared_ptr<const ShaderRecompiler::RecompileResult> result;
    std::shared_ptr<const ShaderRecompiler::HeapStageWords> words;
};

constexpr std::size_t HeapWalkWays = 4;

constexpr std::size_t HeapWalkMemoKeys = 16384;

constexpr std::size_t HeapWalkDrain = 2;

constexpr std::size_t HeapWalkSeenKeys = 1u << 16;

struct HeapWalkMemo {
    std::unordered_map<std::uint64_t, std::vector<HeapWalkEntry>> current;
    std::unordered_map<std::uint64_t, std::vector<HeapWalkEntry>> previous;
    std::vector<std::uint64_t> seen = std::vector<std::uint64_t>(HeapWalkSeenKeys, 0);

    bool admit(std::uint64_t key) {
        auto& last = seen[(key ^ (key >> 29)) & (HeapWalkSeenKeys - 1u)];
        if (last == key) return true;
        last = key;
        return false;
    }

    std::vector<HeapWalkEntry>* find(std::uint64_t key) {
        if (const auto found = current.find(key); found != current.end()) return &found->second;
        const auto old = previous.find(key);
        if (old == previous.end()) return nullptr;
        auto moved = std::move(old->second);
        previous.erase(old);
        auto& ways = slot(key);
        ways = std::move(moved);
        return &ways;
    }

    std::vector<HeapWalkEntry>& slot(std::uint64_t key) {
        for (std::size_t drained = 0; drained < HeapWalkDrain && !previous.empty(); ++drained) previous.erase(previous.begin());
        if (current.size() >= HeapWalkMemoKeys && !current.contains(key)) {
            previous = std::move(current);
            current.clear();
        }
        return current[key];
    }
};

HeapWalkMemo& heapWalkMemo() {
    struct WalkMemoStorage {};
    auto& memo = HostThreadLocal<HeapWalkMemo, WalkMemoStorage>();
    return memo;
}

}

namespace {

using AddressRanges = std::vector<std::pair<std::uint64_t, std::uint64_t>>;

struct WalkReads {
    std::vector<std::uint64_t> words;
    std::uint64_t page = ~std::uint64_t{0};
    AddressRanges accessible;
    bool failed = false;

    void Reset() {
        words.clear();
        accessible.clear();
        failed = false;
        page = ~std::uint64_t{0};
    }
};

bool readWalkWord(void* context, std::uint64_t address, std::uint32_t* value) {
    auto& reads = *static_cast<WalkReads*>(context);
    const auto page = address & ~std::uint64_t{0xfff};
    const bool crossesPage = (address & 0xfffu) > 0x1000u - sizeof(*value);
    if (page != reads.page || crossesPage) {
        if (GuestMemory::Accessible(reinterpret_cast<const void*>(page), 0x1000u)) reads.page = page;
        else if (!GuestMemory::Accessible(reinterpret_cast<const void*>(address), sizeof(*value))) {
            reads.failed = true;
            return false;
        }
    }
    std::memcpy(value, reinterpret_cast<const void*>(address), sizeof(*value));
    reads.words.push_back(address);
    return true;
}

bool walkAccessible(void* context, std::uint64_t address, std::uint64_t bytes) {
    auto& reads = *static_cast<WalkReads*>(context);
    const bool accessible = GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes));
    if (accessible) reads.accessible.emplace_back(address, bytes);
    else reads.failed = true;
    return accessible;
}

void coalesceWords(std::vector<std::uint64_t>& words, AddressRanges& ranges) {
    std::sort(words.begin(), words.end());
    ranges.clear();
    for (const auto address : words) {
        if (!ranges.empty() && address <= ranges.back().second) {
            ranges.back().second = std::max(ranges.back().second, address + 4u);
            continue;
        }
        ranges.emplace_back(address, address + 4u);
    }
}

void copyRangeValues(const AddressRanges& ranges, std::vector<std::uint32_t>& values) {
    values.clear();
    for (const auto& [begin, end] : ranges) {
        const auto offset = values.size();
        values.resize(offset + static_cast<std::size_t>((end - begin) / 4u));
        std::memcpy(values.data() + offset, reinterpret_cast<const void*>(begin), static_cast<std::size_t>(end - begin));
    }
}

bool rangesSettled(const AddressRanges& ranges, const Graphics::Recorder::WriteRanges* before, std::uint64_t generation) {
    const bool queuedWrite = DrawPipeline::Active() && DrawPipeline::Queue0().Busy() && DrawPipeline::Queue0().OverlapsAny(ranges);
    if (queuedWrite || !Graphics::Recorder::ReadsSettled(ranges, before)) return false;
    for (const auto& [begin, end] : ranges) {
        if (GuestMemory::NewestStamp(begin, static_cast<std::size_t>(end - begin)) > generation) return false;
    }
    return true;
}

void guardRanges(const AddressRanges& ranges) {
    for (const auto& [begin, end] : ranges) GuestMemory::GuardRead(begin, static_cast<std::size_t>(end - begin));
}

std::uint64_t walkMemoKey(const ShaderRecompiler::PreparedShaderInvocation& invocation, std::span<const std::uint32_t> userData,
                          std::uint64_t shaderBase) {
    auto key = (invocation.VariantId() * 0x9e3779b97f4a7c15ull ^ shaderBase) + invocation.Request().layout.pushConstantOffsetBytes;
    for (const auto word : userData) key = (key ^ word) * 0x100000001b3ull;
    return key;
}

bool sameWalkInputs(const HeapWalkEntry& entry, const ShaderRecompiler::PreparedShaderInvocation& invocation,
                    std::span<const std::uint32_t> userData, std::uint64_t shaderBase) {
    const bool sameStage = entry.variant == invocation.VariantId() && entry.shaderBase == shaderBase &&
                           entry.pushOffset == invocation.Request().layout.pushConstantOffsetBytes;
    if (!sameStage || !std::equal(entry.userData.begin(), entry.userData.end(), userData.begin(), userData.end())) return false;
    std::size_t cursor = 0;
    for (const auto& [begin, end] : entry.ranges) {
        const auto count = static_cast<std::size_t>((end - begin) / 4u);
        if (GuestMemory::CompareMapped(begin, std::as_bytes(std::span(entry.values.data() + cursor, count))) != GuestMemory::Compare::Equal)
            return false;
        cursor += count;
    }
    for (const auto& [address, bytes] : entry.accessible) {
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes))) return false;
    }
    return true;
}

void rememberWalk(std::uint64_t memoKey, const ShaderRecompiler::PreparedShaderInvocation& invocation,
                  std::span<const std::uint32_t> userData, std::uint64_t shaderBase, const AddressRanges& ranges,
                  const std::vector<std::uint32_t>& values, const AddressRanges& accessible,
                  const std::shared_ptr<const ShaderRecompiler::ResourceCapture>& capture,
                  const std::shared_ptr<const ShaderRecompiler::RecompileResult>& result, const ShaderRecompiler::HeapStageWords& words) {
    auto& ways = heapWalkMemo().slot(memoKey);
    if (ways.size() >= HeapWalkWays) ways.pop_back();
    ways.emplace(ways.begin());
    auto& entry = ways.front();
    entry.variant = invocation.VariantId();
    entry.pushOffset = invocation.Request().layout.pushConstantOffsetBytes;
    entry.shaderBase = shaderBase;
    entry.userData.assign(userData.begin(), userData.end());
    entry.ranges = ranges;
    entry.values = values;
    entry.accessible = accessible;
    entry.userDataUse = capture->userDataUse;
    entry.capture = capture;
    entry.result = result;
    entry.words = std::make_shared<const ShaderRecompiler::HeapStageWords>(words);
}

}

std::shared_ptr<const ShaderRecompiler::RecompileResult>
MaterializeHeapStage(const ShaderRecompiler::PreparedShaderInvocation& invocation, std::span<const std::uint32_t> userData,
                     std::uint64_t shaderBase, ShaderRecompiler::HeapStageWords* words,
                     std::shared_ptr<const ShaderRecompiler::HeapStageWords>* sharedWords, HeapStageReads* stageReads) {
    struct ReadsStorage {};
    struct RangesStorage {};
    struct ValuesStorage {};
    auto& reads = HostThreadLocal<WalkReads, ReadsStorage>();
    auto& ranges = HostThreadLocal<AddressRanges, RangesStorage>();
    reads.Reset();
    Graphics::HeapCostScope snapshotCost(Graphics::HeapCost::Guards);
    const auto before = Graphics::Recorder::PendingWriteSnapshot();
    snapshotCost.Stop();
    const auto generation = GuestMemory::TrackerGeneration();
    const bool memoOn = words != nullptr && !invocation.Request().context.vertex.has_value();
    std::uint64_t memoKey = 0;
    bool memoKnown = false;

    Graphics::HeapCostScope memoCost(Graphics::HeapCost::WalkMemo);
    if (memoOn) {
        memoKey = walkMemoKey(invocation, userData, shaderBase);
        auto* found = heapWalkMemo().find(memoKey);
        memoKnown = found != nullptr;
        for (std::size_t way = 0; found != nullptr && way < found->size(); ++way) {
            const auto& entry = (*found)[way];
            if (!sameWalkInputs(entry, invocation, userData, shaderBase)) continue;
            if (!rangesSettled(entry.ranges, before.get(), generation)) break;
            guardRanges(entry.ranges);
            if (sharedWords != nullptr) *sharedWords = entry.words;
            else *words = *entry.words;
            if (stageReads != nullptr) *stageReads = {entry.ranges, entry.values, entry.userDataUse, entry.capture, true};
            auto result = entry.result;
            if (way != 0)
                std::rotate(found->begin(), found->begin() + static_cast<std::ptrdiff_t>(way),
                            found->begin() + static_cast<std::ptrdiff_t>(way) + 1);
            return result;
        }
    }
    memoCost.Stop();

    Graphics::HeapCostScope walkCost(Graphics::HeapCost::Walk);
    ShaderRecompiler::SrtRuntime runtime;
    runtime.userData = userData;
    runtime.shaderBase = shaderBase;
    runtime.userContext = &reads;
    runtime.readMemory = readWalkWord;
    runtime.accessible = walkAccessible;
    auto capture = invocation.Capture(runtime);
    coalesceWords(reads.words, ranges);
    auto& values = HostThreadLocal<std::vector<std::uint32_t>, ValuesStorage>();
    if ((memoOn || stageReads != nullptr) && !reads.failed) copyRangeValues(ranges, values);
    const bool settled = rangesSettled(ranges, before.get(), generation);
    walkCost.Stop();
    if (!settled) return nullptr;

    Graphics::HeapCostScope guardCost(Graphics::HeapCost::Guards);
    guardRanges(ranges);
    guardCost.Stop();
    Graphics::HeapCostScope materializeCost(Graphics::HeapCost::Materialize);
    std::shared_ptr<const ShaderRecompiler::RecompileResult> result;
    if (words != nullptr) result = invocation.MaterializeHeap(*capture, *words);
    if (result == nullptr) result = invocation.Materialize(*capture);
    materializeCost.Stop();
    const bool complete = !reads.failed && result != nullptr;
    if (stageReads != nullptr && complete) *stageReads = {ranges, values, capture->userDataUse, capture, true};
    Graphics::HeapCostScope insertCost(Graphics::HeapCost::MemoInsert);
    if (memoOn && complete && (memoKnown || heapWalkMemo().admit(memoKey)))
        rememberWalk(memoKey, invocation, userData, shaderBase, ranges, values, reads.accessible, capture, result, *words);
    return result;
}

namespace {

std::atomic<int> unmappedStage{-1};

}

void SetHeapStageUnmapped(std::optional<ShaderRecompiler::ShaderStage> stage) {
    unmappedStage.store(stage.has_value() ? static_cast<int>(*stage) : -1, std::memory_order_relaxed);
}

bool HeapDrawStageMappable(const ShaderRecompiler::RecompileResult& result, ShaderRecompiler::ShaderStage stage) {
    return unmappedStage.load(std::memory_order_relaxed) != static_cast<int>(stage) && HeapStageMappable(result);
}

std::uint64_t HeapStagesTaken() {
    return heapStages().load();
}

std::shared_ptr<const ShaderRecompiler::RecompileResult> PatchedHeapResult(const ShaderRecompiler::RecompileResult& shared,
                                                                           const ShaderRecompiler::HeapStageWords& words) {
    auto result = std::make_shared<ShaderRecompiler::RecompileResult>(shared);
    for (auto& binding : result->bindings) {
        if (binding.role == ShaderRecompiler::DescriptorRole::FlattenedSrt && !words.flattenedSrt.empty())
            binding.guestDescriptor = words.flattenedSrt;
        if (binding.role == ShaderRecompiler::DescriptorRole::ShaderData)
            std::copy_n(words.userData.begin(), std::min(words.userData.size(), binding.guestDescriptor.size()),
                        binding.guestDescriptor.begin());
    }
    if (!words.bindings.empty()) {
        if (words.bindings.size() != result->bindings.size())
            throw std::runtime_error("a heap stage's per-draw bindings differ in number from its template");
        for (std::size_t index = 0; index < words.bindings.size(); ++index) {
            if (words.bindings[index].empty()) continue;
            if (result->bindings[index].role != ShaderRecompiler::DescriptorRole::FlattenedSrt &&
                words.bindings[index].size() != result->bindings[index].guestDescriptor.size())
                throw std::runtime_error("a heap stage's per-draw descriptor words differ in size from its template");
            result->bindings[index].guestDescriptor = words.bindings[index];
        }
    }
    if (!words.vertexAttributes.empty()) result->vertexAttributes = words.vertexAttributes;
    return result;
}

void NoteHeapDirect() {
    heapStages().fetch_add(1, std::memory_order_relaxed);
}

void NoteHeapDirectVerify(const ShaderRecompiler::RecompileResult& direct, const ShaderRecompiler::RecompileResult& expected,
                          const ShaderRecompiler::ResourceCapture* expectedCapture) {
    auto& counts = verifyCounts();
    if (expectedCapture != nullptr) {
        const auto partial = [](const std::vector<ShaderRecompiler::DescriptorValue>& values) {
            return std::ranges::any_of(values, [](const ShaderRecompiler::DescriptorValue& value) {
                return value.availableDwordMask != static_cast<std::uint8_t>((1u << value.dwordCount) - 1u);
            });
        };
        const auto& snapshot = expectedCapture->snapshot;
        if (partial(snapshot.buffers) || partial(snapshot.images) || partial(snapshot.samplers)) {
            counts.notComparable.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }
    counts.verified.fetch_add(1, std::memory_order_relaxed);
    std::size_t index = 0;
    if (const auto* what = resultDifference(direct, expected, index)) {
        counts.differ.fetch_add(1, std::memory_order_relaxed);
        reportHeapMismatch("heap stage vs full capture", what, expected, index);
    }
}

void VerifyHeapAccessor(const Graphics::CompiledShader& stage) {
    if (stage.heap == nullptr || stage.program == nullptr) return;
    const auto patched = PatchedHeapResult(*stage.program, *stage.heap);
    auto& counts = verifyCounts();
    counts.accessor.fetch_add(1, std::memory_order_relaxed);
    for (std::size_t index = 0; index < stage.program->bindings.size(); ++index) {
        if (Graphics::DrawGuestWords(stage, stage.program->bindings[index]) == patched->bindings[index].guestDescriptor) continue;
        counts.accessorDiffer.fetch_add(1, std::memory_order_relaxed);
        reportHeapMismatch("accessor vs patched result", "binding words", *patched, index);
    }
    const auto& attributes = Graphics::DrawVertexAttributes(stage);
    bool sameAttributes = attributes.size() == patched->vertexAttributes.size();
    for (std::size_t index = 0; sameAttributes && index < attributes.size(); ++index)
        sameAttributes = attributes[index].resource.fields == patched->vertexAttributes[index].resource.fields &&
                         attributes[index].fetchIndex == patched->vertexAttributes[index].fetchIndex;
    if (!sameAttributes) {
        counts.accessorDiffer.fetch_add(1, std::memory_order_relaxed);
        reportHeapMismatch("accessor vs patched result", "vertex attributes", *patched, 0);
    }
}

void VerifyRingWords(const ShaderRecompiler::RecompileResult& kept, const ShaderRecompiler::HeapStageWords& ring,
                     const ShaderRecompiler::RecompileResult& fresh, const ShaderRecompiler::HeapStageWords* freshWords) {
    const auto reused = PatchedHeapResult(kept, ring);
    const auto expected =
        freshWords != nullptr ? PatchedHeapResult(fresh, *freshWords) : std::make_shared<const ShaderRecompiler::RecompileResult>(fresh);
    auto& counts = verifyCounts();
    counts.ring.fetch_add(1, std::memory_order_relaxed);
    std::size_t index = 0;
    if (const auto* what = resultDifference(*reused, *expected, index)) {
        counts.ringDiffer.fetch_add(1, std::memory_order_relaxed);
        reportHeapMismatch("ring hit vs fresh build", what, *expected, index);
    }
}

}

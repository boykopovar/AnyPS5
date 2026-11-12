#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "HeapCacheSwitch.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/RecycledShared.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace AgcDriver::DriverDetail {

namespace {

constexpr std::size_t ThreadCacheLimit = 65536u;
constexpr std::uint64_t WalkPageBytes = 0x1000u;
constexpr std::uint64_t WordBytes = sizeof(std::uint32_t);

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

bool HeapPreRasterStage(ShaderRecompiler::ShaderStage stage) {
    using Stage = ShaderRecompiler::ShaderStage;
    return stage == Stage::Vertex || stage == Stage::Local || stage == Stage::TessellationControl ||
           stage == Stage::TessellationEvaluation || stage == Stage::Mesh;
}

const char* HeapStageIneligibility(ShaderRecompiler::ShaderStage stage, const ShaderRecompiler::IrResourcePlan* plan) {
    if (stage != ShaderRecompiler::ShaderStage::Fragment && !HeapPreRasterStage(stage)) return "its stage kind does not take the heap";
    if (plan == nullptr) return "it has no resource plan";
    if (plan->stage == ShaderRecompiler::IrShaderStage::Unknown || plan->stage == ShaderRecompiler::IrShaderStage::Fetch)
        return "its resource plan has a fetch or unknown stage";
    return nullptr;
}

bool HeapStageEligible(ShaderRecompiler::ShaderStage stage, const ShaderRecompiler::IrResourcePlan* plan) {
    return HeapStageIneligibility(stage, plan) == nullptr;
}

namespace {

struct HeapExclusions {
    std::uint32_t stages = 0;
    bool tables = false;
    bool storage = false;
    bool sampled = false;
};

using ExcludableStage = std::pair<std::string_view, ShaderRecompiler::ShaderStage>;

constexpr ExcludableStage ExcludableStages[] = {
    {"compute", ShaderRecompiler::ShaderStage::Compute},
    {"vertex", ShaderRecompiler::ShaderStage::Vertex},
    {"hull", ShaderRecompiler::ShaderStage::TessellationControl},
    {"domain", ShaderRecompiler::ShaderStage::TessellationEvaluation},
    {"geometry", ShaderRecompiler::ShaderStage::Geometry},
    {"pixel", ShaderRecompiler::ShaderStage::Fragment},
    {"local", ShaderRecompiler::ShaderStage::Local},
    {"mesh", ShaderRecompiler::ShaderStage::Mesh}};

bool applyExclusion(HeapExclusions& exclusions, std::string_view item) {
    if (item == "tables") {
        exclusions.tables = true;
        return true;
    }
    if (item == "storage") {
        exclusions.storage = true;
        return true;
    }
    if (item == "sampled") {
        exclusions.sampled = true;
        return true;
    }
    const auto stage = std::find_if(std::begin(ExcludableStages), std::end(ExcludableStages), [&](const auto& entry) { return entry.first == item; });
    if (stage == std::end(ExcludableStages)) return false;
    exclusions.stages |= 1u << static_cast<std::uint32_t>(stage->second);
    return true;
}

const HeapExclusions& heapExclusions() {
    static const HeapExclusions exclusions = [] {
        HeapExclusions result;
        const char* value = std::getenv("APS5_GPU_HEAP_EXCLUDE");
        std::string_view list = value != nullptr ? value : "";
        while (!list.empty()) {
            const auto comma = list.find(',');
            const auto item = list.substr(0, comma);
            if (!applyExclusion(result, item)) throw std::runtime_error("APS5_GPU_HEAP_EXCLUDE: unknown class '" + std::string(item) + "'; use compute, vertex, hull, domain, geometry, pixel, local, mesh, tables, storage or sampled");
            if (comma == std::string_view::npos) break;
            list.remove_prefix(comma + 1u);
        }
        return result;
    }();
    return exclusions;
}

std::string heapClassesFailure(const ShaderRecompiler::RecompileResult& result) {
    using Role = ShaderRecompiler::DescriptorRole;
    using Kind = ShaderRecompiler::DescriptorKind;
    const auto& exclusions = heapExclusions();
    std::uint64_t demand = 0;
    for (const auto& binding : result.bindings) {
        const bool table = binding.role == Role::ImageTable || binding.role == Role::ImageStorageTable;
        const bool image = binding.role == Role::GuestImages && (binding.kind == Kind::SampledImage || binding.kind == Kind::StorageImage);
        if (table || image) demand += binding.count;
        if ((table && exclusions.tables) || (image && binding.kind == Kind::StorageImage && exclusions.storage) ||
            (image && binding.kind == Kind::SampledImage && exclusions.sampled))
            return "AGC graphics: a GPU heap stage uses an image class APS5_GPU_HEAP_EXCLUDE names, which its resource plan did not show";
    }
    const auto capacity = Graphics::BindlessHeap::ImageSlotCapacity();
    if (demand > capacity)
        return "AGC graphics: a GPU heap stage needs " + std::to_string(demand) + " image slots, more than the heap's " + std::to_string(capacity);
    return {};
}

std::string heapStageFailure(const ShaderRecompiler::RecompileResult& result, bool workgroupAddress) {
    if (auto failure = heapClassesFailure(result); !failure.empty()) return failure;
    std::pair<std::uint32_t, std::uint32_t> refused{};
    if (!Graphics::DescriptorHeapMappable(std::span<const std::uint32_t>(result.spirv.data(), result.spirv.size()), workgroupAddress, result.dataAddressPush,
                                          result.functionLdsBuffer, result.meshWindowBuffer, result.scratchPoolSlotDwords != 0u, &refused))
        return "AGC graphics: a GPU heap stage uses descriptor set " + std::to_string(refused.first) + " binding " + std::to_string(refused.second) +
               ", which the heap cannot map";
    return {};
}

const std::string& knownHeapStageFailure(const ShaderRecompiler::RecompileResult& result, bool workgroupAddress) {
    struct MappableStorage {};
    struct RecentStorage {};
    struct Recent {
        std::uint64_t key = 0;
        const std::string* failure = nullptr;
    };
    constexpr std::size_t RecentSlots = 64;
    auto& known = HostThreadLocal<std::unordered_map<std::uint64_t, std::string>, MappableStorage>();
    auto& recent = HostThreadLocal<std::array<Recent, RecentSlots>, RecentStorage>();
    const auto key = result.variantId * 2u + (workgroupAddress ? 1u : 0u);
    auto& slot = recent[(key ^ (key >> 17u)) % RecentSlots];
    if (slot.failure != nullptr && slot.key == key) return *slot.failure;
    auto found = known.find(key);
    if (found == known.end()) {
        if (known.size() >= ThreadCacheLimit) {
            known.clear();
            recent.fill({});
        }
        found = known.emplace(key, heapStageFailure(result, workgroupAddress)).first;
    }
    slot = {key, &found->second};
    return found->second;
}

}

void RequireHeapStageMappable(const ShaderRecompiler::RecompileResult& result, bool workgroupAddress) {
    if (const auto& failure = knownHeapStageFailure(result, workgroupAddress); !failure.empty()) throw std::runtime_error(failure);
}

bool HeapStageFits(const ShaderRecompiler::RecompileResult& result, bool workgroupAddress) {
    return knownHeapStageFailure(result, workgroupAddress).empty();
}

bool HeapStageExcluded(ShaderRecompiler::ShaderStage stage) {
    return (heapExclusions().stages >> static_cast<std::uint32_t>(stage) & 1u) != 0u;
}

bool HeapPlanExcluded(const ShaderRecompiler::IrResourcePlan& plan) {
    using Class = ShaderRecompiler::ImageResourceClass;
    const auto& exclusions = heapExclusions();
    if (exclusions.tables && std::ranges::any_of(plan.descriptorSources, [](const ShaderRecompiler::DescriptorSource& source) { return source.tableColumn.has_value(); }))
        return true;
    const auto excludedImage = [&](const ShaderRecompiler::ImageResource& image) {
        return (exclusions.storage && image.resourceClass == Class::Storage) || (exclusions.sampled && image.resourceClass == Class::Sampled);
    };
    if (std::ranges::any_of(plan.info.images, excludedImage)) return true;
    return std::ranges::any_of(plan.info.runtimeImageModes, [&](const std::vector<ShaderRecompiler::ImageResource>& modes) { return std::ranges::any_of(modes, excludedImage); });
}

bool HeapClassesExcluded() {
    const auto& exclusions = heapExclusions();
    return exclusions.tables || exclusions.storage || exclusions.sampled;
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

void AppendWalkRead(std::vector<std::pair<std::uint64_t, std::uint64_t>>& runs, std::uint64_t address) {
    if (!runs.empty() && address >= runs.back().first && address <= runs.back().second) {
        runs.back().second = std::max(runs.back().second, address + WordBytes);
        return;
    }
    runs.emplace_back(address, address + WordBytes);
}

void CoalesceWalkReads(std::vector<std::pair<std::uint64_t, std::uint64_t>>& runs, std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges) {
    if (!std::is_sorted(runs.begin(), runs.end())) std::sort(runs.begin(), runs.end());
    ranges.clear();
    for (const auto& [begin, end] : runs) {
        if (!ranges.empty() && begin <= ranges.back().second) {
            ranges.back().second = std::max(ranges.back().second, end);
            continue;
        }
        ranges.emplace_back(begin, end);
    }
}

namespace {

using AddressRanges = std::vector<std::pair<std::uint64_t, std::uint64_t>>;

struct WalkReads {
    AddressRanges runs;
    std::uint64_t page = ~std::uint64_t{0};
    bool failed = false;

    void Reset() {
        runs.clear();
        failed = false;
        page = ~std::uint64_t{0};
    }
};

bool readWalkWord(void* context, std::uint64_t address, std::uint32_t* value) {
    auto& reads = *static_cast<WalkReads*>(context);
    const auto page = address & ~(WalkPageBytes - 1u);
    const bool crossesPage = (address & (WalkPageBytes - 1u)) > WalkPageBytes - sizeof(*value);
    if (page != reads.page || crossesPage) {
        if (GuestMemory::Accessible(reinterpret_cast<const void*>(page), WalkPageBytes)) reads.page = page;
        else if (!GuestMemory::Accessible(reinterpret_cast<const void*>(address), sizeof(*value))) {
            reads.failed = true;
            return false;
        }
    }
    std::memcpy(value, reinterpret_cast<const void*>(address), sizeof(*value));
    AppendWalkRead(reads.runs, address);
    return true;
}

bool walkAccessible(void* context, std::uint64_t address, std::uint64_t bytes) {
    auto& reads = *static_cast<WalkReads*>(context);
    const bool accessible = GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes));
    if (!accessible) reads.failed = true;
    return accessible;
}



void copyRangeValues(const AddressRanges& ranges, std::vector<std::uint32_t>& values) {
    values.clear();
    for (const auto& [begin, end] : ranges) {
        const auto offset = values.size();
        values.resize(offset + static_cast<std::size_t>((end - begin) / WordBytes));
        std::memcpy(values.data() + offset, reinterpret_cast<const void*>(begin), static_cast<std::size_t>(end - begin));
    }
}

bool rangesSettled(const AddressRanges& ranges, const Graphics::Recorder::WriteRanges* before, std::uint64_t generation) {
    const bool queuedWrite = DrawPipeline::Active() && DrawPipeline::Queue0().Busy() && DrawPipeline::Queue0().OverlapsAny(ranges);
    if (queuedWrite || !Graphics::Recorder::ReadsSettled(ranges, before)) return false;
    return GuestMemory::NoStampAfter(ranges, generation);
}

void guardRanges(const AddressRanges& ranges) {
    for (const auto& [begin, end] : ranges) GuestMemory::GuardRead(begin, static_cast<std::size_t>(end - begin));
}

std::shared_ptr<const ShaderRecompiler::ResourceCapture>
captureWalk(const ShaderRecompiler::PreparedShaderInvocation& invocation, std::span<const std::uint32_t> userData, std::uint64_t shaderBase,
            WalkReads& reads) {
    ShaderRecompiler::SrtRuntime runtime;
    runtime.userData = userData;
    runtime.shaderBase = shaderBase;
    runtime.userContext = &reads;
    runtime.readMemory = readWalkWord;
    runtime.accessible = walkAccessible;
    return invocation.Capture(runtime);
}

}

namespace {

constexpr std::size_t RecycledKeptBytes = 16u << 10u;

template <typename TVector>
void clearRecycled(TVector& vector) {
    if (vector.capacity() * sizeof(typename TVector::value_type) > RecycledKeptBytes) TVector().swap(vector);
    else vector.clear();
}

}

void ResetHeapStageWords(ShaderRecompiler::HeapStageWords& words) {
    clearRecycled(words.userData);
    clearRecycled(words.flattenedSrt);
    clearRecycled(words.vertexAttributes);
    clearRecycled(words.bindings);
    words.base.reset();
    words.dataOrigin = 0;
    words.drawKeyMemo.words.store(nullptr, std::memory_order_relaxed);
    words.memoizeKey = true;
}

namespace {

constexpr std::size_t RecycledStageRecords = 1024, RecycledStageProbes = 8;

template <typename TObject>
struct RecycledStagePool : RecycledShared<TObject> {
    RecycledStagePool() : RecycledShared<TObject>(RecycledStageRecords, RecycledStageProbes) {}
};

struct RecycledWordsStorage {};
struct RecycledValuesStorage {};
struct RecycledRangesStorage {};

std::shared_ptr<ShaderRecompiler::HeapStageWords> recycledStageWords() {
    return HostThreadLocal<RecycledStagePool<ShaderRecompiler::HeapStageWords>, RecycledWordsStorage>().Take(ResetHeapStageWords);
}

std::shared_ptr<const std::vector<std::uint32_t>> recycledValues(const std::vector<std::uint32_t>& values) {
    auto copy = HostThreadLocal<RecycledStagePool<std::vector<std::uint32_t>>, RecycledValuesStorage>().Take(clearRecycled<std::vector<std::uint32_t>>);
    copy->assign(values.begin(), values.end());
    return copy;
}

std::shared_ptr<const AddressRanges> recycledRanges(const AddressRanges& ranges) {
    auto copy = HostThreadLocal<RecycledStagePool<AddressRanges>, RecycledRangesStorage>().Take(clearRecycled<AddressRanges>);
    copy->assign(ranges.begin(), ranges.end());
    return copy;
}

}

HeapStageMaterialized MaterializeHeapStage(const ShaderRecompiler::PreparedShaderInvocation& invocation, std::span<const std::uint32_t> userData,
                                           std::uint64_t shaderBase) {
    struct ReadsStorage {};
    struct RangesStorage {};
    struct ValuesStorage {};
    auto& reads = HostThreadLocal<WalkReads, ReadsStorage>();
    auto& ranges = HostThreadLocal<AddressRanges, RangesStorage>();
    reads.Reset();
    Graphics::HeapCostScope snapshotCost(Graphics::HeapCost::Guards);
    const auto before = Graphics::Recorder::PendingWriteSnapshot();
    snapshotCost.Stop();
    const auto generation = GuestMemory::StableTrackerGeneration();
    const auto variant = invocation.VariantId();

    Graphics::HeapCostScope walkCost(Graphics::HeapCost::Walk);
    std::shared_ptr<const ShaderRecompiler::ResourceCapture> capture;
    try {
        capture = captureWalk(invocation, userData, shaderBase, reads);
    } catch (const std::exception&) {
        return {};
    }
    CoalesceWalkReads(reads.runs, ranges);
    auto& values = HostThreadLocal<std::vector<std::uint32_t>, ValuesStorage>();
    if (!reads.failed) copyRangeValues(ranges, values);
    const bool settled = rangesSettled(ranges, before.get(), generation);
    walkCost.Stop();
    if (!settled) return {};

    Graphics::HeapCostScope guardCost(Graphics::HeapCost::Guards);
    guardRanges(ranges);
    guardCost.Stop();
    Graphics::HeapCostScope materializeCost(Graphics::HeapCost::Materialize);
    auto words = recycledStageWords();
    auto result = invocation.MaterializeHeap(*capture, *words);
    if (result == nullptr) result = invocation.Materialize(*capture);
    words->dataOrigin = NextHeapDataOrigin();
    materializeCost.Stop();
    const bool complete = !reads.failed && result != nullptr;
    HeapStageMaterialized materialized;
    auto shared = complete ? recycledValues(values) : nullptr;
    auto sharedRanges = complete ? recycledRanges(ranges) : nullptr;
    if (complete) materialized.reads = {sharedRanges, shared, capture->userDataUse, capture, true};
    materialized.result = std::move(result);
    materialized.words = std::move(words);
    return materialized;
}


std::uint64_t NextHeapDataOrigin() {
    static std::atomic<std::uint64_t> next{0};
    return next.fetch_add(1, std::memory_order_relaxed) + 1u;
}

std::uint64_t HeapStagesTaken() {
    return heapStages().load();
}

std::uint64_t HeapRingHitsVerified() {
    return verifyCounts().ring.load();
}

std::uint64_t HeapRingHitsDiffering() {
    return verifyCounts().ringDiffer.load();
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
    if (words.base != nullptr && !words.base->bindings.empty()) {
        if (words.base->bindings.size() != result->bindings.size())
            throw std::runtime_error("a heap stage's ring template bindings differ in number from its program");
        for (std::size_t index = 0; index < words.base->bindings.size(); ++index) {
            const auto role = result->bindings[index].role;
            if (role == ShaderRecompiler::DescriptorRole::ShaderData || role == ShaderRecompiler::DescriptorRole::FlattenedSrt || words.base->bindings[index].empty()) continue;
            if (words.base->bindings[index].size() != result->bindings[index].guestDescriptor.size())
                throw std::runtime_error("a heap stage's ring template descriptor words differ in size from its program");
            result->bindings[index].guestDescriptor = words.base->bindings[index];
        }
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
        throw std::runtime_error(std::string("APS5_VERIFY_GPU_HEAP: a heap stage differs from its full capture in its ") + what);
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
        throw std::runtime_error("APS5_VERIFY_GPU_HEAP: a heap stage's binding words differ from its patched result");
    }
    const auto& attributes = Graphics::DrawVertexAttributes(stage);
    bool sameAttributes = attributes.size() == patched->vertexAttributes.size();
    for (std::size_t index = 0; sameAttributes && index < attributes.size(); ++index)
        sameAttributes = attributes[index].resource.fields == patched->vertexAttributes[index].resource.fields &&
                         attributes[index].fetchIndex == patched->vertexAttributes[index].fetchIndex;
    if (!sameAttributes) {
        counts.accessorDiffer.fetch_add(1, std::memory_order_relaxed);
        reportHeapMismatch("accessor vs patched result", "vertex attributes", *patched, 0);
        throw std::runtime_error("APS5_VERIFY_GPU_HEAP: a heap stage's vertex attributes differ from its patched result");
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
        throw std::runtime_error(std::string("a ring hit's rebuilt words differ from a fresh build in its ") + what);
    }
}

}

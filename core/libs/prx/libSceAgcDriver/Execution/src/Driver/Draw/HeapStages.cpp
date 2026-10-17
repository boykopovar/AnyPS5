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

namespace AgcDriver::DriverDetail {

namespace {

std::atomic<std::uint64_t>& heapStages() {
    static std::atomic<std::uint64_t> count{0};
    return count;
}

const char* resultDifference(const ShaderRecompiler::RecompileResult& left, const ShaderRecompiler::RecompileResult& right, std::size_t& index) {
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
        if (left.specialization[index].id != right.specialization[index].id || left.specialization[index].value != right.specialization[index].value) return "specialization";
    }
    index = 0;
    if (left.vertexAttributes.size() != right.vertexAttributes.size()) return "attribute count";
    for (index = 0; index < left.vertexAttributes.size(); ++index) {
        const auto& a = left.vertexAttributes[index];
        const auto& b = right.vertexAttributes[index];
        if (a.location != b.location || a.components != b.components || a.resource.fields != b.resource.fields || a.fetchIndex != b.fetchIndex || a.formatComponents != b.formatComponents) return "attribute";
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
    std::fprintf(stderr, "[heap-verify] %s: %s differ at %zu (role %d kind %d, template %llu, variant 0x%llx)\n", check, what, index, role, kind, static_cast<unsigned long long>(result.heapTemplateId), static_cast<unsigned long long>(result.PipelineVariantId()));
    std::fflush(stderr);
}

struct VerifyCounts {
    std::atomic<std::uint64_t> verified{0}, differ{0}, notComparable{0}, accessor{0}, accessorDiffer{0}, ring{0}, ringDiffer{0};
};

VerifyCounts& verifyCounts() {
    static VerifyCounts counts;
    static std::once_flag registered;
    std::call_once(registered, [] {
        std::atexit([] {
            auto& c = verifyCounts();
            if (c.verified.load() != 0u || c.notComparable.load() != 0u) std::fprintf(stderr, "[heap-verify] %llu heap stages compared with a full capture, %llu differed, %llu not comparable (the full capture could not read every descriptor)\n", static_cast<unsigned long long>(c.verified.load()), static_cast<unsigned long long>(c.differ.load()), static_cast<unsigned long long>(c.notComparable.load()));
            if (c.accessor.load() != 0u || c.ring.load() != 0u) std::fprintf(stderr, "[heap-verify] %llu accessor checks, %llu differed; %llu ring hits checked, %llu differed\n", static_cast<unsigned long long>(c.accessor.load()), static_cast<unsigned long long>(c.accessorDiffer.load()), static_cast<unsigned long long>(c.ring.load()), static_cast<unsigned long long>(c.ringDiffer.load()));
        });
    });
    return counts;
}

}

bool HeapRefused() {
    return Graphics::BindlessHeap::NearlyFull();
}

bool HeapFragmentsEnabled() {
    return Graphics::GpuHeapEnabled() && !HeapRefused();
}

bool HeapStageEligible(ShaderRecompiler::ShaderStage stage, const ShaderRecompiler::IrResourcePlan* plan, bool gpuConstantsVertex) {
    if (plan == nullptr || (stage != ShaderRecompiler::ShaderStage::Fragment && stage != ShaderRecompiler::ShaderStage::Vertex)) return false;
    if (stage == ShaderRecompiler::ShaderStage::Vertex && !(gpuConstantsVertex && plan->gpuConstants)) return false;
    return (!plan->info.images.empty() || (gpuConstantsVertex && plan->srtReads.size() == plan->gpuConstantCheckSlots)) && ShaderRecompiler::HeapFragmentEligible(*plan);
}

namespace {

struct HeapFragmentKey {
    const void* snapshot;
    std::size_t codeOffset;
    bool operator==(const HeapFragmentKey&) const = default;
};

struct HeapFragmentKeyHash {
    std::size_t operator()(const HeapFragmentKey& key) const { return std::hash<const void*>{}(key.snapshot) ^ (key.codeOffset * 0x9e3779b97f4a7c15ull); }
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
    std::vector<std::uint32_t> userData;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    std::vector<std::uint32_t> values;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> accessible;
    std::shared_ptr<const ShaderRecompiler::RecompileResult> result;
    std::shared_ptr<const ShaderRecompiler::HeapStageWords> words;
};

constexpr std::size_t HeapWalkWays = 4;

std::unordered_map<std::uint64_t, std::vector<HeapWalkEntry>>& heapWalkMemo() {
    struct WalkMemoStorage {};
    auto& memo = HostThreadLocal<std::unordered_map<std::uint64_t, std::vector<HeapWalkEntry>>, WalkMemoStorage>();
    return memo;
}

}

std::shared_ptr<const ShaderRecompiler::RecompileResult> MaterializeHeapStage(const ShaderRecompiler::PreparedShaderInvocation& invocation, std::span<const std::uint32_t> userData, std::uint64_t shaderBase, ShaderRecompiler::HeapStageWords* words, std::shared_ptr<const ShaderRecompiler::HeapStageWords>* sharedWords) {
    const auto finish = [&](const std::shared_ptr<const ShaderRecompiler::ResourceCapture>& capture) {
        if (words != nullptr) {
            if (auto shared = invocation.MaterializeHeap(*capture, *words)) return shared;
        }
        return invocation.Materialize(*capture);
    };
    {
        struct Reads {
            std::vector<std::uint64_t> words;
            std::uint64_t page = ~std::uint64_t{0};
            std::vector<std::pair<std::uint64_t, std::uint64_t>> accessible;
            bool failed = false;
        };
        struct ReadsStorage {};
        struct RangesStorage {};
        auto& reads = HostThreadLocal<Reads, ReadsStorage>();
        auto& ranges = HostThreadLocal<std::vector<std::pair<std::uint64_t, std::uint64_t>>, RangesStorage>();
        reads.words.clear();
        reads.accessible.clear();
        reads.failed = false;
        reads.page = ~std::uint64_t{0};
        const auto before = Graphics::Recorder::PendingWriteSnapshot();
        const auto generation = GuestMemory::TrackerGeneration();
        const bool memoOn = words != nullptr && !invocation.Request().context.vertex.has_value();
        std::uint64_t memoKey = 0;
        if (memoOn) {
            memoKey = invocation.VariantId() * 0x9e3779b97f4a7c15ull ^ shaderBase;
            for (const auto word : userData) memoKey = (memoKey ^ word) * 0x100000001b3ull;
            auto& memo = heapWalkMemo();
            const auto found = memo.find(memoKey);
            for (std::size_t way = 0; found != memo.end() && way < found->second.size(); ++way) {
                const auto& entry = found->second[way];
                bool equal = entry.variant == invocation.VariantId() && entry.shaderBase == shaderBase && std::equal(entry.userData.begin(), entry.userData.end(), userData.begin(), userData.end());
                if (!equal) continue;
                std::size_t cursor = 0;
                for (std::size_t r = 0; equal && r < entry.ranges.size(); ++r) {
                    const auto [begin, end] = entry.ranges[r];
                    const auto count = static_cast<std::size_t>((end - begin) / 4u);
                    equal = GuestMemory::CompareMapped(begin, std::as_bytes(std::span(entry.values.data() + cursor, count))) == GuestMemory::Compare::Equal;
                    cursor += count;
                }
                for (std::size_t a = 0; equal && a < entry.accessible.size(); ++a) equal = GuestMemory::Accessible(reinterpret_cast<const void*>(entry.accessible[a].first), static_cast<std::size_t>(entry.accessible[a].second));
                if (!equal) {
                    continue;
                }
                bool settled = !(DrawPipeline::Active() && DrawPipeline::Queue0().Busy() && DrawPipeline::Queue0().OverlapsAny(entry.ranges)) && Graphics::Recorder::ReadsSettled(entry.ranges, before.get());
                for (std::size_t r = 0; settled && r < entry.ranges.size(); ++r) settled = GuestMemory::NewestStamp(entry.ranges[r].first, static_cast<std::size_t>(entry.ranges[r].second - entry.ranges[r].first)) <= generation;
                if (!settled) {
                    break;
                }
                for (const auto& [begin, end] : entry.ranges) GuestMemory::GuardRead(begin, static_cast<std::size_t>(end - begin));
                if (sharedWords != nullptr) *sharedWords = entry.words;
                else *words = *entry.words;
                auto result = entry.result;
                if (way != 0) std::rotate(found->second.begin(), found->second.begin() + static_cast<std::ptrdiff_t>(way), found->second.begin() + static_cast<std::ptrdiff_t>(way) + 1);
                return result;
            }
        }
        ShaderRecompiler::SrtRuntime plain;
        plain.userData = userData;
        plain.shaderBase = shaderBase;
        plain.userContext = &reads;
        plain.readMemory = [](void* context, std::uint64_t address, std::uint32_t* value) {
            auto& self = *static_cast<Reads*>(context);
            const auto page = address & ~std::uint64_t{0xfff};
            if (page != self.page || (address & 0xfffu) > 0x1000u - sizeof(*value)) {
                if (GuestMemory::Accessible(reinterpret_cast<const void*>(page), 0x1000u)) self.page = page;
                else if (!GuestMemory::Accessible(reinterpret_cast<const void*>(address), sizeof(*value))) {
                    self.failed = true;
                    return false;
                }
            }
            std::memcpy(value, reinterpret_cast<const void*>(address), sizeof(*value));
            self.words.push_back(address);
            return true;
        };
        plain.accessible = [](void* context, std::uint64_t address, std::uint64_t bytes) {
            auto& self = *static_cast<Reads*>(context);
            const bool accessible = GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes));
            if (accessible) self.accessible.emplace_back(address, bytes);
            else self.failed = true;
            return accessible;
        };
        auto capture = invocation.Capture(plain);
        std::sort(reads.words.begin(), reads.words.end());
        ranges.clear();
        for (const auto address : reads.words) {
            if (!ranges.empty() && address <= ranges.back().second) {
                ranges.back().second = std::max(ranges.back().second, address + 4u);
                continue;
            }
            ranges.emplace_back(address, address + 4u);
        }
        struct ValuesStorage {};
        auto& values = HostThreadLocal<std::vector<std::uint32_t>, ValuesStorage>();
        if (memoOn && !reads.failed) {
            values.clear();
            for (const auto& [begin, end] : ranges) {
                const auto offset = values.size();
                values.resize(offset + static_cast<std::size_t>((end - begin) / 4u));
                std::memcpy(values.data() + offset, reinterpret_cast<const void*>(begin), static_cast<std::size_t>(end - begin));
            }
        }
        bool settled = !(DrawPipeline::Active() && DrawPipeline::Queue0().Busy() && DrawPipeline::Queue0().OverlapsAny(ranges)) && Graphics::Recorder::ReadsSettled(ranges, before.get());
        for (std::size_t i = 0; settled && i < ranges.size(); ++i) settled = GuestMemory::NewestStamp(ranges[i].first, static_cast<std::size_t>(ranges[i].second - ranges[i].first)) <= generation;
        if (settled) {
            for (const auto& [begin, end] : ranges) GuestMemory::GuardRead(begin, static_cast<std::size_t>(end - begin));
            auto result = finish(capture);
            if (memoOn && !reads.failed && result != nullptr) {
                auto& memo = heapWalkMemo();
                if (memo.size() >= 16384) memo.clear();
                auto& ways = memo[memoKey];
                if (ways.size() >= HeapWalkWays) ways.pop_back();
                ways.emplace(ways.begin());
                auto& entry = ways.front();
                entry.variant = invocation.VariantId();
                entry.shaderBase = shaderBase;
                entry.userData.assign(userData.begin(), userData.end());
                entry.ranges = ranges;
                entry.values = values;
                entry.accessible = reads.accessible;
                entry.result = result;
                entry.words = std::make_shared<const ShaderRecompiler::HeapStageWords>(*words);
            }
            return result;
        }
    }
    return nullptr;
}

std::uint64_t HeapStagesTaken() {
    return heapStages().load();
}

std::shared_ptr<const ShaderRecompiler::RecompileResult> PatchedHeapResult(const ShaderRecompiler::RecompileResult& shared, const ShaderRecompiler::HeapStageWords& words) {
    auto result = std::make_shared<ShaderRecompiler::RecompileResult>(shared);
    for (auto& binding : result->bindings) {
        if (binding.role == ShaderRecompiler::DescriptorRole::FlattenedSrt && !words.flattenedSrt.empty()) binding.guestDescriptor = words.flattenedSrt;
        if (binding.role == ShaderRecompiler::DescriptorRole::ShaderData) std::copy_n(words.userData.begin(), std::min(words.userData.size(), binding.guestDescriptor.size()), binding.guestDescriptor.begin());
    }
    if (!words.bindings.empty()) {
        if (words.bindings.size() != result->bindings.size()) throw std::runtime_error("a heap stage's per-draw bindings differ in number from its template");
        for (std::size_t index = 0; index < words.bindings.size(); ++index) {
            if (words.bindings[index].empty()) continue;
            if (result->bindings[index].role != ShaderRecompiler::DescriptorRole::FlattenedSrt && words.bindings[index].size() != result->bindings[index].guestDescriptor.size()) throw std::runtime_error("a heap stage's per-draw descriptor words differ in size from its template");
            result->bindings[index].guestDescriptor = words.bindings[index];
        }
    }
    if (!words.vertexAttributes.empty()) result->vertexAttributes = words.vertexAttributes;
    return result;
}

void NoteHeapDirect() {
    heapStages().fetch_add(1, std::memory_order_relaxed);
}

void NoteHeapDirectVerify(const ShaderRecompiler::RecompileResult& direct, const ShaderRecompiler::RecompileResult& expected, const ShaderRecompiler::ResourceCapture* expectedCapture) {
    auto& counts = verifyCounts();
    if (expectedCapture != nullptr) {
        const auto partial = [](const std::vector<ShaderRecompiler::DescriptorValue>& values) {
            return std::ranges::any_of(values, [](const ShaderRecompiler::DescriptorValue& value) { return value.availableDwordMask != static_cast<std::uint8_t>((1u << value.dwordCount) - 1u); });
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
    for (std::size_t index = 0; sameAttributes && index < attributes.size(); ++index) sameAttributes = attributes[index].resource.fields == patched->vertexAttributes[index].resource.fields && attributes[index].fetchIndex == patched->vertexAttributes[index].fetchIndex;
    if (!sameAttributes) {
        counts.accessorDiffer.fetch_add(1, std::memory_order_relaxed);
        reportHeapMismatch("accessor vs patched result", "vertex attributes", *patched, 0);
    }
}

void VerifyRingWords(const ShaderRecompiler::RecompileResult& kept, const ShaderRecompiler::HeapStageWords& ring, const ShaderRecompiler::RecompileResult& fresh, const ShaderRecompiler::HeapStageWords* freshWords) {
    const auto reused = PatchedHeapResult(kept, ring);
    const auto expected = freshWords != nullptr ? PatchedHeapResult(fresh, *freshWords) : std::make_shared<const ShaderRecompiler::RecompileResult>(fresh);
    auto& counts = verifyCounts();
    counts.ring.fetch_add(1, std::memory_order_relaxed);
    std::size_t index = 0;
    if (const auto* what = resultDifference(*reused, *expected, index)) {
        counts.ringDiffer.fetch_add(1, std::memory_order_relaxed);
        reportHeapMismatch("ring hit vs fresh build", what, *expected, index);
    }
}

}

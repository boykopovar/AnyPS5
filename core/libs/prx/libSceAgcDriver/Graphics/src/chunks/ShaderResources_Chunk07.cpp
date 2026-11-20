#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <memory>
#include <mutex>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <stdexcept>
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderStage.hpp"
#include "ImageTableAbi.hpp"
#include <cstring>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "prx/libc/include/GuestAllocations.hpp"

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
bool ShaderResources::SpaceReuseDisabled() {
    static const bool disabled = std::getenv("APS5_NO_ADDRESS_DRAW_REUSE") != nullptr;
    return disabled;
}

// APS5_PROFILE_DRAW: the per-device descriptor caches' counters, every 10 s.
void ShaderResources::reportDescriptorCaches() const {
    static AgcDriver::Mutex reportMutex;
    static auto lastReport = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(reportMutex);
    if (now - lastReport < std::chrono::seconds(10)) return;
    lastReport = now;
    const auto descriptors = context.descriptorCache != nullptr ? context.descriptorCache->Counters() : DescriptorCache::Stats{};
    const auto samplerHits = context.samplerCache != nullptr ? context.samplerCache->Hits() : 0;
    const auto samplerMisses = context.samplerCache != nullptr ? context.samplerCache->Misses() : 0;
    AgcDriver::ProfilePrint_nid_no_patch("[descriptors] layouts %llu hits / %llu created, sets %llu from %llu pools and %llu dedicated pools, samplers %llu hits / %llu created\n", static_cast<unsigned long long>(descriptors.layoutHits), static_cast<unsigned long long>(descriptors.layoutMisses), static_cast<unsigned long long>(descriptors.sets), static_cast<unsigned long long>(descriptors.pools), static_cast<unsigned long long>(descriptors.dedicatedPools), static_cast<unsigned long long>(samplerHits), static_cast<unsigned long long>(samplerMisses));
}

constexpr std::size_t CompactTableKeyWords = 8192;

struct TableKeyHash {
    static constexpr std::uint64_t P1 = 0x9e3779b185ebca87ull;
    static constexpr std::uint64_t P2 = 0xc2b2ae3d27d4eb4full;
    std::uint64_t lanes[4] = {P1 + P2, P2, 0, 0 - P1};
    std::uint64_t total = 0;
    static std::uint64_t round(std::uint64_t lane, std::uint64_t value) {
        lane += value * P2;
        lane = (lane << 31u) | (lane >> 33u);
        return lane * P1;
    }
    void bytes(const void* data, std::size_t size) {
        const auto* at = static_cast<const unsigned char*>(data);
        std::size_t i = 0;
        for (; i + 32 <= size; i += 32) {
            std::uint64_t v[4];
            std::memcpy(v, at + i, sizeof(v));
            lanes[0] = round(lanes[0], v[0]);
            lanes[1] = round(lanes[1], v[1]);
            lanes[2] = round(lanes[2], v[2]);
            lanes[3] = round(lanes[3], v[3]);
        }
        for (; i < size; ++i) lanes[i % 4] = round(lanes[i % 4], at[i] | (static_cast<std::uint64_t>(i) << 8u));
        total += size;
        add(size);
    }
    void add(std::uint64_t value) {
        const auto lane = total++ % 4;
        lanes[lane] = round(lanes[lane], value);
    }
    void bits(const ShaderRecompiler::ElementFlags& set) {
        add(set.size());
        set.ForEachSet([&](std::size_t index) { add(index); });
        add(~0ull);
    }
    std::uint64_t a() const { return round(lanes[0], lanes[1]) ^ round(lanes[2], lanes[3]); }
    std::uint64_t b() const { return round(lanes[3] ^ total, lanes[0]) + round(lanes[1], lanes[2] ^ P1); }
};

template<typename TVisitor>
void VisitContentKey(const CompiledShader& shader, bool dataWords, bool movableBuffers, const TVisitor& visit) {
    Require(shader.program != nullptr, "missing compiled shader");
    const auto& program = *shader.program;
    visit(dataWords ? 1u : 0u);
    visit(static_cast<std::uint32_t>(shader.stage));
    visit(static_cast<std::uint32_t>(program.PipelineVariantId()));
    visit(static_cast<std::uint32_t>(program.PipelineVariantId() >> 32u));
    visit(static_cast<std::uint32_t>(program.bindings.size()));
    const auto packBits = [&](const ShaderRecompiler::ElementFlags& bits) {
        visit(static_cast<std::uint32_t>(bits.size()));
        std::uint32_t word = 0;
        for (std::size_t i = 0; i < bits.size(); ++i) {
            if (bits[i]) word |= 1u << (i % 32u);
            if (i % 32u == 31u || i + 1 == bits.size()) {
                visit(word);
                word = 0;
            }
        }
    };
    for (const auto& binding : program.bindings) {
        for (const auto word : {static_cast<std::uint32_t>(binding.kind), static_cast<std::uint32_t>(binding.role), binding.descriptorSet, binding.binding, binding.count, binding.readOnly ? 1u : 0u, binding.imageShape.has_value() ? static_cast<std::uint32_t>(*binding.imageShape) + 1u : 0u, static_cast<std::uint32_t>(binding.guestDescriptor.size())}) visit(word);
        if (binding.role == ShaderRecompiler::DescriptorRole::ImageTable && binding.guestDescriptor.size() > CompactTableKeyWords) {
            TableKeyHash hash;
            hash.bytes(binding.guestDescriptor.data(), binding.guestDescriptor.size() * sizeof(std::uint32_t));
            hash.bytes(binding.imageSamplers.data(), binding.imageSamplers.size() * sizeof(binding.imageSamplers[0]));
            hash.bytes(binding.imageShapes.data(), binding.imageShapes.size() * sizeof(binding.imageShapes[0]));
            for (const auto* set : {&binding.imageWritten, &binding.samplerDepthCompare, &binding.imageDepthCompare, &binding.imageAtomic, &binding.samplerUnnormalized, &binding.samplerIntegerBorder, &binding.samplerCoordinatesIgnored, &binding.imageUnnormalized, &binding.imageUnnormalizedBaseLevel, &binding.bufferWritten}) hash.bits(*set);
            const auto first = hash.a();
            const auto second = hash.b();
            for (const auto word : {0xc0de7ab1u, static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(first >> 32u), static_cast<std::uint32_t>(second), static_cast<std::uint32_t>(second >> 32u)}) visit(word);
            continue;
        }
        if (movableBuffers && binding.role == ShaderRecompiler::DescriptorRole::GuestBuffers && binding.guestDescriptor.size() == static_cast<std::size_t>(binding.count) * 4u) {
            for (std::uint32_t element = 0; element < binding.count; ++element) {
                const auto* words = binding.guestDescriptor.data() + static_cast<std::size_t>(element) * 4u;
                const bool written = element >= binding.bufferWritten.size() || binding.bufferWritten[element];
                const bool empty = words[2] == 0 || (words[0] == 0 && (words[1] & 0xffffu) == 0);
                if (written || empty) { for (std::uint32_t index = 0; index < 4u; ++index) visit(words[index]); }
                else { for (const auto word : {0u, words[1] & 0xffff0000u, 0u, words[3]}) visit(word); }
            }
        } else if (binding.role == ShaderRecompiler::DescriptorRole::ImageTableMap && binding.guestDescriptor.size() > CompactTableKeyWords) {
            TableKeyHash hash;
            hash.bytes(binding.guestDescriptor.data(), binding.guestDescriptor.size() * sizeof(std::uint32_t));
            const auto first = hash.a();
            const auto second = hash.b();
            for (const auto word : {0xc0de7ab2u, static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(first >> 32u), static_cast<std::uint32_t>(second), static_cast<std::uint32_t>(second >> 32u)}) visit(word);
        } else if ((dataWords && !movableBuffers) || !DataRole(binding.role)) {
            for (const auto word : binding.guestDescriptor) visit(word);
        }
        packBits(binding.imageWritten);
        packBits(binding.samplerDepthCompare);
        packBits(binding.imageDepthCompare);
        packBits(binding.imageAtomic);
        packBits(binding.samplerUnnormalized);
        packBits(binding.samplerIntegerBorder);
        packBits(binding.samplerCoordinatesIgnored);
        packBits(binding.imageUnnormalized);
        packBits(binding.imageUnnormalizedBaseLevel);
        // Read-only elements are bound without a write set: an object built for one written set
        // must not serve a build with another (the variant implies it, this makes it explicit).
        packBits(binding.bufferWritten);
        visit(static_cast<std::uint32_t>(binding.imageSamplers.size()));
        for (const auto sampler : binding.imageSamplers) {
            visit(static_cast<std::uint32_t>(sampler));
            visit(static_cast<std::uint32_t>(sampler >> 32u));
        }
        visit(static_cast<std::uint32_t>(binding.imageShapes.size()));
        for (const auto shape : binding.imageShapes) visit(static_cast<std::uint32_t>(shape));
    }
}

std::vector<std::uint32_t> ShaderResources::ContentKey(const CompiledShader& shader, bool dataWords, bool movableBuffers) {
    std::vector<std::uint32_t> key;
    AppendContentKey(shader, dataWords, movableBuffers, key);
    return key;
}

void ShaderResources::AppendContentKey(const CompiledShader& shader, bool dataWords, bool movableBuffers, std::vector<std::uint32_t>& key) {
    Require(shader.program != nullptr, "missing compiled shader");
    std::size_t words = key.size() + 5;
    const auto addWords = [&](std::size_t count) {
        if (count > key.max_size() - words) throw std::length_error("resource content key exceeds vector limits");
        words += count;
    };
    for (const auto& binding : shader.program->bindings) {
        addWords(20);
        if (binding.role == ShaderRecompiler::DescriptorRole::ImageTable && binding.guestDescriptor.size() > CompactTableKeyWords) continue;
        addWords(binding.imageSamplers.size() * 2u);
        addWords(binding.imageShapes.size());
        if (binding.role == ShaderRecompiler::DescriptorRole::ImageTableMap && binding.guestDescriptor.size() > CompactTableKeyWords) addWords(5);
        else if ((dataWords && !movableBuffers) || !DataRole(binding.role)) addWords(binding.guestDescriptor.size());
        for (const auto* bits : {&binding.imageWritten, &binding.samplerDepthCompare, &binding.imageDepthCompare, &binding.imageAtomic, &binding.samplerUnnormalized, &binding.samplerIntegerBorder, &binding.samplerCoordinatesIgnored, &binding.imageUnnormalized, &binding.imageUnnormalizedBaseLevel, &binding.bufferWritten}) addWords(bits->size() / 32u + (bits->size() % 32u != 0 ? 1u : 0u));
    }
    key.reserve(words);
    VisitContentKey(shader, dataWords, movableBuffers, [&](std::uint32_t word) { key.push_back(word); });
}

namespace {

using Component = ContentKeyComponent;

bool LabelShaderKey(std::span<const std::uint32_t> key, std::size_t at, std::size_t end, bool movable, std::vector<std::uint8_t>& labels) {
    const auto label = [&](std::size_t first, std::size_t count, Component component) {
        if (first + count > end) return false;
        std::fill(labels.begin() + static_cast<std::ptrdiff_t>(first), labels.begin() + static_cast<std::ptrdiff_t>(first + count), static_cast<std::uint8_t>(component));
        return true;
    };
    if (!label(at, 5, Component::Layout)) return false;
    const bool dataWords = key[at] != 0;
    const auto bindings = key[at + 4];
    at += 5;
    for (std::uint32_t binding = 0; binding < bindings; ++binding) {
        if (!label(at, 8, Component::Layout)) return false;
        const auto role = static_cast<ShaderRecompiler::DescriptorRole>(key[at + 1]);
        const auto size = key[at + 7];
        at += 8;
        const auto words = DataRole(role) && !(dataWords && !movable) ? 0u : size;
        if (at + words > end) return false;
        for (std::uint32_t i = 0; i < words; ++i) {
            auto component = Component::Other;
            if (role == ShaderRecompiler::DescriptorRole::GuestBuffers) component = static_cast<Component>(static_cast<std::size_t>(Component::BufferWord0) + i % 4u);
            else if (role == ShaderRecompiler::DescriptorRole::GuestImages || role == ShaderRecompiler::DescriptorRole::ImageTable) component = Component::Image;
            else if (role == ShaderRecompiler::DescriptorRole::GuestSamplers || role == ShaderRecompiler::DescriptorRole::SamplerTable) component = Component::Sampler;
            else if (DataRole(role)) component = Component::ShaderData;
            labels[at + i] = static_cast<std::uint8_t>(component);
        }
        at += words;
        for (int flags = 0; flags < 10; ++flags) {
            if (at >= end) return false;
            const auto count = 1 + (static_cast<std::size_t>(key[at]) + 31) / 32;
            if (!label(at, count, Component::Other)) return false;
            at += count;
        }
        if (at >= end) return false;
        const auto samplers = static_cast<std::size_t>(key[at]);
        if (!label(at, 1 + samplers, Component::Sampler)) return false;
        at += 1 + samplers;
        if (at >= end) return false;
        const auto shapes = static_cast<std::size_t>(key[at]);
        if (!label(at, 1 + shapes, Component::Image)) return false;
        at += 1 + shapes;
    }
    return at == end;
}

bool LabelContentKey(std::span<const std::uint32_t> key, bool draw, bool movable, std::vector<std::uint8_t>& labels) {
    labels.assign(key.size(), static_cast<std::uint8_t>(Component::Layout));
    if (!draw) {
        if (LabelShaderKey(key, 0, key.size(), movable, labels)) return true;
        if (key.size() < 2) return false;
        labels.assign(key.size(), static_cast<std::uint8_t>(Component::Layout));
        if (!LabelShaderKey(key, 0, key.size() - 2, movable, labels)) return false;
        labels[key.size() - 2] = static_cast<std::uint8_t>(Component::Other);
        labels[key.size() - 1] = static_cast<std::uint8_t>(Component::Other);
        return true;
    }
    if (key.size() < 4) return false;
    std::fill(labels.begin() + 1, labels.begin() + 3, static_cast<std::uint8_t>(Component::Other));
    std::size_t at = 4;
    for (std::uint32_t stage = 0; stage < key[3]; ++stage) {
        if (at >= key.size()) return false;
        const auto size = static_cast<std::size_t>(key[at]);
        ++at;
        if (at + size > key.size() || !LabelShaderKey(key, at, at + size, movable, labels)) return false;
        at += size;
    }
    if (at >= key.size()) return false;
    for (auto tail = at + 1; tail < key.size(); ++tail) labels[tail] = static_cast<std::uint8_t>(Component::DrawTarget);
    return true;
}

}

bool ShaderResources::DiffContentKeys(std::span<const std::uint32_t> previous, std::span<const std::uint32_t> key, bool draw, ContentKeyDiff& diff) {
    diff = {};
    std::vector<std::uint8_t> labels;
    bool labelled = false;
    for (const bool movable : {draw, false}) {
        labelled = LabelContentKey(key, draw, movable, labels);
        if (labelled) break;
    }
    if (!labelled || previous.size() != key.size()) {
        diff[static_cast<std::size_t>(Component::Layout)] = true;
        return false;
    }
    for (std::size_t i = 0; i < key.size(); ++i) {
        if (previous[i] != key[i]) diff[labels[i]] = true;
    }
    return true;
}

bool MatchesContentKey(const CompiledShader& shader, std::span<const std::uint32_t> key) {
    std::size_t cursor = 0;
    bool equal = true;
    VisitContentKey(shader, false, true, [&](std::uint32_t word) {
        equal &= cursor < key.size() && key[cursor] == word;
        ++cursor;
    });
    return equal && cursor == key.size();
}

namespace {

// APS5_PROFILE_DRAW: Revalidate outcomes and time, printed as [rescache] every 10 s next to the
// device's hit/miss line: failed = the object could not be reused (whichever image path it took);
// of the reused ones, fast = every image proved current from stamps, full = the lookups were repeated.
// Why the fast path left an object to the full walk (the "fast-fail by reason" counts).
using FastFail = ShaderResources::FastFail;
constexpr std::array<const char*, static_cast<std::size_t>(FastFail::Count)> FastFailNames{"no record", "collect", "pending image", "evicted image", "memory changed", "keys", "cleared view", "storage keys", "depth surface", "replaced texture", "audit"};
using OwnRefreshFallback = ShaderResources::OwnRefreshFallback;
constexpr std::array<const char*, static_cast<std::size_t>(OwnRefreshFallback::Count)> OwnRefreshFallbackNames{"disabled", "snapshot texture", "cleared view", "foreign view", "surface key", "not imported", "uncached", "re-run failed"};

struct RevalidateProfile {
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> nanoseconds{0};
    std::atomic<std::uint64_t> fast{0};
    std::atomic<std::uint64_t> full{0};
    std::atomic<std::uint64_t> failed{0};
    std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(FastFail::Count)> fastFails{};
    // Epoch gate: registry scans skipped because the pending serial was unchanged, and import
    // serial loops skipped because the table's identity was unchanged.
    std::atomic<std::uint64_t> serialSkips{0};
    std::atomic<std::uint64_t> importSkips{0};
    // Elements with DCC keys the fast path accepted through the images' key proofs.
    std::atomic<std::uint64_t> keysProven{0};
    std::atomic<std::uint64_t> storageKeysProven{0};
    // The 'pending image' fast-fails by what overlapped: a surface with an own object (the query
    // excepts it, so the overlapping image is foreign to the surface) or a snapshot texture (no own
    // object); and the fails whose overlapping surfaces all have an own object (the T1 trim of
    // design_cpu_final applies: flush the foreign image, refresh the own object, no full walk).
    std::atomic<std::uint64_t> pendingForeign{0};
    std::atomic<std::uint64_t> pendingSnapshot{0};
    std::atomic<std::uint64_t> pendingT1Eligible{0};
    // The full walks by the reason of the fast proof that preceded them (fastFails also counts the
    // Pending failures the own-object refresh resolved), the Pending failures it resolved (with the
    // objects it refreshed: storage images, view sources) and those it left to the walk by reason;
    // view surfaces a foreign image overlapped that the proof accepted because FindPending names
    // their own source; surfaces a foreign image was still pending over after their own object's
    // refresh (its alias, a straddling image) that the re-run accepted; proofs the verify switch
    // checked against the full walk.
    std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(FastFail::Count)> fullByReason{};
    std::atomic<std::uint64_t> ownRefreshed{0};
    std::atomic<std::uint64_t> ownStorageRefreshes{0};
    std::atomic<std::uint64_t> ownViewRefreshes{0};
    std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(OwnRefreshFallback::Count)> ownFallbacks{};
    std::atomic<std::uint64_t> ownSourceViews{0};
    std::atomic<std::uint64_t> refreshedOverlaps{0};
    std::atomic<std::uint64_t> proofsVerified{0};
    std::atomic<std::uint64_t> indexNanoseconds{0};
    std::atomic<std::int64_t> lastReport{0};
    std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(FastFail::Count)> fullNanoseconds{};
    std::atomic<std::uint64_t> refusedSampled{0};
    std::atomic<std::uint64_t> refusedStorage{0};
    std::atomic<std::uint64_t> refusedUncommitted{0};
    std::atomic<std::uint64_t> refusedUnwatched{0};
    std::atomic<std::uint64_t> refusedOther{0};
    std::mutex refusedMutex;
    std::unordered_map<std::uint64_t, std::uint64_t> refusedSurfaces;
};

RevalidateProfile& Revalidations() {
    static RevalidateProfile profile;
    return profile;
}

void countFastFail(FastFail reason) {
    if (BuildProfiled() && !indexProof) Revalidations().fastFails[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
}

void countPendingFail(std::span<const StorageTexture::PendingQuery> pending) {
    if (!BuildProfiled() || indexProof) return;
    auto& profile = Revalidations();
    bool eligible = true;
    for (const auto& query : pending) {
        if (!query.overlaps) continue;
        if (query.except != nullptr) {
            profile.pendingForeign.fetch_add(1, std::memory_order_relaxed);
        } else {
            profile.pendingSnapshot.fetch_add(1, std::memory_order_relaxed);
            eligible = false;
        }
    }
    if (eligible) profile.pendingT1Eligible.fetch_add(1, std::memory_order_relaxed);
}

void countKeysProven(bool storage) {
    if (BuildProfiled()) (storage ? Revalidations().storageKeysProven : Revalidations().keysProven).fetch_add(1, std::memory_order_relaxed);
}

void countFullWalk(FastFail reason) {
    if (BuildProfiled() && reason != FastFail::Count) Revalidations().fullByReason[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
}

void countFullWalkTime(FastFail reason, std::chrono::steady_clock::time_point start) {
    if (!BuildProfiled() || reason == FastFail::Count) return;
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    Revalidations().fullNanoseconds[static_cast<std::size_t>(reason)].fetch_add(static_cast<std::uint64_t>(nanoseconds), std::memory_order_relaxed);
}

constexpr std::size_t RefusedSurfaceLimit = 65536;

void countCollectRefusal(bool storage, std::uint64_t address, std::size_t bytes) {
    if (!BuildProfiled() || indexProof) return;
    auto& profile = Revalidations();
    (storage ? profile.refusedStorage : profile.refusedSampled).fetch_add(1, std::memory_order_relaxed);
    const bool accessible = GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes);
    const bool watched = accessible && GuestMemory::Watched(address, bytes);
    (!accessible ? profile.refusedUncommitted : !watched ? profile.refusedUnwatched : profile.refusedOther).fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(profile.refusedMutex);
    if (profile.refusedSurfaces.size() < RefusedSurfaceLimit || profile.refusedSurfaces.contains(address)) profile.refusedSurfaces[address] = bytes;
}

void countOwnRefreshFallback(OwnRefreshFallback reason) {
    if (BuildProfiled()) Revalidations().ownFallbacks[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
}

void countOwnRefresh(bool storage) {
    TextureCounts().ownRefreshes.fetch_add(1, std::memory_order_relaxed);
    if (BuildProfiled()) (storage ? Revalidations().ownStorageRefreshes : Revalidations().ownViewRefreshes).fetch_add(1, std::memory_order_relaxed);
}

// APS5_NO_OWN_IMAGE_REFRESH=1: a Pending failure of the fast proof takes the full walk, as before
// the own-object refresh (T1).
bool OwnImageRefreshEnabled() {
    static const bool disabled = std::getenv("APS5_NO_OWN_IMAGE_REFRESH") != nullptr;
    return !disabled;
}

// APS5_VERIFY_PROOFS=1: after every own-object refresh the full walk runs beside the proof and the
// process aborts when it would return another object or upload again (a different decision).
bool VerifyProofs() {
    static const bool enabled = std::getenv("APS5_VERIFY_PROOFS") != nullptr;
    return enabled;
}

// Whether a sampled surface's keys are the very keys of the image the view follows (the same
// metadata, extent, format and alpha placement: the same scan), so the image's proof serves it.
bool SameKeySurface(const StorageTexture* source, const GuestTextureResource& resource, std::uint64_t guestBytes) {
    if (source == nullptr) return false;
    const auto& own = source->Descriptor();
    return own.dccAddress == resource.dccAddress && source->GuestBytes() == guestBytes && own.format == resource.format && own.dccAlphaOnMsb == resource.dccAlphaOnMsb;
}

void countRevalidate(bool fast, bool ok, std::chrono::steady_clock::time_point start) {
    auto& profile = Revalidations();
    const auto now = std::chrono::steady_clock::now();
    profile.calls.fetch_add(1, std::memory_order_relaxed);
    profile.nanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count()), std::memory_order_relaxed);
    (!ok ? profile.failed : fast ? profile.fast : profile.full).fetch_add(1, std::memory_order_relaxed);
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto last = profile.lastReport.load();
    if (nowMs - last < 10000 || !profile.lastReport.compare_exchange_strong(last, nowMs)) return;
    const auto byReason = [](auto& counts, const auto& names) {
        std::string reasons;
        for (std::size_t i = 0; i < names.size(); ++i) {
            const auto count = counts[i].exchange(0, std::memory_order_relaxed);
            if (count == 0) continue;
            reasons += " " + std::string(names[i]) + " " + std::to_string(count);
        }
        return reasons;
    };
    const auto reasons = byReason(profile.fastFails, FastFailNames);
    const auto fullReasons = byReason(profile.fullByReason, FastFailNames);
    const auto fallbacks = byReason(profile.ownFallbacks, OwnRefreshFallbackNames);
    std::string walkMs;
    for (std::size_t i = 0; i < FastFailNames.size(); ++i) {
        const auto nanoseconds = profile.fullNanoseconds[i].exchange(0, std::memory_order_relaxed);
        if (nanoseconds == 0) continue;
        char text[64];
        std::snprintf(text, sizeof(text), " %s %.1f", FastFailNames[i], static_cast<double>(nanoseconds) / 1e6);
        walkMs += text;
    }
    std::uint64_t refusedSurfaces = 0;
    std::uint64_t refusedBytes = 0;
    {
        std::lock_guard lock(profile.refusedMutex);
        refusedSurfaces = profile.refusedSurfaces.size();
        for (const auto& surface : profile.refusedSurfaces) refusedBytes += surface.second;
        profile.refusedSurfaces.clear();
    }
    AgcDriver::ProfilePrint_nid_no_patch("[rescache] revalidate (10 s): %llu calls %.1f ms: fast %llu, full %llu, failed %llu; fast-fail by reason:%s; full walks: T1 refreshed %llu (%llu storage images, %llu view sources, %llu views served by their own pending source, %llu overlaps left by the refresh accepted), full by reason:%s, T1 fallback by reason:%s, proofs verified %llu; fast-fail pending image: own 0 (the query excepts the own object), foreign %llu, snapshot %llu, T1 eligible %llu; keys proven: %llu sampled, %llu storage; epoch gate: %llu registry scans skipped, %llu import loops skipped; lookup log: %llu builds and full walks kept their records past the bound; full walk ms by reason:%s; collect refusals: sampled %llu, storage %llu (uncommitted pages %llu, unwatched %llu, other %llu) over %llu surfaces, %.1f MiB\n", static_cast<unsigned long long>(profile.calls.exchange(0, std::memory_order_relaxed)), profile.nanoseconds.exchange(0, std::memory_order_relaxed) / 1e6, static_cast<unsigned long long>(profile.fast.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.full.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.failed.exchange(0, std::memory_order_relaxed)), reasons.c_str(), static_cast<unsigned long long>(profile.ownRefreshed.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.ownStorageRefreshes.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.ownViewRefreshes.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.ownSourceViews.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.refreshedOverlaps.exchange(0, std::memory_order_relaxed)), fullReasons.c_str(), fallbacks.c_str(), static_cast<unsigned long long>(profile.proofsVerified.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.pendingForeign.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.pendingSnapshot.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.pendingT1Eligible.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.keysProven.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.storageKeysProven.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.serialSkips.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.importSkips.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(lookupBuildsPastBound.exchange(0, std::memory_order_relaxed)), walkMs.c_str(), static_cast<unsigned long long>(profile.refusedSampled.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.refusedStorage.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.refusedUncommitted.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.refusedUnwatched.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(profile.refusedOther.exchange(0, std::memory_order_relaxed)), static_cast<unsigned long long>(refusedSurfaces), static_cast<double>(refusedBytes) / 1048576.0);
    if (!GuestMemory::WriteIndexEnabled()) return;
    const auto index = GuestMemory::WriteIndexCounted();
    GuestMemory::ResetWriteIndexCounts();
    std::string sources;
    constexpr std::array<const char*, static_cast<std::size_t>(GuestMemory::WriteBumpSource::Count)> SourceNames{"cpu", "driver", "pending", "unwatch", "forget", "release", "import"};
    for (std::size_t i = 0; i < SourceNames.size(); ++i) sources += " " + std::string(SourceNames[i]) + " " + std::to_string(index.bySource[i]);
    static std::array<std::uint64_t, IndexCountCount> reported{};
    std::array<std::uint64_t, IndexCountCount> counts{};
    for (std::size_t i = 0; i < counts.size(); ++i) {
        const auto total = IndexCounts()[i].load(std::memory_order_relaxed);
        counts[i] = total - reported[i];
        reported[i] = total;
    }
    AgcDriver::ProfilePrint_nid_no_patch("[rescache] write index (10 s): %llu proofs %.1f ms: %llu accepted, %llu partial (%llu bumped elements, %llu looked up again, %llu re-bound), %llu unwatched elements compared, %llu fallbacks, %llu builds subscribed; %llu subscriptions over %llu blocks, %llu bumps by source:%s, %llu sweeps (%llu failed), %llu bump lists overflowed, writers' index lookups %llu in %.1f ms\n", static_cast<unsigned long long>(counts[IndexAccepted] + counts[IndexPartial] + counts[IndexFallback]), profile.indexNanoseconds.exchange(0, std::memory_order_relaxed) / 1e6, static_cast<unsigned long long>(counts[IndexAccepted]), static_cast<unsigned long long>(counts[IndexPartial]), static_cast<unsigned long long>(counts[IndexBumped]), static_cast<unsigned long long>(counts[IndexLookedUp]), static_cast<unsigned long long>(counts[IndexRebound]), static_cast<unsigned long long>(counts[IndexCompared]), static_cast<unsigned long long>(counts[IndexFallback]), static_cast<unsigned long long>(counts[IndexBuilds]), static_cast<unsigned long long>(index.subscriptions), static_cast<unsigned long long>(index.blocks), static_cast<unsigned long long>(index.bumps), sources.c_str(), static_cast<unsigned long long>(index.sweeps), static_cast<unsigned long long>(index.sweepFailures), static_cast<unsigned long long>(index.overflows), static_cast<unsigned long long>(index.lookups), static_cast<double>(index.lookupNanoseconds) / 1e6);
}

// APS5_NO_EPOCH_REVALIDATE=1: the per-element registry, cache and stamp checks and the per-region
// flush and import lookups of every Revalidate, as before the epoch gate.
bool EpochRevalidate() {
    static const bool enabled = std::getenv("APS5_NO_EPOCH_REVALIDATE") == nullptr;
    return enabled;
}

std::chrono::steady_clock::duration TemplateAuditInterval() {
    static const auto interval = [] {
        const char* text = std::getenv("APS5_TEMPLATE_AUDIT_MS");
        return std::chrono::milliseconds(text != nullptr ? std::strtoull(text, nullptr, 10) : 1000ull);
    }();
    return interval;
}

bool AuditEveryInterval() {
    static const bool always = std::getenv("APS5_TEMPLATE_AUDIT_ALWAYS") != nullptr;
    return always;
}

bool TemplateAuditDue(std::chrono::steady_clock::time_point last) {
    const auto interval = TemplateAuditInterval();
    return interval.count() != 0 && std::chrono::steady_clock::now() - last >= interval;
}

std::uint32_t ImportAuditBudget() {
    static const auto budget = [] {
        const char* text = std::getenv("APS5_TEMPLATE_AUDIT_BUDGET");
        return static_cast<std::uint32_t>(text != nullptr ? std::strtoul(text, nullptr, 10) : 8ul);
    }();
    return budget;
}

bool ImportAuditAdmitted() {
    static std::atomic<std::int64_t> slot{-1};
    static std::atomic<std::uint32_t> admitted{0};
    const auto budget = ImportAuditBudget();
    if (budget == 0) return true;
    const auto now = std::chrono::steady_clock::now().time_since_epoch() / std::chrono::milliseconds(4);
    auto seen = slot.load(std::memory_order_relaxed);
    if (seen != now && slot.compare_exchange_strong(seen, now, std::memory_order_relaxed)) admitted.store(0, std::memory_order_relaxed);
    return admitted.fetch_add(1, std::memory_order_relaxed) < budget;
}

}

}
}
}
}
}
}
}
}
}
}

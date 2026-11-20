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
namespace {
namespace {
namespace {
namespace {
namespace {
std::shared_ptr<ShaderResources> ResourceCache::Find(const Key& key) {
    resourceCacheFinds.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(mutex);
    const auto found = index.find(key);
    if (found == index.end()) {
        noteReturn(key);
        noteMiss(key);
        return nullptr;
    }
    entries.splice(entries.begin(), entries, found->second);
    noteUse(key);
    return found->second->second;
}

void ResourceCache::noteEvicted(const Key& key) {
    if (fixedCapacity) return;
    const auto hash = KeyHash{}(key);
    if (const auto found = evictedKeys.find(hash); found != evictedKeys.end()) evictedOrder.erase(found->second);
    evictedOrder.push_front(hash);
    evictedKeys[hash] = evictedOrder.begin();
    while (evictedOrder.size() > capacity) {
        evictedKeys.erase(evictedOrder.back());
        evictedOrder.pop_back();
    }
    if (++evictionsSinceCheck < capacity / 4u) return;
    const bool churning = returnsSinceCheck * 2u > evictionsSinceCheck;
    evictionsSinceCheck = 0;
    returnsSinceCheck = 0;
    if (!churning || capacity >= MaximumEntries) return;
    const auto previous = capacity;
    capacity = std::min(capacity * 2u, MaximumEntries);
    if (shrankLast) shrinkWindow = std::min(shrinkWindow * 2u, MaximumShrinkWindow);
    shrankLast = false;
    usedKeys.clear();
    usesSinceCheck = 0;
    std::fprintf(stderr, "[rescache] capacity %zu -> %zu entries: keys it evicted keep coming back\n", previous, capacity);
}

void ResourceCache::noteUse(const Key& key) {
    if (fixedCapacity || capacity <= baseEntries) return;
    usedKeys.insert(KeyHash{}(key));
    if (++usesSinceCheck < capacity * shrinkWindow) return;
    const auto used = usedKeys.size();
    const auto uses = usesSinceCheck;
    usedKeys.clear();
    usesSinceCheck = 0;
    if (used * 4u > capacity) return;
    const auto previous = capacity;
    capacity = std::max(capacity / 2u, baseEntries);
    shrankLast = true;
    evictionsSinceCheck = 0;
    returnsSinceCheck = 0;
    std::fprintf(stderr, "[rescache] capacity %zu -> %zu entries: %zu keys served the last %zu uses\n", previous, capacity, used, uses);
}

void ResourceCache::noteReturn(const Key& key) {
    if (fixedCapacity) return;
    const auto found = evictedKeys.find(KeyHash{}(key));
    if (found == evictedKeys.end()) return;
    evictedOrder.erase(found->second);
    evictedKeys.erase(found);
    ++returnsSinceCheck;
}

namespace {

void traceMiss(ChurnProfile& churn, bool draw, std::uint64_t variants, const ContentKeyDiff* parts) {
    if (churn.tracedLines >= MaxTracedLines || ++churn.tracedByVariant[variants] > MaxTracedLinesPerVariant) return;
    ++churn.tracedLines;
    std::string changed = parts == nullptr ? "first-seen" : "";
    for (std::size_t i = 0; parts != nullptr && i < parts->size(); ++i) {
        if (!(*parts)[i]) continue;
        if (!changed.empty()) changed += ", ";
        changed += componentName(i);
    }
    if (changed.empty()) changed = "no-component-change";
    const auto hashed = (variants ^ 0x9e3779b97f4a7c15ull) * 0xff51afd7ed558ccdull;
    AgcDriver::ProfilePrint_nid_no_patch("[keymiss] resource-content %s shader set %016llx: %s\n", draw ? "draw" : "dispatch", static_cast<unsigned long long>(hashed ^ (hashed >> 33u)), changed.c_str());
}

}

void ResourceCache::noteMiss(const Key& key) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    static const bool trace = std::getenv("APS5_TRACE_RESOURCE_KEY_MISSES") != nullptr;
    if ((!profile && !trace) || key.size() < 5) return;
    // The variant(s) the key belongs to: a compute key starts with the data-words flag, the stage
    // and its variant, a draw key with a marker, the device and its stages' keys (Draw.cpp
    // DrawResourceKey).
    const bool draw = key[0] == 0xffffffffu;
    std::uint64_t variants = 0;
    if (draw) {
        std::size_t at = 4;
        for (std::uint32_t stage = 0; stage < key[3] && at + 4 < key.size(); ++stage) {
            variants = variants * 1000003ull ^ (key[at + 3] | (static_cast<std::uint64_t>(key[at + 4]) << 32u));
            at += 1 + key[at];
        }
    } else {
        variants = key[2] | (static_cast<std::uint64_t>(key[3]) << 32u);
    }
    auto& churn = Churn();
    std::lock_guard lock(churn.mutex);
    auto& counts = draw ? churn.draws : churn.dispatch;
    const auto found = churn.lastByVariant.find(variants);
    if (found == churn.lastByVariant.end()) {
        ++counts.firstSeen;
        ++counts.window.noCandidate;
        if (trace) traceMiss(churn, draw, variants, nullptr);
        churn.lastByVariant.emplace(variants, key);
    } else {
        const auto& previous = found->second;
        const auto common = std::min(key.size(), previous.size());
        std::size_t diff = 0;
        while (diff < common && key[diff] == previous[diff]) ++diff;
        if (diff == common && key.size() == previous.size()) {
            ++counts.sameKey;
        } else if (draw) {
            std::size_t at = 4;
            bool charged = diff < at;
            if (charged) ++counts.layout;
            for (std::uint32_t stage = 0; stage < key[3] && at < key.size() && !charged; ++stage) {
                const auto end = std::min(key.size(), at + 1 + static_cast<std::size_t>(key[at]));
                if (diff == at) {
                    ++counts.layout;
                    charged = true;
                } else if (diff < end) {
                    ++at;
                    if (!chargeShaderKey(counts, key, at, end, diff)) ++counts.layout;
                    charged = true;
                }
                at = end;
            }
            if (!charged) ++counts.drawWords;
        } else {
            std::size_t at = 0;
            if (!chargeShaderKey(counts, key, at, key.size(), diff)) ++counts.layout;
        }
        if (trace) {
            ContentKeyDiff parts;
            ++counts.window.compared;
            if (!ShaderResources::DiffContentKeys(previous, key, draw, parts)) ++counts.window.unlabelled;
            bool anyPart = false;
            for (std::size_t i = 0; i < parts.size(); ++i) {
                if (!parts[i]) continue;
                ++counts.window.differing[i];
                anyPart = true;
            }
            if (!anyPart) ++counts.window.identical;
            traceMiss(churn, draw, variants, &parts);
        }
        found->second = key;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - churn.lastReport < std::chrono::seconds(10)) return;
    churn.lastReport = now;
    const auto report = [](const char* what, const ChurnCounts& churn) {
        std::string line = std::string("[rescache] miss churn (") + what + "), first differing word by role:";
        char item[256];
        for (std::size_t role = 0; role < churn.roles.size(); ++role) {
            if (churn.roles[role] == 0) continue;
            std::snprintf(item, sizeof(item), " %s %llu", roleName(static_cast<ShaderRecompiler::DescriptorRole>(role)), static_cast<unsigned long long>(churn.roles[role]));
            line += item;
            if (role == static_cast<std::size_t>(ShaderRecompiler::DescriptorRole::GuestBuffers)) {
                std::snprintf(item, sizeof(item), " (V# word %llu/%llu/%llu/%llu)", static_cast<unsigned long long>(churn.bufferWords[0]), static_cast<unsigned long long>(churn.bufferWords[1]), static_cast<unsigned long long>(churn.bufferWords[2]), static_cast<unsigned long long>(churn.bufferWords[3]));
                line += item;
            } else if (role == static_cast<std::size_t>(ShaderRecompiler::DescriptorRole::GuestImages)) {
                std::snprintf(item, sizeof(item), " (T# word %llu/%llu/%llu/%llu/%llu/%llu/%llu/%llu)", static_cast<unsigned long long>(churn.imageWords[0]), static_cast<unsigned long long>(churn.imageWords[1]), static_cast<unsigned long long>(churn.imageWords[2]), static_cast<unsigned long long>(churn.imageWords[3]), static_cast<unsigned long long>(churn.imageWords[4]), static_cast<unsigned long long>(churn.imageWords[5]), static_cast<unsigned long long>(churn.imageWords[6]), static_cast<unsigned long long>(churn.imageWords[7]));
                line += item;
            }
        }
        std::snprintf(item, sizeof(item), "; layout %llu, draw target/index %llu, same key %llu (not inserted or evicted), first seen %llu", static_cast<unsigned long long>(churn.layout), static_cast<unsigned long long>(churn.drawWords), static_cast<unsigned long long>(churn.sameKey), static_cast<unsigned long long>(churn.firstSeen));
        line += item;
        AgcDriver::ProfilePrint_nid_no_patch("%s\n", line.c_str());
    };
    if (profile) {
        report("dispatch", churn.dispatch);
        report("draws", churn.draws);
    }
    const auto keyMiss = [](const char* what, KeyMissWindow& window) {
        const auto count = [](std::uint64_t value) { return static_cast<unsigned long long>(value); };
        const auto part = [&](ContentKeyComponent component) { return count(window.differing[static_cast<std::size_t>(component)]); };
        AgcDriver::ProfilePrint_nid_no_patch("[keymiss] resource-content %s (10 s): misses %llu; differing: layout %llu, V# word0 %llu, V# word1 %llu, V# word2 %llu, V# word3 %llu, image %llu, sampler %llu, shader-data %llu, other %llu, draw target/index %llu; no-candidate %llu; identical %llu; not comparable %llu\n", what, count(window.compared + window.noCandidate), part(ContentKeyComponent::Layout), part(ContentKeyComponent::BufferWord0), part(ContentKeyComponent::BufferWord1), part(ContentKeyComponent::BufferWord2), part(ContentKeyComponent::BufferWord3), part(ContentKeyComponent::Image), part(ContentKeyComponent::Sampler), part(ContentKeyComponent::ShaderData), part(ContentKeyComponent::Other), part(ContentKeyComponent::DrawTarget), count(window.noCandidate), count(window.identical), count(window.unlabelled));
        window = KeyMissWindow{};
    };
    if (trace) {
        keyMiss("dispatch", churn.dispatch.window);
        keyMiss("draw", churn.draws.window);
    }
}

void ResourceCache::Insert(const Key& key, std::shared_ptr<ShaderResources> resources, std::vector<std::shared_ptr<ShaderResources>>* evicted) {
    std::lock_guard lock(mutex);
    dropReleasedTextures(evicted);
    if (!resources->HasCachedStorageImages() || resources->HoldsReleasedTexture()) return;
    if (const auto found = index.find(key); found != index.end()) {
        if (evicted != nullptr) evicted->push_back(std::move(found->second->second));
        entries.erase(found->second);
        index.erase(found);
    }
    entries.emplace_front(key, std::move(resources));
    index.emplace(key, entries.begin());
    if (capacity == 0) {
        const char* value = std::getenv("APS5_RESOURCE_CACHE_ENTRIES");
        const auto parsed = value ? std::strtoull(value, nullptr, 10) : 0ull;
        fixedCapacity = parsed != 0;
        capacity = fixedCapacity ? static_cast<std::size_t>(parsed) : baseEntries;
    }
    noteUse(key);
    while (entries.size() > capacity) {
        noteEvicted(entries.back().first);
        if (evicted != nullptr) evicted->push_back(std::move(entries.back().second));
        index.erase(entries.back().first);
        entries.pop_back();
    }
    if (entries.front().second->TableElements() == 0) return;
    std::uint64_t tables = 0;
    for (const auto& entry : entries) tables += entry.second->TableElements();
    for (auto it = std::prev(entries.end()); tables > TableElementBudget() && it != entries.begin();) {
        const auto elements = it->second->TableElements();
        if (elements == 0) {
            --it;
            continue;
        }
        tables -= elements;
        if (evicted != nullptr) evicted->push_back(std::move(it->second));
        index.erase(it->first);
        it = std::prev(entries.erase(it));
    }
}

void ResourceCache::Remove(const Key& key, const ShaderResources* object) {
    std::lock_guard lock(mutex);
    if (object != nullptr) {
        const auto found = index.find(key);
        if (found == index.end() || found->second->second.get() != object) return;
    }
    erase(key);
}

void ResourceCache::RemoveImage(const StorageTexture* image, std::vector<std::shared_ptr<ShaderResources>>* evicted) {
    std::lock_guard lock(mutex);
    removeImage(image, evicted);
}

bool ResourceCache::TryRemoveImage(const StorageTexture* image, std::vector<std::shared_ptr<ShaderResources>>* evicted) {
    std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    removeImage(image, evicted);
    return true;
}

void ResourceCache::removeImage(const StorageTexture* image, std::vector<std::shared_ptr<ShaderResources>>* evicted) {
    for (auto it = entries.begin(); it != entries.end();) {
        if (!it->second->ReadsImage(image)) {
            ++it;
            continue;
        }
        if (evicted != nullptr) evicted->push_back(std::move(it->second));
        index.erase(it->first);
        it = entries.erase(it);
    }
}

void ResourceCache::dropReleasedTextures(std::vector<std::shared_ptr<ShaderResources>>* evicted) {
    if (!TexturesReleased().exchange(false, std::memory_order_acq_rel)) return;
    for (auto it = entries.begin(); it != entries.end();) {
        if (!it->second->HoldsReleasedTexture()) {
            ++it;
            continue;
        }
        if (evicted != nullptr) evicted->push_back(std::move(it->second));
        index.erase(it->first);
        it = entries.erase(it);
    }
}

bool ResourceCache::Touch(const Key& key) {
    resourceCacheTouches.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(mutex);
    const auto found = index.find(key);
    if (found == index.end()) return false;
    entries.splice(entries.begin(), entries, found->second);
    noteUse(key);
    return true;
}

std::uint64_t ResourceCache::Finds() {
    return resourceCacheFinds.load(std::memory_order_relaxed);
}

std::uint64_t ResourceCache::Touches() {
    return resourceCacheTouches.load(std::memory_order_relaxed);
}

void ResourceCache::Clear() {
    std::lock_guard lock(mutex);
    index.clear();
    entries.clear();
    evictedKeys.clear();
    evictedOrder.clear();
    evictionsSinceCheck = 0;
    returnsSinceCheck = 0;
    usedKeys.clear();
    usesSinceCheck = 0;
    shrinkWindow = baseShrinkWindow;
    shrankLast = false;
    if (!fixedCapacity) capacity = 0;
}

void ResourceCache::SetBase(std::size_t entries, std::size_t window) {
    std::lock_guard lock(mutex);
    baseEntries = entries;
    baseShrinkWindow = window;
}

std::size_t ResourceCache::Size() const {
    std::lock_guard lock(mutex);
    return entries.size();
}

std::size_t ResourceCache::Capacity() const {
    std::lock_guard lock(mutex);
    return capacity == 0 ? baseEntries : capacity;
}

std::uint64_t ResourceCache::TableElementBudget() {
    static const std::uint64_t elements = [] {
        const char* value = std::getenv("APS5_RESOURCE_CACHE_TABLE_ELEMENTS");
        const auto parsed = value ? std::strtoull(value, nullptr, 10) : 65536ull;
        return static_cast<std::uint64_t>(parsed != 0 ? parsed : 65536ull);
    }();
    return elements;
}

std::uint64_t ResourceCache::TableElements() const {
    std::lock_guard lock(mutex);
    std::uint64_t tables = 0;
    for (const auto& entry : entries) tables += entry.second->TableElements();
    return tables;
}

void ResourceCache::erase(const Key& key) {
    const auto found = index.find(key);
    if (found == index.end()) return;
    entries.erase(found->second);
    index.erase(found);
}

ResourceCache& SharedResourceCache() {
    static auto* cache = new ResourceCache();
    return *cache;
}

namespace {
std::size_t KeptSets() {
    static const std::size_t count = [] {
        const char* text = std::getenv("APS5_KEPT_DESCRIPTOR_SETS");
        return text != nullptr ? static_cast<std::size_t>(std::strtoull(text, nullptr, 10)) : std::size_t{8};
    }();
    return count;
}

}

DescriptorCache::DescriptorCache(const Context& context) : context(context), destroyLayout(context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")), destroyPool(context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")), resetPool(context.Function<PFN_vkResetDescriptorPool>("vkResetDescriptorPool")), freeDescriptorSets(context.individuallyFreeDescriptorSets ? context.Function<PFN_vkFreeDescriptorSets>("vkFreeDescriptorSets") : nullptr) {
    kept.reserve(KeptSets() + 1u);
}

DescriptorCache::~DescriptorCache() {
    for (const auto& pool : pools) destroyPool(context.device, pool.handle, nullptr);
    for (const auto& pool : updateAfterBindPools) destroyPool(context.device, pool.handle, nullptr);
    for (const auto pool : dedicated) destroyPool(context.device, pool, nullptr);
    for (const auto& set : kept) destroyPool(context.device, set.pool, nullptr);
    for (const auto& [key, layout] : layouts) destroyLayout(context.device, layout, nullptr);
}

VkDescriptorSetLayout DescriptorCache::Layout(std::span<const std::uint32_t> key, std::span<const VkDescriptorSetLayoutBinding> bindings, bool updateAfterBind, std::span<const VkDescriptorBindingFlags> flags) {
    std::vector<VkDescriptorBindingFlags> defaults;
    if (flags.empty()) {
        for (const auto& binding : bindings) defaults.push_back(updateAfterBind && binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ? VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT : 0);
        flags = defaults;
    }
    Require(flags.size() == bindings.size(), "descriptor binding flag count differs from layout");
    std::lock_guard lock(mutex);
    std::vector<std::uint32_t> keyCopy(key.begin(), key.end());
    keyCopy.push_back(static_cast<std::uint32_t>(flags.size()));
    keyCopy.insert(keyCopy.end(), flags.begin(), flags.end());
    if (const auto found = layouts.find(keyCopy); found != layouts.end()) {
        ++stats.layoutHits;
        return found->second;
    }
    ValidateDescriptorLayout(context, bindings, updateAfterBind);
    ++stats.layoutMisses;
    const auto layout = CreateSetLayout(context, bindings, updateAfterBind, flags);
    try {
        layouts.emplace(std::move(keyCopy), layout);
    } catch (...) {
        destroyLayout(context.device, layout, nullptr);
        throw;
    }
    return layout;
}

namespace {
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
}
}
}
}

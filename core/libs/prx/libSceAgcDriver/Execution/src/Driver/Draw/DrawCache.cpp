#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/WriteEvidence.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <cstdlib>

namespace AgcDriver::DriverDetail {

bool DrawRecipeRecord::Matches(const std::vector<std::shared_ptr<DispatchVariant>>& variants) const {
    if (stages.size() != variants.size()) return false;
    for (std::size_t i = 0; i < stages.size(); ++i) {
        if (stages[i].owner_before(variants[i]) || variants[i].owner_before(stages[i])) return false;
    }
    return true;
}

bool DrawRecipeRecord::Expired() const {
    return std::any_of(stages.begin(), stages.end(), [](const std::weak_ptr<const DispatchVariant>& stage) { return stage.expired(); });
}

bool Driver::drawEntries() {
    static const bool entries = std::getenv("APS5_NO_DRAW_SRT_ENTRIES") == nullptr && !stampValidate();
    return entries;
}

bool Driver::drawDataHits() {
    static const bool hits = std::getenv("APS5_NO_DRAW_DATA_HITS") == nullptr;
    return hits;
}

bool Driver::verifyDrawDataHits() {
    static const bool verify = std::getenv("APS5_VERIFY_DRAW_DATA_HITS") != nullptr;
    return verify;
}

bool Driver::verifyDrawEntries() {
    static const bool verify = std::getenv("APS5_VERIFY_DRAW_ENTRIES") != nullptr;
    return verify;
}

bool Driver::registerKeyEnabled() {
    static const bool registerKey = std::getenv("APS5_NO_DRAW_KEY") == nullptr;
    return registerKey;
}

bool Driver::verifyDrawRecipe() {
    static const bool verify = std::getenv("APS5_VERIFY_DRAW_RECIPE") != nullptr;
    return verify;
}

std::size_t Driver::drawCacheEntries() {
    static const std::size_t entries = [] {
        const char* text = std::getenv("APS5_DRAW_CACHE_ENTRIES");
        const auto parsed = text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
        return parsed != 0 ? static_cast<std::size_t>(parsed) : std::size_t{4096};
    }();
    return entries;
}

void Driver::accountDrawVariant(const DispatchVariant& variant, bool added) {
    if (added) {
        ++drawCacheVariants;
        drawCacheVariantBytes += variantBytes(variant);
    } else {
        --drawCacheVariants;
        drawCacheVariantBytes -= variantBytes(variant);
    }
}

void Driver::insertDrawEntry(std::uint64_t key, std::vector<std::shared_ptr<DispatchVariant>>& fresh, std::shared_ptr<const DrawDecode> decode, std::uint64_t shape) {
    std::lock_guard cacheLock(drawCacheMutex);
    auto& counters = drawEntryCounters;
    ++counters.inserts;
    const auto found = drawCache.find(key);
    auto replacement = std::make_shared<DrawEntry>();
    replacement->stages.resize(fresh.size());
    replacement->decode = std::move(decode);
    replacement->shape = shape;
    if (found != drawCache.end()) {
        if (found->second->stages.size() == fresh.size()) replacement->stages = found->second->stages;
        if (found->second->decode != nullptr) replacement->decode = found->second->decode;
        replacement->recipes.store(found->second->recipes.load());
    }
    auto superseded = drawCache.end();
    if (found == drawCache.end() && shape != 0) {
        if (const auto previous = drawShapes.find(shape); previous != drawShapes.end() && previous->second != key) {
            superseded = drawCache.find(previous->second);
        }
    }
    for (std::size_t i = 0; i < fresh.size(); ++i) {
        if (fresh[i] == nullptr) continue;
        auto& variants = replacement->stages[i];
        const auto present = std::find_if(variants.begin(), variants.end(), [&](const std::shared_ptr<DispatchVariant>& kept) { return kept->pushOffset == fresh[i]->pushOffset && kept->runs == fresh[i]->runs && kept->words == fresh[i]->words; });
        if (present != variants.end()) {
            ++counters.present;
            fresh[i] = *present;
            continue;
        }
        accountDrawVariant(*fresh[i], true);
        variants.insert(variants.begin(), fresh[i]);
        ++counters.variantsInserted;
        while (variants.size() > dispatchVariants()) {
            accountDrawVariant(*variants.back(), false);
            variants.pop_back();
            ++counters.variantsEvicted;
        }
    }
    replacement->touched = drawCacheHits;
    if (superseded != drawCache.end()) {
        for (const auto& variants : superseded->second->stages) {
            for (const auto& variant : variants) accountDrawVariant(*variant, false);
        }
        drawOrder.erase(superseded->second->order);
        drawCache.erase(superseded);
        ++counters.superseded;
    }
    if (replacement->decode != nullptr && shape != 0) drawShapes[shape] = key;
    if (found == drawCache.end()) {
        drawOrder.push_front(key);
        replacement->order = drawOrder.begin();
        drawCache.emplace(key, std::move(replacement));
    } else {
        replacement->order = found->second->order;
        drawOrder.splice(drawOrder.begin(), drawOrder, replacement->order);
        found->second = std::move(replacement);
    }
    while (drawCache.size() > drawCacheEntries()) {
        const auto last = drawCache.find(drawOrder.back());
        for (const auto& variants : last->second->stages) {
            for (const auto& variant : variants) accountDrawVariant(*variant, false);
        }
        if (traceDrawCache()) traceInsert(traceKeysEvicted, last->first);
        if (const auto shapeOf = drawShapes.find(last->second->shape); shapeOf != drawShapes.end() && shapeOf->second == last->first) drawShapes.erase(shapeOf);
        drawOrder.erase(last->second->order);
        drawCache.erase(last);
        ++drawCacheEvictions;
        ++counters.evictions;
    }
}

std::shared_ptr<const DrawRecipe> Driver::findDrawRecipe(std::uint64_t key, const std::vector<std::shared_ptr<DispatchVariant>>& stages) {
    std::shared_ptr<const std::vector<DrawRecipeRecord>> records;
    {
        std::lock_guard cacheLock(drawCacheMutex);
        const auto found = drawCache.find(key);
        if (found == drawCache.end()) return nullptr;
        records = found->second->recipes.load();
    }
    if (records == nullptr) return nullptr;
    for (const auto& record : *records) {
        if (record.Matches(stages)) return record.recipe;
    }
    return nullptr;
}

void Driver::attachDrawRecipe(std::uint64_t key, const std::vector<std::shared_ptr<DispatchVariant>>& stages, std::shared_ptr<const DrawRecipe> recipe) {
    std::shared_ptr<DrawEntry> entry;
    {
        std::lock_guard cacheLock(drawCacheMutex);
        const auto found = drawCache.find(key);
        if (found == drawCache.end()) return;
        entry = found->second;
    }
    const auto old = entry->recipes.load();
    auto records = std::make_shared<std::vector<DrawRecipeRecord>>();
    records->push_back({std::vector<std::weak_ptr<const DispatchVariant>>(stages.begin(), stages.end()), std::move(recipe)});
    if (old != nullptr) {
        for (const auto& record : *old) {
            if (record.Matches(stages) || record.Expired() || records->size() >= dispatchVariants()) continue;
            records->push_back(record);
        }
    }
    entry->recipes.store(std::move(records));
    VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Attach, VulkanDevice::RecipeKind::Draw);
}

std::chrono::milliseconds Driver::drawFailureRetry() {
    static const std::chrono::milliseconds retry{[] { const char* text = std::getenv("APS5_DRAW_FAILURE_RETRY_MS"); return text != nullptr ? std::strtoll(text, nullptr, 10) : 500ll; }()};
    return retry;
}

std::uint64_t Driver::drawFailureKey(std::uint64_t drawKey, const Pm4::DrawParameters& parameters) {
    std::uint64_t key = drawKey;
    for (const std::uint64_t value : {parameters.indexAddress, static_cast<std::uint64_t>(parameters.indexCount), static_cast<std::uint64_t>(parameters.indexSize), static_cast<std::uint64_t>(parameters.instanceCount), static_cast<std::uint64_t>(parameters.flags), static_cast<std::uint64_t>(parameters.indexed), static_cast<std::uint64_t>(parameters.firstVertex), static_cast<std::uint64_t>(parameters.firstInstance)}) {
        key ^= value;
        key *= 0x100000001b3ull;
    }
    return key;
}

bool Driver::drawFailureCurrent(const DrawFailure& failure) {
    if (failure.runs.empty()) return true;
    if ((GuestMemory::ForgetSerial() & 1) != 0) return false;
    if (Graphics::StorageTexture::AnyPendingOverlaps(failure.runs) || Graphics::AnyShadowedOverlaps(failure.runs)) return false;
    for (const auto& label : deferredLabels().labels) {
        for (const auto& [begin, end] : failure.runs) {
            if (label.address < end && begin < label.address + label.size) return false;
        }
    }
    PendingView pending;
    pending.Load();
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DrawCache);
    std::size_t offset = 0;
    for (const auto& [begin, end] : failure.runs) {
        const auto size = static_cast<std::size_t>(end - begin);
        if (pending.Overlaps(begin, size)) return false;
        if (GuestMemory::CompareMapped(begin, std::span<const std::byte>(failure.bytes).subspan(offset, size)) != GuestMemory::Compare::Equal) return false;
        offset += size;
    }
    return true;
}

std::shared_ptr<const DrawFailure> Driver::currentDrawFailure(std::uint64_t key) {
    std::shared_ptr<const DrawFailure> failure;
    {
        std::lock_guard cacheLock(drawCacheMutex);
        const auto found = drawFailures.find(key);
        if (found == drawFailures.end()) return nullptr;
        failure = found->second;
        if (std::chrono::steady_clock::now() - failure->recorded > drawFailureRetry()) {
            drawFailures.erase(found);
            drawFailureCount.store(drawFailures.size(), std::memory_order_relaxed);
            ++drawEntryCounters.failureRetries;
            return nullptr;
        }
    }
    const bool current = drawFailureCurrent(*failure);
    std::lock_guard cacheLock(drawCacheMutex);
    if (!current) {
        const auto found = drawFailures.find(key);
        if (found != drawFailures.end() && found->second == failure) drawFailures.erase(found);
        drawFailureCount.store(drawFailures.size(), std::memory_order_relaxed);
        ++drawEntryCounters.failureChanged;
        return nullptr;
    }
    ++drawEntryCounters.failureSkips;
    return failure;
}

PendingDrawFailure& Driver::pendingDrawFailure() {
    static thread_local PendingDrawFailure pending;
    return pending;
}

void Driver::failureRuns(std::vector<ShaderRecompiler::MemoryRegion>& regions, std::vector<std::pair<std::uint64_t, std::uint64_t>>& runs, std::vector<std::byte>& bytes) {
    std::stable_sort(regions.begin(), regions.end(), [](const ShaderRecompiler::MemoryRegion& a, const ShaderRecompiler::MemoryRegion& b) { return a.guestAddress < b.guestAddress; });
    for (const auto& region : regions) {
        if (region.bytes.empty()) continue;
        runs.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
        bytes.insert(bytes.end(), region.bytes.begin(), region.bytes.end());
    }
}

void Driver::keepPendingDrawFailure(const std::exception& error) {
    auto& pending = pendingDrawFailure();
    if (pending.key == 0) return;
    const auto key = pending.key;
    pending.key = 0;
    recordDrawFailure(key, error, pending.memoryFailures, std::move(pending.runs), std::move(pending.bytes));
    pending = {};
}

void Driver::recordDrawFailure(std::uint64_t key, const std::exception& error, std::uint64_t memoryFailures, std::vector<std::pair<std::uint64_t, std::uint64_t>> runs, std::vector<std::byte> bytes) {
    std::lock_guard cacheLock(drawCacheMutex);
    if (dynamic_cast<const std::bad_alloc*>(&error) != nullptr || Graphics::OutOfMemoryFailures() != memoryFailures) {
        ++drawEntryCounters.failureNotKept;
        return;
    }
    auto failure = std::make_shared<DrawFailure>();
    failure->message = error.what();
    failure->recorded = std::chrono::steady_clock::now();
    failure->runs = std::move(runs);
    failure->bytes = std::move(bytes);
    if (drawFailures.size() >= drawCacheEntries()) drawFailures.clear();
    drawFailures[key] = std::move(failure);
    drawFailureCount.store(drawFailures.size(), std::memory_order_relaxed);
    ++drawEntryCounters.failureRecords;
}

}

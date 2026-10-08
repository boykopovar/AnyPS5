#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/WriteEvidence.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace AgcDriver::DriverDetail {

DrawPlanCache::DrawPlanCache(std::size_t capacity) : seen(capacity) {
    if (capacity == 0) throw std::invalid_argument("draw plan cache capacity is zero");
}

std::shared_ptr<DrawEntry> DrawPlanCache::Find(std::uint64_t shape) {
    const auto found = entries.find(shape);
    if (found == entries.end()) return nullptr;
    order.splice(order.begin(), order, found->second.order);
    return found->second.draw;
}

void DrawPlanCache::Keep(std::uint64_t shape, std::shared_ptr<DrawEntry> entry) {
    if (entry == nullptr || entry->plan == nullptr) throw std::invalid_argument("draw plan cache requires a plan");
    if (const auto found = entries.find(shape); found != entries.end()) {
        found->second.draw = std::move(entry);
        order.splice(order.begin(), order, found->second.order);
        return;
    }
    order.push_front(shape);
    entries.emplace(shape, Entry{std::move(entry), order.begin()});
    if (entries.size() <= seen.size()) return;
    entries.erase(order.back());
    order.pop_back();
}

bool DrawPlanCache::Admit(std::uint64_t key) {
    auto& slot = seen[(key ^ (key >> 33u)) % seen.size()];
    const bool repeated = slot.occupied && slot.key == key;
    slot = {key, true};
    return repeated;
}

bool DrawRecipeRecord::Matches(std::span<const std::shared_ptr<DispatchVariant>> variants) const {
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

void Driver::insertDrawEntry(std::uint64_t key, std::span<DrawStage> work, std::shared_ptr<const DrawPlan> decode, std::span<const DrawProgram> programs, std::uint64_t shape) {
    std::lock_guard cacheLock(drawCacheMutex);
    auto& counters = drawEntryCounters;
    const auto found = drawCache.find(key);
    auto replacement = std::make_shared<DrawEntry>();
    replacement->stages.resize(work.size());
    replacement->plan = std::move(decode);
    replacement->programs.assign(programs.begin(), programs.end());
    if (found != drawCache.end()) {
        if (found->second.entry->stages.size() == work.size()) replacement->stages = found->second.entry->stages;
        replacement->recipes.store(found->second.entry->recipes.load());
    }
    for (std::size_t i = 0; i < work.size(); ++i) {
        if (work[i].fresh == nullptr) continue;
        auto& variants = replacement->stages[i];
        const auto present = std::find_if(variants.begin(), variants.end(), [&](const std::shared_ptr<DispatchVariant>& kept) { return kept->pushOffset == work[i].fresh->pushOffset && kept->runs == work[i].fresh->runs && kept->words == work[i].fresh->words; });
        if (present != variants.end()) {
            ++counters.present;
            work[i].fresh = *present;
            continue;
        }
        variants.insert(variants.begin(), work[i].fresh);
        ++counters.variantsInserted;
        while (variants.size() > dispatchVariants()) {
            variants.pop_back();
            ++counters.variantsEvicted;
        }
    }
    if (replacement->plan != nullptr && shape != 0) drawPlans.Keep(shape, replacement);
    if (found == drawCache.end() && !drawPlans.Admit(key)) {
        ++counters.admissionsSkipped;
        return;
    }
    ++counters.inserts;
    if (found != drawCache.end()) {
        for (const auto& variants : found->second.entry->stages) {
            for (const auto& variant : variants) accountDrawVariant(*variant, false);
        }
    }
    for (const auto& variants : replacement->stages) {
        for (const auto& variant : variants) accountDrawVariant(*variant, true);
    }
    if (found == drawCache.end()) {
        drawOrder.push_front(key);
        drawCache.emplace(key, DrawCacheSlot{std::move(replacement), drawOrder.begin(), drawCacheHits});
    } else {
        drawOrder.splice(drawOrder.begin(), drawOrder, found->second.order);
        found->second.entry = std::move(replacement);
        found->second.touched = drawCacheHits;
    }
    while (drawCache.size() > drawCacheEntries()) {
        const auto last = drawCache.find(drawOrder.back());
        for (const auto& variants : last->second.entry->stages) {
            for (const auto& variant : variants) accountDrawVariant(*variant, false);
        }
        if (traceDrawCache()) traceInsert(traceKeysEvicted, last->first);
        drawOrder.erase(last->second.order);
        drawCache.erase(last);
        ++drawCacheEvictions;
        ++counters.evictions;
    }
}

std::shared_ptr<const DrawRecipe> Driver::findDrawRecipe(std::uint64_t key, std::span<const std::shared_ptr<DispatchVariant>> stages) {
    std::shared_ptr<const std::vector<DrawRecipeRecord>> records;
    {
        std::lock_guard cacheLock(drawCacheMutex);
        const auto found = drawCache.find(key);
        if (found == drawCache.end()) return nullptr;
        records = found->second.entry->recipes.load();
    }
    if (records == nullptr) return nullptr;
    for (const auto& record : *records) {
        if (record.Matches(stages)) return record.recipe;
    }
    return nullptr;
}

void Driver::attachDrawRecipe(std::uint64_t key, std::span<const std::shared_ptr<DispatchVariant>> stages, std::shared_ptr<const DrawRecipe> recipe) {
    std::shared_ptr<DrawEntry> entry;
    {
        std::lock_guard cacheLock(drawCacheMutex);
        const auto found = drawCache.find(key);
        if (found == drawCache.end()) return;
        entry = found->second.entry;
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

}

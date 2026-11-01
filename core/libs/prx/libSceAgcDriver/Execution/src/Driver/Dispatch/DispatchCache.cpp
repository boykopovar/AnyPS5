#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchCache.hpp"
#include <cstdlib>

namespace AgcDriver::DriverDetail {

bool Driver::stampValidate() {
    static const bool stamp = std::getenv("APS5_DISPATCH_STAMP_VALIDATE") != nullptr;
    return stamp;
}

bool Driver::dataHits() {
    static const bool hits = std::getenv("APS5_NO_DATA_HITS") == nullptr && VulkanDevice::TemplateDataRefresh();
    return hits;
}

bool Driver::verifyDataHits() {
    static const bool verify = std::getenv("APS5_VERIFY_DATA_HITS") != nullptr;
    return verify;
}

std::size_t Driver::dispatchVariants() {
    static const std::size_t variants = [] {
        if (stampValidate()) return std::size_t{1};
        const char* text = std::getenv("APS5_DISPATCH_VARIANTS");
        const auto parsed = text != nullptr ? std::strtoull(text, nullptr, 10) : 4ull;
        return static_cast<std::size_t>(std::clamp<unsigned long long>(parsed, 1, MaxDispatchVariants));
    }();
    return variants;
}

bool Driver::insertCompare() {
    static const bool compare = std::getenv("APS5_INSERT_COMPARE") != nullptr;
    return compare;
}

std::size_t Driver::dispatchCacheEntries() {
    static const std::size_t entries = [] {
        const char* text = std::getenv("APS5_DISPATCH_CACHE_ENTRIES");
        const auto parsed = text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
        if (parsed != 0) return static_cast<std::size_t>(parsed);
        if (stampValidate()) return std::size_t{4096};
        return dispatchVariants() > 2 ? std::size_t{8192} : std::size_t{16384};
    }();
    return entries;
}

std::uint64_t Driver::DispatchCacheBytes() {
    static const std::uint64_t bytes = [] {
        const char* text = std::getenv("APS5_DISPATCH_CACHE_BYTES");
        const auto parsed = text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
        return parsed != 0 ? static_cast<std::uint64_t>(parsed) : std::uint64_t{256} << 20u;
    }();
    return bytes;
}

EntryCounters Driver::DispatchCacheCounters() {
    std::lock_guard lock(dispatchCacheMutex);
    return entryCounters;
}

Driver::DispatchCacheFootprint Driver::DispatchCacheSize() {
    std::lock_guard lock(dispatchCacheMutex);
    return {dispatchCache.size(), dispatchCacheVariantBytes};
}

std::uint64_t Driver::variantBytes(const DispatchVariant& variant) {
    const auto held = [](const auto& values) { return static_cast<std::uint64_t>(values.capacity() * sizeof(values[0])); };
    return sizeof(DispatchVariant) + held(variant.runs) + held(variant.words) + held(variant.shared) + held(variant.dataPositions) + held(variant.dataSlots) + held(variant.runBases) + held(variant.shaderDataPatches) + held(variant.captured) + held(variant.spans) + (variant.vertexInfo != nullptr ? sizeof(ShaderRecompiler::ShaderVertexStageInfo) : 0) + (variant.memory != nullptr ? variant.memory->Bytes() : 0);
}

void Driver::accountVariant(const DispatchVariant& variant, bool added) {
    if (added) {
        ++dispatchCacheVariants;
        dispatchCacheVariantBytes += variantBytes(variant);
    } else {
        --dispatchCacheVariants;
        dispatchCacheVariantBytes -= variantBytes(variant);
    }
}

void Driver::eraseDispatchEntry(std::unordered_map<std::uint64_t, std::shared_ptr<DispatchEntry>>::iterator it) {
    for (const auto& variant : it->second->variants) accountVariant(*variant, false);
    dispatchOrder.erase(it->second->order);
    dispatchCache.erase(it);
}

}

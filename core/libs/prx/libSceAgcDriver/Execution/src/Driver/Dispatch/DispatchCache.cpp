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
        if (stampValidate())
            return std::size_t{1};
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
        if (parsed != 0)
            return static_cast<std::size_t>(parsed);
        if (stampValidate())
            return std::size_t{4096};
        return dispatchVariants() > 2 ? std::size_t{8192} : std::size_t{16384};
    }();
    return entries;
}

std::uint64_t Driver::variantBytes(const DispatchVariant& variant) {
    return sizeof(DispatchVariant) + variant.words.size() * sizeof(std::uint32_t) +
           variant.runs.size() * sizeof(std::pair<std::uint64_t, std::uint64_t>) +
           variant.captured.size() * sizeof(ShaderRecompiler::MemoryRegion) +
           (variant.vertexInfo != nullptr ? sizeof(ShaderRecompiler::ShaderVertexStageInfo) : 0);
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
    for (const auto& variant : it->second->variants)
        accountVariant(*variant, false);
    dispatchOrder.erase(it->second->order);
    dispatchCache.erase(it);
}

}

#pragma once

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace ShaderRecompiler {

enum class HeapCache : std::uint32_t {
    TemplateFront,
    Templates,
    Slots,
    StageData,
    TableMap,
    PushImage,
    DrawKey,
    DrawDataHits,
    DrawStageDataHits,
    DrawSrtEntries,
    DrawRecipe,
    DrawResourceCache,
    ResourceCache,
    ValidateCache,
    ShapeMemo,
    MovedBufferTemplates,
    ResultMemo,
    Count,
};

inline constexpr std::uint32_t HeapOnlyCacheCount = static_cast<std::uint32_t>(HeapCache::DrawKey);

inline constexpr std::array<std::string_view, static_cast<std::size_t>(HeapCache::Count)> HeapCacheNames{
    "template-front", "templates", "slots", "stage-data", "table-map", "push-image", "draw-key", "draw-data-hits", "draw-stage-data-hits", "draw-srt-entries",
    "draw-recipe", "draw-resource-cache", "resource-cache", "validate-cache", "shape-memo", "moved-buffer-templates", "result-memo",
};

[[nodiscard]] inline std::uint64_t ParseHeapCacheBypass(std::string_view text) {
    std::uint64_t mask = 0;
    constexpr auto count = static_cast<std::uint32_t>(HeapCache::Count);
    while (!text.empty()) {
        const auto comma = text.find(',');
        const auto name = text.substr(0, comma);
        text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
        if (name.empty()) continue;
        if (name == "all" || name == "1") {
            mask |= (std::uint64_t{1} << count) - 1u;
            continue;
        }
        if (name == "heap") {
            mask |= (std::uint64_t{1} << HeapOnlyCacheCount) - 1u;
            continue;
        }
        bool known = false;
        for (std::uint32_t index = 0; index < count; ++index) {
            if (HeapCacheNames[index] != name) continue;
            mask |= std::uint64_t{1} << index;
            known = true;
        }
        if (!known) throw std::runtime_error("APS5_HEAP_NO_CACHE names an unknown cache: " + std::string(name));
    }
    return mask;
}

[[nodiscard]] inline bool HeapCacheLegacyBypassed(HeapCache cache) {
    switch (cache) {
    case HeapCache::DrawKey: return std::getenv("APS5_NO_DRAW_KEY") != nullptr;
    case HeapCache::DrawDataHits: return std::getenv("APS5_NO_DRAW_DATA_HITS") != nullptr;
    case HeapCache::DrawStageDataHits: return std::getenv("APS5_NO_DRAW_STAGE_DATA_HITS") != nullptr;
    case HeapCache::DrawSrtEntries: return std::getenv("APS5_NO_DRAW_SRT_ENTRIES") != nullptr;
    case HeapCache::DrawRecipe: return std::getenv("APS5_NO_DRAW_RECIPE") != nullptr;
    case HeapCache::DrawResourceCache: return std::getenv("APS5_NO_DRAW_RESOURCE_CACHE") != nullptr;
    case HeapCache::ResourceCache: return std::getenv("APS5_NO_RESOURCE_CACHE") != nullptr;
    case HeapCache::ValidateCache: return std::getenv("APS5_NO_VALIDATE_CACHE") != nullptr;
    case HeapCache::ShapeMemo: return std::getenv("APS5_NO_SHAPE_MEMO") != nullptr;
    case HeapCache::MovedBufferTemplates: return std::getenv("APS5_NO_MOVED_BUFFER_TEMPLATES") != nullptr;
    case HeapCache::ResultMemo: return std::getenv("APS5_NO_RESULT_MEMO") != nullptr;
    default: return false;
    }
}

[[nodiscard]] inline std::uint64_t HeapCacheBypassMask() {
    static const std::uint64_t mask = [] {
        const char* text = std::getenv("APS5_HEAP_NO_CACHE");
        std::uint64_t bypassed = text == nullptr ? std::uint64_t{0} : ParseHeapCacheBypass(text);
        for (std::uint32_t index = 0; index < static_cast<std::uint32_t>(HeapCache::Count); ++index) {
            if (HeapCacheLegacyBypassed(static_cast<HeapCache>(index))) bypassed |= std::uint64_t{1} << index;
        }
        return bypassed;
    }();
    return mask;
}

[[nodiscard]] inline bool HeapCacheEnabled(HeapCache cache) {
    return (HeapCacheBypassMask() >> static_cast<std::uint32_t>(cache) & 1u) == 0u;
}

[[nodiscard]] inline bool HeapVerifyEnabled() {
    static const bool enabled = std::getenv("APS5_VERIFY_GPU_HEAP") != nullptr;
    return enabled;
}

[[nodiscard]] inline std::uint64_t HeapVerifyEvery() {
    static const std::uint64_t every = [] {
        const char* text = std::getenv("APS5_VERIFY_GPU_HEAP");
        if (text == nullptr) return std::uint64_t{0};
        std::uint64_t value = 0;
        const auto* end = text + std::strlen(text);
        if (text != end) {
            const auto parsed = std::from_chars(text, end, value);
            if (parsed.ec != std::errc{} || parsed.ptr != end) throw std::runtime_error("APS5_VERIFY_GPU_HEAP must be empty or a non-negative integer");
        }
        return value == 0 ? std::uint64_t{1} : value;
    }();
    return every;
}

}

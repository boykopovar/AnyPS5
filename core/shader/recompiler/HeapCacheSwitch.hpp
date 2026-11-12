#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
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

[[nodiscard]] inline std::uint64_t HeapCacheBypassMask() {
    static const std::uint64_t mask = [] {
        const char* text = std::getenv("APS5_HEAP_NO_CACHE");
        return text == nullptr ? std::uint64_t{0} : ParseHeapCacheBypass(text);
    }();
    return mask;
}

[[nodiscard]] inline bool HeapCacheBypassed(HeapCache cache) {
    return (HeapCacheBypassMask() >> static_cast<std::uint32_t>(cache) & 1u) != 0u;
}

}

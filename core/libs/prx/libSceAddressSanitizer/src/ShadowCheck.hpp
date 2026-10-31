#pragma once

#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

[[noreturn]] inline void InvalidArgument_nid_no_patch(const char* function) {
    throw std::invalid_argument(std::string(function) + ": address range outside guest memory or misaligned");
}

[[noreturn]] inline void ShadowUnavailable_nid_no_patch(const char* function) {
    throw std::runtime_error(std::string(function) + ": shadow memory reservation failed");
}

[[nodiscard]] static std::optional<std::size_t> FindFirstPoisoned_nid_no_patch(const std::int8_t* shadow, std::size_t count, std::size_t start, std::size_t size) {
    if (size == 0) {
        return std::nullopt;
    }
    if (!shadow) {
        InvalidArgument_nid_no_patch(__func__);
    }
    if (start + size < start) {
        InvalidArgument_nid_no_patch(__func__);
    }
    std::size_t end = start + size;
    std::size_t lastGranule = (end - 1) >> 3;
    if (count <= lastGranule) {
        InvalidArgument_nid_no_patch(__func__);
    }
    for (std::size_t g = start >> 3; g <= lastGranule; ++g) {
        std::int8_t val = shadow[g];
        if (val == 0 || val >= 8) {
            continue;
        }
        std::size_t p = (g << 3) + (val < 0 ? 0 : static_cast<std::size_t>(val));
        if (p < start) {
            p = start;
        }
        if (p < end) {
            return p;
        }
    }
    return std::nullopt;
}

static void UpdateStackShadow_nid_no_patch(std::int8_t* shadow, std::size_t shadowCount, std::size_t first, std::size_t size, bool poison) {
    if (size == 0) {
        return;
    }
    if (first % 8 != 0 || size > std::numeric_limits<std::size_t>::max() - first || !shadow) {
        InvalidArgument_nid_no_patch(__func__);
    }
    std::size_t startIdx = first / 8;
    std::size_t complete = size / 8;
    std::size_t remainder = size % 8;
    std::size_t totalGranules = complete + (remainder != 0 ? 1 : 0);
    if (startIdx > shadowCount || totalGranules > shadowCount - startIdx) {
        InvalidArgument_nid_no_patch(__func__);
    }
    for (std::size_t i = 0; i < complete; ++i) {
        shadow[startIdx + i] = poison ? static_cast<std::int8_t>(0xf8) : 0;
    }
    if (remainder != 0) {
        std::size_t finalIdx = startIdx + complete;
        std::int8_t orig = shadow[finalIdx];
        if (poison) {
            if (orig > 0 && orig <= static_cast<std::int8_t>(remainder)) {
                shadow[finalIdx] = static_cast<std::int8_t>(0xf8);
            }
        } else {
            if (orig != 0) {
                shadow[finalIdx] = std::max(orig, static_cast<std::int8_t>(remainder));
            }
        }
    }
}

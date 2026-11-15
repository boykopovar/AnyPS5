#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <stdexcept>

static_assert(std::atomic_ref<std::int32_t>::required_alignment <= alignof(std::int32_t));

inline int Retain_nid_no_patch(std::int32_t& references) {
    std::atomic_ref<std::int32_t> count(references);
    auto previous = count.load();
    for (;;) {
        if (previous == std::numeric_limits<std::int32_t>::max())
            throw std::overflow_error("LibreSSL3 reference count overflow");
        if (count.compare_exchange_weak(previous, previous + 1))
            return previous > 0;
    }
}

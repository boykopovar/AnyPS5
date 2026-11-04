#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace AgcDriver::Tests {

inline std::array<std::size_t, 2> TargetAddressWindowPair(std::uintptr_t base, std::size_t blockBytes) {
    const auto first = base;
    const auto second = first + blockBytes;
    const auto third = second + blockBytes;
    if ((first >> 32u) == (second >> 32u)) return {0u, 1u};
    if ((second >> 32u) == (third >> 32u)) return {1u, 2u};
    throw std::runtime_error("three contiguous blocks do not contain a same-window target pair");
}

}

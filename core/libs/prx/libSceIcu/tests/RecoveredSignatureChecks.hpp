#pragma once

#include "Testing.hpp"
#include <array>
#include <cstdint>
#include <exception>
#include <source_location>

namespace IcuRecoveredTest {

inline constexpr std::uint32_t FillerWord = 0xa5a5a5a5;

template<typename TAction>
void RequireNoThrow(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::exception&) {
        Fail("an exception was thrown", location);
    }
}

inline std::array<std::uint32_t, 8> Filler() {
    std::array<std::uint32_t, 8> storage{};
    storage.fill(FillerWord);
    return storage;
}

inline bool Unchanged(const std::array<std::uint32_t, 8>& storage) {
    for (const auto word : storage) {
        if (word != FillerWord) return false;
    }
    return true;
}

} // namespace IcuRecoveredTest

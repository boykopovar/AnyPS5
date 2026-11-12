#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>

namespace IcuRecoveredTest {

inline constexpr std::uint32_t FillerWord = 0xa5a5a5a5;

inline void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU recovered signature check failed at %s:%u\n", location.file_name(),
                 static_cast<unsigned>(location.line()));
    std::abort();
}

inline void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU recovered signature check failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, location);
}

template<typename TAction>
void RequireNoThrow(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::exception&) {
        Require(false, location);
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

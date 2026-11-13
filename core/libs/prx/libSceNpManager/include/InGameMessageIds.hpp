#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

inline std::int32_t TakeInGameMessageId(std::int32_t& next, const char* function) {
    if (next == std::numeric_limits<std::int32_t>::max()) throw std::runtime_error(std::string(function) + ": the in-game message ids are exhausted");
    return next++;
}

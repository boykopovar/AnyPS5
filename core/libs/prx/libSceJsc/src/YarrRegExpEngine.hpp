#pragma once

#include <cstddef>

namespace Jsc::YarrEngine {

inline constexpr std::size_t StackBudget = 128u << 10;
inline constexpr unsigned TimeoutPolls = 100;

struct State {
    const char* stackBase = nullptr;
    unsigned polls = 0;
};

}

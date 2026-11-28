#pragma once

#include <cstdint>
#include <limits>

namespace WebCore {

inline int ClampToInteger(float value) {
    if (value >= static_cast<float>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    if (!(value > static_cast<float>(std::numeric_limits<int>::min()))) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

inline int ClampToInteger(double value) {
    if (value >= static_cast<double>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }
    if (!(value > static_cast<double>(std::numeric_limits<int>::min()))) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

inline int SaturatedSum(int a, int b) {
    std::int64_t sum = static_cast<std::int64_t>(a) + b;
    if (sum > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    if (sum < std::numeric_limits<int>::min()) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(sum);
}

inline int SaturatedDifference(int a, int b) {
    std::int64_t difference = static_cast<std::int64_t>(a) - b;
    if (difference > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    if (difference < std::numeric_limits<int>::min()) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(difference);
}

}

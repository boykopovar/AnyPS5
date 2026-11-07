#include <ps5lift/State.h>

#include <algorithm>
#include <cstdint>

namespace {

constexpr std::uint32_t Words = 1u << 0;
constexpr std::uint32_t Signed = 1u << 1;
constexpr std::uint32_t EqualAny = 0;
constexpr std::uint32_t Ranges = 1;
constexpr std::uint32_t EqualEach = 2;
constexpr std::uint32_t Negative = 1;
constexpr std::uint32_t MaskedNegative = 3;

struct Operand {
    std::uint64_t Low;
    std::uint64_t High;
};

std::int32_t Element(const Operand& operand, const unsigned index, const std::uint32_t control) {
    const unsigned bits = (control & Words) ? 16 : 8;
    const unsigned shift = index * bits;
    const auto half = shift < 64 ? operand.Low : operand.High;
    const auto raw = static_cast<std::uint32_t>((half >> (shift % 64)) & ((1u << bits) - 1));
    if (!(control & Signed)) return static_cast<std::int32_t>(raw);
    return (control & Words) ? static_cast<std::int16_t>(raw) : static_cast<std::int8_t>(raw);
}

unsigned ImplicitLength(const Operand& operand, const unsigned count, const std::uint32_t control) {
    for (unsigned index = 0; index < count; ++index) {
        if (Element(operand, index, control) == 0) return index;
    }
    return count;
}

unsigned ExplicitLength(const std::int64_t value, const unsigned count) {
    const auto magnitude = value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
    return static_cast<unsigned>(std::min<std::uint64_t>(magnitude, count));
}

std::uint32_t Aggregate(const Operand& first, const Operand& second, const unsigned firstLength, const unsigned secondLength, const unsigned count, const std::uint32_t control) {
    const auto a = [&](const unsigned index) { return Element(first, index, control); };
    const auto b = [&](const unsigned index) { return Element(second, index, control); };
    std::uint32_t result = 0;
    switch ((control >> 2) & 3) {
    case EqualAny:
        for (unsigned j = 0; j < secondLength; ++j) {
            for (unsigned i = 0; i < firstLength; ++i) {
                if (a(i) == b(j)) {
                    result |= 1u << j;
                    break;
                }
            }
        }
        return result;
    case Ranges:
        for (unsigned j = 0; j < secondLength; ++j) {
            for (unsigned i = 0; i + 1 < firstLength; i += 2) {
                if (a(i) <= b(j) && b(j) <= a(i + 1)) {
                    result |= 1u << j;
                    break;
                }
            }
        }
        return result;
    case EqualEach:
        for (unsigned i = 0; i < count; ++i) {
            const bool firstValid = i < firstLength;
            const bool secondValid = i < secondLength;
            if (firstValid == secondValid && (!firstValid || a(i) == b(i))) result |= 1u << i;
        }
        return result;
    default:
        for (unsigned j = 0; j < count; ++j) {
            bool match = true;
            for (unsigned k = 0; k < firstLength && j + k < count && match; ++k) match = j + k < secondLength && a(k) == b(j + k);
            if (match) result |= 1u << j;
        }
        return result;
    }
}

}

extern "C" uint32_t ps5lift_string_compare(const uint64_t firstLow, const uint64_t firstHigh, const uint64_t secondLow, const uint64_t secondHigh, const int64_t firstLength,
                                           const int64_t secondLength, const uint32_t control) {
    const Operand first{firstLow, firstHigh};
    const Operand second{secondLow, secondHigh};
    const unsigned count = (control & Words) ? 8 : 16;
    const bool implicit = (control & PS5LIFT_STRING_IMPLICIT) != 0;
    const auto firstValid = implicit ? ImplicitLength(first, count, control) : ExplicitLength(firstLength, count);
    const auto secondValid = implicit ? ImplicitLength(second, count, control) : ExplicitLength(secondLength, count);
    auto result = Aggregate(first, second, firstValid, secondValid, count, control);
    const auto polarity = (control >> 4) & 3;
    if (polarity == Negative) result ^= (1u << count) - 1;
    if (polarity == MaskedNegative) result ^= (1u << secondValid) - 1;
    return result | (secondValid < count ? PS5LIFT_STRING_SHORT_SECOND : 0) | (firstValid < count ? PS5LIFT_STRING_SHORT_FIRST : 0);
}

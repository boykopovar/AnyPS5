#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

namespace {

constexpr std::int32_t SetMagic = 0x55534554;
constexpr std::int32_t MaximumCodePoint = 0x10ffff;
constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t IndexOutOfBoundsError = 8;

struct Range {
    std::int32_t start;
    std::int32_t end;
};

std::int32_t Pin(std::int32_t codePoint) {
    return std::clamp(codePoint, 0, MaximumCodePoint);
}

struct CodePointSet {
    std::int32_t magic = SetMagic;
    std::vector<Range> ranges;

    void Add(std::int32_t start, std::int32_t end) {
        auto first = std::lower_bound(ranges.begin(), ranges.end(), start, [](const Range& range, std::int32_t value) {
            return range.end + 1 < value;
        });
        auto last = first;
        while (last != ranges.end() && last->start <= end + 1) {
            start = std::min(start, last->start);
            end = std::max(end, last->end);
            ++last;
        }
        first = ranges.erase(first, last);
        ranges.insert(first, Range{start, end});
    }

    bool Contains(std::int32_t codePoint) const {
        if (codePoint < 0 || codePoint > MaximumCodePoint) return false;
        const auto found = std::lower_bound(ranges.begin(), ranges.end(), codePoint, [](const Range& range, std::int32_t value) {
            return range.end < value;
        });
        return found != ranges.end() && found->start <= codePoint;
    }
};

const CodePointSet& Check(const USet* set) {
    if (!set) APS5_INVALID_ARG_EX;
    std::int32_t magic = 0;
    std::memcpy(&magic, set, sizeof(magic));
    if (magic != SetMagic) APS5_INVALID_ARG_EX;
    return *reinterpret_cast<const CodePointSet*>(set);
}

CodePointSet& Check(USet* set) {
    return const_cast<CodePointSet&>(Check(static_cast<const USet*>(set)));
}

}

extern "C" {

USet* APS5_VABI uset_openEmpty_nid_postfix() {
    return reinterpret_cast<USet*>(new (std::nothrow) CodePointSet());
}

void APS5_VABI uset_close_nid_postfix(USet* set) {
    if (!set) return;
    auto& target = Check(set);
    target.magic = 0;
    delete &target;
}

void APS5_VABI uset_add_nid_postfix(USet* set, std::int32_t codePoint) {
    const std::int32_t pinned = Pin(codePoint);
    Check(set).Add(pinned, pinned);
}

void APS5_VABI uset_addRange_nid_postfix(USet* set, std::int32_t start, std::int32_t end) {
    auto& target = Check(set);
    start = Pin(start);
    end = Pin(end);
    if (start <= end) target.Add(start, end);
}

void APS5_VABI uset_addAll_nid_postfix(USet* set, const USet* additionalSet) {
    auto& target = Check(set);
    const std::vector<Range> ranges = Check(additionalSet).ranges;
    for (const auto& range : ranges) target.Add(range.start, range.end);
}

std::int8_t APS5_VABI uset_contains_nid_postfix(const USet* set, std::int32_t codePoint) {
    return Check(set).Contains(codePoint);
}

std::int32_t APS5_VABI uset_getItemCount_nid_postfix(const USet* set) {
    return static_cast<std::int32_t>(Check(set).ranges.size());
}

std::int32_t APS5_VABI uset_getItem_nid_postfix(const USet* set, std::int32_t itemIndex, std::int32_t* start, std::int32_t* end,
                                                char16_t*, std::int32_t, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    if (*error > 0) return 0;
    if (itemIndex < 0) {
        *error = IllegalArgumentError;
        return -1;
    }
    const auto& ranges = Check(set).ranges;
    if (static_cast<std::size_t>(itemIndex) >= ranges.size()) {
        *error = IndexOutOfBoundsError;
        return -1;
    }
    if (!start || !end) APS5_INVALID_ARG_EX;
    *start = ranges[static_cast<std::size_t>(itemIndex)].start;
    *end = ranges[static_cast<std::size_t>(itemIndex)].end;
    return 0;
}

}

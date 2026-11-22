#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <cstdint>

namespace {

bool Lead(std::int32_t unit) { return unit >= 0xd800 && unit <= 0xdbff; }
bool Trail(std::int32_t unit) { return unit >= 0xdc00 && unit <= 0xdfff; }

std::int64_t Measure(const char16_t* text) {
    std::int64_t length = 0;
    while (text[length] != 0) ++length;
    return length;
}

std::int32_t CodePointOrder(const char16_t* text, std::int64_t index, std::int64_t length) {
    std::int32_t unit = text[index];
    const bool pair = (Lead(unit) && (length < 0 || index + 1 < length) && Trail(text[index + 1])) ||
                      (Trail(unit) && index > 0 && Lead(text[index - 1]));
    return pair ? unit : unit - 0x2800;
}

}

extern "C" {

std::int32_t APS5_VABI u_strCompare_nid_postfix(const char16_t* first, std::int32_t firstLength, const char16_t* second,
                                                std::int32_t secondLength, std::int8_t codePointOrder) {
    if (!first || firstLength < -1 || !second || secondLength < -1) return 0;
    const bool terminated = firstLength < 0 && secondLength < 0;
    std::int64_t limit1 = firstLength;
    std::int64_t limit2 = secondLength;
    std::int32_t equalResult = 0;
    std::int64_t index = 0;
    if (terminated) {
        if (first == second) return 0;
        for (; first[index] == second[index]; ++index) {
            if (first[index] == 0) return 0;
        }
    } else {
        if (limit1 < 0) limit1 = Measure(first);
        if (limit2 < 0) limit2 = Measure(second);
        const auto common = limit1 < limit2 ? limit1 : limit2;
        equalResult = limit1 < limit2 ? -1 : limit1 > limit2 ? 1 : 0;
        if (first == second) return equalResult;
        for (; index < common && first[index] == second[index]; ++index) {}
        if (index == common) return equalResult;
    }
    std::int32_t unit1 = first[index];
    std::int32_t unit2 = second[index];
    if (unit1 >= 0xd800 && unit2 >= 0xd800 && codePointOrder) {
        unit1 = CodePointOrder(first, index, terminated ? -1 : limit1);
        unit2 = CodePointOrder(second, index, terminated ? -1 : limit2);
    }
    return unit1 - unit2;
}

}

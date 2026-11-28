#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

bool Lead(std::int32_t unit) { return unit >= 0xd800 && unit <= 0xdbff; }
bool Trail(std::int32_t unit) { return unit >= 0xdc00 && unit <= 0xdfff; }

class Text {
public:
    explicit Text(const char16_t* guest) : guest_(guest) {}

    char16_t operator[](std::int64_t index) {
        if (index < base_ || index >= base_ + count_) Load(index);
        return chunk_[index - base_];
    }

private:
    void Load(std::int64_t index) {
        const auto* source = guest_ + index;
        const auto address = reinterpret_cast<std::uintptr_t>(source);
        const std::size_t count = (std::max)(std::size_t{1}, (0x1000 - address % 0x1000) / sizeof(char16_t));
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(chunk_, source, count * sizeof(char16_t));
        errno = savedErrno;
        if (!copied) throw std::invalid_argument("u_strCompare: unreadable string");
        base_ = index;
        count_ = static_cast<std::int64_t>(count);
    }

    const char16_t* guest_;
    char16_t chunk_[0x800];
    std::int64_t base_ = 0;
    std::int64_t count_ = 0;
};

std::int64_t Measure(Text& text) {
    std::int64_t length = 0;
    while (text[length] != 0) ++length;
    return length;
}

std::int32_t CodePointOrder(Text& text, std::int64_t index, std::int64_t length) {
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
    Text left(first);
    Text right(second);
    if (terminated) {
        if (first == second) return 0;
        for (; left[index] == right[index]; ++index) {
            if (left[index] == 0) return 0;
        }
    } else {
        if (limit1 < 0) limit1 = Measure(left);
        if (limit2 < 0) limit2 = Measure(right);
        const auto common = limit1 < limit2 ? limit1 : limit2;
        equalResult = limit1 < limit2 ? -1 : limit1 > limit2 ? 1 : 0;
        if (first == second) return equalResult;
        for (; index < common && left[index] == right[index]; ++index) {}
        if (index == common) return equalResult;
    }
    std::int32_t unit1 = left[index];
    std::int32_t unit2 = right[index];
    if (unit1 >= 0xd800 && unit2 >= 0xd800 && codePointOrder) {
        unit1 = CodePointOrder(left, index, terminated ? -1 : limit1);
        unit2 = CodePointOrder(right, index, terminated ? -1 : limit2);
    }
    return unit1 - unit2;
}

}

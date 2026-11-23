#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

class UnitReader {
public:
    explicit UnitReader(const char16_t* base) : base_(base) {}

    char16_t At(std::size_t index) {
        if (index < start_ || index - start_ >= count_) Load(index);
        return buffer_[index - start_];
    }

private:
    static constexpr std::size_t PageBytes = 0x1000;
    static constexpr std::size_t FirstChunkUnits = 16;

    void Load(std::size_t index) {
        const auto address = reinterpret_cast<std::uintptr_t>(base_);
        if (index > (std::numeric_limits<std::uintptr_t>::max() - address) / sizeof(char16_t)) {
            throw std::invalid_argument("ICU: unreadable string");
        }
        const auto at = address + index * sizeof(char16_t);
        const std::size_t pageUnits = std::max<std::size_t>((PageBytes - at % PageBytes) / sizeof(char16_t), 1);
        const std::size_t units = std::min(pageUnits, chunkUnits_);
        const bool copied = GuestCopyIn_nid_no_patch(buffer_.data(), reinterpret_cast<const void*>(at), units * sizeof(char16_t));
        if (!copied) throw std::invalid_argument("ICU: unreadable string");
        start_ = index;
        count_ = units;
        chunkUnits_ = std::min(chunkUnits_ * 2, buffer_.size());
    }

    const char16_t* base_;
    std::array<char16_t, PageBytes / sizeof(char16_t)> buffer_;
    std::size_t start_ = 0;
    std::size_t count_ = 0;
    std::size_t chunkUnits_ = FirstChunkUnits;
};

std::int32_t Length(const char16_t* text) {
    if (text == nullptr) throw std::invalid_argument("ICU: null string");
    UnitReader reader(text);
    std::int32_t length = 0;
    while (reader.At(static_cast<std::size_t>(length)) != 0) {
        if (length == std::numeric_limits<std::int32_t>::max()) throw std::overflow_error("ICU: string length exceeds int32");
        ++length;
    }
    return length;
}

void RequireBuffers(const char16_t* first, const char16_t* second) {
    if (first == nullptr || second == nullptr) throw std::invalid_argument("ICU: null comparison buffer");
}

bool Lead(char16_t value) {
    return value >= 0xd800 && value <= 0xdbff;
}

bool Trail(char16_t value) {
    return value >= 0xdc00 && value <= 0xdfff;
}

bool Paired(UnitReader& text, std::size_t length, std::size_t index, char16_t value) {
    return (Lead(value) && index + 1 < length && Trail(text.At(index + 1))) ||
           (Trail(value) && index != 0 && Lead(text.At(index - 1)));
}

} // namespace

extern "C" {

std::int32_t APS5_VABI u_strlen_nid_postfix(const char16_t* text) {
    return Length(text);
}

std::int32_t APS5_VABI u_strcmp_nid_postfix(const char16_t* first, const char16_t* second) {
    RequireBuffers(first, second);
    UnitReader left(first);
    UnitReader right(second);
    for (std::size_t index = 0;; ++index) {
        const auto unit = left.At(index);
        const auto difference = static_cast<std::int32_t>(unit) - static_cast<std::int32_t>(right.At(index));
        if (difference != 0 || unit == 0) return difference;
    }
}

std::int32_t APS5_VABI u_strncmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    if (count <= 0) return 0;
    RequireBuffers(first, second);
    UnitReader left(first);
    UnitReader right(second);
    for (std::int32_t index = 0; index < count; ++index) {
        const auto unit = left.At(static_cast<std::size_t>(index));
        const auto difference = static_cast<std::int32_t>(unit) - static_cast<std::int32_t>(right.At(static_cast<std::size_t>(index)));
        if (difference != 0 || unit == 0) return difference;
    }
    return 0;
}

std::int32_t APS5_VABI u_memcmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    if (count <= 0) return 0;
    RequireBuffers(first, second);
    UnitReader left(first);
    UnitReader right(second);
    for (std::int32_t index = 0; index < count; ++index) {
        const auto difference = static_cast<std::int32_t>(left.At(static_cast<std::size_t>(index))) -
                                static_cast<std::int32_t>(right.At(static_cast<std::size_t>(index)));
        if (difference != 0) return difference;
    }
    return 0;
}

std::int32_t APS5_VABI u_memcmpCodePointOrder_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    if (count == 0 || first == second) return 0;
    RequireBuffers(first, second);
    const auto firstLength = count < 0 ? static_cast<std::size_t>(Length(first)) + 1 : static_cast<std::size_t>(count);
    const auto secondLength = count < 0 ? static_cast<std::size_t>(Length(second)) + 1 : static_cast<std::size_t>(count);
    const auto common = std::min(firstLength, secondLength);
    UnitReader left(first);
    UnitReader right(second);
    for (std::size_t index = 0; index < common; ++index) {
        const auto leftUnit = left.At(index);
        const auto rightUnit = right.At(index);
        std::int32_t leftValue = leftUnit;
        std::int32_t rightValue = rightUnit;
        if (leftValue == rightValue) continue;
        if (leftValue >= 0xd800 && rightValue >= 0xd800) {
            if (!Paired(left, firstLength, index, leftUnit)) leftValue -= 0x2800;
            if (!Paired(right, secondLength, index, rightUnit)) rightValue -= 0x2800;
        }
        return leftValue - rightValue;
    }
    return 0;
}

}

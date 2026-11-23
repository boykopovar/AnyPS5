#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"

namespace {

void CopyUnits(std::uint16_t* destination, const std::uint16_t* source, std::size_t count, const char* caller) {
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, count * sizeof(std::uint16_t));
    if (!copied) throw std::invalid_argument(std::string(caller) + ": unreadable text");
}

std::vector<std::uint16_t> ReadText(const std::uint16_t* text, std::int32_t length, const char* caller) {
    std::vector<std::uint16_t> units;
    if (length >= 0) {
        units.resize(static_cast<std::size_t>(length));
        CopyUnits(units.data(), text, units.size(), caller);
        return units;
    }
    for (;;) {
        std::uint16_t chunk[0x800];
        const auto address = reinterpret_cast<std::uintptr_t>(text + units.size());
        const std::size_t count = (std::max)(std::size_t{1}, (0x1000 - address % 0x1000) / sizeof(std::uint16_t));
        CopyUnits(chunk, text + units.size(), count, caller);
        for (std::size_t index = 0; index < count; ++index) {
            units.push_back(chunk[index]);
            if (chunk[index] == 0) return units;
        }
    }
}

bool IsLead_nid_no_patch(std::uint16_t value) {
    return value >= 0xd800 && value <= 0xdbff;
}

bool IsTrail_nid_no_patch(std::uint16_t value) {
    return value >= 0xdc00 && value <= 0xdfff;
}

std::uint16_t* FindScalar_nid_no_patch(const std::uint16_t* text, std::int32_t scalar, std::int32_t length, bool last) {
    std::uint16_t* result = nullptr;
    for (std::size_t offset = 0; length < 0 || offset < static_cast<std::size_t>(length);) {
        const auto unit = text[offset];
        std::uint32_t value = unit;
        std::size_t width = 1;
        if (IsLead_nid_no_patch(unit) && (length < 0 || offset + 1 < static_cast<std::size_t>(length)) && IsTrail_nid_no_patch(text[offset + 1])) {
            value = 0x10000 + ((unit - 0xd800) << 10) + text[offset + 1] - 0xdc00;
            width = 2;
        }
        if (value == static_cast<std::uint32_t>(scalar)) {
            result = const_cast<std::uint16_t*>(text + offset);
            if (!last) return result;
        }
        if (length < 0 && unit == 0) break;
        offset += width;
    }
    return result;
}

} // namespace

extern "C" {

std::int32_t APS5_VABI u_countChar32_nid_postfix(const std::uint16_t* guestText, std::int32_t length) {
    if (!guestText || length < -1) return 0;
    const auto units = ReadText(guestText, length, __func__);
    const auto* text = units.data();
    std::int32_t count = 0;
    for (std::size_t offset = 0; length < 0 || offset < static_cast<std::size_t>(length); ++count) {
        const auto unit = text[offset];
        if (length < 0 && unit == 0) break;
        if (count == std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
        ++offset;
        if (IsLead_nid_no_patch(unit) && (length < 0 || offset < static_cast<std::size_t>(length)) && IsTrail_nid_no_patch(text[offset])) ++offset;
    }
    return count;
}

std::int8_t APS5_VABI u_strHasMoreChar32Than_nid_postfix(const std::uint16_t* guestText, std::int32_t length, std::int32_t number) {
    if (number < 0) return 1;
    if (!guestText || length < -1) return 0;
    if (length >= 0) {
        if (length / 2 + length % 2 > number) return 1;
        if (length <= number) return 0;
    }
    const auto units = ReadText(guestText, length, __func__);
    const auto* text = units.data();
    for (std::size_t offset = 0; length < 0 || offset < static_cast<std::size_t>(length); --number) {
        const auto unit = text[offset];
        if (length < 0 && unit == 0) break;
        if (number == 0) return 1;
        ++offset;
        if (IsLead_nid_no_patch(unit) && (length < 0 || offset < static_cast<std::size_t>(length)) && IsTrail_nid_no_patch(text[offset])) ++offset;
    }
    return 0;
}

std::uint16_t* APS5_VABI u_strchr32_nid_postfix(const std::uint16_t* text, std::int32_t scalar) {
    if (static_cast<std::uint32_t>(scalar) > 0x10ffff) return nullptr;
    if (!text && scalar >= 0xd800 && scalar <= 0xdfff) return nullptr;
    if (!text) NotImplemented_nid_no_patch(__func__);
    return FindScalar_nid_no_patch(text, scalar, -1, false);
}

std::uint16_t* APS5_VABI u_strrchr32_nid_postfix(const std::uint16_t* text, std::int32_t scalar) {
    if (static_cast<std::uint32_t>(scalar) > 0x10ffff) return nullptr;
    if (!text && scalar >= 0xd800 && scalar <= 0xdfff) return nullptr;
    if (!text) NotImplemented_nid_no_patch(__func__);
    return FindScalar_nid_no_patch(text, scalar, -1, true);
}

std::uint16_t* APS5_VABI u_memchr32_nid_postfix(const std::uint16_t* text, std::int32_t scalar, std::int32_t count) {
    if (count <= 0 || static_cast<std::uint32_t>(scalar) > 0x10ffff) return nullptr;
    if (scalar >= 0x10000 && count < 2) return nullptr;
    if (!text && scalar >= 0xd800 && scalar <= 0xdfff) return nullptr;
    if (!text) NotImplemented_nid_no_patch(__func__);
    return FindScalar_nid_no_patch(text, scalar, count, false);
}

}

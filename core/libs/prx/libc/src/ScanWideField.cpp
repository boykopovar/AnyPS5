#include "prx/libc/include/ScanWideField.hpp"
#include "prx/libc/include/General.hpp"

#include <cstring>

namespace {

std::size_t DecodeUtf8(const char* text, char32_t* point) {
    const auto lead = static_cast<unsigned char>(text[0]);
    if (lead < 0x80) {
        *point = lead;
        return 1;
    }
    std::size_t size = 0;
    char32_t value = 0;
    if ((lead & 0xE0) == 0xC0) { size = 2; value = lead & 0x1F; }
    else if ((lead & 0xF0) == 0xE0) { size = 3; value = lead & 0x0F; }
    else if ((lead & 0xF8) == 0xF0) { size = 4; value = lead & 0x07; }
    else return 0;
    for (std::size_t index = 1; index < size; ++index) {
        const auto next = static_cast<unsigned char>(text[index]);
        if ((next & 0xC0) != 0x80) return 0;
        value = (value << 6) | (next & 0x3F);
    }
    constexpr char32_t Smallest[] = {0, 0, 0x80, 0x800, 0x10000};
    if (value < Smallest[size] || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return 0;
    *point = value;
    return size;
}

}

namespace LibcDetail {

std::size_t ReadWideScanCharacter(const char* text, char32_t* point) {
    const std::size_t size = DecodeUtf8(text, point);
    if (size == 0) NotImplemented_nid_no_patch("scanf 16-bit wide conversion of input that is not UTF-8");
    if (*point > 0xFFFF) NotImplemented_nid_no_patch("scanf 16-bit wide conversion of a character outside the BMP");
    return size;
}

bool InWideScanset(const std::string& specifier, char32_t point) {
    std::size_t index = 1;
    const bool negated = specifier[index] == '^';
    if (negated) ++index;
    const std::size_t end = specifier.size() - 1;
    bool member = false;
    for (std::size_t first = index; index < end; ++index) {
        const auto value = static_cast<unsigned char>(specifier[index]);
        if (value >= 0x80) NotImplemented_nid_no_patch("scanf 16-bit wide scanset with a character outside ASCII");
        if (specifier[index] == '-' && index != first && index + 1 < end) {
            const auto low = static_cast<unsigned char>(specifier[index - 1]);
            const auto high = static_cast<unsigned char>(specifier[index + 1]);
            if (high >= 0x80) NotImplemented_nid_no_patch("scanf 16-bit wide scanset with a character outside ASCII");
            if (point >= low && point <= high) member = true;
            ++index;
            continue;
        }
        if (point == value) member = true;
    }
    return member != negated;
}

void RequireNoPositionalWideScan(const char* format) {
    if (std::strchr(format, '$') != nullptr) NotImplemented_nid_no_patch("sscanf positional arguments with a 16-bit wide conversion on Linux");
}

}

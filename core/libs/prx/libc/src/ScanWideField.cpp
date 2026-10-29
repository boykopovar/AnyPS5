#include "prx/libc/include/ScanWideField.hpp"
#include "prx/libc/include/General.hpp"

#include <cstring>
#include <vector>

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
    return size;
}

std::size_t EncodeWideScanCharacter(char32_t point, char16_t* units) {
    if (point <= 0xFFFF) {
        units[0] = static_cast<char16_t>(point);
        return 1;
    }
    const char32_t value = point - 0x10000;
    units[0] = static_cast<char16_t>(0xD800 + (value >> 10));
    units[1] = static_cast<char16_t>(0xDC00 + (value & 0x3FF));
    return 2;
}

bool InWideScanset(const std::string& specifier, char32_t point) {
    const std::size_t end = specifier.size() - 1;
    std::vector<char32_t> members;
    for (std::size_t index = 1; index < end;) {
        char32_t value = 0;
        const std::size_t size = DecodeUtf8(specifier.data() + index, &value);
        if (size == 0 || index + size > end) NotImplemented_nid_no_patch("scanf 16-bit wide scanset that is not UTF-8");
        members.push_back(value);
        index += size;
    }
    std::size_t first = 0;
    const bool negated = !members.empty() && members[0] == U'^';
    if (negated) first = 1;
    bool member = false;
    for (std::size_t index = first; index < members.size(); ++index) {
        if (members[index] == U'-' && index != first && index + 1 < members.size()) {
            if (point >= members[index - 1] && point <= members[index + 1]) member = true;
            ++index;
            continue;
        }
        if (point == members[index]) member = true;
    }
    return member != negated;
}

void RequireNoPositionalWideScan(const char* format) {
    if (std::strchr(format, '$') != nullptr) NotImplemented_nid_no_patch("sscanf positional arguments with a 16-bit wide conversion on Linux");
}

}

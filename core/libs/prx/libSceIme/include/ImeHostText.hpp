#ifndef CORE_LIBS_PRX_LIBSCEIME_INCLUDE_IMEHOSTTEXT_HPP
#define CORE_LIBS_PRX_LIBSCEIME_INCLUDE_IMEHOSTTEXT_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace ImeHostText {

inline constexpr std::size_t MaxClipboardBytes = 65536;

inline std::size_t BoundedUtf8Size(const char* text, std::size_t size) {
    if (size <= MaxClipboardBytes) return size;
    std::size_t end = MaxClipboardBytes;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
    return end;
}

inline std::u32string DecodeUtf8Lenient(const char* text, std::size_t size, bool dropIncompleteTail) {
    std::u32string points;
    std::size_t i = 0;
    while (i < size) {
        const auto lead = static_cast<unsigned char>(text[i]);
        if (lead < 0x80) {
            points.push_back(lead);
            ++i;
            continue;
        }
        std::size_t extra = 0;
        char32_t point = 0;
        char32_t minimum = 0;
        if (lead >= 0xc2 && lead <= 0xdf) { extra = 1; point = lead & 0x1f; minimum = 0x80; }
        else if ((lead & 0xf0) == 0xe0) { extra = 2; point = lead & 0x0f; minimum = 0x800; }
        else if (lead >= 0xf0 && lead <= 0xf4) { extra = 3; point = lead & 0x07; minimum = 0x10000; }
        else {
            points.push_back(0xfffd);
            ++i;
            continue;
        }
        std::size_t present = 0;
        while (present < extra && i + 1 + present < size && (static_cast<unsigned char>(text[i + 1 + present]) & 0xc0) == 0x80) {
            point = (point << 6) | (static_cast<unsigned char>(text[i + 1 + present]) & 0x3f);
            ++present;
        }
        if (present < extra) {
            const bool endOfInput = i + 1 + present >= size;
            if (!(endOfInput && dropIncompleteTail)) points.push_back(0xfffd);
            i += 1 + present;
            continue;
        }
        i += 1 + extra;
        const bool surrogate = point >= 0xd800 && point <= 0xdfff;
        points.push_back(point < minimum || point > 0x10ffff || surrogate ? char32_t{0xfffd} : point);
    }
    return points;
}

inline void AppendUtf16(std::u16string& units, char32_t point) {
    if (point >= 0x10000) {
        point -= 0x10000;
        units.push_back(static_cast<char16_t>(0xd800 + (point >> 10)));
        units.push_back(static_cast<char16_t>(0xdc00 + (point & 0x3ff)));
    } else {
        units.push_back(static_cast<char16_t>(point));
    }
}

inline bool IsControl(char32_t point) {
    return point < 0x20 || (point >= 0x7f && point <= 0x9f);
}

inline std::u16string PreeditUnits(const char* text, std::size_t size) {
    std::u16string units;
    for (const char32_t point : DecodeUtf8Lenient(text, size, true)) {
        if (!IsControl(point)) AppendUtf16(units, point);
    }
    return units;
}

inline std::u16string ClipboardUnits(const char* text, std::size_t size, bool multiline) {
    std::u16string units;
    const std::u32string points = DecodeUtf8Lenient(text, BoundedUtf8Size(text, size), false);
    for (std::size_t i = 0; i < points.size(); ++i) {
        const char32_t point = points[i];
        const bool lineBreak = point == U'\n' || point == U'\r' || point == 0x85 || point == 0x2028 || point == 0x2029;
        if (lineBreak) {
            if (point == U'\r' && i + 1 < points.size() && points[i + 1] == U'\n') ++i;
            if (multiline) units.push_back(u'\n');
            continue;
        }
        if (!IsControl(point)) AppendUtf16(units, point);
    }
    return units;
}

}

#endif

#ifndef CORE_LIBS_PRX_LIBSCEIME_INCLUDE_IMECLIPBOARD_HPP
#define CORE_LIBS_PRX_LIBSCEIME_INCLUDE_IMECLIPBOARD_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ImeClipboard {

inline std::string Utf8(const std::u16string& text) {
    std::string out;
    for (std::size_t index = 0; index < text.size(); ++index) {
        std::uint32_t code = text[index];
        if (code >= 0xD800 && code <= 0xDBFF && index + 1 < text.size() && text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF) {
            code = 0x10000 + ((code - 0xD800) << 10) + (text[index + 1] - 0xDC00);
            ++index;
        } else if (code >= 0xD800 && code <= 0xDFFF) {
            code = 0xFFFD;
        }
        if (code < 0x80) {
            out += static_cast<char>(code);
        } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (code >> 18));
            out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
        }
    }
    return out;
}

inline bool CopyEnabled(const char* value, bool apple) {
    return !apple && value != nullptr && std::string_view(value) == "1";
}

}

#endif

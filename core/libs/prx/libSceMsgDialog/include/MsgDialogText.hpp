#ifndef CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_MSGDIALOGTEXT_HPP
#define CORE_LIBS_PRX_LIBSCEMSGDIALOG_INCLUDE_MSGDIALOGTEXT_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace MsgDialog {

inline std::u16string DecodeUtf8(const std::string& bytes) {
    std::u16string text;
    std::size_t index = 0;
    while (index < bytes.size()) {
        const auto lead = static_cast<std::uint8_t>(bytes[index]);
        std::size_t length = 1;
        char32_t code = lead;
        if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
            code = lead & 0x1Fu;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            length = 3;
            code = lead & 0x0Fu;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            code = lead & 0x07u;
        } else if (lead >= 0x80) {
            text.push_back(u'�');
            ++index;
            continue;
        }
        bool valid = index + length <= bytes.size();
        for (std::size_t follow = 1; valid && follow < length; ++follow) {
            const auto next = static_cast<std::uint8_t>(bytes[index + follow]);
            valid = (next & 0xC0u) == 0x80u;
            code = (code << 6) | (next & 0x3Fu);
        }
        if (valid && length == 3 && (code < 0x800 || (code >= 0xD800 && code <= 0xDFFF))) valid = false;
        if (valid && length == 4 && (code < 0x10000 || code > 0x10FFFF)) valid = false;
        if (!valid) {
            text.push_back(u'�');
            ++index;
            continue;
        }
        if (code >= 0x10000) {
            code -= 0x10000;
            text.push_back(static_cast<char16_t>(0xD800 + (code >> 10)));
            text.push_back(static_cast<char16_t>(0xDC00 + (code & 0x3FF)));
        } else {
            text.push_back(static_cast<char16_t>(code));
        }
        index += length;
    }
    return text;
}

}

#endif

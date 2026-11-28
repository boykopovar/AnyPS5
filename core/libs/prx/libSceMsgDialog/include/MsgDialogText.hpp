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
        if (lead < 0x80u) {
            text.push_back(static_cast<char16_t>(lead));
            ++index;
            continue;
        }
        std::size_t length = 0;
        char32_t code = 0;
        std::uint8_t secondMin = 0x80u;
        std::uint8_t secondMax = 0xBFu;
        if (lead >= 0xC2u && lead <= 0xDFu) {
            length = 2;
            code = lead & 0x1Fu;
        } else if (lead >= 0xE0u && lead <= 0xEFu) {
            length = 3;
            code = lead & 0x0Fu;
            if (lead == 0xE0u) {
                secondMin = 0xA0u;
            } else if (lead == 0xEDu) {
                secondMax = 0x9Fu;
            }
        } else if (lead >= 0xF0u && lead <= 0xF4u) {
            length = 4;
            code = lead & 0x07u;
            if (lead == 0xF0u) {
                secondMin = 0x90u;
            } else if (lead == 0xF4u) {
                secondMax = 0x8Fu;
            }
        } else {
            text.push_back(u'\uFFFD');
            ++index;
            continue;
        }
        std::size_t follow = 1;
        for (; follow < length && index + follow < bytes.size(); ++follow) {
            const auto next = static_cast<std::uint8_t>(bytes[index + follow]);
            const std::uint8_t minByte = follow == 1 ? secondMin : 0x80u;
            const std::uint8_t maxByte = follow == 1 ? secondMax : 0xBFu;
            if (next < minByte || next > maxByte) {
                break;
            }
            code = (code << 6) | (next & 0x3Fu);
        }
        if (follow < length) {
            text.push_back(u'\uFFFD');
            index += follow;
            continue;
        }
        if (code >= 0x10000u) {
            code -= 0x10000u;
            text.push_back(static_cast<char16_t>(0xD800u + (code >> 10)));
            text.push_back(static_cast<char16_t>(0xDC00u + (code & 0x3FFu)));
        } else {
            text.push_back(static_cast<char16_t>(code));
        }
        index += length;
    }
    return text;
}

}

#endif

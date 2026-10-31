#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SCANFLOATFIELD_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SCANFLOATFIELD_HPP

#include <cstddef>
#include <string_view>

namespace LibcDetail {

enum class Fit { Invalid, Prefix, Valid };

inline int DigitValue(char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'z') return character - 'a' + 10;
    if (character >= 'A' && character <= 'Z') return character - 'A' + 10;
    return 36;
}

inline bool IsDecimal(char character) {
    return character >= '0' && character <= '9';
}

inline bool IsHex(char character) {
    return DigitValue(character) < 16;
}

inline char Lower(char character) {
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
}

inline bool StartsWord(std::string_view text, std::string_view word) {
    if (text.size() > word.size()) return false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (Lower(text[index]) != word[index]) return false;
    }
    return true;
}

inline std::string_view Unsigned(std::string_view text) {
    if (!text.empty() && (text[0] == '+' || text[0] == '-')) text.remove_prefix(1);
    return text;
}

inline Fit SpecialFloatFit(std::string_view text) {
    if (StartsWord(text, "infinity")) return text.size() == 3 || text.size() == 8 ? Fit::Valid : Fit::Prefix;
    if (StartsWord(text, "nan")) return text.size() == 3 ? Fit::Valid : Fit::Prefix;
    if (text.size() < 4 || !StartsWord(text.substr(0, 3), "nan") || text[3] != '(') return Fit::Invalid;
    for (std::size_t index = 4; index < text.size(); ++index) {
        const char character = text[index];
        if (character == ')') return index + 1 == text.size() ? Fit::Valid : Fit::Invalid;
        if (DigitValue(character) == 36 && character != '_') return Fit::Invalid;
    }
    return Fit::Prefix;
}

inline Fit FloatFit(std::string_view text) {
    text = Unsigned(text);
    if (text.empty()) return Fit::Prefix;
    if (Lower(text[0]) == 'i' || Lower(text[0]) == 'n') return SpecialFloatFit(text);
    const bool hex = text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    const auto digit = [hex](char character) { return hex ? IsHex(character) : IsDecimal(character); };
    std::size_t index = hex ? 2 : 0;
    std::size_t digits = 0;
    for (; index < text.size() && digit(text[index]); ++index) ++digits;
    if (index < text.size() && text[index] == '.') {
        for (++index; index < text.size() && digit(text[index]); ++index) ++digits;
    }
    if (index == text.size()) return digits != 0 ? Fit::Valid : Fit::Prefix;
    if (digits == 0) return Fit::Invalid;
    if (Lower(text[index]) != (hex ? 'p' : 'e')) return Fit::Invalid;
    ++index;
    if (index == text.size()) return Fit::Valid;
    if (text[index] == '+' || text[index] == '-') ++index;
    if (index == text.size()) return Fit::Valid;
    for (; index < text.size(); ++index) {
        if (!IsDecimal(text[index])) return Fit::Invalid;
    }
    return Fit::Valid;
}

}

#endif

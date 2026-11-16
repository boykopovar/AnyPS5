#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string_view>

namespace NKWebKit::WtfAbi {

inline char16_t ToASCIILowerUnchecked(char16_t character) {
    return character | 0x20;
}

inline char16_t ToASCIILower(char16_t character) {
    return character | ((character >= 'A' && character <= 'Z') << 5);
}

inline bool HasLettersPrefixIgnoringASCIICase(const StringView& string, std::string_view letters) {
    for (unsigned i = 0; i < letters.size(); ++i) {
        if (ToASCIILowerUnchecked(CodeUnitAt(string, i)) != static_cast<unsigned char>(letters[i])) return false;
    }
    return true;
}

inline bool StartsWithLettersIgnoringASCIICase(const StringView& string, std::string_view letters) {
    if (letters.empty()) return true;
    if (string.length < letters.size()) return false;
    return HasLettersPrefixIgnoringASCIICase(string, letters);
}

inline bool EqualLettersIgnoringASCIICase(const StringView& string, std::string_view letters) {
    return string.length == letters.size() && HasLettersPrefixIgnoringASCIICase(string, letters);
}

inline bool LettersLessThanIgnoringASCIICase(std::string_view letters, const StringView& string) {
    for (unsigned i = 0; i < string.length; ++i) {
        if (i == letters.size()) return true;
        char16_t character = ToASCIILower(CodeUnitAt(string, i));
        if (character != static_cast<unsigned char>(letters[i])) return static_cast<unsigned char>(letters[i]) < character;
    }
    return false;
}

template <typename Entry, std::size_t Size, typename Key>
const Entry* FindLettersIgnoringASCIICase(const Entry (&sorted)[Size], Key key, const StringView& string) {
    constexpr std::size_t BinarySearchThreshold = 20;
    if (Size < BinarySearchThreshold) {
        auto found = std::find_if(std::begin(sorted), std::end(sorted), [&](const Entry& entry) {
            return EqualLettersIgnoringASCIICase(string, key(entry));
        });
        return found == std::end(sorted) ? nullptr : found;
    }
    auto found = std::lower_bound(std::begin(sorted), std::end(sorted), string, [&](const Entry& entry, const StringView& value) {
        return LettersLessThanIgnoringASCIICase(key(entry), value);
    });
    if (found == std::end(sorted) || !EqualLettersIgnoringASCIICase(string, key(found[0]))) return nullptr;
    return found;
}

}  // namespace NKWebKit::WtfAbi

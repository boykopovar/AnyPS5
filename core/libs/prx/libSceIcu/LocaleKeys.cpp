// Derived from icu4c/source/common/uloc_keytype.cpp, uloc.cpp and uloc_tag.cpp@049e0d6a420629ac7db77256987d083a563287b5 (ICU 78.1):
// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/uloc_keytype.cpp
// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/uloc.cpp
// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/uloc_tag.cpp
//
// The upstream files' notice, verbatim:
//   © 2016 and later: Unicode, Inc. and others.
//   License & terms of use: http://www.unicode.org/copyright.html
//   Copyright (C) 1997-2016, International Business Machines
//   Corporation and others.  All Rights Reserved.

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/LocaleKeyData.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using Icu::LocaleKeyData::Key;
using Icu::LocaleKeyData::Type;

char Lower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

bool Letter(char value) {
    const char lower = Lower(value);
    return lower >= 'a' && lower <= 'z';
}

bool Digit(char value) {
    return value >= '0' && value <= '9';
}

bool AlphaNumeric(char value) {
    return Letter(value) || Digit(value);
}

bool HexDigit(char value) {
    const char lower = Lower(value);
    return Digit(value) || (lower >= 'a' && lower <= 'f');
}

std::string Lowered(std::string_view value) {
    std::string result(value);
    for (char& part : result) part = Lower(part);
    return result;
}

std::string ReadText(const char* source, const char* caller) {
    std::string text;
    while (true) {
        char chunk[0x1000];
        const auto address = reinterpret_cast<std::uintptr_t>(source) + text.size();
        const std::size_t count = sizeof(chunk) - address % sizeof(chunk);
        const bool copied = GuestCopyIn_nid_no_patch(chunk, source + text.size(), count);
        if (!copied) throw std::invalid_argument(std::string(caller) + ": unreadable string");
        const auto* end = static_cast<const char*>(std::memchr(chunk, 0, count));
        text.append(chunk, end ? static_cast<std::size_t>(end - chunk) : count);
        if (end) return text;
    }
}

const Key* FindKey(std::string_view keyword) {
    const std::string lowered = Lowered(keyword);
    for (const Key& key : Icu::LocaleKeyData::Keys) {
        if (lowered == key.legacy || lowered == key.bcp) return &key;
    }
    return nullptr;
}

const Type* FindType(const Key& key, std::string_view value) {
    const std::string lowered = Lowered(value);
    const auto found = std::lower_bound(key.types.begin(), key.types.end(), lowered,
                                        [](const Type& type, const std::string& target) { return type.lookup < target; });
    return found != key.types.end() && found->lookup == lowered ? &*found : nullptr;
}

bool Subtags(std::string_view value, char separator, std::size_t minimum, std::size_t maximum, bool (*accept)(char)) {
    std::size_t length = 0;
    for (char part : value) {
        if (part == separator) {
            if (length < minimum || length > maximum) return false;
            length = 0;
        } else if (accept(part)) {
            ++length;
        } else {
            return false;
        }
    }
    return length >= minimum && length <= maximum;
}

bool RegionKeyValue(std::string_view value) {
    std::size_t length = 0;
    for (char part : value) {
        if ((length < 2 && Letter(part)) || (length >= 2 && Lower(part) == 'z')) {
            ++length;
        } else {
            return false;
        }
    }
    return length == 6;
}

bool SpecialType(const Key& key, std::string_view value) {
    using namespace Icu::LocaleKeyData;
    if ((key.special & Codepoints) && Subtags(value, '-', 4, 6, HexDigit)) return true;
    if ((key.special & ReorderCode) && Subtags(value, '-', 3, 8, Letter)) return true;
    return (key.special & RgKeyValue) && RegionKeyValue(value);
}

bool LegacyType(std::string_view value) {
    std::size_t length = 0;
    for (char part : value) {
        if (part == '_' || part == '/' || part == '-') {
            if (length == 0) return false;
            length = 0;
        } else if (AlphaNumeric(part)) {
            ++length;
        } else {
            return false;
        }
    }
    return length != 0;
}

template <typename Select, typename WellFormed>
const char* ConvertType(const char* keyword, const char* value, const char* caller, Select select, WellFormed wellFormed) {
    if (!keyword || !value) return nullptr;
    const std::string key = ReadText(keyword, caller);
    const std::string type = ReadText(value, caller);
    if (key.empty() || type.empty()) return nullptr;
    if (const Key* found = FindKey(key)) {
        if (const Type* match = FindType(*found, type)) return select(*match).data();
        if (SpecialType(*found, type)) return value;
    }
    return wellFormed(type) ? value : nullptr;
}

}

extern "C" {

const char* APS5_VABI uloc_toLegacyKey_nid_postfix(const char* keyword) {
    if (!keyword) return nullptr;
    const std::string text = ReadText(keyword, __func__);
    if (text.empty()) return nullptr;
    if (const Key* key = FindKey(text)) return key->legacy.data();
    return std::all_of(text.begin(), text.end(), AlphaNumeric) ? keyword : nullptr;
}

const char* APS5_VABI uloc_toUnicodeLocaleKey_nid_postfix(const char* keyword) {
    if (!keyword) return nullptr;
    const std::string text = ReadText(keyword, __func__);
    if (text.empty()) return nullptr;
    if (const Key* key = FindKey(text)) return key->bcp.data();
    const bool wellFormed = text.size() == 2 && AlphaNumeric(text[0]) && Letter(text[1]);
    return wellFormed ? keyword : nullptr;
}

const char* APS5_VABI uloc_toLegacyType_nid_postfix(const char* keyword, const char* value) {
    return ConvertType(keyword, value, __func__, [](const Type& type) { return type.legacy; }, LegacyType);
}

const char* APS5_VABI uloc_toUnicodeLocaleType(const char* keyword, const char* value) {
    return ConvertType(keyword, value, __func__, [](const Type& type) { return type.bcp; },
                       [](std::string_view type) { return Subtags(type, '-', 3, 8, AlphaNumeric); });
}

}

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/Enumeration.hpp"
#include "prx/libSceIcu/Status.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::int32_t kIllegalArgument = 1;
constexpr std::int32_t kBufferOverflow = 15;
constexpr std::int32_t kStringNotTerminated = -124;

char Lower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

char Upper(char value) {
    return value >= 'a' && value <= 'z' ? static_cast<char>(value - ('a' - 'A')) : value;
}

bool Alpha(char value) {
    const char lower = Lower(value);
    return lower >= 'a' && lower <= 'z';
}

bool Digit(char value) {
    return value >= '0' && value <= '9';
}

std::string ChangeCase(std::string_view value, bool upper) {
    std::string result(value);
    for (char& part : result) part = upper ? Upper(part) : Lower(part);
    return result;
}

bool IsScript(std::string_view value) {
    return value.size() == 4 && std::all_of(value.begin(), value.end(), Alpha);
}

bool IsRegion(std::string_view value) {
    if (value.size() == 2)
        return std::all_of(value.begin(), value.end(), Alpha);
    return value.size() == 3 && std::all_of(value.begin(), value.end(), Digit);
}

struct Parts {
    std::string language;
    std::string script;
    std::string country;
    std::string variant;
    std::string encoding;
    std::string keywords;
};

Parts Parse(const char* locale) {
    const std::string_view input(locale);
    const auto keyword = input.find('@');
    const std::string_view head = input.substr(0, keyword);
    Parts parts;
    if (keyword != std::string_view::npos) parts.keywords = std::string(input.substr(keyword + 1));

    const auto encoding = head.find('.');
    const std::string_view base = head.substr(0, encoding);
    if (encoding != std::string_view::npos) parts.encoding = std::string(head.substr(encoding));
    const std::string special = ChangeCase(base, false);
    if (special == "i-klingon" || special == "x-private") {
        parts.language = special;
        return parts;
    }

    std::vector<std::string> tokens;
    std::size_t start = 0;
    while (start <= base.size()) {
        const auto end = base.find_first_of("_-", start);
        tokens.emplace_back(base.substr(start, end == std::string_view::npos ? end : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }

    if (!tokens.empty()) parts.language = ChangeCase(tokens.front(), false);
    if (parts.language == "und" || parts.language == "root") parts.language.clear();
    std::size_t index = 1;
    if (index < tokens.size() && IsScript(tokens[index])) {
        parts.script = ChangeCase(tokens[index], false);
        parts.script.front() = Upper(parts.script.front());
        ++index;
    }
    if (index < tokens.size() && IsRegion(tokens[index])) {
        parts.country = ChangeCase(tokens[index], true);
        ++index;
    } else if (index < tokens.size() && tokens[index].empty()) {
        ++index;
        if (index < tokens.size() && IsRegion(tokens[index])) {
            parts.country = ChangeCase(tokens[index], true);
            ++index;
        } else if (index < tokens.size()) {
            parts.variant += "_";
        }
    }
    for (; index < tokens.size(); ++index) {
        if (tokens[index].empty()) continue;
        if (!parts.variant.empty() && parts.variant.back() != '_') parts.variant.push_back('_');
        parts.variant += ChangeCase(tokens[index], true);
    }
    return parts;
}

std::string BaseName(const Parts& parts) {
    if (parts.language == "i-klingon") return "tlh";
    if (parts.language == "x-private") return "";
    std::string result = parts.language;
    if (!parts.script.empty()) result += "_" + parts.script;
    if (!parts.country.empty()) result += "_" + parts.country;
    if (!parts.variant.empty()) {
        if (parts.country.empty()) result += "_";
        result += parts.variant.front() == '_' ? parts.variant : "_" + parts.variant;
    }
    return result + parts.encoding;
}

std::int32_t Write(std::string_view value, char* destination, std::int32_t capacity,
                   std::int32_t* status, bool fullOnly) {
    if (!status || *status > 0) return 0;
    if (capacity < 0 || (capacity > 0 && !destination)) {
        *status = kIllegalArgument;
        return 0;
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        *status = kIllegalArgument;
        return 0;
    }
    const auto size = static_cast<std::int32_t>(value.size());
    if (size > 0 && destination && capacity > 0 && (!fullOnly || capacity >= size)) {
        std::memcpy(destination, value.data(), static_cast<std::size_t>(std::min(capacity, size)));
    }
    if (capacity > size && destination) destination[size] = '\0';
    else if (capacity == size) *status = kStringNotTerminated;
    else *status = kBufferOverflow;
    return size;
}

std::int32_t Component(const char* locale, char* result, std::int32_t capacity,
                       std::int32_t* status, int which) {
    if (!status || *status > 0) return 0;
    if (!locale) {
        *status = kIllegalArgument;
        return 0;
    }
    Parts parts = Parse(locale);
    const std::string& value = which == 0 ? parts.language : which == 1 ? parts.country : parts.script;
    return Write(value, result, capacity, status, true);
}

constexpr std::int32_t kInvalidFormat = 3;
constexpr std::int32_t kInternalProgramError = 5;
constexpr std::int32_t kMemoryAllocation = 7;
constexpr std::int32_t kFullNameCapacity = 157;
constexpr std::size_t kKeywordBufferLength = 25;
constexpr std::size_t kMaximumKeywords = 25;

struct Keyword {
    std::string name;
    std::string_view value;
};

std::int32_t ParseKeywords(std::string_view list, std::vector<Keyword>& keywords) {
    std::size_t position = 0;
    while (true) {
        while (position < list.size() && list[position] == ' ') ++position;
        if (position == list.size()) break;
        if (keywords.size() == kMaximumKeywords) return kInternalProgramError;
        const auto equal = list.find('=', position);
        const auto semicolon = list.find(';', position);
        if (equal == std::string_view::npos || (semicolon != std::string_view::npos && semicolon < equal))
            return kInvalidFormat;
        if (equal - position >= kKeywordBufferLength) return kInternalProgramError;
        Keyword keyword;
        for (std::size_t index = position; index < equal; ++index) {
            if (list[index] != ' ') keyword.name.push_back(Lower(list[index]));
        }
        if (keyword.name.empty()) return kInvalidFormat;
        std::size_t valueStart = equal + 1;
        while (valueStart < list.size() && list[valueStart] == ' ') ++valueStart;
        if (valueStart == list.size() || valueStart == semicolon) return kInvalidFormat;
        std::size_t valueEnd = semicolon == std::string_view::npos ? list.size() : semicolon;
        while (valueEnd > valueStart && list[valueEnd - 1] == ' ') --valueEnd;
        keyword.value = list.substr(valueStart, valueEnd - valueStart);
        const bool duplicate = std::any_of(keywords.begin(), keywords.end(), [&](const Keyword& existing) {
            return existing.name == keyword.name;
        });
        if (!duplicate) keywords.push_back(std::move(keyword));
        if (semicolon == std::string_view::npos) break;
        position = semicolon + 1;
    }
    std::sort(keywords.begin(), keywords.end(), [](const Keyword& left, const Keyword& right) {
        return left.name < right.name;
    });
    return 0;
}

bool HasLanguageTagExtension(std::string_view id) {
    if (id.find('@') != std::string_view::npos) return false;
    std::size_t shortest = id.size();
    std::size_t current = 0;
    bool reset = true;
    for (const char value : id) {
        if (value != '_' && value != '-') {
            if (reset) {
                current = 0;
                reset = false;
            }
            ++current;
        } else {
            if (current != 0 && current < shortest) shortest = current;
            reset = true;
        }
    }
    return shortest == 1;
}

bool Resolve(const char* locale, std::string& id, std::int32_t* status) {
    id = locale;
    if (!HasLanguageTagExtension(id)) return true;
    std::array<char, kFullNameCapacity> buffer{};
    const std::int32_t length = uloc_forLanguageTag(locale, buffer.data(), kFullNameCapacity, nullptr, status);
    if (length <= 0 || *status > 0 || *status == kStringNotTerminated) {
        if (*status == kStringNotTerminated) *status = kBufferOverflow;
        return *status <= 0;
    }
    id.assign(buffer.data(), static_cast<std::size_t>(length));
    return true;
}

bool NamesDefaultLocale(std::string_view id) {
    constexpr std::string_view alias = "i-default";
    if (id.substr(0, alias.size()) != alias) return false;
    return id.size() == alias.size() || std::string_view("._-@").find(id[alias.size()]) != std::string_view::npos;
}

std::int32_t Emit(std::string_view value, char* destination, std::int32_t capacity, std::int32_t* status) {
    const std::int32_t length = Icu::Checked(static_cast<std::int64_t>(value.size()));
    const std::int32_t room = std::max(capacity, 0);
    const std::int32_t copied = std::min(length, room);
    if (copied > 0) std::memcpy(destination, value.data(), static_cast<std::size_t>(copied));
    if (*status > 0) return length;
    if (length > room) {
        *status = kBufferOverflow;
        return length;
    }
    Icu::Terminate(destination, capacity, length, status);
    return length;
}

struct KeywordList {
    UEnumeration enumeration;
    std::string names;
    std::size_t current;
    std::u16string unicode;
};

KeywordList& ListOf(UEnumeration* enumeration) {
    return *static_cast<KeywordList*>(enumeration->context);
}

void APS5_VABI CloseKeywords(UEnumeration* enumeration) {
    delete &ListOf(enumeration);
}

std::int32_t APS5_VABI CountKeywords(UEnumeration* enumeration, std::int32_t*) {
    const std::string& names = ListOf(enumeration).names;
    return Icu::Checked(static_cast<std::int64_t>(std::count(names.begin(), names.end(), '\0')));
}

const char* APS5_VABI NextKeyword(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t*) {
    KeywordList& list = ListOf(enumeration);
    if (list.current >= list.names.size()) {
        if (resultLength) *resultLength = 0;
        return nullptr;
    }
    const char* result = list.names.c_str() + list.current;
    const std::size_t length = std::strlen(result);
    list.current += length + 1;
    if (resultLength) *resultLength = Icu::Checked(static_cast<std::int64_t>(length));
    return result;
}

const char16_t* APS5_VABI NextUnicodeKeyword(UEnumeration* enumeration, std::int32_t* resultLength,
                                             std::int32_t* status) {
    std::int32_t length = 0;
    const char* name = NextKeyword(enumeration, &length, status);
    if (resultLength) *resultLength = length;
    if (!name) return nullptr;
    std::u16string& unicode = ListOf(enumeration).unicode;
    unicode.assign(name, name + length);
    return unicode.c_str();
}

void APS5_VABI ResetKeywords(UEnumeration* enumeration, std::int32_t*) {
    ListOf(enumeration).current = 0;
}

}

extern "C" {

std::int32_t APS5_VABI uloc_getLanguage(const char* locale, char* result,
                                       std::int32_t capacity, std::int32_t* status) {
    return Component(locale, result, capacity, status, 0);
}

std::int32_t APS5_VABI uloc_getCountry(const char* locale, char* result,
                                      std::int32_t capacity, std::int32_t* status) {
    return Component(locale, result, capacity, status, 1);
}

std::int32_t APS5_VABI uloc_getScript(const char* locale, char* result,
                                     std::int32_t capacity, std::int32_t* status) {
    return Component(locale, result, capacity, status, 2);
}

std::int32_t APS5_VABI uloc_getBaseName(const char* locale, char* result,
                                       std::int32_t capacity, std::int32_t* status) {
    if (!status || *status > 0) return 0;
    if (!locale) {
        *status = kIllegalArgument;
        return 0;
    }
    return Write(BaseName(Parse(locale)), result, capacity, status, false);
}

std::int32_t APS5_VABI uloc_getKeywordValue(const char* locale, const char* keyword,
                                           char* result, std::int32_t capacity,
                                           std::int32_t* status) {
    if (!status || *status > 0) return 0;
    if (!locale || !keyword || !*keyword) {
        *status = kIllegalArgument;
        return 0;
    }
    const std::string requested = ChangeCase(keyword, false);
    const std::string values = Parse(locale).keywords;
    std::size_t start = 0;
    while (start < values.size()) {
        const auto end = values.find(';', start);
        const auto item = std::string_view(values).substr(
            start, end == std::string::npos ? end : end - start);
        const auto equal = item.find('=');
        if (equal == std::string_view::npos) {
            *status = kIllegalArgument;
            return 0;
        }
        if (ChangeCase(item.substr(0, equal), false) == requested) {
            if (equal + 1 == item.size()) {
                *status = kIllegalArgument;
                return 0;
            }
            return Write(item.substr(equal + 1), result, capacity, status, false);
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return Write("", result, capacity, status, false);
}

std::int32_t APS5_VABI uloc_canonicalize(const char* locale, char* result,
                                        std::int32_t capacity, std::int32_t* status) {
    if (!status || *status > 0) return 0;
    if (!locale) {
        *status = kIllegalArgument;
        return 0;
    }
    Parts parts = Parse(locale);
    parts.encoding.clear();
    std::string canonical = parts.language == "x-private" ? "@x=private" : BaseName(parts);
    if (!parts.keywords.empty()) {
        std::vector<std::string> keywords;
        std::size_t start = 0;
        while (start < parts.keywords.size()) {
            const auto end = parts.keywords.find(';', start);
            const std::string item = parts.keywords.substr(
                start, end == std::string::npos ? end : end - start);
            if (!item.empty()) keywords.push_back(item);
            if (end == std::string::npos) break;
            start = end + 1;
        }
        std::sort(keywords.begin(), keywords.end());
        for (const auto& keyword : keywords)
            canonical += std::string(canonical.find('@') == std::string::npos ? "@" : ";") + keyword;
    }
    return Write(canonical, result, capacity, status, false);
}

std::int32_t APS5_VABI uloc_getName(const char* locale, char* result,
                                   std::int32_t capacity, std::int32_t* status) {
    if (Icu::Failure(status)) return 0;
    if (!locale) NotImplemented_nid_no_patch("uloc_getName with a null locale, which names the default locale");
    if (capacity > 0 && !result) APS5_INVALID_ARG_EX;
    std::string id;
    if (!Resolve(locale, id, status)) return 0;
    if (NamesDefaultLocale(id)) NotImplemented_nid_no_patch("uloc_getName of i-default, which names the default locale");
    std::string name = BaseName(Parse(id.c_str()));
    const auto at = id.find('@');
    if (at != std::string::npos) {
        const auto assign = id.find('=', at);
        const auto separator = id.find(';', at);
        if (assign == std::string::npos) {
            name += id.substr(at);
        } else if (separator == std::string::npos || separator > assign) {
            name += '@';
            std::vector<Keyword> keywords;
            if (const std::int32_t error = ParseKeywords(std::string_view(id).substr(at + 1), keywords)) {
                *status = error;
                return Emit(name, result, capacity, status);
            }
            for (std::size_t index = 0; index < keywords.size(); ++index) {
                if (index) name += ';';
                name += keywords[index].name;
                name += '=';
                name += keywords[index].value;
            }
        }
    }
    return Emit(name, result, capacity, status);
}

UEnumeration* APS5_VABI uloc_openKeywords(const char* locale, std::int32_t* status) {
    if (!status || *status > 0) return nullptr;
    if (!locale) NotImplemented_nid_no_patch("uloc_openKeywords with a null locale, which names the default locale");
    std::string id;
    if (!Resolve(locale, id, status)) return nullptr;
    const auto at = id.find('@');
    if (at == std::string::npos) return nullptr;
    std::vector<Keyword> keywords;
    if (const std::int32_t error = ParseKeywords(std::string_view(id).substr(at + 1), keywords)) {
        *status = error;
        return nullptr;
    }
    auto* list = new (std::nothrow) KeywordList{};
    if (!list) {
        *status = kMemoryAllocation;
        return nullptr;
    }
    for (const auto& keyword : keywords) {
        list->names += keyword.name;
        list->names += '\0';
    }
    list->enumeration = UEnumeration{nullptr, list, CloseKeywords, CountKeywords, NextUnicodeKeyword,
                                     NextKeyword, ResetKeywords};
    return &list->enumeration;
}

}

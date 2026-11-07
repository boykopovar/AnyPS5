#include "prx/libSceIcu/include/InventoryExports.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
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

}

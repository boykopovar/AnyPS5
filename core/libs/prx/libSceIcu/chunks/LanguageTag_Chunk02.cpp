// Derived from icu4c/source/common/uloc_tag.cpp and uloc.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2)
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2009-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/Status.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
void AppendKeywords(const Tag& tag, std::string& text, std::int32_t& status) {
    std::vector<Extension> keywords;
    bool posixVariant = !tag.variants.empty();

    for (const Extension& extension : tag.extensions) {
        if (extension.key[0] == kUnicodeExtension) {
            if (!AppendUnicodeExtension(extension.value, keywords, posixVariant, status)) break;
        } else if (!AddExtension(keywords, extension)) {
            status = Icu::IllegalArgument;
            break;
        }
    }

    if (status <= 0 && !tag.privateUse.empty()) {
        if (!AddExtension(keywords, Extension{std::string(1, kPrivateUse), tag.privateUse})) status = Icu::IllegalArgument;
    }

    if (status <= 0 && posixVariant) text += kPosixVariant;

    if (status <= 0 && !keywords.empty()) {
        bool first = true;
        for (const Extension& keyword : keywords) {
            text += first ? '@' : ';';
            first = false;
            text += keyword.key;
            text += '=';
            text += keyword.value;
        }
    }
}

Output Convert(std::string_view input) {
    Output output;
    Parsed parsed = Parse(input);
    Tag& tag = parsed.tag;
    std::string& text = output.text;
    bool empty = true;
    bool noRegion = true;

    const std::string& language = tag.extlang.empty() ? tag.language : tag.extlang[0];
    if (language != "und" && !language.empty()) {
        text += language;
        empty = false;
    }

    if (!tag.script.empty()) {
        text += '_';
        text += tag.script;
        empty = false;
    }

    if (!tag.region.empty()) {
        text += '_';
        text += tag.region;
        empty = false;
        noRegion = false;
    }

    std::sort(tag.variants.begin(), tag.variants.end());
    if (!tag.variants.empty()) {
        if (noRegion) {
            text += '_';
            empty = false;
        }
        for (const std::string& variant : tag.variants) {
            text += '_';
            text += variant;
        }
    }

    if (!tag.extensions.empty() || !tag.privateUse.empty()) {
        if (empty && !tag.extensions.empty()) text += "und";
        AppendKeywords(tag, text, output.status);
    }

    output.length = parsed.length;
    return output;
}

}

namespace {

constexpr std::int32_t kInvalidFormat = 3;
constexpr std::int32_t kInternalProgram = 5;
constexpr std::int32_t kLanguageCapacity = 12;
constexpr std::int32_t kScriptCapacity = 6;
constexpr std::int32_t kCountryCapacity = 4;
constexpr std::size_t kFullNameCapacity = 157;
constexpr std::size_t kKeywordNameCapacity = 25;
constexpr std::size_t kMaximumKeywords = 25;
constexpr std::string_view kUndetermined = "und";
constexpr std::string_view kVariantPrivateUse = "-x-lvariant-";

constexpr Replacement kDeprecatedLanguages[] = {
    {"in", "id"}, {"iw", "he"}, {"ji", "yi"}, {"jw", "jv"}, {"mo", "ro"},
    {"aam", "aas"}, {"adp", "dz"}, {"aue", "ktz"}, {"ayx", "nun"}, {"bgm", "bcg"},
    {"bjd", "drl"}, {"ccq", "rki"}, {"cjr", "mom"}, {"cka", "cmr"}, {"cmk", "xch"},
    {"coy", "pij"}, {"cqu", "quh"}, {"drh", "khk"}, {"drw", "prs"}, {"gav", "dev"},
    {"gfx", "vaj"}, {"ggn", "gvr"}, {"gti", "nyc"}, {"guv", "duz"}, {"hrr", "jal"},
    {"ibi", "opa"}, {"ilw", "gal"}, {"jeg", "oyb"}, {"kgc", "tdf"}, {"kgh", "kml"},
    {"koj", "kwv"}, {"krm", "bmf"}, {"ktr", "dtp"}, {"kvs", "gdj"}, {"kwq", "yam"},
    {"kxe", "tvd"}, {"kzj", "dtp"}, {"kzt", "dtp"}, {"lii", "raq"}, {"lmm", "rmx"},
    {"meg", "cir"}, {"mst", "mry"}, {"mwj", "vaj"}, {"myt", "mry"}, {"nad", "xny"},
    {"ncp", "kdz"}, {"nnx", "ngv"}, {"nts", "pij"}, {"oun", "vaj"}, {"pcr", "adx"},
    {"pmc", "huw"}, {"pmu", "phr"}, {"ppa", "bfy"}, {"ppr", "lcq"}, {"pry", "prt"},
    {"puz", "pub"}, {"sca", "hle"}, {"skk", "oyb"}, {"tdu", "dtp"}, {"thc", "tpo"},
    {"thx", "oyb"}, {"tie", "ras"}, {"tkk", "twm"}, {"tlw", "weo"}, {"tmp", "tyj"},
    {"tne", "kak"}, {"tnf", "prs"}, {"tsf", "taj"}, {"uok", "ema"}, {"xba", "cax"},
    {"xia", "acn"}, {"xkh", "waw"}, {"xsj", "suj"}, {"ybd", "rki"}, {"yma", "lrr"},
    {"ymt", "mtm"}, {"yos", "zom"}, {"yuu", "yug"},
};

constexpr Replacement kDeprecatedRegions[] = {
    {"BU", "MM"}, {"DD", "DE"}, {"FX", "FR"}, {"TP", "TL"}, {"YD", "YE"}, {"ZR", "CD"},
};

bool IdSeparator(char value) {
    return value == '_' || value == kSeparator;
}

bool IdTerminator(char value) {
    return value == 0 || value == '.' || value == '@';
}

bool AsciiAlphaNumeric(char value) {
    return Alpha(value) || Numeric(value);
}

bool ValuePunctuation(char value) {
    return value == '_' || value == '-' || value == '+' || value == '/';
}

bool SeparatedList(bool (*test)(std::string_view), std::string_view value) {
    if (value.empty()) return false;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = value.find(kSeparator, start);
        const std::string_view part = value.substr(start, end == std::string_view::npos ? end : end - start);
        if (part.empty() || !test(part)) return false;
        if (end == std::string_view::npos) return true;
        start = end + 1;
    }
}

std::size_t ShortestSubtag(std::string_view id) {
    std::size_t shortest = id.size();
    std::size_t length = 0;
    bool reset = true;
    for (const char value : id) {
        if (!IdSeparator(value)) {
            if (reset) {
                length = 0;
                reset = false;
            }
            ++length;
        } else {
            if (length != 0 && length < shortest) shortest = length;
            reset = true;
        }
    }
    return shortest;
}

std::string_view Replace(std::string_view value, const Replacement* begin, const Replacement* end) {
    for (const Replacement* entry = begin; entry != end; ++entry)
        if (entry->from == value) return entry->to;
    return value;
}

std::string IcuVariant(const std::string& id) {
    const char* position = id.c_str();
    if (EqualsIgnoringCase(id, "root")) {
        position += 4;
    } else if (id.size() >= 3 && EqualsIgnoringCase(std::string_view(id).substr(0, 3), kUndetermined) &&
               (id[3] == 0 || IdSeparator(id[3]) || id[3] == '@')) {
        position += 3;
    }
    if ((Lower(position[0]) == 'x' || Lower(position[0]) == 'i') && IdSeparator(position[1])) position += 2;
    while (!IdTerminator(*position) && !IdSeparator(*position)) ++position;
    if (!IdSeparator(*position)) return std::string();

    std::size_t script = 0;
    while (!IdTerminator(position[1 + script]) && !IdSeparator(position[1 + script]) && Alpha(position[1 + script])) ++script;
    if (script == 4) position += 1 + script;
    if (!IdSeparator(*position)) return std::string();

    std::size_t country = 0;
    while (!IdTerminator(position[1 + country]) && !IdSeparator(position[1 + country])) ++country;
    const char* countryEnd = position + 1 + (country == 2 || country == 3 ? country : 0);
    if (countryEnd != position + 1) position = countryEnd;
    if (!IdSeparator(*position)) return std::string();
    if (position != countryEnd && IdSeparator(position[1])) ++position;

    std::string variant;
    for (const char* part = position + 1; !IdTerminator(*part); ++part) variant += *part == kSeparator ? '_' : Upper(*part);
    if (!variant.empty()) return variant;
    const char* keywords = std::strchr(position + 1, '@');
    if (!keywords) return variant;
    for (const char* part = keywords + 1; !IdTerminator(*part); ++part)
        variant += *part == kSeparator || *part == ',' ? '_' : Upper(*part);
    return variant;
}

std::int32_t KeywordNames(std::string_view id, std::vector<std::string>& names, bool& present) {
    const std::size_t at = id.find('@');
    present = at != std::string_view::npos;
    if (!present) return 0;
    const std::string_view list = id.substr(at + 1);
    std::size_t position = 0;
    bool more = true;
    while (more) {
        while (position < list.size() && list[position] == ' ') ++position;
        if (position >= list.size()) break;
        if (names.size() == kMaximumKeywords) return kInternalProgram;
        const std::size_t equal = list.find('=', position);
        const std::size_t semicolon = list.find(';', position);
        if (equal == std::string_view::npos || semicolon < equal) return kInvalidFormat;
        if (equal - position >= kKeywordNameCapacity) return kInternalProgram;
        std::string name;
        for (std::size_t index = position; index < equal; ++index)
            if (list[index] != ' ') name += Lower(list[index]);
        if (name.empty()) return kInvalidFormat;
        std::size_t value = equal + 1;
        while (value < list.size() && list[value] == ' ') ++value;
        if (value >= list.size() || value == semicolon) return kInvalidFormat;
        if (semicolon == std::string_view::npos) {
            more = false;
        } else {
            position = semicolon + 1;
        }
        if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(std::move(name));
    }
    std::sort(names.begin(), names.end());
    return 0;
}

std::int32_t KeywordValue(std::string_view id, std::string_view keyword, std::string& value) {
    value.clear();
    std::string wanted;
    for (const char part : keyword) {
        if (!AsciiAlphaNumeric(part)) return Icu::IllegalArgument;
        if (wanted.size() >= kKeywordNameCapacity - 1) return kInternalProgram;
        wanted += Lower(part);
    }
    if (wanted.empty()) return Icu::IllegalArgument;

    std::size_t search = id.find('@');
    while (search != std::string_view::npos) {
        ++search;
        const std::size_t equal = id.find('=', search);
        if (equal == std::string_view::npos) return Icu::IllegalArgument;
        while (id[search] == ' ') ++search;
        std::size_t tail = equal;
        while (tail > search && id[tail - 1] == ' ') --tail;
        if (search == tail) return Icu::IllegalArgument;
        std::string name;
        for (std::size_t index = search; index < tail; ++index) {
            if (!AsciiAlphaNumeric(id[index])) return Icu::IllegalArgument;
            if (name.size() >= kKeywordNameCapacity - 1) return kInternalProgram;
            name += Lower(id[index]);
        }
        search = id.find(';', equal);
        if (name != wanted) continue;
        std::size_t start = equal + 1;
        while (start < id.size() && id[start] == ' ') ++start;
        std::size_t end = search == std::string_view::npos ? id.size() : search;
        while (end > start && id[end - 1] == ' ') --end;
        if (start >= end) return Icu::IllegalArgument;
        for (std::size_t index = start; index < end; ++index) {
            if (!AsciiAlphaNumeric(id[index]) && !ValuePunctuation(id[index])) return Icu::IllegalArgument;
            value += id[index];
        }
        return 0;
    }
    return 0;
}

bool ValidPrivateUse(std::string_view value) {
    return IsPrivateUseValue(value);
}

bool ValidExtension(std::string_view value) {
    return IsExtensionSubtag(value);
}

int CompareForTag(std::string_view left, std::string_view right) {
    if (left.size() == 1 && right.size() == 1) {
        if (left[0] == right[0]) return 0;
        if (left[0] == kPrivateUse) return 1;
        if (right[0] == kPrivateUse) return -1;
        return left[0] - right[0];
    }
    if (left.size() == 1) return left[0] - kUnicodeExtension;
    if (right.size() == 1) return kUnicodeExtension - right[0];
    const int order = left.compare(right);
    if (order != 0) {
        if (right == kAttributeKey) return 1;
        if (left == kAttributeKey) return -1;
    }
    return order;
}

bool AddForTag(std::vector<Extension>& list, Extension extension) {
    auto position = list.begin();
    for (; position != list.end(); ++position) {
        const int order = CompareForTag(extension.key, position->key);
        if (order == 0) return false;
        if (order < 0) break;
    }
    list.insert(position, std::move(extension));
    return true;
}

bool AddAttribute(std::vector<std::string>& list, std::string attribute) {
    auto position = list.begin();
    for (; position != list.end(); ++position) {
        const int order = attribute.compare(*position);
        if (order == 0) return false;
        if (order < 0) break;
    }
    list.insert(position, std::move(attribute));
    return true;
}

template<class Visit>
bool EachSubtag(std::string& variant, Visit visit) {
    std::string current;
    bool have = false;
    for (std::size_t index = 0;; ++index) {
        const char value = index < variant.size() ? variant[index] : 0;
        if (value == kSeparator || value == '_' || value == 0) {
            for (char& part : current) part = Lower(part);
            if (!visit(have, current)) return false;
            current.clear();
            have = false;
            if (value == 0) return true;
        } else {
            current += value;
            have = true;
        }
    }
}

struct ToTag {
    const std::string& canonical;
    bool strict;
    std::string& sink;
    std::int32_t& status;
    bool hadPosix = false;

    bool Failed() const {
        return status > 0;
    }

    void Language() {
        char buffer[kLanguageCapacity];
        std::int32_t local = 0;
        std::int32_t length = uloc_getLanguage(canonical.c_str(), buffer, kLanguageCapacity, &local);
        if (local > 0 || local == Icu::NotTerminated) {
            if (strict) {
                status = Icu::IllegalArgument;
                return;
            }
            length = 0;
        }
        const std::string_view language(buffer, static_cast<std::size_t>(length));
        if (length == 0) {
            sink += kUndetermined;
        } else if (!IsLanguage(language)) {
            if (strict) {
                status = Icu::IllegalArgument;
                return;
            }
            sink += kUndetermined;
        } else {
            sink += Replace(language, std::begin(kDeprecatedLanguages), std::end(kDeprecatedLanguages));
        }
    }

    void Script() {
        if (Failed()) return;
        char buffer[kScriptCapacity];
        std::int32_t local = 0;
        const std::int32_t length = uloc_getScript(canonical.c_str(), buffer, kScriptCapacity, &local);
        if (local > 0 || local == Icu::NotTerminated) {
            if (strict) status = Icu::IllegalArgument;
            return;
        }
        if (length == 0) return;
        const std::string_view script(buffer, static_cast<std::size_t>(length));
        if (!IsScript(script)) {
            if (strict) status = Icu::IllegalArgument;
            return;
        }
        sink += kSeparator;
        sink += script;
    }

    void Region() {
        if (Failed()) return;
        char buffer[kCountryCapacity];
        std::int32_t local = 0;
        const std::int32_t length = uloc_getCountry(canonical.c_str(), buffer, kCountryCapacity, &local);
        if (local > 0 || local == Icu::NotTerminated) {
            if (strict) status = Icu::IllegalArgument;
            return;
        }
        if (length == 0) return;
        const std::string_view region(buffer, static_cast<std::size_t>(length));
        if (!IsRegion(region)) {
            if (strict) status = Icu::IllegalArgument;
            return;
        }
        sink += kSeparator;
        sink += Replace(region, std::begin(kDeprecatedRegions), std::end(kDeprecatedRegions));
    }

    void Variants() {
        if (Failed()) return;
        std::string variant = IcuVariant(canonical);
        if (variant.size() >= kFullNameCapacity) {
            if (strict) status = Icu::IllegalArgument;
            return;
        }
        if (variant.empty()) return;
        const std::size_t whole = variant.size();
        std::vector<std::string> list;
        EachSubtag(variant, [&](bool have, const std::string& subtag) {
            if (!have) {
                if (strict) {
                    status = Icu::IllegalArgument;
                    return false;
                }
                return true;
            }
            if (IsVariant(subtag)) {
                if (subtag != kPosixValue || whole != kPosixValue.size()) {
                    if (std::find(list.begin(), list.end(), subtag) != list.end()) {
                        if (strict) {
                            status = Icu::IllegalArgument;
                            return false;
                        }
                    } else {
                        list.push_back(subtag);
                    }
                } else {
                    hadPosix = true;
                }
                return true;
            }
            if (strict) {
                status = Icu::IllegalArgument;
                return false;
            }
            return !IsPrivateUseValue(subtag);
        });
        if (Failed()) return;
        std::sort(list.begin(), list.end());
        for (const std::string& entry : list) {
            sink += kSeparator;
            sink += entry;
        }
    }

    void Keywords() {
        if (Failed()) return;
        std::vector<std::string> names;
        bool present = false;
        if (const std::int32_t error = KeywordNames(canonical, names, present); error != 0) {
            status = error;
            return;
        }
        if (!present && !hadPosix) return;

        std::vector<Extension> extensions;
        std::vector<std::string> attributes;
        for (const std::string& name : names) {
            std::string value;
            if (KeywordValue(canonical, name, value) != 0) {
                if (strict) {
                    status = Icu::IllegalArgument;
                    return;
                }
                continue;
            }
            Extension extension;
            if (name == kAttributeKey) {
                std::size_t index = 0;
                while (true) {
                    std::string attribute;
                    for (; index < value.size(); ++index) {
                        if (value[index] != kSeparator) {
                            attribute += value[index];
                        } else {
                            ++index;
                            break;
                        }
                    }
                    if (attribute.empty() && index >= value.size()) break;
                    if (!AddAttribute(attributes, std::move(attribute)) && strict) {
                        status = Icu::IllegalArgument;
                        return;
                    }
                }
                extension = Extension{std::string(kAttributeKey), std::string()};
            } else if (name.size() > 1) {
                const char* key = uloc_toUnicodeLocaleKey_nid_postfix(name.c_str());
                const char* type = key ? uloc_toUnicodeLocaleType(name.c_str(), value.c_str()) : nullptr;
                if (!type) {
                    if (strict) {
                        status = Icu::IllegalArgument;
                        return;
                    }
                    continue;
                }
                extension = Extension{std::string(key), type == value.c_str() ? Lowered(value) : std::string(type)};
            } else {
                const bool valid = name[0] == kPrivateUse ? SeparatedList(ValidPrivateUse, value)
                                                          : IsExtensionSingleton(name) && SeparatedList(ValidExtension, value);
                if (!valid) {
                    if (strict) {
                        status = Icu::IllegalArgument;
                        return;
                    }
                    continue;
                }
                extension = Extension{name, value};
            }
            if (!AddForTag(extensions, std::move(extension)) && strict) {
                status = Icu::IllegalArgument;
                return;
            }
        }
        if (hadPosix) AddForTag(extensions, Extension{std::string(kPosixKey), std::string(kPosixValue)});
        if (extensions.empty() && attributes.empty()) return;

        bool unicode = false;
        for (const Extension& extension : extensions) {
            if (!unicode && extension.key.size() > 1) {
                sink += kSeparator;
                sink += kUnicodeExtension;
                unicode = true;
            }
            if (extension.key == kAttributeKey) {
                for (const std::string& attribute : attributes) {
                    sink += kSeparator;
                    sink += attribute;
                }
                continue;
            }
            sink += kSeparator;
            sink += extension.key;
            if (extension.value != "true" && extension.value != "yes") {
                sink += kSeparator;
                sink += extension.value;
            }
        }
    }

    void PrivateUse() {
        if (Failed()) return;
        std::string variant = IcuVariant(canonical);
        if (variant.size() >= kFullNameCapacity) {
            if (strict) status = Icu::IllegalArgument;
            return;
        }
        std::string text;
        bool first = true;
        EachSubtag(variant, [&](bool have, const std::string& subtag) {
            if (!have) return true;
            if (!IsPrivateUseValue(subtag)) {
                if (strict) status = Icu::IllegalArgument;
                return false;
            }
            if (first && IsVariant(subtag)) return true;
            text += first ? kVariantPrivateUse : std::string_view("-");
            first = false;
            text += subtag;
            return true;
        });
        if (Failed()) return;
        sink += text;
    }
};

std::string SourceLocale(const char* localeID, std::int32_t& status) {
    const std::string_view id(localeID);
    if (id.find('@') != std::string_view::npos || ShortestSubtag(id) != 1) return std::string(id);
    std::string buffer(kFullNameCapacity, '\0');
    std::int32_t local = 0;
    std::int32_t length = uloc_forLanguageTag(localeID, buffer.data(), static_cast<std::int32_t>(buffer.size()), nullptr, &local);
    if (local == Icu::BufferOverflow || local == Icu::NotTerminated) {
        buffer.assign(static_cast<std::size_t>(length) + 1, '\0');
        local = 0;
        length = uloc_forLanguageTag(localeID, buffer.data(), static_cast<std::int32_t>(buffer.size()), nullptr, &local);
    }
    if (local > 0) {
        status = Icu::IllegalArgument;
        return std::string(id);
    }
    if (length <= 0) return std::string(id);
    buffer.resize(static_cast<std::size_t>(length));
    return buffer;
}

std::string Canonical(const std::string& source, std::int32_t& status) {
    if (source.empty()) return std::string();
    std::int32_t capacity = Icu::Checked(static_cast<std::int64_t>(source.size()));
    std::string buffer;
    std::int32_t local = 0;
    std::int32_t length = 0;
    while (true) {
        buffer.assign(static_cast<std::size_t>(capacity), '\0');
        local = 0;
        length = uloc_canonicalize(source.c_str(), buffer.data(), capacity, &local);
        if (local != Icu::BufferOverflow) break;
        capacity = length;
    }
    if (local > 0) {
        status = Icu::IllegalArgument;
        return std::string();
    }
    buffer.resize(static_cast<std::size_t>(length));
    return buffer;
}

bool PrivateUseOnly(const std::string& source, const std::string& canonical, bool strict, std::string& sink, std::int32_t& status) {
    if (canonical.empty() || canonical[0] != '@') return false;
    std::vector<std::string> names;
    bool present = false;
    if (KeywordNames(canonical, names, present) != 0) return false;
    if (names.size() != 1 || names[0].size() != 1 || names[0][0] != kPrivateUse) return false;
    std::string value;
    if (KeywordValue(source, names[0], value) != 0) {
        status = Icu::IllegalArgument;
        return true;
    }
    if (SeparatedList(ValidPrivateUse, value)) {
        sink += kPrivateUse;
        sink += kSeparator;
        sink += value;
        return true;
    }
    if (strict) {
        status = Icu::IllegalArgument;
        return true;
    }
    return false;
}

std::string ToLanguageTag(const char* localeID, bool strict, std::int32_t& status) {
    std::string sink;
    const std::string source = SourceLocale(localeID, status);
    if (status > 0) return sink;
    const std::string canonical = Canonical(source, status);
    if (status > 0) return sink;
    if (PrivateUseOnly(source, canonical, strict, sink, status)) return sink;
    ToTag tag{canonical, strict, sink, status};
    tag.Language();
    tag.Script();
    tag.Region();
    tag.Variants();
    tag.Keywords();
    tag.PrivateUse();
    return sink;
}

}


std::int32_t APS5_VABI uloc_forLanguageTag(const char* langtag, char* localeID, std::int32_t capacity,
                                           std::int32_t* parsedLength, std::int32_t* status) {
    if (Icu::Failure(status)) return 0;
    if (!langtag) APS5_INVALID_ARG_EX;
    if (capacity > 0 && !localeID) APS5_INVALID_ARG_EX;

    Output output = Convert(langtag);
    if (parsedLength) *parsedLength = output.length;

    const std::int32_t length = Icu::Checked(static_cast<std::int64_t>(output.text.size()));
    const std::int32_t room = capacity > 0 ? capacity : 0;
    const std::int32_t written = std::min(length, room);
    if (written > 0) std::copy_n(output.text.data(), written, localeID);

    if (output.status > 0) {
        *status = output.status;
        return length;
    }
    if (length > room) {
        *status = Icu::BufferOverflow;
    } else {
        Icu::Terminate(localeID, capacity, length, status);
    }
    return length;
}

std::int32_t APS5_VABI uloc_toLanguageTag(const char* localeID, char* langtag, std::int32_t capacity,
                                          std::int8_t strict, std::int32_t* status) {
    if (Icu::Failure(status)) return 0;
    if (!localeID) APS5_INVALID_ARG_EX;
    if (capacity > 0 && !langtag) APS5_INVALID_ARG_EX;

    std::int32_t error = 0;
    const std::string text = ToLanguageTag(localeID, strict != 0, error);
    const std::int32_t length = Icu::Checked(static_cast<std::int64_t>(text.size()));
    const std::int32_t room = capacity > 0 ? capacity : 0;
    const std::int32_t written = std::min(length, room);
    if (written > 0) std::copy_n(text.data(), written, langtag);

    if (error > 0) {
        *status = error;
        return length;
    }
    if (length > room) {
        *status = Icu::BufferOverflow;
    } else {
        Icu::Terminate(langtag, capacity, length, status);
    }
    return length;
}


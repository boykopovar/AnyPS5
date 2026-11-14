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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct Replacement {
    std::string_view from;
    std::string_view to;
};

constexpr Replacement kLegacy[] = {
    {"art-lojban", "jbo"},
    {"en-gb-oed", "en-gb-oxendict"},
    {"i-ami", "ami"},
    {"i-bnn", "bnn"},
    {"i-hak", "hak"},
    {"i-klingon", "tlh"},
    {"i-lux", "lb"},
    {"i-navajo", "nv"},
    {"i-pwn", "pwn"},
    {"i-tao", "tao"},
    {"i-tay", "tay"},
    {"i-tsu", "tsu"},
    {"no-bok", "nb"},
    {"no-nyn", "nn"},
    {"sgn-be-fr", "sfb"},
    {"sgn-be-nl", "vgt"},
    {"sgn-ch-de", "sgg"},
    {"zh-guoyu", "cmn"},
    {"zh-hakka", "hak"},
    {"zh-min-nan", "nan"},
    {"zh-xiang", "hsn"},
    {"cel-gaulish", "xtg-x-cel-gaulish"},
    {"i-default", "en-x-i-default"},
    {"i-enochian", "und-x-i-enochian"},
    {"i-mingo", "see-x-i-mingo"},
    {"zh-min", "nan-x-zh-min"},
};

constexpr Replacement kRedundant[] = {
    {"sgn-br", "bzs"},
    {"sgn-co", "csn"},
    {"sgn-de", "gsg"},
    {"sgn-dk", "dsl"},
    {"sgn-es", "ssp"},
    {"sgn-fr", "fsl"},
    {"sgn-gb", "bfi"},
    {"sgn-gr", "gss"},
    {"sgn-ie", "isg"},
    {"sgn-it", "ise"},
    {"sgn-jp", "jsl"},
    {"sgn-mx", "mfs"},
    {"sgn-ni", "ncs"},
    {"sgn-nl", "dse"},
    {"sgn-no", "nsl"},
    {"sgn-pt", "psr"},
    {"sgn-se", "swl"},
    {"sgn-us", "ase"},
    {"sgn-za", "sfs"},
    {"zh-cmn", "cmn"},
    {"zh-cmn-hans", "cmn-hans"},
    {"zh-cmn-hant", "cmn-hant"},
    {"zh-gan", "gan"},
    {"zh-wuu", "wuu"},
    {"zh-yue", "yue"},
    {"ja-latn-hepburn-heploc", "ja-latn-alalc97"},
};

constexpr char kSeparator = '-';
constexpr char kPrivateUse = 'x';
constexpr char kUnicodeExtension = 'u';
constexpr std::size_t kMinimumTagLength = 2;
constexpr std::size_t kMaximumExtlang = 3;
constexpr std::size_t kAttributeCapacity = 100;
constexpr std::size_t kNone = static_cast<std::size_t>(-1);
constexpr std::string_view kPosixVariant = "_POSIX";
constexpr std::string_view kAttributeKey = "attribute";
constexpr std::string_view kPrivateUseVariantPrefix = "lvariant";

constexpr unsigned kLanguage = 1;
constexpr unsigned kExtlang = 2;
constexpr unsigned kScript = 4;
constexpr unsigned kRegion = 8;
constexpr unsigned kVariant = 16;
constexpr unsigned kExtension = 32;
constexpr unsigned kExtensionValue = 64;
constexpr unsigned kPrivate = 128;

char Lower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

char Upper(char value) {
    return value >= 'a' && value <= 'z' ? static_cast<char>(value - ('a' - 'A')) : value;
}

bool Alpha(char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

bool Numeric(char value) {
    return value >= '0' && value <= '9';
}

std::string Lowered(std::string_view value) {
    std::string result(value);
    for (char& part : result) part = Lower(part);
    return result;
}

std::string Uppered(std::string_view value) {
    std::string result(value);
    for (char& part : result) part = Upper(part);
    return result;
}

std::string Titled(std::string_view value) {
    std::string result = Lowered(value);
    if (!result.empty()) result[0] = Upper(result[0]);
    return result;
}

bool EqualsIgnoringCase(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index)
        if (Lower(left[index]) != Lower(right[index])) return false;
    return true;
}

bool AllAlpha(std::string_view value) {
    return std::all_of(value.begin(), value.end(), Alpha);
}

bool AllNumeric(std::string_view value) {
    return std::all_of(value.begin(), value.end(), Numeric);
}

bool AllAlphaNumeric(std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](char part) { return Alpha(part) || Numeric(part); });
}

bool AlphaNumericBetween(std::string_view value, std::size_t minimum, std::size_t maximum) {
    return value.size() >= minimum && value.size() <= maximum && AllAlphaNumeric(value);
}

bool IsLanguage(std::string_view value) {
    return value.size() >= 2 && value.size() <= 8 && AllAlpha(value);
}

bool IsExtlang(std::string_view value) {
    return value.size() == 3 && AllAlpha(value);
}

bool IsScript(std::string_view value) {
    return value.size() == 4 && AllAlpha(value);
}

bool IsRegion(std::string_view value) {
    if (value.size() == 2 && AllAlpha(value)) return true;
    return value.size() == 3 && AllNumeric(value);
}

bool IsVariant(std::string_view value) {
    if (AlphaNumericBetween(value, 5, 8)) return true;
    return value.size() == 4 && Numeric(value[0]) && AllAlphaNumeric(value.substr(1));
}

bool IsPrivateUseVariant(std::string_view value) {
    return AlphaNumericBetween(value, 1, 8);
}

bool IsExtensionSingleton(std::string_view value) {
    return value.size() == 1 && (Alpha(value[0]) || Numeric(value[0])) && Lower(value[0]) != kPrivateUse;
}

bool IsExtensionSubtag(std::string_view value) {
    return AlphaNumericBetween(value, 2, 8);
}

bool IsPrivateUseValue(std::string_view value) {
    return AlphaNumericBetween(value, 1, 8);
}

bool IsUnicodeLocaleKey(std::string_view value) {
    return value.size() == 2 && (Alpha(value[0]) || Numeric(value[0])) && Alpha(value[1]);
}

struct Extension {
    std::string key;
    std::string value;
};

bool AddExtension(std::vector<Extension>& list, Extension extension) {
    auto position = list.begin();
    for (; position != list.end(); ++position) {
        const int order = extension.key.compare(position->key);
        if (order == 0) return false;
        if (order < 0) break;
    }
    list.insert(position, std::move(extension));
    return true;
}

struct Tag {
    std::string language;
    std::vector<std::string> extlang;
    std::string script;
    std::string region;
    std::vector<std::string> variants;
    std::vector<Extension> extensions;
    std::string privateUse;
};

bool AddVariant(std::vector<std::string>& list, std::string variant) {
    if (std::find(list.begin(), list.end(), variant) != list.end()) return false;
    list.push_back(std::move(variant));
    return true;
}

struct Parsed {
    Tag tag;
    std::int32_t length = 0;
};

std::size_t FindSeparator(const std::string& text, std::size_t from) {
    const std::size_t found = text.find(kSeparator, from);
    return found == std::string::npos ? text.size() : found;
}

Parsed Parse(std::string_view input) {
    Parsed parsed;
    if (input.size() < kMinimumTagLength) return parsed;

    std::string work(input);
    std::int64_t delta = 0;
    bool legacyMatched = false;

    for (const Replacement& entry : kLegacy) {
        const std::size_t length = entry.from.size();
        if (input.size() < length) continue;
        if (input.size() > length && input[length] != kSeparator) continue;
        if (!EqualsIgnoringCase(input.substr(0, length), entry.from)) continue;
        legacyMatched = true;
        delta = static_cast<std::int64_t>(length) - static_cast<std::int64_t>(entry.to.size());
        work = std::string(entry.to) + std::string(input.substr(length));
        break;
    }

    if (!legacyMatched) {
        for (const Replacement& entry : kRedundant) {
            const std::size_t length = entry.from.size();
            if (input.size() < length) continue;
            if (!EqualsIgnoringCase(input.substr(0, length), entry.from)) continue;
            if (input.size() != length && input[length] != kSeparator) continue;
            delta = static_cast<std::int64_t>(length) - static_cast<std::int64_t>(entry.to.size());
            work = std::string(entry.to) + std::string(input.substr(length));
            break;
        }
    }

    Tag& tag = parsed.tag;
    unsigned next = kLanguage | kPrivate;
    std::size_t nextStart = 0;
    bool hasNext = true;
    std::size_t lastGood = 0;
    bool privateUseVariants = false;
    bool hasExtension = false;
    Extension extension;
    std::size_t valueStart = kNone;
    std::size_t valueEnd = kNone;

    const auto finishExtension = [&]() {
        extension.value = Lowered(std::string_view(work).substr(valueStart, valueEnd - valueStart));
        if (!AddExtension(tag.extensions, std::move(extension))) {
            hasExtension = false;
            return false;
        }
        lastGood = valueEnd;
        return true;
    };

    while (hasNext) {
        const std::size_t start = nextStart;
        const std::size_t separator = FindSeparator(work, start);
        if (separator == work.size()) {
            hasNext = false;
        } else {
            nextStart = separator + 1;
        }
        const std::string_view subtag = std::string_view(work).substr(start, separator - start);

        if (next & kLanguage) {
            if (IsLanguage(subtag)) {
                tag.language = Lowered(subtag);
                lastGood = separator;
                next = kScript | kRegion | kVariant | kExtension | kPrivate;
                if (subtag.size() <= 3) next |= kExtlang;
                continue;
            }
        }
        if (next & kExtlang) {
            if (IsExtlang(subtag)) {
                tag.extlang.push_back(Lowered(subtag));
                lastGood = separator;
                if (tag.extlang.size() < kMaximumExtlang) {
                    next = kExtlang | kScript | kRegion | kVariant | kExtension | kPrivate;
                } else {
                    next = kScript | kRegion | kVariant | kExtension | kPrivate;
                }
                continue;
            }
        }
        if (next & kScript) {
            if (IsScript(subtag)) {
                tag.script = Titled(subtag);
                lastGood = separator;
                next = kRegion | kVariant | kExtension | kPrivate;
                continue;
            }
        }
        if (next & kRegion) {
            if (IsRegion(subtag)) {
                tag.region = Uppered(subtag);
                lastGood = separator;
                next = kVariant | kExtension | kPrivate;
                continue;
            }
        }
        if (next & kVariant) {
            if (IsVariant(subtag) || (privateUseVariants && IsPrivateUseVariant(subtag))) {
                if (!AddVariant(tag.variants, Uppered(subtag))) break;
                lastGood = separator;
                next = kVariant | kExtension | kPrivate;
                continue;
            }
        }
        if (next & kExtension) {
            if (IsExtensionSingleton(subtag)) {
                if (hasExtension) {
                    if (valueStart == kNone || valueEnd == kNone) {
                        hasExtension = false;
                        break;
                    }
                    if (!finishExtension()) break;
                }
                extension = Extension{Lowered(subtag), std::string()};
                hasExtension = true;
                valueStart = kNone;
                valueEnd = kNone;
                next = kExtensionValue;
                continue;
            }
        }
        if (next & kExtensionValue) {
            if (IsExtensionSubtag(subtag)) {
                if (valueStart == kNone) valueStart = start;
                valueEnd = separator;
                next = kExtension | kExtensionValue | kPrivate;
                continue;
            }
        }
        if (next & kPrivate) {
            if (subtag.size() == 1 && Lower(subtag[0]) == kPrivateUse) {
                if (hasExtension) {
                    if (valueStart == kNone || valueEnd == kNone) {
                        hasExtension = false;
                        break;
                    }
                    if (!finishExtension()) break;
                    hasExtension = false;
                }
                if (!hasNext) break;
                const std::size_t privateStart = nextStart;
                while (hasNext) {
                    const std::size_t valueBegin = nextStart;
                    const std::size_t valueSeparator = FindSeparator(work, valueBegin);
                    if (valueSeparator == work.size()) {
                        hasNext = false;
                    } else {
                        nextStart = valueSeparator + 1;
                    }
                    const std::string_view value = std::string_view(work).substr(valueBegin, valueSeparator - valueBegin);
                    if (std::string_view(work).substr(valueBegin, kPrivateUseVariantPrefix.size()) == kPrivateUseVariantPrefix) {
                        next = kVariant;
                        privateUseVariants = true;
                        break;
                    }
                    if (IsPrivateUseValue(value)) {
                        lastGood = valueSeparator;
                    } else {
                        break;
                    }
                }
                if (next == kVariant) continue;
                if (static_cast<std::ptrdiff_t>(lastGood) - static_cast<std::ptrdiff_t>(privateStart) > 0)
                    tag.privateUse = Lowered(std::string_view(work).substr(privateStart, lastGood - privateStart));
                break;
            }
            break;
        }
        break;
    }

    if (hasExtension) {
        if (valueStart != kNone && valueEnd != kNone) finishExtension();
    }

    parsed.length = static_cast<std::int32_t>(static_cast<std::int64_t>(lastGood) + delta);
    return parsed;
}

struct Output {
    std::string text;
    std::int32_t status = 0;
    std::int32_t length = 0;
};

bool AppendUnicodeExtension(std::string_view value, std::vector<Extension>& into, bool& posixVariant, std::int32_t& status) {
    posixVariant = false;
    std::size_t position = 0;
    bool hasKeys = false;
    std::vector<std::string> attributes;
    std::size_t used = 0;

    while (position < value.size()) {
        std::size_t end = value.find(kSeparator, position);
        if (end == std::string_view::npos) end = value.size();
        const std::string_view subtag = value.substr(position, end - position);
        if (IsUnicodeLocaleKey(subtag)) {
            hasKeys = true;
            break;
        }
        if (subtag.size() >= kAttributeCapacity - used) {
            status = Icu::IllegalArgument;
            return false;
        }
        used += subtag.size() + 1;
        const auto place = std::lower_bound(attributes.begin(), attributes.end(), subtag,
                                            [](const std::string& left, std::string_view right) { return std::string_view(left) < right; });
        if (place == attributes.end() || std::string_view(*place) != subtag) attributes.emplace(place, subtag);
        position = end;
        if (position < value.size()) ++position;
    }

    std::vector<Extension> collected;
    if (!attributes.empty()) {
        std::string joined;
        for (std::size_t index = 0; index < attributes.size(); ++index) {
            if (index) joined += kSeparator;
            joined += attributes[index];
        }
        AddExtension(collected, Extension{std::string(kAttributeKey), std::move(joined)});
    }

    if (hasKeys) NotImplemented_nid_no_patch("uloc_forLanguageTag: -u- keywords need the ICU keyTypeData resource");

    for (Extension& entry : collected) AddExtension(into, std::move(entry));
    return true;
}

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

extern "C" {

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

}

#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uchar.h"
#include "unicode/uscript.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::string_view LanguageDirections = "root-en-es-pt-zh-ja-ko-de-fr-it-ar+he+fa+ru-nl-pl-th-tr-";
constexpr std::array<std::pair<std::string_view, std::string_view>, 17> DirectionAliases{{
    {"ara", "ar"}, {"deu", "de"}, {"eng", "en"}, {"fas", "fa"}, {"fra", "fr"}, {"heb", "he"},
    {"ita", "it"}, {"jpn", "ja"}, {"kor", "ko"}, {"nld", "nl"}, {"pol", "pl"}, {"por", "pt"},
    {"rus", "ru"}, {"spa", "es"}, {"tha", "th"}, {"tur", "tr"}, {"zho", "zh"}
}};
constexpr std::size_t LanguageLimit = 11;
constexpr std::size_t ScriptLength = 4;

char Lower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

char Upper(char value) {
    return value >= 'a' && value <= 'z' ? static_cast<char>(value - ('a' - 'A')) : value;
}

bool Letter(char value) {
    const char lower = Lower(value);
    return lower >= 'a' && lower <= 'z';
}

bool Separator(char value) {
    return value == '_' || value == '-';
}

bool Terminator(char value) {
    return value == '.' || value == '@';
}

std::string Lowered(std::string_view value) {
    std::string result(value);
    for (char& part : result) part = Lower(part);
    return result;
}

bool SameIgnoringCase(std::string_view left, std::string_view right) {
    return left.size() == right.size() && Lowered(left) == Lowered(right);
}

struct Subtags {
    std::string language;
    std::string script;
    bool failed = false;
};

Subtags LanguageAndScript(std::string_view locale) {
    Subtags subtags;
    if (locale.size() == 4 && SameIgnoringCase(locale, "root")) locale.remove_prefix(4);
    std::size_t length = locale.size() >= 2 && (Lower(locale[0]) == 'x' || Lower(locale[0]) == 'i') &&
                         Separator(locale[1]) ? 2 : 0;
    while (length < locale.size() && !Terminator(locale[length]) && !Separator(locale[length])) {
        if (length == LanguageLimit) {
            subtags.failed = true;
            return subtags;
        }
        ++length;
    }
    subtags.language = Lowered(locale.substr(0, length));
    if (length >= 2 && Separator(locale[1])) subtags.language[1] = '-';
    for (const auto& [code, alias] : DirectionAliases) {
        if (subtags.language == code) subtags.language = alias;
    }
    locale.remove_prefix(length);
    if (locale.empty() || !Separator(locale.front())) return subtags;
    locale.remove_prefix(1);
    std::size_t letters = 0;
    while (letters < locale.size() && !Terminator(locale[letters]) && !Separator(locale[letters]) &&
           Letter(locale[letters])) {
        if (letters == ScriptLength) return subtags;
        ++letters;
    }
    if (letters != ScriptLength) return subtags;
    subtags.script = Lowered(locale.substr(0, ScriptLength));
    subtags.script.front() = Upper(subtags.script.front());
    return subtags;
}

}

extern "C" {

std::int8_t APS5_VABI uloc_isRightToLeft_nid_postfix(const char* locale) {
    if (!locale) NotImplemented_nid_no_patch("uloc_isRightToLeft of the default locale");
    const Subtags subtags = LanguageAndScript(locale);
    if (subtags.failed || subtags.script.empty()) {
        const auto found = subtags.language.empty() ? std::string_view::npos : LanguageDirections.find(subtags.language);
        const std::size_t next = found + subtags.language.size();
        if (found != std::string_view::npos && next < LanguageDirections.size()) {
            if (LanguageDirections[next] == '-') return 0;
            if (LanguageDirections[next] == '+') return 1;
        }
        NotImplemented_nid_no_patch("uloc_isRightToLeft of a locale whose script comes from the likely subtags data");
    }
    const auto script = static_cast<UScriptCode>(u_getPropertyValueEnum(UCHAR_SCRIPT, subtags.script.c_str()));
    return uscript_isRightToLeft(script) ? 1 : 0;
}

}

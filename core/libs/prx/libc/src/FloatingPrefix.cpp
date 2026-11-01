#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"

namespace {

template<typename TChar> struct CharacterRules;

template<> struct CharacterRules<char> {
    static bool Space(char value) { return isspace_nid_postfix(static_cast<unsigned char>(value)) != 0; }
    static bool Payload(char value) { return isalnum_nid_postfix(static_cast<unsigned char>(value)) != 0 || value == '_'; }
    static bool Hex(char value) { return isxdigit_nid_postfix(static_cast<unsigned char>(value)) != 0; }
};

template<> struct CharacterRules<char16_t> {
    static bool Space(char16_t value) { return iswspace_nid_postfix(value) != 0; }
    static bool Payload(char16_t value) { return iswalnum_nid_postfix(value) != 0 || value == '_'; }
    static bool Hex(char16_t value) { return iswxdigit_nid_postfix(value) != 0; }
};

template<typename TChar>
bool Matches(const TChar* input, const char* word) {
    while (*word) {
        if (*input != *word && *input != *word - ('a' - 'A')) return false;
        ++input;
        ++word;
    }
    return true;
}

template<typename TChar>
int ParsePrefix(const TChar** cursor, TChar** end) {
    const TChar* const original = *cursor;
    const TChar* current = original;
    while (CharacterRules<TChar>::Space(*current)) ++current;
    const int negative = *current == '-' ? 8 : 0;
    if (*current == '-' || *current == '+') ++current;
    int result = negative | 1;
    if (*current == 'n' || *current == 'N') {
        if (Matches(current, "nan")) {
            current += 3;
            result = 4;
            if (*current == '(') {
                const TChar* payload = current + 1;
                while (CharacterRules<TChar>::Payload(*payload)) ++payload;
                if (*payload == ')') current = payload + 1;
            }
        } else {
            current = original;
            result = 0;
        }
        if (end) *end = const_cast<TChar*>(current);
    } else if (*current == 'i' || *current == 'I') {
        if (Matches(current, "inf")) {
            current += 3;
            if (Matches(current, "inity")) current += 5;
            result = negative | 3;
        } else {
            current = original;
            result = 0;
        }
        if (end) *end = const_cast<TChar*>(current);
    } else if (*current == '0' && (current[1] == 'x' || current[1] == 'X')) {
        const TChar* digit = current + 2;
        if (*digit == '.') ++digit;
        if (CharacterRules<TChar>::Hex(*digit)) {
            current += 2;
            result = negative | 2;
        }
    }
    *cursor = current;
    return result;
}

}

extern "C" {

int APS5_VABI _Stopfx_nid_postfix(const char** cursor, char** end) {
    if (cursor == nullptr || *cursor == nullptr) APS5_INVALID_ARG_EX;
    return ParsePrefix(cursor, end);
}

int APS5_VABI _WStopfx_nid_postfix(const char16_t** cursor, char16_t** end) {
    if (cursor == nullptr || *cursor == nullptr) APS5_INVALID_ARG_EX;
    return ParsePrefix(cursor, end);
}

}

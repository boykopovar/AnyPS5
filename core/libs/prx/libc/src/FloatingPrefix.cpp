#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"
#include <cerrno>

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
TChar ReadGuestChar(const TChar* address) {
    const int savedErrno = errno;
    TChar value{};
    const bool ok = GuestCopyIn_nid_no_patch(&value, address, sizeof(value));
    errno = savedErrno;
    if (!ok) APS5_INVALID_ARG_EX;
    return value;
}

template<typename TChar>
bool Matches(const TChar* input, const char* word) {
    while (*word) {
        const TChar ch = ReadGuestChar(input);
        if (ch != *word && ch != *word - ('a' - 'A')) return false;
        ++input;
        ++word;
    }
    return true;
}

template<typename TChar>
int ParsePrefix(const TChar** cursor, TChar** end) {
    if (cursor == nullptr) APS5_INVALID_ARG_EX;
    const TChar* original = nullptr;
    {
        const int savedErrno = errno;
        const bool readable = GuestCopyIn_nid_no_patch(&original, cursor, sizeof(original));
        const bool writableCursor = readable && original != nullptr && GuestWritable_nid_no_patch(const_cast<TChar**>(cursor), sizeof(*cursor));
        const bool writableEnd = end == nullptr || GuestWritable_nid_no_patch(end, sizeof(*end));
        errno = savedErrno;
        if (!readable || original == nullptr || !writableCursor || !writableEnd) APS5_INVALID_ARG_EX;
    }
    const TChar* current = original;
    while (CharacterRules<TChar>::Space(ReadGuestChar(current))) ++current;
    const TChar signChar = ReadGuestChar(current);
    const int negative = signChar == '-' ? 8 : 0;
    if (signChar == '-' || signChar == '+') ++current;
    int result = negative | 1;
    bool writeEnd = false;
    const TChar head = ReadGuestChar(current);
    if (head == 'n' || head == 'N') {
        if (Matches(current, "nan")) {
            current += 3;
            result = 4;
            if (ReadGuestChar(current) == '(') {
                const TChar* payload = current + 1;
                while (CharacterRules<TChar>::Payload(ReadGuestChar(payload))) ++payload;
                if (ReadGuestChar(payload) == ')') current = payload + 1;
            }
        } else {
            current = original;
            result = 0;
        }
        writeEnd = true;
    } else if (head == 'i' || head == 'I') {
        if (Matches(current, "inf")) {
            current += 3;
            if (Matches(current, "inity")) current += 5;
            result = negative | 3;
        } else {
            current = original;
            result = 0;
        }
        writeEnd = true;
    } else if (head == '0') {
        const TChar next = ReadGuestChar(current + 1);
        if (next == 'x' || next == 'X') {
            const TChar* digit = current + 2;
            if (ReadGuestChar(digit) == '.') ++digit;
            if (CharacterRules<TChar>::Hex(ReadGuestChar(digit))) {
                current += 2;
                result = negative | 2;
            }
        }
    }
    {
        const int savedErrno = errno;
        if (writeEnd && end != nullptr) {
            auto* endValue = const_cast<TChar*>(current);
            if (!GuestCopyOut_nid_no_patch(end, &endValue, sizeof(endValue))) {
                errno = savedErrno;
                APS5_INVALID_ARG_EX;
            }
        }
        if (!GuestCopyOut_nid_no_patch(const_cast<TChar**>(cursor), &current, sizeof(current))) {
            errno = savedErrno;
            APS5_INVALID_ARG_EX;
        }
        errno = savedErrno;
    }
    return result;
}

}

extern "C" {

int APS5_VABI _Stopfx_nid_postfix(const char** cursor, char** end) {
    return ParsePrefix(cursor, end);
}

int APS5_VABI _WStopfx_nid_postfix(const char16_t** cursor, char16_t** end) {
    return ParsePrefix(cursor, end);
}

}

#include "prx/libSceLibcInternal/include/FloatDigits.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int MaxWords = 5;
constexpr int MaxDigitsPerWord = 9;
constexpr std::int64_t ExponentLimit = 100000000;

struct DecimalDigits {
    static constexpr int Base = 10;
    static constexpr int DigitsPerWord = 9;
    static constexpr int ExponentScale = 1;
    static constexpr char ExponentLower = 'e';
    static constexpr char ExponentUpper = 'E';

    template<typename TChar>
    static int Value(TChar character) {
        return character >= '0' && character <= '9' ? static_cast<int>(character - '0') : -1;
    }
};

struct HexadecimalDigits {
    static constexpr int Base = 16;
    static constexpr int DigitsPerWord = 7;
    static constexpr int ExponentScale = 4;
    static constexpr char ExponentLower = 'p';
    static constexpr char ExponentUpper = 'P';

    template<typename TChar>
    static int Value(TChar character) {
        if (character >= '0' && character <= '9') return static_cast<int>(character - '0');
        if (character >= 'a' && character <= 'f') return static_cast<int>(character - 'a') + 10;
        if (character >= 'A' && character <= 'F') return static_cast<int>(character - 'A') + 10;
        return -1;
    }
};

char16_t DecimalPoint(char16_t) {
    const char point = localeconv_nid_postfix()->decimal_point[0];
    if (point == '\0') return 0;
    const std::uint32_t wide = btowc_nid_postfix(static_cast<unsigned char>(point));
    return wide < 0x10000 ? static_cast<char16_t>(wide) : 0;
}

char DecimalPoint(char) {
    return localeconv_nid_postfix()->decimal_point[0];
}

template<typename TDigits, typename TChar>
int ScanSignificand(const TChar* start, const TChar* text, TChar** end, std::int64_t* words, int maxWords, const char* function) {
    if (start == nullptr || text == nullptr || words == nullptr) throw std::invalid_argument(std::string(function) + ": null argument");
    if (maxWords < 1) NotImplemented_nid_no_patch(function);
    const int limit = (maxWords < MaxWords ? maxWords : MaxWords) * TDigits::DigitsPerWord;
    const TChar point = DecimalPoint(TChar{});

    char digits[MaxWords * MaxDigitsPerWord + 1];
    int count = 0;
    bool seen = false;
    int word = 0;
    words[0] = 0;
    words[1] = 0;

    for (; *text == '0'; ++text) seen = true;
    for (int value; (value = TDigits::Value(*text)) >= 0; ++text, seen = true) {
        if (count <= limit) {
            digits[count++] = static_cast<char>(value);
        } else {
            ++words[0];
        }
    }
    if (point != 0 && *text == point) ++text;
    if (count == 0) {
        for (; *text == '0'; ++text, seen = true) --words[0];
    }
    for (int value; (value = TDigits::Value(*text)) >= 0; ++text, seen = true) {
        if (count <= limit) {
            digits[count++] = static_cast<char>(value);
            --words[0];
        }
    }

    if (limit < count) {
        if (TDigits::Base / 2 <= digits[limit]) ++digits[limit - 1];
        count = limit;
        ++words[0];
    }
    for (; count > 0 && digits[count - 1] == 0; --count) ++words[0];
    if (count == 0) digits[count++] = 0;
    words[0] *= TDigits::ExponentScale;

    if (seen) {
        int position = TDigits::DigitsPerWord - count % TDigits::DigitsPerWord;
        word = position % TDigits::DigitsPerWord == 0 ? 0 : 1;
        for (int index = 0; index < count; ++index, ++position) {
            if (position % TDigits::DigitsPerWord == 0) {
                words[++word] = digits[index];
            } else {
                words[word] = words[word] * TDigits::Base + digits[index];
            }
        }

        if (*text == TDigits::ExponentLower || *text == TDigits::ExponentUpper) {
            const TChar* marker = text++;
            const bool negative = *text == '-';
            if (*text == '+' || *text == '-') ++text;
            bool exponentSeen = false;
            std::int64_t exponent = 0;
            for (; *text >= '0' && *text <= '9'; ++text, exponentSeen = true) {
                if (exponent < ExponentLimit) exponent = exponent * 10 + static_cast<int>(*text - '0');
            }
            if (exponentSeen) {
                words[0] += negative ? -exponent : exponent;
            } else {
                text = marker;
            }
        }
    }

    if (!seen) word = 0;
    if (end != nullptr) *end = const_cast<TChar*>(seen ? text : start);
    return word;
}

template<typename TFloat>
TFloat ScaledWide(const char16_t* text, char16_t** end, std::int64_t pten, int* error, TFloat (APS5_VABI *parse)(const char*, char**, std::int64_t, int*), const char* function) {
    if (text == nullptr) throw std::invalid_argument(std::string(function) + ": null text");
    std::string prefix;
    for (const char16_t* character = text; *character != 0 && *character < 0x80; ++character) prefix.push_back(static_cast<char>(*character));
    char* stop = nullptr;
    const TFloat value = parse(prefix.c_str(), &stop, pten, error);
    if (end != nullptr) *end = const_cast<char16_t*>(text) + (stop - prefix.c_str());
    return value;
}

}

extern "C" {

int APS5_VABI _Stoflt_nid_postfix(const char* start, const char* text, char** end, std::int64_t* words, int maxWords) {
    return ScanSignificand<DecimalDigits>(start, text, end, words, maxWords, "_Stoflt");
}

int APS5_VABI _Stoxflt_nid_postfix(const char* start, const char* text, char** end, std::int64_t* words, int maxWords) {
    return ScanSignificand<HexadecimalDigits>(start, text, end, words, maxWords, "_Stoxflt");
}

int APS5_VABI _WStoflt_nid_postfix(const char16_t* start, const char16_t* text, char16_t** end, std::int64_t* words, int maxWords) {
    return ScanSignificand<DecimalDigits>(start, text, end, words, maxWords, "_WStoflt");
}

int APS5_VABI _WStoxflt_nid_postfix(const char16_t* start, const char16_t* text, char16_t** end, std::int64_t* words, int maxWords) {
    return ScanSignificand<HexadecimalDigits>(start, text, end, words, maxWords, "_WStoxflt");
}

double APS5_VABI _WStodx_nid_postfix(const char16_t* text, char16_t** end, std::int64_t pten, int* error) {
    return ScaledWide<double>(text, end, pten, error, _Stodx_nid_postfix, "_WStodx");
}

float APS5_VABI _WStofx_nid_postfix(const char16_t* text, char16_t** end, std::int64_t pten, int* error) {
    return ScaledWide<float>(text, end, pten, error, _Stofx_nid_postfix, "_WStofx");
}

long double APS5_VABI _WStoldx_nid_postfix(const char16_t* text, char16_t** end, std::int64_t pten, int* error) {
    return ScaledWide<long double>(text, end, pten, error, _Stoldx_nid_postfix, "_WStoldx");
}

}

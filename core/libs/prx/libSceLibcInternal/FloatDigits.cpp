#include "prx/libSceLibcInternal/include/FloatDigits.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int WordCapacity = 5;
constexpr std::int64_t ExponentCeiling = 999999999;

struct Radix {
    int base;
    int digitsPerWord;
    int bitsPerDigit;
    char exponentMarker;
};

constexpr Radix Decimal{10, 9, 0, 'e'};
constexpr Radix Hexadecimal{16, 7, 4, 'p'};

template<typename TChar>
int DigitValue(const Radix& radix, TChar character) {
    int value = -1;
    if (character >= '0' && character <= '9') {
        value = static_cast<int>(character - '0');
    } else if (character >= 'a' && character <= 'z') {
        value = static_cast<int>(character - 'a') + 10;
    } else if (character >= 'A' && character <= 'Z') {
        value = static_cast<int>(character - 'A') + 10;
    }
    return value < radix.base ? value : -1;
}

template<typename TChar>
bool IsExponentMarker(const Radix& radix, TChar character) {
    return character == radix.exponentMarker || character == radix.exponentMarker - ('a' - 'A');
}

char LocalePoint(const char*) {
    return localeconv_nid_postfix()->decimal_point[0];
}

char16_t LocalePoint(const char16_t*) {
    const char narrow = localeconv_nid_postfix()->decimal_point[0];
    if (narrow == '\0') return 0;
    const std::uint32_t wide = btowc_nid_postfix(static_cast<unsigned char>(narrow));
    return wide < 0x10000 ? static_cast<char16_t>(wide) : 0;
}

class Significand {
public:
    Significand(const Radix& radix, int wordLimit) : radix_(radix), keep_(wordLimit * radix.digitsPerWord) {}

    void Accept(int value, bool fraction) {
        sawDigit_ = true;
        if (length_ == 0 && value == 0) {
            if (fraction) --scale_;
            return;
        }
        if (length_ <= keep_) {
            digits_[length_++] = static_cast<std::uint8_t>(value);
            if (fraction) --scale_;
        } else if (!fraction) {
            ++scale_;
        }
    }

    bool SawDigit() const {
        return sawDigit_;
    }

    void Normalize() {
        if (length_ > keep_) {
            const bool roundUp = 2 * digits_[keep_] >= radix_.base;
            length_ = keep_;
            ++scale_;
            if (roundUp) Increment();
        }
        while (length_ > 0 && digits_[length_ - 1] == 0) {
            --length_;
            ++scale_;
        }
    }

    int Store(std::int64_t* words) const {
        if (length_ == 0) {
            words[0] = 0;
            words[1] = 0;
            return 1;
        }
        const int count = (length_ + radix_.digitsPerWord - 1) / radix_.digitsPerWord;
        int stop = length_;
        for (int slot = count; slot >= 1; --slot) {
            const int first = std::max(0, stop - radix_.digitsPerWord);
            std::int64_t word = 0;
            for (int index = first; index < stop; ++index) word = word * radix_.base + digits_[index];
            words[slot] = word;
            stop = first;
        }
        words[0] = radix_.bitsPerDigit == 0 ? scale_ : scale_ * radix_.bitsPerDigit;
        return count;
    }

    bool IsZero() const {
        return length_ == 0;
    }

private:
    void Increment() {
        for (int index = length_ - 1; index >= 0; --index) {
            if (++digits_[index] < radix_.base) return;
            digits_[index] = 0;
        }
        digits_[0] = 1;
        ++scale_;
    }

    const Radix& radix_;
    const int keep_;
    std::array<std::uint8_t, WordCapacity * 9 + 1> digits_{};
    int length_ = 0;
    std::int64_t scale_ = 0;
    bool sawDigit_ = false;
};

template<typename TChar>
bool ReadExponent(const Radix& radix, const TChar*& cursor, std::int64_t& exponent) {
    if (!IsExponentMarker(radix, *cursor)) return false;
    const TChar* probe = cursor + 1;
    bool negative = false;
    if (*probe == '+' || *probe == '-') negative = *probe++ == '-';
    if (*probe < '0' || *probe > '9') return false;
    std::int64_t magnitude = 0;
    for (; *probe >= '0' && *probe <= '9'; ++probe) {
        magnitude = std::min(ExponentCeiling, magnitude * 10 + static_cast<int>(*probe - '0'));
    }
    exponent = negative ? -magnitude : magnitude;
    cursor = probe;
    return true;
}

template<typename TChar>
int Scan(const Radix& radix, const TChar* start, const TChar* text, TChar** end, std::int64_t* words, int maxWords, const char* function) {
    if (start == nullptr || text == nullptr || words == nullptr) throw std::invalid_argument(std::string(function) + ": null argument");
    if (maxWords < 1) NotImplemented_nid_no_patch(function);

    const TChar point = LocalePoint(start);
    Significand significand(radix, std::min(maxWords, WordCapacity));
    const TChar* cursor = text;
    bool fraction = false;
    for (;; ++cursor) {
        if (!fraction && point != 0 && *cursor == point) {
            fraction = true;
            continue;
        }
        const int value = DigitValue(radix, *cursor);
        if (value < 0) break;
        significand.Accept(value, fraction);
    }

    if (!significand.SawDigit()) {
        words[0] = 0;
        words[1] = 0;
        if (end != nullptr) *end = const_cast<TChar*>(start);
        return 0;
    }

    significand.Normalize();
    const int count = significand.Store(words);
    std::int64_t exponent = 0;
    if (ReadExponent(radix, cursor, exponent) && !significand.IsZero()) words[0] += exponent;
    if (end != nullptr) *end = const_cast<TChar*>(cursor);
    return count;
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
    return Scan(Decimal, start, text, end, words, maxWords, "_Stoflt");
}

int APS5_VABI _Stoxflt_nid_postfix(const char* start, const char* text, char** end, std::int64_t* words, int maxWords) {
    return Scan(Hexadecimal, start, text, end, words, maxWords, "_Stoxflt");
}

int APS5_VABI _WStoflt_nid_postfix(const char16_t* start, const char16_t* text, char16_t** end, std::int64_t* words, int maxWords) {
    return Scan(Decimal, start, text, end, words, maxWords, "_WStoflt");
}

int APS5_VABI _WStoxflt_nid_postfix(const char16_t* start, const char16_t* text, char16_t** end, std::int64_t* words, int maxWords) {
    return Scan(Hexadecimal, start, text, end, words, maxWords, "_WStoxflt");
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

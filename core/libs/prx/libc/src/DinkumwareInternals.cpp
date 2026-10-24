#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "prx/libc/include/General.hpp"

extern "C" {
int* APS5_VABI __error_nid_postfix();
double APS5_VABI strtod_nid_postfix(const char* str, char** endptr);
float APS5_VABI strtof_nid_postfix(const char* str, char** endptr);
long double APS5_VABI strtold_nid_postfix(const char* str, char** endptr);
long long APS5_VABI strtoll_nid_postfix(const char* str, char** endptr, int base);
unsigned long long APS5_VABI strtoull_nid_postfix(const char* str, char** endptr, int base);
double APS5_VABI sin_nid_postfix(double x);
double APS5_VABI cos_nid_postfix(double x);
float APS5_VABI sinf_nid_postfix(float x);
float APS5_VABI cosf_nid_postfix(float x);
long double APS5_VABI sinl_nid_postfix(long double x);
long double APS5_VABI cosl_nid_postfix(long double x);
double APS5_VABI log_nid_postfix(double x);
double APS5_VABI log10_nid_postfix(double x);
double APS5_VABI log2_nid_postfix(double x);
float APS5_VABI logf_nid_postfix(float x);
float APS5_VABI log10f_nid_postfix(float x);
float APS5_VABI log2f_nid_postfix(float x);
}

namespace {

constexpr long long ExponentLimit = 1000000000;
constexpr int InvalidArgument = 22;
constexpr int RangeError = 34;

bool Hexadecimal(const char* text) {
    while (std::isspace(static_cast<unsigned char>(*text))) ++text;
    if (*text == '+' || *text == '-') ++text;
    return text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
}

template<typename TFloat>
TFloat ScaledFloat(const char* text, char** end, std::int64_t pten, int* error, TFloat (APS5_VABI *parse)(const char*, char**), const char* function) {
    int& guestErrno = *__error_nid_postfix();
    const int saved = guestErrno;
    if (error != nullptr) *error = 0;
    guestErrno = 0;
    char* stop = nullptr;
    TFloat value = parse(text, &stop);
    if (end != nullptr) *end = stop;
    const bool overflowed = std::isinf(value) && guestErrno == RangeError;
    if (pten != 0 && stop != text && (std::isfinite(value) || overflowed)) {
        if (Hexadecimal(text)) NotImplemented_nid_no_patch(function);
        std::string number(text, static_cast<const char*>(stop));
        long long exponent = 0;
        const auto marker = number.find_first_of("eE");
        if (marker != std::string::npos) {
            exponent = std::strtoll(number.c_str() + marker + 1, nullptr, 10);
            number.resize(marker);
        }
        exponent = std::clamp(exponent, -ExponentLimit, ExponentLimit) + std::clamp<std::int64_t>(pten, -ExponentLimit, ExponentLimit);
        number += 'e';
        number += std::to_string(exponent);
        guestErrno = 0;
        value = parse(number.c_str(), nullptr);
    }
    if (guestErrno == RangeError && (value == 0 || std::isinf(value))) {
        if (error != nullptr) *error = 1;
    } else if (guestErrno == 0 || guestErrno == RangeError) {
        guestErrno = saved;
    }
    return value;
}

bool InvalidBase(const char* text, char** end, int base, int* error) {
    if (base >= 0 && base != 1 && base <= 36) return false;
    if (end != nullptr) *end = const_cast<char*>(text);
    if (error != nullptr) *error = InvalidArgument;
    return true;
}

template<typename TInteger>
TInteger ParsedInteger(const char* text, char** end, int base, int* error, TInteger (APS5_VABI *parse)(const char*, char**, int)) {
    if (error != nullptr) *error = 0;
    if (InvalidBase(text, end, base, error)) return 0;
    int& guestErrno = *__error_nid_postfix();
    const int saved = guestErrno;
    guestErrno = 0;
    const TInteger value = parse(text, end, base);
    if (error != nullptr) *error = guestErrno;
    if (guestErrno == 0) guestErrno = saved;
    return value;
}

}

extern "C" {

double APS5_VABI _Stodx_nid_postfix(const char* text, char** end, std::int64_t pten, int* error) {
    return ScaledFloat<double>(text, end, pten, error, strtod_nid_postfix, "_Stodx (hexadecimal text with a power-of-ten scale)");
}

float APS5_VABI _Stofx_nid_postfix(const char* text, char** end, std::int64_t pten, int* error) {
    return ScaledFloat<float>(text, end, pten, error, strtof_nid_postfix, "_Stofx (hexadecimal text with a power-of-ten scale)");
}

long double APS5_VABI _Stoldx_nid_postfix(const char* text, char** end, std::int64_t pten, int* error) {
    return ScaledFloat<long double>(text, end, pten, error, strtold_nid_postfix, "_Stoldx (hexadecimal text with a power-of-ten scale)");
}

double APS5_VABI _Stod_nid_postfix(const char* text, char** end, std::int64_t pten) {
    return _Stodx_nid_postfix(text, end, pten, nullptr);
}

float APS5_VABI _Stof_nid_postfix(const char* text, char** end, std::int64_t pten) {
    return _Stofx_nid_postfix(text, end, pten, nullptr);
}

long double APS5_VABI _Stold_nid_postfix(const char* text, char** end, std::int64_t pten) {
    return _Stoldx_nid_postfix(text, end, pten, nullptr);
}

long long APS5_VABI _Stollx_nid_postfix(const char* text, char** end, int base, int* error) {
    return ParsedInteger<long long>(text, end, base, error, strtoll_nid_postfix);
}

unsigned long long APS5_VABI _Stoullx_nid_postfix(const char* text, char** end, int base, int* error) {
    return ParsedInteger<unsigned long long>(text, end, base, error, strtoull_nid_postfix);
}

std::uint64_t APS5_VABI _Stoulx_nid_postfix(const char* text, char** end, int base, int* error) {
    return _Stoullx_nid_postfix(text, end, base, error);
}

std::int64_t APS5_VABI _Stolx_nid_postfix(const char* text, char** end, int base, int* error) {
    if (error != nullptr) *error = 0;
    if (InvalidBase(text, end, base, error)) return 0;
    int& guestErrno = *__error_nid_postfix();
    const int saved = guestErrno;
    guestErrno = 0;
    const long long value = strtoll_nid_postfix(text, end, base);
    if (error != nullptr) *error = guestErrno;
    guestErrno = 0;
    static_cast<void>(strtoull_nid_postfix(text, nullptr, base));
    if (guestErrno == 0) guestErrno = saved;
    return value;
}

long long APS5_VABI _Stoll_nid_postfix(const char* text, char** end, int base) {
    return _Stollx_nid_postfix(text, end, base, nullptr);
}

double APS5_VABI _Sin_nid_postfix(double x, unsigned int quadrant) {
    return quadrant != 0 ? cos_nid_postfix(x) : sin_nid_postfix(x);
}

float APS5_VABI _FSin_nid_postfix(float x, unsigned int quadrant) {
    return quadrant != 0 ? cosf_nid_postfix(x) : sinf_nid_postfix(x);
}

long double APS5_VABI _LSin_nid_postfix(long double x, unsigned int quadrant) {
    return quadrant != 0 ? cosl_nid_postfix(x) : sinl_nid_postfix(x);
}

double APS5_VABI _Log_nid_postfix(double x, int base) {
    if (base > 0) return log10_nid_postfix(x);
    return base == 0 ? log_nid_postfix(x) : log2_nid_postfix(x);
}

float APS5_VABI _FLog_nid_postfix(float x, int base) {
    if (base > 0) return log10f_nid_postfix(x);
    return base == 0 ? logf_nid_postfix(x) : log2f_nid_postfix(x);
}

}

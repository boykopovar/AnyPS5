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
long long APS5_VABI strtoll_nid_postfix(const char* str, char** endptr, int base);
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
constexpr int RangeError = 34;

bool Hexadecimal(const char* text) {
    while (std::isspace(static_cast<unsigned char>(*text))) ++text;
    if (*text == '+' || *text == '-') ++text;
    return text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
}

}

extern "C" {

double APS5_VABI _Stod_nid_postfix(const char* text, char** end, std::int64_t pten) {
    int& guestErrno = *__error_nid_postfix();
    const int saved = guestErrno;
    guestErrno = 0;
    char* stop = nullptr;
    const double value = strtod_nid_postfix(text, &stop);
    if (end != nullptr) *end = stop;
    const bool overflowed = std::isinf(value) && guestErrno == RangeError;
    if (pten == 0 || stop == text || !(std::isfinite(value) || overflowed)) {
        if (guestErrno == 0) guestErrno = saved;
        return value;
    }
    if (Hexadecimal(text)) NotImplemented_nid_no_patch("_Stod (hexadecimal text with a power-of-ten scale)");
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
    guestErrno = saved;
    return strtod_nid_postfix(number.c_str(), nullptr);
}

long long APS5_VABI _Stoll_nid_postfix(const char* text, char** end, int base) {
    return strtoll_nid_postfix(text, end, base);
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

#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <bit>
#include <cctype>
#include <cerrno>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"

namespace {

constexpr long long ExponentLimit = 1000000000;
constexpr int InvalidArgument = 22;
constexpr int DomainError = 33;
constexpr int RangeError = 34;

constexpr long double LogPolynomial[] = {
    0x87f45b3375e0c9edp-66L, 0x87a58c55129af2f8p-66L, 0x9d90cfd7547e4ed2p-66L, 0xba2e6ba9488d3d0cp-66L,
    0xe38e393a60e2888bp-66L, 0x924924920646cc89p-65L, 0xcccccccccd00c2ebp-65L, 0xaaaaaaaaaaaaa334p-64L,
};

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
    if (const char* stop = NanSubjectEnd_nid_no_patch(text)) {
        if (end != nullptr) *end = const_cast<char*>(stop);
        return std::numeric_limits<TFloat>::quiet_NaN();
    }
    guestErrno = 0;
    char* stop = nullptr;
    TFloat value = parse(text, &stop);
    if (end != nullptr) *end = stop;
    if (pten == 0 && Hexadecimal(text)) {
        guestErrno = saved;
        return value;
    }
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

constexpr short Denormal = -2, Finite = -1, Zero = 0, Infinite = 1, NotANumber = 2;

void RaiseLibcError(int exceptions) {
    if ((exceptions & (FE_OVERFLOW | FE_UNDERFLOW)) != 0) exceptions |= FE_INEXACT;
    std::feraiseexcept(exceptions);
    const bool range = (exceptions & FE_INVALID) == 0 &&
        (exceptions & (FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW)) != 0;
    errno = range ? ERANGE : EDOM;
}

template<typename TFloat>
short ClassifyValue(TFloat value) {
    switch (std::fpclassify(value)) {
    case FP_NAN:
        return NotANumber;
    case FP_INFINITE:
        return Infinite;
    case FP_ZERO:
        return Zero;
    case FP_SUBNORMAL:
        return Denormal;
    default:
        return Finite;
    }
}

template<typename TFloat>
short ScaleProduct(TFloat& value, TFloat factor, TFloat multiplier, long exponent) {
    std::fexcept_t saved;
    std::fegetexceptflag(&saved, FE_ALL_EXCEPT);
    const int savedErrno = errno;
    TFloat result = factor * multiplier;
    if (!std::isfinite(result) || result == static_cast<TFloat>(0)) {
        int factorExponent = 0;
        int multiplierExponent = 0;
        const TFloat factorMantissa = std::frexp(factor, &factorExponent);
        const TFloat multiplierMantissa = std::frexp(multiplier, &multiplierExponent);
        const long long combinedExponent = static_cast<long long>(factorExponent) + multiplierExponent + exponent;
        result = std::scalbn(factorMantissa * multiplierMantissa, static_cast<int>(combinedExponent));
    } else {
        result = std::scalbn(result, static_cast<int>(exponent));
    }
    value = result;
    const short kind = ClassifyValue(result);
    const int raised = std::fetestexcept(FE_ALL_EXCEPT) & ~static_cast<int>(saved) & FE_ALL_EXCEPT;
    std::fesetexceptflag(&saved, FE_ALL_EXCEPT);
    errno = savedErrno;
    if (kind == Infinite) {
        RaiseLibcError(FE_OVERFLOW);
        return Infinite;
    }
    if (kind == Zero) {
        RaiseLibcError(FE_UNDERFLOW);
        return Zero;
    }
    if (kind == Denormal) return Finite;
    if (kind == NotANumber) return NotANumber;
    if ((raised & FE_INEXACT) != 0) std::feraiseexcept(FE_INEXACT);
    return Finite;
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

short APS5_VABI _Exp_nid_postfix(double* value, double y, short offset) {
    if (value == nullptr) throw std::invalid_argument("_Exp: null value");
    std::fexcept_t entryFlags;
    std::fegetexceptflag(&entryFlags, FE_ALL_EXCEPT);
    const int entryErrno = errno;
    const double x = *value;
    constexpr double HugeExp = 1842.0;
    constexpr double Eps = std::numeric_limits<double>::epsilon();
    constexpr double InverseLn2 = 1.4426950408889634073599246810018921;
    constexpr double Ln2High = 0x1.62e42f8p-1;
    constexpr double Ln2Low = 0x1.be8e7bcd5e4f2p-27;
    constexpr double P0 = 0x1.152b741a5e84bp-15;
    constexpr double P1 = 0x1.c70e8daf3bd0bp-7;
    constexpr double Q0 = 0x1.03fa081572e11p-10;
    constexpr double Q1 = 0x1.c718f8c124358p-4;
    constexpr double E = 0x1.5bf0a8b145769p+1;
    const short xKind = ClassifyValue(x);
    const short yKind = ClassifyValue(y);
    if (xKind == NotANumber) return NotANumber;
    if (yKind == NotANumber) return NotANumber;
    if (xKind == Infinite) {
        if (x > 0.0) {
            if (yKind == Zero) {
                *value = std::numeric_limits<double>::quiet_NaN();
                RaiseLibcError(FE_INVALID);
                return NotANumber;
            }
            if (yKind == Infinite) {
                *value = y;
            } else {
                *value = std::copysign(std::numeric_limits<double>::infinity(), y);
            }
            return Infinite;
        }
        if (yKind == Infinite) {
            *value = std::numeric_limits<double>::quiet_NaN();
            RaiseLibcError(FE_INVALID);
            return NotANumber;
        }
        *value = std::copysign(0.0, y);
        return Zero;
    }
    if (yKind == Zero) {
        *value = y;
        return Zero;
    }
    if (yKind == Infinite) {
        *value = y;
        return Infinite;
    }
    if (xKind == Zero) {
        if (offset == 0) {
            *value = y;
            return Finite;
        }
        const short result = ScaleProduct(*value, 1.0, y, offset);
        if (result == Finite && std::fpclassify(*value) == FP_SUBNORMAL) {
            std::fesetexceptflag(&entryFlags, FE_ALL_EXCEPT);
            errno = entryErrno;
        }
        return result;
    }
    if (offset == 0 && x == 1.0 && y == 1.0) {
        *value = E;
        return Finite;
    }
    if (x < -HugeExp) {
        *value = std::copysign(0.0, y);
        return Zero;
    }
    if (x > HugeExp) {
        *value = std::copysign(std::numeric_limits<double>::infinity(), y);
        RaiseLibcError(FE_OVERFLOW);
        return Infinite;
    }
    double g = x * InverseLn2;
    long exponent = static_cast<long>(g + (g < 0.0 ? -0.5 : 0.5));
    g = static_cast<double>(exponent);
    g = (x - g * Ln2High) - g * Ln2Low;
    double factor;
    if (-Eps * 0.5 < g && g < Eps * 0.5) {
        factor = 1.0;
    } else {
        const double z = g * g;
        const double w = (Q0 * z + Q1) * z;
        g *= (P0 * z + P1) * z + 0.5;
        factor = (w + g + 1.0) / (w - g + 1.0);
    }
    const short result = ScaleProduct(*value, factor, y, exponent + offset);
    if (result == Finite && std::fpclassify(*value) == FP_SUBNORMAL) {
        std::fesetexceptflag(&entryFlags, FE_ALL_EXCEPT);
        errno = entryErrno;
    }
    return result;
}

short APS5_VABI _FExp_nid_postfix(float* value, float y, short offset) {
    if (value == nullptr) throw std::invalid_argument("_FExp: null value");
    std::fexcept_t entryFlags;
    std::fegetexceptflag(&entryFlags, FE_ALL_EXCEPT);
    const int entryErrno = errno;
    const float x = *value;
    constexpr float HugeExp = 229.0f;
    constexpr float Eps = std::numeric_limits<float>::epsilon();
    constexpr float InverseLn2 = 1.4426950216f;
    constexpr float Ln2High = 0.69287109375f;
    constexpr float Ln2Low = 0.00027608682f;
    constexpr float Q = 0.09997485578f;
    constexpr float P0 = 0.5f;
    constexpr float P1 = 0.0083208261f;
    constexpr float E = 0x1.5bf0a8p+1f;
    const short xKind = ClassifyValue(x);
    const short yKind = ClassifyValue(y);
    if (xKind == NotANumber) return NotANumber;
    if (yKind == NotANumber) return NotANumber;
    if (xKind == Infinite) {
        if (x > 0.0f) {
            if (yKind == Zero) {
                *value = std::numeric_limits<float>::quiet_NaN();
                RaiseLibcError(FE_INVALID);
                return NotANumber;
            }
            if (yKind == Infinite) {
                *value = y;
            } else {
                *value = std::copysign(std::numeric_limits<float>::infinity(), y);
            }
            return Infinite;
        }
        if (yKind == Infinite) {
            *value = std::numeric_limits<float>::quiet_NaN();
            RaiseLibcError(FE_INVALID);
            return NotANumber;
        }
        *value = std::copysign(0.0f, y);
        return Zero;
    }
    if (yKind == Zero) {
        *value = y;
        return Zero;
    }
    if (yKind == Infinite) {
        *value = y;
        return Infinite;
    }
    if (xKind == Zero) {
        if (offset == 0) {
            *value = y;
            return Finite;
        }
        const short result = ScaleProduct(*value, 1.0f, y, offset);
        if (result == Finite && std::fpclassify(*value) == FP_SUBNORMAL) {
            std::fesetexceptflag(&entryFlags, FE_ALL_EXCEPT);
            errno = entryErrno;
        }
        return result;
    }
    if (offset == 0 && x == 1.0f && y == 1.0f) {
        *value = E;
        return Finite;
    }
    if (x < -HugeExp) {
        *value = std::copysign(0.0f, y);
        return Zero;
    }
    if (x > HugeExp) {
        *value = std::copysign(std::numeric_limits<float>::infinity(), y);
        RaiseLibcError(FE_OVERFLOW);
        return Infinite;
    }
    float g = x * InverseLn2;
    long exponent = static_cast<long>(g + (g < 0.0f ? -0.5f : 0.5f));
    g = static_cast<float>(exponent);
    g = (x - g * Ln2High) - g * Ln2Low;
    float factor;
    if (-Eps * 0.5f < g && g < Eps * 0.5f) {
        factor = 1.0f;
    } else {
        const float z = g * g;
        const float w = Q * z;
        g *= P1 * z + P0;
        factor = (w + g + 1.0f) / (w - g + 1.0f);
    }
    const short result = ScaleProduct(*value, factor, y, exponent + offset);
    if (result == Finite && std::fpclassify(*value) == FP_SUBNORMAL) {
        std::fesetexceptflag(&entryFlags, FE_ALL_EXCEPT);
        errno = entryErrno;
    }
    return result;
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

double APS5_VABI _Divide_nid_postfix(double x, double y) {
    return x / y;
}

float APS5_VABI _FDivide_nid_postfix(float x, float y) {
    return x / y;
}

double APS5_VABI _Recip_nid_postfix(double x) {
    return 1.0 / x;
}

float APS5_VABI _FRecip_nid_postfix(float x) {
    return 1.0f / x;
}

double APS5_VABI _Log_nid_postfix(double x, int base) {
    if (base > 0) return log10_nid_postfix(x);
    return base == 0 ? log_nid_postfix(x) : log2_nid_postfix(x);
}

float APS5_VABI _FLog_nid_postfix(float x, int base) {
    if (base > 0) return log10f_nid_postfix(x);
    return base == 0 ? logf_nid_postfix(x) : log2f_nid_postfix(x);
}

long double APS5_VABI _LLog_nid_postfix(long double x, int base) {
    static_assert(sizeof(long double) == 16 && std::numeric_limits<long double>::digits == 64);
    std::uint64_t mantissa;
    std::uint16_t signExponent;
    std::memcpy(&mantissa, &x, sizeof(mantissa));
    std::memcpy(&signExponent, reinterpret_cast<const unsigned char*>(&x) + sizeof(mantissa), sizeof(signExponent));
    const int exponent = signExponent & 0x7fff;
    const bool negative = (signExponent & 0x8000) != 0;
    if (exponent == 0x7fff) {
        if ((mantissa & 0x7fffffffffffffffull) != 0 || !negative) return x;
    } else if (mantissa == 0) {
        std::feraiseexcept(FE_DIVBYZERO);
        *__error_nid_postfix() = RangeError;
        return -std::numeric_limits<long double>::infinity();
    }
    if (negative) {
        std::feraiseexcept(FE_INVALID);
        *__error_nid_postfix() = DomainError;
        return std::numeric_limits<long double>::quiet_NaN();
    }
    const int shift = std::countl_zero(mantissa);
    int scale = std::max(exponent, 1) - shift - 0x3ffe;
    const std::uint64_t normalized = mantissa << shift;
    const std::uint16_t halfExponent = 0x3ffe;
    long double fraction = 0;
    std::memcpy(&fraction, &normalized, sizeof(normalized));
    std::memcpy(reinterpret_cast<unsigned char*>(&fraction) + sizeof(normalized), &halfExponent, sizeof(halfExponent));
    if (fraction < 0xb504f333f9de6484p-64L) {
        fraction = fraction + fraction;
        --scale;
    }
    const long double y = fraction - 1.0L;
    const long double z = y / (fraction + 1.0L);
    const long double w = z * z;
    long double p = LogPolynomial[0];
    for (std::size_t index = 1; index < std::size(LogPolynomial); ++index) p = p * w + LogPolynomial[index];
    const long double q = z * (y - p * w);
    const long double e = scale;
    if (base == 0) return (y - (q + -0xfbe8e7bcd5e4f1dap-87L * e)) + e * 0x1.62e42cp-1f;
    if (base > 0) return ((0xa937287195355babp-89L * y - (q * 0xde5bd8a937287195p-65L + -0xa13ef3fde623e256p-87L * e)) + y * 0x1.bcb7bp-2f) + e * 0x1.34413p-2f;
    return ((q * -0xb8aa3b295c17f0bcp-63L + 0xa5705fc2eefa1ffbp-89L * y) + y * 0x1.715476p+0f) + e;
}

long double APS5_VABI _LDivide_nid_postfix(long double x, long double y) {
    return x / y;
}

long double APS5_VABI _LRecip_nid_postfix(long double x) {
    return 1.0L / x;
}

}

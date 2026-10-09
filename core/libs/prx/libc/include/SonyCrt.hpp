#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SONYCRT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SONYCRT_HPP

#include <cfenv>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <stdexcept>

#include "SceTypes.hpp"

namespace {

using SonyConstraintHandler = void (*)(const char* message, void* pointer, int error);

inline void SonyDefaultConstraintHandler(const char* message, void* pointer, int error) {
    (void)message;
    (void)pointer;
    (void)error;
    std::abort();
}

inline std::mutex& SonyConstraintMutex() {
    static std::mutex mutex;
    return mutex;
}

inline SonyConstraintHandler& SonyConstraintHandlerSlot() {
    static SonyConstraintHandler handler = SonyDefaultConstraintHandler;
    return handler;
}

inline void SonyInvokeConstraintHandler(const char* message, void* pointer, int error) {
    SonyConstraintHandler handler = nullptr;
    {
        std::lock_guard<std::mutex> lock(SonyConstraintMutex());
        handler = SonyConstraintHandlerSlot();
    }
    if (handler) handler(message, pointer, error);
}

inline char* SonyStrtokS(char* str, const char* delim, char** context) {
    if (!delim || !context) return nullptr;
    char* cursor = str ? str : *context;
    if (!cursor) return nullptr;
    cursor += std::strspn(cursor, delim);
    if (*cursor == '\0') {
        *context = cursor;
        return nullptr;
    }
    char* token = cursor;
    cursor += std::strcspn(cursor, delim);
    if (*cursor != '\0') {
        *cursor = '\0';
        *context = cursor + 1;
    } else {
        *context = cursor;
    }
    return token;
}

inline int SonyCtimeS(char* buffer, std::size_t size, const std::time_t* time) {
    static const char* const days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (!buffer || size < 26 || !time) return EINVAL;
    const std::tm* parts = std::localtime(time);
    if (!parts || parts->tm_wday < 0 || parts->tm_wday > 6 || parts->tm_mon < 0 || parts->tm_mon > 11) return EINVAL;
    std::snprintf(buffer, size, "%.3s %.3s %2d %.2d:%.2d:%.2d %4d\n", days[parts->tm_wday], months[parts->tm_mon],
        parts->tm_mday, parts->tm_hour, parts->tm_min, parts->tm_sec, 1900 + parts->tm_year);
    return 0;
}

inline int SonyAsctimeS(char* buffer, std::size_t size, const std::tm* time) {
    static const char* const days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (!buffer || size < 26 || !time) return EINVAL;
    if (time->tm_wday < 0 || time->tm_wday > 6 || time->tm_mon < 0 || time->tm_mon > 11) return EINVAL;
    std::snprintf(buffer, size, "%.3s %.3s %2d %.2d:%.2d:%.2d %4d\n", days[time->tm_wday], months[time->tm_mon],
        time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec, 1900 + time->tm_year);
    return 0;
}

inline void SonyIgnoreHandlerS(const char* message, void* pointer, int error) {
    (void)message;
    (void)pointer;
    (void)error;
}

inline void* SonySetConstraintHandlerS(void* handler) {
    std::lock_guard<std::mutex> lock(SonyConstraintMutex());
    void* previous = reinterpret_cast<void*>(SonyConstraintHandlerSlot());
    SonyConstraintHandlerSlot() = handler ? reinterpret_cast<SonyConstraintHandler>(handler) : SonyDefaultConstraintHandler;
    return previous;
}

inline int SonyWcscpyS(std::uint16_t* dest, std::size_t size, const std::uint16_t* src) {
    if (!dest || !src || size == 0) {
        SonyInvokeConstraintHandler("wcscpy_s", dest, EINVAL);
        return EINVAL;
    }
    std::size_t i = 0;
    while (i + 1 < size && src[i] != 0) {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = 0;
    if (src[i] != 0) {
        SonyInvokeConstraintHandler("wcscpy_s", dest, ERANGE);
        return ERANGE;
    }
    return 0;
}

inline std::size_t SonyMbstowcs(std::uint16_t* dest, const char* src, std::size_t size) {
    if (!src) return 0;
    std::size_t converted = 0;
    while (converted < size) {
        const auto value = static_cast<unsigned char>(src[converted]);
        if (dest) dest[converted] = value;
        if (value == 0) return converted;
        ++converted;
    }
    return converted;
}

inline SonyLconv* SonyLocaleconv(void) {
    static SonyLconv value{};
    static bool initialized = false;
    if (!initialized) {
        for (auto& pointer : value.reserved_pointers) pointer = nullptr;
        for (auto& byte : value.reserved_max) byte = CHAR_MAX;
        for (auto& byte : value.reserved_pad) byte = 0;
        initialized = true;
    }
    if (const std::lconv* host = std::localeconv()) {
        value.decimal_point = host->decimal_point;
        value.thousands_sep = host->thousands_sep;
        value.grouping = host->grouping;
        value.int_curr_symbol = host->int_curr_symbol;
        value.currency_symbol = host->currency_symbol;
        value.mon_decimal_point = host->mon_decimal_point;
        value.mon_thousands_sep = host->mon_thousands_sep;
        value.mon_grouping = host->mon_grouping;
        value.positive_sign = host->positive_sign;
        value.negative_sign = host->negative_sign;
        value.int_frac_digits = host->int_frac_digits;
        value.frac_digits = host->frac_digits;
        value.p_cs_precedes = host->p_cs_precedes;
        value.p_sep_by_space = host->p_sep_by_space;
        value.n_cs_precedes = host->n_cs_precedes;
        value.n_sep_by_space = host->n_sep_by_space;
        value.p_sign_posn = host->p_sign_posn;
        value.n_sign_posn = host->n_sign_posn;
    }
    return &value;
}

inline int SonyFpclassifyd(double value) {
    if (std::isnan(value)) return 2;
    if (std::isinf(value)) return 1;
    if (value == 0.0) return 0;
    if (std::fpclassify(value) == FP_SUBNORMAL) return -2;
    return -1;
}

inline double SonySinh(double x, double y) {
    return y * std::sinh(x);
}

inline int SonyFltrounds(void) {
    switch (std::fegetround()) {
    case FE_TONEAREST: return 1;
    case FE_DOWNWARD: return 3;
    case FE_UPWARD: return 2;
    case FE_TOWARDZERO: return 0;
    default: return 1;
    }
}

inline unsigned __int128 SonyUdivti3(unsigned __int128 dividend, unsigned __int128 divisor) {
    if (divisor == 0) throw std::runtime_error("__udivti3: integer divide by zero");
    unsigned __int128 quotient = 0;
    unsigned __int128 remainder = 0;
    for (int i = 127; i >= 0; --i) {
        remainder = (remainder << 1) | ((dividend >> i) & 1);
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (unsigned __int128)1 << i;
        }
    }
    return quotient;
}

}

#endif

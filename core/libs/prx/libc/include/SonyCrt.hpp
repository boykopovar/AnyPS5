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
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
GuestTm* APS5_VABI localtime_s_nid_postfix(const std::int64_t* timer, GuestTm* result);
extern FileStream* __stderrp_nid_postfix;
}

using SonyConstraintHandler = void (APS5_VABI *)(const char* message, void* pointer, int error);

inline void APS5_VABI SonyDefaultConstraintHandler(const char* message, void* pointer, int error) {
    (void)pointer;
    (void)error;
    fputs_nid_postfix(message, __stderrp_nid_postfix);
    fputc_nid_postfix('\n', __stderrp_nid_postfix);
}

inline std::mutex& SonyConstraintMutex() {
    static std::mutex mutex;
    return mutex;
}

inline SonyConstraintHandler& SonyConstraintHandlerSlot() {
    static SonyConstraintHandler handler = SonyDefaultConstraintHandler;
    return handler;
}

namespace {

template<typename TInteger>
inline TInteger ParseInteger(const char* text, char** end, int base, TInteger (*parse)(const char*, char**, int)) {
#ifdef __APPLE__
    const int saved = errno;
    char* parsedEnd = nullptr;
    const TInteger value = parse(text, &parsedEnd, base);
    if (parsedEnd == text && errno == EINVAL && (base == 0 || (base >= 2 && base <= 36))) errno = saved;
    if (end != nullptr) *end = parsedEnd;
    return value;
#else
    return parse(text, end, base);
#endif
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

inline int SonyFormatAsctime(char* buffer, std::size_t size, const GuestTm& time) {
    static const char* const days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (!buffer || size < 26) return EINVAL;
    const int year = 1900 + time.tm_year;
    if (time.tm_wday < 0 || time.tm_wday > 6 || time.tm_mon < 0 || time.tm_mon > 11 ||
        time.tm_mday < 1 || time.tm_mday > 31 || time.tm_hour < 0 || time.tm_hour > 23 ||
        time.tm_min < 0 || time.tm_min > 59 || time.tm_sec < 0 || time.tm_sec > 60 ||
        year < 0 || year > 9999) {
        buffer[0] = '\0';
        return EINVAL;
    }
    std::snprintf(buffer, size, "%.3s %.3s %2d %.2d:%.2d:%.2d %4d\n", days[time.tm_wday], months[time.tm_mon],
        time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec, year);
    return 0;
}

inline int SonyCtimeS(char* buffer, std::size_t size, const std::int64_t* time) {
    if (!buffer || size < 26) return EINVAL;
    if (!time) {
        buffer[0] = '\0';
        return EINVAL;
    }
    GuestTm parts{};
    if (!localtime_s_nid_postfix(time, &parts)) {
        buffer[0] = '\0';
        return EINVAL;
    }
    return SonyFormatAsctime(buffer, size, parts);
}

inline int SonyAsctimeS(char* buffer, std::size_t size, const GuestTm* time) {
    if (!buffer || size < 26) return EINVAL;
    if (!time) {
        buffer[0] = '\0';
        return EINVAL;
    }
    return SonyFormatAsctime(buffer, size, *time);
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

constexpr std::size_t SonyRsizeMax = (std::size_t{1} << 61) - 1;

inline int SonySafeViolation(const char* function, const char* reason, int error) {
    char message[64];
    std::snprintf(message, sizeof(message), "%s: %s", function, reason);
    SonyInvokeConstraintHandler(message, nullptr, error);
    return error;
}

template<typename TChar>
bool SonySafeOverlaps(const TChar* first, std::size_t firstCount, const TChar* second, std::size_t secondCount) {
    const auto left = reinterpret_cast<std::uintptr_t>(first);
    const auto right = reinterpret_cast<std::uintptr_t>(second);
    return left < right + secondCount * sizeof(TChar) && right < left + firstCount * sizeof(TChar);
}

template<typename TChar>
int SonyCopyStringS(const char* function, TChar* dest, std::size_t destsz, const TChar* src, std::size_t count, bool counted) {
    if (!dest || destsz == 0 || destsz > SonyRsizeMax) return SonySafeViolation(function, "bad destination argument", 22);
    if (!src || (counted && count > SonyRsizeMax)) {
        dest[0] = 0;
        return SonySafeViolation(function, "bad argument", 22);
    }
    std::size_t length = 0;
    while (length < count && src[length] != 0) ++length;
    if (length >= destsz) {
        dest[0] = 0;
        return SonySafeViolation(function, "buffer too small", 34);
    }
    if (SonySafeOverlaps(dest, length + 1, src, length < count ? length + 1 : length)) {
        dest[0] = 0;
        return SonySafeViolation(function, "buffer overlap", 22);
    }
    std::memcpy(dest, src, length * sizeof(TChar));
    dest[length] = 0;
    return 0;
}

template<typename TChar>
int SonyAppendStringS(const char* function, TChar* dest, std::size_t destsz, const TChar* src, std::size_t count, bool counted, bool terminators) {
    if (!dest || destsz == 0 || destsz > SonyRsizeMax) return SonySafeViolation(function, "bad destination argument", 22);
    std::size_t used = 0;
    while (used < destsz && dest[used] != 0) ++used;
    if (used == destsz || !src || (counted && count > SonyRsizeMax)) {
        dest[0] = 0;
        return SonySafeViolation(function, "bad argument", 22);
    }
    std::size_t length = 0;
    while (length < count && src[length] != 0) ++length;
    if (length >= destsz - used) {
        dest[0] = 0;
        return SonySafeViolation(function, "buffer too small", 34);
    }
    const std::size_t written = terminators ? length + 1 : length;
    const std::size_t read = terminators && length < count ? length + 1 : length;
    if (SonySafeOverlaps(dest + used, written, src, read)) {
        dest[0] = 0;
        return SonySafeViolation(function, "buffer overlap", 22);
    }
    std::memcpy(dest + used, src, length * sizeof(TChar));
    dest[used + length] = 0;
    return 0;
}

template<typename TChar>
int SonyCopyMemoryS(const char* function, TChar* dest, std::size_t destsz, const TChar* src, std::size_t count, bool overlapping) {
    if (!dest || destsz > SonyRsizeMax) return SonySafeViolation(function, "bad destination argument", 22);
    if (!src || count > SonyRsizeMax) {
        std::memset(dest, 0, destsz * sizeof(TChar));
        return SonySafeViolation(function, "bad argument", 22);
    }
    if (count > destsz) {
        std::memset(dest, 0, destsz * sizeof(TChar));
        return SonySafeViolation(function, "buffer too small", 34);
    }
    if (!overlapping && SonySafeOverlaps(dest, count, src, count)) {
        std::memset(dest, 0, destsz * sizeof(TChar));
        return SonySafeViolation(function, "buffer overlap", 22);
    }
    std::memmove(dest, src, count * sizeof(TChar));
    return 0;
}

inline int SonyWcscpyS(std::uint16_t* dest, std::size_t size, const std::uint16_t* src) {
    return SonyCopyStringS("wcscpy_s", dest, size, src, SIZE_MAX, false);
}

inline std::size_t SonyMbstowcs(std::uint16_t* dest, const char* src, std::size_t size) {
    if (!src) return 0;
    if (!dest) return std::strlen(src);
    std::size_t converted = 0;
    while (converted < size) {
        const auto value = static_cast<unsigned char>(src[converted]);
        dest[converted] = value;
        if (value == 0) return converted;
        ++converted;
    }
    return converted;
}

inline SonyLconv* SonyLocaleconv(void) {
    static char empty[] = "";
    static char point[] = ".";
    static char falseText[] = "false";
    static char trueText[] = "true";
    static SonyLconv value = [] {
        SonyLconv init{};
        for (auto& pointer : init.monetary) pointer = empty;
        for (auto& byte : init.chars) byte = CHAR_MAX;
        init.decimal_point = point;
        init.grouping = empty;
        init.thousands_sep = empty;
        init.frac_grouping = empty;
        init.frac_sep = empty;
        init.false_text = falseText;
        init.true_text = trueText;
        init.no_text = empty;
        init.yes_text = empty;
        return init;
    }();
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

inline unsigned __int128 SonyUdivmod128(unsigned __int128 dividend, unsigned __int128 divisor, unsigned __int128* remainderOut) {
    unsigned __int128 quotient = 0;
    unsigned __int128 remainder = 0;
    for (int i = 127; i >= 0; --i) {
        remainder = (remainder << 1) | ((dividend >> i) & 1);
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (unsigned __int128)1 << i;
        }
    }
    *remainderOut = remainder;
    return quotient;
}

inline unsigned __int128 SonyUdivti3(unsigned __int128 dividend, unsigned __int128 divisor) {
    if (divisor == 0) throw std::runtime_error("__udivti3: integer divide by zero");
    unsigned __int128 remainder = 0;
    return SonyUdivmod128(dividend, divisor, &remainder);
}

inline unsigned __int128 SonyUmodti3(unsigned __int128 dividend, unsigned __int128 divisor) {
    if (divisor == 0) {
        char message[96];
        std::snprintf(message, sizeof(message), "__umodti3: integer divide by zero (0x%016llx%016llx %% 0)",
            (unsigned long long)(dividend >> 64), (unsigned long long)dividend);
        throw std::runtime_error(message);
    }
    unsigned __int128 remainder = 0;
    SonyUdivmod128(dividend, divisor, &remainder);
    return remainder;
}

inline __int128 SonyDivti3(__int128 dividend, __int128 divisor) {
    if (divisor == 0) throw std::runtime_error("__divti3: integer divide by zero");
    const bool negative = (dividend < 0) != (divisor < 0);
    const unsigned __int128 magnitudeDividend = dividend < 0
        ? (unsigned __int128)0 - (unsigned __int128)dividend
        : (unsigned __int128)dividend;
    const unsigned __int128 magnitudeDivisor = divisor < 0
        ? (unsigned __int128)0 - (unsigned __int128)divisor
        : (unsigned __int128)divisor;
    const unsigned __int128 magnitude = SonyUdivti3(magnitudeDividend, magnitudeDivisor);
    return (__int128)(negative ? (unsigned __int128)0 - magnitude : magnitude);
}

inline __int128 SonyModti3(__int128 dividend, __int128 divisor) {
    if (divisor == 0) throw std::runtime_error("__modti3: integer divide by zero");
    const bool negative = dividend < 0;
    const unsigned __int128 magnitudeDividend = negative
        ? (unsigned __int128)0 - (unsigned __int128)dividend
        : (unsigned __int128)dividend;
    const unsigned __int128 magnitudeDivisor = divisor < 0
        ? (unsigned __int128)0 - (unsigned __int128)divisor
        : (unsigned __int128)divisor;
    unsigned __int128 remainder = 0;
    SonyUdivmod128(magnitudeDividend, magnitudeDivisor, &remainder);
    return (__int128)(negative ? (unsigned __int128)0 - remainder : remainder);
}

}

#endif

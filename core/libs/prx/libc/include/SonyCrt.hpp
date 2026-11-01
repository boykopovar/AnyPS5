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
#include <string>

#include "SceTypes.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" GuestTm* APS5_VABI localtime_s_nid_postfix(const std::int64_t* timer, GuestTm* result);

using SonyConstraintHandler = void (APS5_VABI *)(const char* message, void* pointer, int error);

inline void APS5_VABI SonyDefaultConstraintHandler(const char* message, void* pointer, int error) {
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

namespace {

struct SonyHostTimeZone {
    std::int64_t standardEast;
    std::int64_t daylight;
};

#ifdef _WIN32
inline void SonyRefreshHostZone_nid_no_patch() {
    _tzset();
    const char* zone = std::getenv("TZ");
    if (zone != nullptr && zone[0] != 0) {
        int daylight = 0;
        _get_daylight(&daylight);
        if (daylight != 0) _dstbias = -3600;
    }
}
#endif

inline SonyHostTimeZone SonyHostZoneAt_nid_no_patch(std::int64_t utcTime) {
    std::tm local{};
#ifdef _WIN32
    constexpr std::int64_t cycleSeconds = 146097LL * 86400LL;
    constexpr std::int64_t lastHostSecond = 32535215999LL;
    const std::int64_t cycles = utcTime >= 0 && utcTime <= lastHostSecond ? 0 : (utcTime >= 0 ? utcTime / cycleSeconds : -((-(utcTime + 1)) / cycleSeconds + 1));
    const std::time_t t = static_cast<std::time_t>(utcTime - cycles * cycleSeconds);
    SonyRefreshHostZone_nid_no_patch();
    if (localtime_s(&local, &t) != 0) throw std::runtime_error(std::string(__func__) + ": no host local time for " + std::to_string(utcTime));
    long standardBias = 0;
    long daylightBias = 0;
    _get_timezone(&standardBias);
    _get_dstbias(&daylightBias);
    return {-static_cast<std::int64_t>(standardBias), local.tm_isdst > 0 ? -static_cast<std::int64_t>(daylightBias) : 0};
#else
    const std::time_t t = static_cast<std::time_t>(utcTime);
    ::tzset();
    if (localtime_r(&t, &local) == nullptr) throw std::runtime_error(std::string(__func__) + ": no host local time for " + std::to_string(utcTime));
    const std::int64_t daylight = local.tm_isdst > 0 ? static_cast<std::int64_t>(local.tm_gmtoff) + ::timezone : 0;
    return {static_cast<std::int64_t>(local.tm_gmtoff) - daylight, daylight};
#endif
}

inline std::int64_t SonyHostDaylightAmount_nid_no_patch(std::int64_t utcTime) {
    constexpr std::int64_t monthSeconds = 2629746;
    for (int month = 0; month <= 11; ++month) {
        for (const int direction : {1, -1}) {
            const SonyHostTimeZone zone = SonyHostZoneAt_nid_no_patch(utcTime + direction * month * monthSeconds);
            if (zone.daylight != 0) return zone.daylight;
        }
    }
    return 0;
}

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
    const std::int64_t year = std::int64_t{time.tm_year} + 1900;
    if (time.tm_wday < 0 || time.tm_wday > 6 || time.tm_mon < 0 || time.tm_mon > 11 ||
        time.tm_mday < 1 || time.tm_mday > 31 || time.tm_hour < 0 || time.tm_hour > 23 ||
        time.tm_min < 0 || time.tm_min > 59 || time.tm_sec < 0 || time.tm_sec > 60 ||
        year < 0 || year > 9999) {
        buffer[0] = '\0';
        return EINVAL;
    }
    std::snprintf(buffer, size, "%.3s %.3s %2d %.2d:%.2d:%.2d %4d\n", days[time.tm_wday], months[time.tm_mon],
        time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec, static_cast<int>(year));
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

inline int SonyWcscpyS(std::uint16_t* dest, std::size_t size, const std::uint16_t* src) {
    constexpr int GuestEinval = 22;
    constexpr int GuestErange = 34;
    constexpr std::size_t rsizeMax = (SIZE_MAX >> 1) / sizeof(std::uint16_t);
    if (!dest || size == 0) {
        SonyInvokeConstraintHandler("wcscpy_s", dest, GuestEinval);
        return GuestEinval;
    }
    if (size > rsizeMax) {
        SonyInvokeConstraintHandler("wcscpy_s", dest, GuestErange);
        return GuestErange;
    }
    if (!src) {
        dest[0] = 0;
        SonyInvokeConstraintHandler("wcscpy_s", dest, GuestEinval);
        return GuestEinval;
    }
    std::size_t length = 0;
    while (length < size && src[length] != 0) ++length;
    if (length >= size) {
        dest[0] = 0;
        SonyInvokeConstraintHandler("wcscpy_s", dest, GuestErange);
        return GuestErange;
    }
    const auto destination = reinterpret_cast<std::uintptr_t>(dest);
    const auto source = reinterpret_cast<std::uintptr_t>(src);
    const auto distance = destination < source ? source - destination : destination - source;
    if (length != 0 && distance < (length + 1) * sizeof(std::uint16_t)) {
        dest[0] = 0;
        SonyInvokeConstraintHandler("wcscpy_s", dest, GuestEinval);
        return GuestEinval;
    }
    std::memcpy(dest, src, (length + 1) * sizeof(std::uint16_t));
    return 0;
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

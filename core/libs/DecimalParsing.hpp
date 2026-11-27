#ifndef CORE_LIBS_DECIMALPARSING_HPP
#define CORE_LIBS_DECIMALPARSING_HPP

#include <charconv>
#if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 260000
#include <cctype>
#include <cerrno>
#include <cfenv>
#include <cmath>
#include <string>
#include <string_view>
#include <strings.h>
#include <type_traits>
#include <xlocale.h>
#endif

template<typename TFloat>
std::from_chars_result ParseDecimal_nid_no_patch(const char* first, const char* last, TFloat& value) {
#if defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 260000
    static_assert(std::is_same_v<TFloat, float> || std::is_same_v<TFloat, double>);
    const char* cursor = first;
    bool hasDigit = false;
    bool hasNonzeroDigit = false;
    const auto isDigit = [&] { return cursor != last && *cursor >= '0' && *cursor <= '9'; };
    const auto takeSignificand = [&] {
        for (; isDigit(); ++cursor) {
            hasDigit = true;
            hasNonzeroDigit = hasNonzeroDigit || *cursor != '0';
        }
    };
    const auto takeWord = [&](std::string_view word) {
        if (static_cast<std::size_t>(last - cursor) < word.size() || strncasecmp(cursor, word.data(), word.size()) != 0) return false;
        cursor += word.size();
        return true;
    };
    if (cursor != last && *cursor == '-') ++cursor;
    const bool isInfinity = takeWord("infinity") || takeWord("inf");
    const bool isNan = !isInfinity && takeWord("nan");
    const char* end = cursor;
    if (isNan && cursor != last && *cursor == '(') {
        do ++cursor;
        while (cursor != last && (std::isalnum(static_cast<unsigned char>(*cursor)) || *cursor == '_'));
        if (cursor != last && *cursor == ')') end = cursor + 1;
    }
    if (!isInfinity && !isNan) {
        takeSignificand();
        if (cursor != last && *cursor == '.') {
            ++cursor;
            takeSignificand();
        }
        if (!hasDigit) return {first, std::errc::invalid_argument};
        end = cursor;
        if (cursor != last && (*cursor == 'e' || *cursor == 'E')) {
            ++cursor;
            if (cursor != last && (*cursor == '+' || *cursor == '-')) ++cursor;
            if (isDigit()) {
                while (isDigit()) ++cursor;
                end = cursor;
            }
        }
    }
    const std::string text(first, end);
    const int savedErrno = errno;
    std::fenv_t environment;
    std::fegetenv(&environment);
    std::fesetenv(FE_DFL_ENV);
    TFloat parsed;
    if constexpr (std::is_same_v<TFloat, float>) parsed = strtof_l(text.c_str(), nullptr, LC_C_LOCALE);
    else parsed = strtod_l(text.c_str(), nullptr, LC_C_LOCALE);
    const int category = std::fpclassify(parsed);
    std::fesetenv(&environment);
    errno = savedErrno;
    if ((category == FP_INFINITE && !isInfinity) || (category == FP_ZERO && hasNonzeroDigit)) return {end, std::errc::result_out_of_range};
    value = parsed;
    return {end, std::errc{}};
#else
    return std::from_chars(first, last, value);
#endif
}

#endif

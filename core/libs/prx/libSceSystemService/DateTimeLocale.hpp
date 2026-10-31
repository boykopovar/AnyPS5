#ifndef CORE_LIBS_PRX_LIBSCESYSTEMSERVICE_DATETIMELOCALE_HPP
#define CORE_LIBS_PRX_LIBSCESYSTEMSERVICE_DATETIMELOCALE_HPP

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#else
#include <langinfo.h>
#include <locale.h>
#endif

namespace SystemServiceDateTime {

constexpr int DATE_FORMAT_YYYYMMDD = 0;
constexpr int DATE_FORMAT_DDMMYYYY = 1;
constexpr int DATE_FORMAT_MMDDYYYY = 2;
constexpr int TIME_FORMAT_12HOUR = 0;
constexpr int TIME_FORMAT_24HOUR = 1;

namespace Detail {

constexpr char DAY = 'D';
constexpr char MONTH = 'M';
constexpr char YEAR = 'Y';

inline std::optional<int> DateFormatOfOrder(std::string_view order) {
    if (order == "YMD") return DATE_FORMAT_YYYYMMDD;
    if (order == "DMY") return DATE_FORMAT_DDMMYYYY;
    if (order == "MDY") return DATE_FORMAT_MMDDYYYY;
    return std::nullopt;
}

inline bool AppendField(std::string& order, char field) {
    if (order.find(field) != std::string::npos) return false;
    order.push_back(field);
    return true;
}

template <typename Visitor>
void VisitPatternLetters(std::string_view pattern, Visitor&& visit) {
    bool quoted = false;
    for (std::size_t index = 0; index < pattern.size();) {
        const char letter = pattern[index];
        if (letter == '\'') {
            quoted = !quoted;
            ++index;
            continue;
        }
        std::size_t run = 1;
        while (index + run < pattern.size() && pattern[index + run] == letter) ++run;
        index += run;
        if (!quoted) visit(letter, run);
    }
}

}

inline std::optional<int> DateFormatOfStrftime(std::string_view pattern) {
    std::string order;
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        if (pattern[index] != '%') continue;
        ++index;
        while (index < pattern.size() && (pattern[index] == '-' || pattern[index] == '_' || pattern[index] == '0' || pattern[index] == '^' || pattern[index] == '#')) ++index;
        while (index < pattern.size() && pattern[index] >= '0' && pattern[index] <= '9') ++index;
        while (index < pattern.size() && (pattern[index] == 'E' || pattern[index] == 'O')) ++index;
        if (index >= pattern.size()) return std::nullopt;
        bool unique = true;
        switch (pattern[index]) {
            case 'd': case 'e': unique = Detail::AppendField(order, Detail::DAY); break;
            case 'm': case 'b': case 'B': case 'h': unique = Detail::AppendField(order, Detail::MONTH); break;
            case 'y': case 'Y': unique = Detail::AppendField(order, Detail::YEAR); break;
            case 'D': unique = Detail::AppendField(order, Detail::MONTH) && Detail::AppendField(order, Detail::DAY) && Detail::AppendField(order, Detail::YEAR); break;
            case 'F': unique = Detail::AppendField(order, Detail::YEAR) && Detail::AppendField(order, Detail::MONTH) && Detail::AppendField(order, Detail::DAY); break;
            case 'c': case 'x': case 'X': return std::nullopt;
            default: break;
        }
        if (!unique) return std::nullopt;
    }
    return Detail::DateFormatOfOrder(order);
}

inline std::optional<int> TimeFormatOfStrftime(std::string_view pattern) {
    bool twelve = false;
    bool twentyFour = false;
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        if (pattern[index] != '%') continue;
        ++index;
        while (index < pattern.size() && (pattern[index] == '-' || pattern[index] == '_' || pattern[index] == '0' || pattern[index] == '^' || pattern[index] == '#')) ++index;
        while (index < pattern.size() && pattern[index] >= '0' && pattern[index] <= '9') ++index;
        while (index < pattern.size() && (pattern[index] == 'E' || pattern[index] == 'O')) ++index;
        if (index >= pattern.size()) return std::nullopt;
        switch (pattern[index]) {
            case 'I': case 'l': case 'p': case 'P': case 'r': twelve = true; break;
            case 'H': case 'k': case 'T': case 'R': twentyFour = true; break;
            case 'c': case 'x': case 'X': return std::nullopt;
            default: break;
        }
    }
    if (twelve == twentyFour) return std::nullopt;
    return twelve ? TIME_FORMAT_12HOUR : TIME_FORMAT_24HOUR;
}

inline std::optional<int> DateFormatOfLdml(std::string_view pattern) {
    std::string order;
    bool unique = true;
    Detail::VisitPatternLetters(pattern, [&](char letter, std::size_t run) {
        if (letter == 'd' && run <= 2) unique = Detail::AppendField(order, Detail::DAY) && unique;
        else if (letter == 'M' || letter == 'L') unique = Detail::AppendField(order, Detail::MONTH) && unique;
        else if (letter == 'y') unique = Detail::AppendField(order, Detail::YEAR) && unique;
    });
    if (!unique) return std::nullopt;
    return Detail::DateFormatOfOrder(order);
}

inline std::optional<int> TimeFormatOfLdml(std::string_view pattern) {
    bool twelve = false;
    bool twentyFour = false;
    Detail::VisitPatternLetters(pattern, [&](char letter, std::size_t) {
        if (letter == 'h' || letter == 'K' || letter == 'a' || letter == 't') twelve = true;
        else if (letter == 'H' || letter == 'k') twentyFour = true;
    });
    if (twelve == twentyFour) return std::nullopt;
    return twelve ? TIME_FORMAT_12HOUR : TIME_FORMAT_24HOUR;
}

enum class PatternSyntax { Strftime, Ldml };

struct HostPatterns {
    bool available = false;
    PatternSyntax syntax = PatternSyntax::Strftime;
    std::string date;
    std::string time;
    std::string source;
};

using HostPatternSource = std::function<HostPatterns()>;
using Notice = std::function<void(const std::string&)>;

inline int ResolveHostDateFormat(const HostPatterns& host, const Notice& notice) {
    const std::string prefix = "ANYPS5_DATE_FORMAT=auto: ";
    if (!host.available) {
        notice(prefix + host.source + "; using day, month, year (1)");
        return DATE_FORMAT_DDMMYYYY;
    }
    const auto format = host.syntax == PatternSyntax::Strftime ? DateFormatOfStrftime(host.date) : DateFormatOfLdml(host.date);
    if (format) return *format;
    notice(prefix + "the date pattern \"" + host.date + "\" of " + host.source + " has no console date format; using day, month, year (1)");
    return DATE_FORMAT_DDMMYYYY;
}

inline int ResolveHostTimeFormat(const HostPatterns& host, const Notice& notice) {
    const std::string prefix = "ANYPS5_TIME_FORMAT=auto: ";
    if (!host.available) {
        notice(prefix + host.source + "; using 24-hour (1)");
        return TIME_FORMAT_24HOUR;
    }
    const auto format = host.syntax == PatternSyntax::Strftime ? TimeFormatOfStrftime(host.time) : TimeFormatOfLdml(host.time);
    if (format) return *format;
    notice(prefix + "the time pattern \"" + host.time + "\" of " + host.source + " has no console time format; using 24-hour (1)");
    return TIME_FORMAT_24HOUR;
}

struct LangInfo {
    bool available = false;
    std::string dateFormat;
    std::string timeFormat;
};

using Environment = std::function<const char*(const char*)>;
using LangInfoReader = std::function<LangInfo(const std::string&)>;

namespace Detail {

inline bool StartsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

inline bool IsCLocaleName(std::string_view name) {
    for (const std::string_view c : {std::string_view("C"), std::string_view("POSIX")}) {
        if (name == c) return true;
        if (StartsWith(name, c) && name.size() > c.size() && (name[c.size()] == '.' || name[c.size()] == '@')) return true;
    }
    return false;
}

}

inline HostPatterns PosixHostPatterns(const Environment& environment, const LangInfoReader& read) {
    HostPatterns host;
    host.syntax = PatternSyntax::Strftime;
    for (const char* variable : {"LC_ALL", "LC_TIME", "LANG"}) {
        const char* value = environment(variable);
        if (value == nullptr || value[0] == '\0') continue;
        const std::string name = value;
        if (Detail::IsCLocaleName(name)) {
            host.source = std::string(variable) + " is \"" + name + "\", the C locale";
            return host;
        }
        const LangInfo info = read(name);
        if (!info.available) {
            host.source = "the locale \"" + name + "\" from " + variable + " is not installed";
            return host;
        }
        host.available = true;
        host.date = info.dateFormat;
        host.time = info.timeFormat;
        host.source = "locale \"" + name + "\" (" + variable + ")";
        return host;
    }
    host.source = "none of LC_ALL, LC_TIME and LANG is set";
    return host;
}

#if !defined(_WIN32) && !defined(__APPLE__)
inline LangInfo ReadLangInfo(const std::string& name) {
    locale_t locale = newlocale(LC_TIME_MASK, name.c_str(), static_cast<locale_t>(nullptr));
    if (locale == static_cast<locale_t>(nullptr)) return {};
    LangInfo info{true, nl_langinfo_l(D_FMT, locale), nl_langinfo_l(T_FMT, locale)};
    freelocale(locale);
    return info;
}
#endif

#if defined(_WIN32)
inline std::string WindowsLocaleText(LCTYPE type) {
    wchar_t wide[160];
    const int wideLength = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, type, wide, static_cast<int>(sizeof(wide) / sizeof(wide[0])));
    if (wideLength <= 1) return {};
    char narrow[480];
    const int narrowLength = WideCharToMultiByte(CP_UTF8, 0, wide, wideLength - 1, narrow, static_cast<int>(sizeof(narrow)), nullptr, nullptr);
    if (narrowLength <= 0) return {};
    return std::string(narrow, static_cast<std::size_t>(narrowLength));
}

inline HostPatterns WindowsHostPatterns() {
    HostPatterns host;
    host.syntax = PatternSyntax::Ldml;
    host.date = WindowsLocaleText(LOCALE_SSHORTDATE);
    host.time = WindowsLocaleText(LOCALE_STIMEFORMAT);
    host.available = !host.date.empty() && !host.time.empty();
    host.source = host.available ? "the Windows user locale" : "GetLocaleInfoEx gave no short date or time format for the Windows user locale";
    return host;
}
#elif defined(__APPLE__)
inline std::string MacTemplatePattern(const char* text, CFLocaleRef locale) {
    CFStringRef templateText = CFStringCreateWithCString(kCFAllocatorDefault, text, kCFStringEncodingUTF8);
    if (templateText == nullptr) return {};
    CFStringRef pattern = CFDateFormatterCreateDateFormatFromTemplate(kCFAllocatorDefault, templateText, 0, locale);
    CFRelease(templateText);
    if (pattern == nullptr) return {};
    char buffer[256];
    const bool copied = CFStringGetCString(pattern, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(pattern);
    return copied ? std::string(buffer) : std::string();
}

inline HostPatterns MacHostPatterns() {
    HostPatterns host;
    host.syntax = PatternSyntax::Ldml;
    CFLocaleRef locale = CFLocaleCopyCurrent();
    if (locale != nullptr) {
        host.date = MacTemplatePattern("yMd", locale);
        host.time = MacTemplatePattern("jms", locale);
        CFRelease(locale);
    }
    host.available = !host.date.empty() && !host.time.empty();
    host.source = host.available ? "the macOS current locale" : "CFDateFormatter gave no date or time pattern for the macOS current locale";
    return host;
}
#endif

inline HostPatterns HostPatternsOfThisHost() {
#if defined(_WIN32)
    return WindowsHostPatterns();
#elif defined(__APPLE__)
    return MacHostPatterns();
#else
    return PosixHostPatterns([](const char* name) { return std::getenv(name); }, ReadLangInfo);
#endif
}

inline bool EqualsAuto(std::string_view text) {
    constexpr std::string_view automatic = "auto";
    if (text.size() != automatic.size()) return false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char letter = text[index] >= 'A' && text[index] <= 'Z' ? static_cast<char>(text[index] - 'A' + 'a') : text[index];
        if (letter != automatic[index]) return false;
    }
    return true;
}

inline int ParseSetting(const char* configured, std::string_view variable, std::string_view description, int last, int fallback, const std::function<int()>& host, std::string_view caller) {
    if (configured == nullptr || configured[0] == '\0') return fallback;
    const std::string_view text(configured);
    if (EqualsAuto(text)) return host();
    int value = -1;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < 0 || value > last) {
        throw std::runtime_error(std::string(caller) + ": " + std::string(variable) + " must be a console " + std::string(description) + " number from 0 to " +
            std::to_string(last) + " or auto, not \"" + std::string(text) + "\"");
    }
    return value;
}

inline const HostPatterns& HostPatternsOnce() {
    static const HostPatterns patterns = HostPatternsOfThisHost();
    return patterns;
}

inline void PrintNotice(const std::string& line) {
    std::fprintf(stderr, "%s\n", line.c_str());
}

inline int ConsoleDateFormat(std::string_view caller) {
    return ParseSetting(std::getenv("ANYPS5_DATE_FORMAT"), "ANYPS5_DATE_FORMAT", "date format", DATE_FORMAT_MMDDYYYY, DATE_FORMAT_DDMMYYYY, [] {
        static const int format = ResolveHostDateFormat(HostPatternsOnce(), PrintNotice);
        return format;
    }, caller);
}

inline int ConsoleTimeFormat(std::string_view caller) {
    return ParseSetting(std::getenv("ANYPS5_TIME_FORMAT"), "ANYPS5_TIME_FORMAT", "time format", TIME_FORMAT_24HOUR, TIME_FORMAT_24HOUR, [] {
        static const int format = ResolveHostTimeFormat(HostPatternsOnce(), PrintNotice);
        return format;
    }, caller);
}

}

#endif

#ifndef CORE_LIBS_PRX_LIBSCESYSTEMSERVICE_DATETIMELOCALE_HPP
#define CORE_LIBS_PRX_LIBSCESYSTEMSERVICE_DATETIMELOCALE_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
            if (index + 1 < pattern.size() && pattern[index + 1] == '\'') {
                index += 2;
                continue;
            }
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

}

#endif

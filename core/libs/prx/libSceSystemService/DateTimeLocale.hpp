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

}

#endif

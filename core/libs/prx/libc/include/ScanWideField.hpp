#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SCANWIDEFIELD_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SCANWIDEFIELD_HPP

#include <string>

namespace LibcDetail {

bool InWideScanset(const std::string& specifier, unsigned char byte);

inline std::string TitleScanset(const char* begin, const char* end, bool negated) {
    bool members[256]{};
    for (const char* member = begin; member != end;) {
        const auto first = static_cast<unsigned char>(member[0]);
        if (end - member > 2 && member[1] == '-') {
            const auto last = static_cast<unsigned char>(member[2]);
            for (unsigned value = first; value <= last; ++value) members[value] = true;
            member += 3;
        } else {
            members[first] = true;
            ++member;
        }
    }
    bool empty = true;
    for (unsigned value = 1; value < 256; ++value) {
        if (negated) members[value] = !members[value];
        if (members[value] && value != '^' && value != '-') empty = false;
    }
    const bool complement = empty;
    std::string text = complement ? "[^" : "[";
    const auto listed = [&](unsigned value) { return members[value] != complement; };
    if (listed(']')) text += ']';
    for (unsigned value = 1; value < 256; ++value) {
        if (listed(value) && value != ']' && value != '-' && value != '^') text += static_cast<char>(value);
    }
    if (listed('^')) text += '^';
    if (listed('-')) text += '-';
    return text + ']';
}

}

#endif

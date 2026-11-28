#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SCANDANGLINGEXPONENT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SCANDANGLINGEXPONENT_HPP

#include <cctype>
#include <climits>
#include <cstddef>

namespace LibcDetail {

inline std::size_t DanglingExponentMantissa(const char* field, unsigned long width, std::size_t& span) {
    const unsigned long limit = width == 0 ? ULONG_MAX : width;
    const auto at = [&](unsigned long index) { return index < limit ? static_cast<unsigned char>(field[index]) : 0; };
    unsigned long index = at(0) == '+' || at(0) == '-' ? 1 : 0;
    const bool hex = at(index) == '0' && (at(index + 1) == 'x' || at(index + 1) == 'X');
    if (hex) index += 2;
    const auto digit = [&](unsigned long position) { return (hex ? std::isxdigit(at(position)) : std::isdigit(at(position))) != 0; };
    bool digits = false;
    for (; digit(index); ++index) digits = true;
    if (at(index) == '.') for (++index; digit(index); ++index) digits = true;
    const std::size_t mantissa = index;
    const unsigned char marker = at(index);
    if (!digits || (hex ? marker != 'p' && marker != 'P' : marker != 'e' && marker != 'E')) return 0;
    ++index;
    if (at(index) == '+' || at(index) == '-') ++index;
    if (std::isdigit(at(index))) return 0;
    span = index;
    return mantissa;
}

}

#endif

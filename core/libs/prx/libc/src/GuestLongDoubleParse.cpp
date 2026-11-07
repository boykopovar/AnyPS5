#include "prx/libc/include/GuestLongDouble.hpp"

#ifdef ANYPS5_GUEST_LIFTED

#include <algorithm>
#include <bit>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <vector>

namespace {

using Natural = std::vector<std::uint32_t>;
using LibcDetail::GuestLongDouble;

constexpr long long ExponentLimit = 1000000000;
constexpr std::uint64_t Integer = 0x8000000000000000ull;

void Trim(Natural& value) {
    while (!value.empty() && value.back() == 0) value.pop_back();
}

void MultiplyAdd(Natural& value, const std::uint32_t factor, const std::uint32_t addend) {
    std::uint64_t carry = addend;
    for (auto& limb : value) {
        const std::uint64_t product = static_cast<std::uint64_t>(limb) * factor + carry;
        limb = static_cast<std::uint32_t>(product);
        carry = product >> 32;
    }
    if (carry != 0) value.push_back(static_cast<std::uint32_t>(carry));
}

void MultiplyByPowerOfFive(Natural& value, long long exponent) {
    for (; exponent >= 13; exponent -= 13) MultiplyAdd(value, 1220703125u, 0);
    std::uint32_t rest = 1;
    for (; exponent > 0; --exponent) rest *= 5;
    MultiplyAdd(value, rest, 0);
}

std::size_t BitLength(const Natural& value) {
    return value.empty() ? 0 : 32 * (value.size() - 1) + static_cast<std::size_t>(32 - std::countl_zero(value.back()));
}

bool Bit(const Natural& value, const std::size_t index) {
    const std::size_t limb = index / 32;
    return limb < value.size() && ((value[limb] >> (index % 32)) & 1u) != 0;
}

bool AnyBelow(const Natural& value, const std::size_t count) {
    const std::size_t whole = std::min(count / 32, value.size());
    for (std::size_t index = 0; index < whole; ++index) {
        if (value[index] != 0) return true;
    }
    return whole < value.size() && count % 32 != 0 && (value[whole] & ((1u << (count % 32)) - 1)) != 0;
}

std::uint64_t BitsFrom(const Natural& value, const std::size_t from, const long long count) {
    std::uint64_t bits = 0;
    for (long long index = 0; index < count; ++index) {
        if (Bit(value, from + static_cast<std::size_t>(index))) bits |= std::uint64_t {1} << index;
    }
    return bits;
}

Natural ShiftedLeft(const Natural& value, const std::size_t bits) {
    Natural result(bits / 32, 0);
    const unsigned shift = bits % 32;
    std::uint32_t carry = 0;
    for (const auto limb : value) {
        result.push_back(shift == 0 ? limb : (limb << shift) | carry);
        carry = shift == 0 ? 0 : limb >> (32 - shift);
    }
    if (carry != 0) result.push_back(carry);
    Trim(result);
    return result;
}

int Compare(const Natural& left, const Natural& right) {
    if (left.size() != right.size()) return left.size() < right.size() ? -1 : 1;
    for (std::size_t index = left.size(); index-- > 0;) {
        if (left[index] != right[index]) return left[index] < right[index] ? -1 : 1;
    }
    return 0;
}

void Subtract(Natural& left, const Natural& right) {
    std::int64_t borrow = 0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        std::int64_t difference = static_cast<std::int64_t>(left[index]) - borrow - (index < right.size() ? right[index] : 0);
        borrow = difference < 0 ? 1 : 0;
        if (difference < 0) difference += std::int64_t {1} << 32;
        left[index] = static_cast<std::uint32_t>(difference);
    }
    Trim(left);
}

struct Quotient {
    Natural value;
    bool inexact;
};

Quotient Divide(const Natural& dividend, const Natural& divisor) {
    Natural remainder = dividend;
    Natural quotient;
    const std::size_t dividendBits = BitLength(dividend);
    const std::size_t divisorBits = BitLength(divisor);
    if (dividendBits >= divisorBits) {
        const std::size_t bits = dividendBits - divisorBits + 1;
        quotient.assign((bits + 31) / 32, 0);
        for (std::size_t shift = bits; shift-- > 0;) {
            const Natural scaled = ShiftedLeft(divisor, shift);
            if (Compare(remainder, scaled) >= 0) {
                Subtract(remainder, scaled);
                quotient[shift / 32] |= 1u << (shift % 32);
            }
        }
        Trim(quotient);
    }
    return {quotient, !remainder.empty()};
}

GuestLongDouble Overflow(const bool negative, const unsigned rounding) {
    errno = ERANGE;
    const auto sign = static_cast<std::uint16_t>(negative ? 0x8000 : 0);
    const bool infinite = rounding == 0 || (rounding == 1 && negative) || (rounding == 2 && !negative);
    return infinite ? GuestLongDouble::FromBits(sign | 0x7fff, Integer) : GuestLongDouble::FromBits(sign | 0x7ffe, ~std::uint64_t {0});
}

GuestLongDouble Round(const bool negative, const Natural& value, const long long scale, const bool sticky) {
    const auto rounding = static_cast<unsigned>((GuestCpu::X87Control() >> 10) & 3);
    const auto sign = static_cast<std::uint16_t>(negative ? 0x8000 : 0);
    const auto length = static_cast<long long>(BitLength(value));
    if (length == 0) return GuestLongDouble::FromBits(sign, 0);
    const long long top = length - 1 + scale;
    const long long precision = top < -16382 ? 64 - (-16382 - top) : 64;
    const long long shift = length - precision;
    std::uint64_t mantissa = 0;
    bool half = false;
    bool rest = sticky;
    if (shift <= 0) {
        mantissa = BitsFrom(value, 0, length) << -shift;
    } else {
        mantissa = precision > 0 ? BitsFrom(value, static_cast<std::size_t>(shift), precision) : 0;
        half = Bit(value, static_cast<std::size_t>(shift - 1));
        rest = rest || AnyBelow(value, static_cast<std::size_t>(shift - 1));
    }
    bool up = false;
    switch (rounding) {
    case 0: up = half && (rest || (mantissa & 1) != 0); break;
    case 1: up = negative && (half || rest); break;
    case 2: up = !negative && (half || rest); break;
    default: break;
    }
    if (precision < 64) {
        if (up) ++mantissa;
        if ((mantissa >> 63) != 0) return GuestLongDouble::FromBits(sign | 1, mantissa);
        if (half || rest) errno = ERANGE;
        return GuestLongDouble::FromBits(sign, mantissa);
    }
    long long biased = top + 16383;
    if (up && ++mantissa == 0) {
        mantissa = Integer;
        ++biased;
    }
    if (biased >= 0x7fff) return Overflow(negative, rounding);
    return GuestLongDouble::FromBits(static_cast<std::uint16_t>(sign | biased), mantissa);
}

int DigitValue(const char character, const int radix) {
    int value = 36;
    if (character >= '0' && character <= '9') value = character - '0';
    else if (character >= 'a' && character <= 'z') value = character - 'a' + 10;
    else if (character >= 'A' && character <= 'Z') value = character - 'A' + 10;
    return value < radix ? value : -1;
}

bool StartsWithWord(const char* text, const char* word) {
    for (; *word != '\0'; ++text, ++word) {
        if (std::tolower(static_cast<unsigned char>(*text)) != *word) return false;
    }
    return true;
}

}

LibcDetail::GuestLongDouble LibcDetail::GuestLongDoubleParse(const char* text, char** end) {
    const char* cursor = text;
    while (std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
    const bool negative = *cursor == '-';
    if (*cursor == '+' || *cursor == '-') ++cursor;
    const auto sign = static_cast<std::uint16_t>(negative ? 0x8000 : 0);
    const auto finish = [end](const char* stop, const GuestLongDouble value) {
        if (end != nullptr) *end = const_cast<char*>(stop);
        return value;
    };
    if (StartsWithWord(cursor, "inf")) return finish(cursor + (StartsWithWord(cursor, "infinity") ? 8 : 3), GuestLongDouble::FromBits(sign | 0x7fff, Integer));
    if (StartsWithWord(cursor, "nan")) {
        const char* stop = cursor + 3;
        if (*stop == '(') {
            const char* close = stop + 1;
            while (std::isalnum(static_cast<unsigned char>(*close)) || *close == '_') ++close;
            if (*close == ')') stop = close + 1;
        }
        return finish(stop, GuestLongDouble::FromBits(sign | 0x7fff, 0xc000000000000000ull));
    }
    const bool hexadecimal = cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X') &&
        (DigitValue(cursor[2], 16) >= 0 || (cursor[2] == '.' && DigitValue(cursor[3], 16) >= 0));
    const int radix = hexadecimal ? 16 : 10;
    Natural value;
    long long significant = 0;
    long long fraction = 0;
    bool any = false;
    bool point = false;
    const char* scan = hexadecimal ? cursor + 2 : cursor;
    for (;; ++scan) {
        if (*scan == '.' && !point) {
            point = true;
            continue;
        }
        const int digit = DigitValue(*scan, radix);
        if (digit < 0) break;
        any = true;
        if (point && fraction < ExponentLimit) ++fraction;
        if (value.empty() && digit == 0) continue;
        MultiplyAdd(value, static_cast<std::uint32_t>(radix), static_cast<std::uint32_t>(digit));
        ++significant;
    }
    if (!any) return finish(text, GuestLongDouble {});
    long long exponent = 0;
    if (std::tolower(static_cast<unsigned char>(*scan)) == (hexadecimal ? 'p' : 'e')) {
        const char* exponentText = scan + 1;
        const bool minus = *exponentText == '-';
        if (*exponentText == '+' || *exponentText == '-') ++exponentText;
        if (std::isdigit(static_cast<unsigned char>(*exponentText))) {
            for (; std::isdigit(static_cast<unsigned char>(*exponentText)); ++exponentText) exponent = std::min(exponent * 10 + (*exponentText - '0'), ExponentLimit);
            if (minus) exponent = -exponent;
            scan = exponentText;
        }
    }
    if (value.empty()) return finish(scan, GuestLongDouble::FromBits(sign, 0));
    if (hexadecimal) {
        const long long binary = exponent - 4 * fraction;
        const long long top = static_cast<long long>(BitLength(value)) - 1 + binary;
        if (top > 16384) return finish(scan, Overflow(negative, static_cast<unsigned>((GuestCpu::X87Control() >> 10) & 3)));
        if (top < -16500) return finish(scan, Round(negative, Natural {1}, -20000, true));
        return finish(scan, Round(negative, value, binary, false));
    }
    const long long scale = exponent - fraction;
    const long long magnitude = significant - 1 + scale;
    if (magnitude > 4933) return finish(scan, Overflow(negative, static_cast<unsigned>((GuestCpu::X87Control() >> 10) & 3)));
    if (magnitude < -4952) return finish(scan, Round(negative, Natural {1}, -20000, true));
    if (scale >= 0) {
        MultiplyByPowerOfFive(value, scale);
        return finish(scan, Round(negative, value, scale, false));
    }
    Natural power {1};
    MultiplyByPowerOfFive(power, -scale);
    const long long widen = std::max(0ll, static_cast<long long>(BitLength(power)) - static_cast<long long>(BitLength(value)) + 66);
    const Quotient quotient = Divide(ShiftedLeft(value, static_cast<std::size_t>(widen)), power);
    return finish(scan, Round(negative, quotient.value, scale - widen, quotient.inexact));
}

#endif

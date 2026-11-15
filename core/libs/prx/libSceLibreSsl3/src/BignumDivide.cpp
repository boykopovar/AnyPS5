#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

#include <bit>
#include <climits>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr int BignumLibrary = 3;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int DivisionByZero = 103;
constexpr int ExpandOnStaticData = 105;
constexpr int NotInitialized = 107;
constexpr int BignumTooLong = 114;
constexpr int FlagStaticData = 0x02;
constexpr int FlagConstantTime = 0x04;
constexpr std::size_t MaxWords = INT_MAX / (4 * 64);

using Words = std::vector<std::uint64_t>;

int Fail(int reason) {
    LibreSslPutError_nid_no_patch(BignumLibrary, AnyFunction, reason);
    return 0;
}

Words Magnitude(const Bignum* value) {
    if (value->top < 0 || (value->top > 0 && !value->d))
        throw std::invalid_argument("BN_div: BIGNUM with a negative word count or no storage");
    return Words(value->d, value->d + value->top);
}

std::size_t BitLength(const Words& words) {
    return words.empty() ? 0 : (words.size() - 1) * 64 + std::bit_width(words.back());
}

void Trim(Words& words) {
    while (!words.empty() && words.back() == 0) words.pop_back();
}

bool AtLeast(const Words& value, const Words& divisor) {
    for (std::size_t i = value.size(); i-- > 0;) {
        const std::uint64_t other = i < divisor.size() ? divisor[i] : 0;
        if (value[i] != other) return value[i] > other;
    }
    return true;
}

void Divide(const Words& numerator, const Words& divisor, Words& quotient, Words& remainder) {
    quotient.assign(numerator.size(), 0);
    remainder.assign(divisor.size() + 1, 0);
    for (std::size_t bit = BitLength(numerator); bit-- > 0;) {
        for (std::size_t i = remainder.size(); i-- > 1;) remainder[i] = remainder[i] << 1 | remainder[i - 1] >> 63;
        remainder[0] = remainder[0] << 1 | (numerator[bit / 64] >> (bit % 64) & 1);
        if (!AtLeast(remainder, divisor)) continue;
        std::uint64_t borrow = 0;
        for (std::size_t i = 0; i < remainder.size(); ++i) {
            const std::uint64_t other = i < divisor.size() ? divisor[i] : 0;
            const std::uint64_t difference = remainder[i] - other - borrow;
            borrow = remainder[i] < other || (remainder[i] == other && borrow) ? 1 : 0;
            remainder[i] = difference;
        }
        quotient[bit / 64] |= std::uint64_t{1} << (bit % 64);
    }
    Trim(quotient);
    Trim(remainder);
}

std::size_t ShiftedWords(const Words& value, std::size_t shift) {
    return value.empty() ? 0 : (BitLength(value) + shift + 63) / 64;
}

bool Reserve(Bignum* target, std::size_t words) {
    if (target->dmax < 0 || (target->dmax > 0 && !target->d))
        throw std::invalid_argument("BN_div: output BIGNUM with a negative capacity or no storage");
    if (words <= static_cast<std::size_t>(target->dmax)) return true;
    if (words > MaxWords) return Fail(BignumTooLong);
    if (target->flags & FlagStaticData) return Fail(ExpandOnStaticData);
    auto* grown = static_cast<std::uint64_t*>(calloc_nid_postfix(words, sizeof(std::uint64_t)));
    if (!grown) return Fail(MallocFailure);
    if (target->d) std::memcpy(grown, target->d, static_cast<std::size_t>(target->dmax) * sizeof(std::uint64_t));
    free_nid_postfix(target->d);
    target->d = grown;
    target->dmax = static_cast<int>(words);
    return true;
}

void Assign(Bignum* target, const Words& words) {
    if (!words.empty()) std::memcpy(target->d, words.data(), words.size() * sizeof(std::uint64_t));
    target->top = static_cast<int>(words.size());
}

}

extern "C" {

int APS5_VABI BN_div_nid_postfix(Bignum* quotient, Bignum* remainder, const Bignum* numerator,
                                 const Bignum* divisor, BignumContext* context) {
    if (!numerator || !divisor || !context) throw std::invalid_argument("BN_div requires a numerator, a divisor and a context");
    const Words dividend = Magnitude(numerator);
    if (!dividend.empty() && dividend.back() == 0) return Fail(NotInitialized);
    const Words divisorWords = Magnitude(divisor);
    if (divisorWords.empty()) return Fail(DivisionByZero);
    if (divisorWords.back() == 0) throw std::invalid_argument("BN_div: divisor with a zero top word");
    const int quotientSign = numerator->neg ^ divisor->neg;
    const int remainderSign = numerator->neg;
    const bool constantTime = ((numerator->flags | divisor->flags) & FlagConstantTime) != 0;
    Words quotientWords;
    Words remainderWords;
    Divide(dividend, divisorWords, quotientWords, remainderWords);
    if (!constantTime && quotientWords.empty()) {
        if (remainder && remainder != numerator) {
            if (!Reserve(remainder, dividend.size())) return 0;
            Assign(remainder, dividend);
            remainder->neg = remainderSign;
        }
        if (quotient) {
            quotient->top = 0;
            quotient->neg = 0;
        }
        return 1;
    }
    const std::size_t shift = 64 - BitLength(divisorWords) % 64;
    const std::size_t shiftedDivisor = ShiftedWords(divisorWords, shift);
    std::size_t shiftedNumerator = ShiftedWords(dividend, shift + 64);
    if (constantTime) shiftedNumerator = shiftedNumerator <= shiftedDivisor + 1 ? shiftedDivisor + 2 : shiftedNumerator + 1;
    if (quotient) {
        if (!Reserve(quotient, shiftedNumerator - shiftedDivisor + 1)) return 0;
        Assign(quotient, quotientWords);
    }
    if (remainder) {
        const std::size_t shiftedRemainder = ShiftedWords(remainderWords, shift + 64);
        const std::size_t shiftWords = (shift + 64) / 64;
        if (shiftedRemainder <= shiftWords) {
            remainder->top = 0;
            remainder->neg = 0;
        } else if (Reserve(remainder, shiftedRemainder - shiftWords)) {
            Assign(remainder, remainderWords);
        }
        remainder->neg = remainder->top != 0 && remainderSign != 0;
    }
    if (quotient) quotient->neg = quotient->top != 0 && quotientSign != 0;
    return 1;
}

}

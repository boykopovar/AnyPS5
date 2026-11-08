#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Magnitude = std::vector<std::uint64_t>;

constexpr int Allocated = 1;
constexpr int StaticData = 2;
constexpr int BignumLibrary = 3;
constexpr int AnyFunction = 0xfff;
constexpr int DivisionByZero = 103;
constexpr int NotInitialized = 107;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(BignumLibrary, AnyFunction, reason);
}

void Check_nid_no_patch(const Bignum* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + ": null BIGNUM");
    if (value->top < 0) throw std::invalid_argument(std::string(function) + ": negative BIGNUM top");
    if (value->top > 0 && (!value->d || value->dmax < value->top))
        throw std::invalid_argument(std::string(function) + ": BIGNUM has fewer limbs than its top");
}

void Trim_nid_no_patch(Magnitude& magnitude) {
    while (!magnitude.empty() && magnitude.back() == 0) magnitude.pop_back();
}

int Compare_nid_no_patch(const Magnitude& first, const Magnitude& second) {
    if (first.size() != second.size()) return first.size() < second.size() ? -1 : 1;
    for (std::size_t index = first.size(); index-- > 0;) {
        if (first[index] != second[index]) return first[index] < second[index] ? -1 : 1;
    }
    return 0;
}

Magnitude Subtract_nid_no_patch(const Magnitude& first, const Magnitude& second) {
    Magnitude result(first.size());
    std::uint64_t borrow = 0;
    for (std::size_t index = 0; index < first.size(); ++index) {
        const std::uint64_t value = first[index];
        const std::uint64_t subtract = index < second.size() ? second[index] : 0;
        const std::uint64_t difference = value - subtract;
        const std::uint64_t underflow = value < subtract;
        result[index] = difference - borrow;
        borrow = underflow | (difference < borrow);
    }
    Trim_nid_no_patch(result);
    return result;
}

std::size_t BitLength_nid_no_patch(const Magnitude& magnitude) {
    if (magnitude.empty()) return 0;
    return (magnitude.size() - 1) * 64 + std::bit_width(magnitude.back());
}

Magnitude ShiftLeft_nid_no_patch(const Magnitude& magnitude, std::size_t bits) {
    if (magnitude.empty()) return {};
    const std::size_t words = bits / 64;
    const unsigned remainder = static_cast<unsigned>(bits % 64);
    Magnitude result(magnitude.size() + words + 1, 0);
    for (std::size_t index = 0; index < magnitude.size(); ++index) {
        result[index + words] |= magnitude[index] << remainder;
        if (remainder) result[index + words + 1] |= magnitude[index] >> (64 - remainder);
    }
    Trim_nid_no_patch(result);
    return result;
}

void ShiftRightOne_nid_no_patch(Magnitude& magnitude) {
    for (std::size_t index = 0; index < magnitude.size(); ++index) {
        magnitude[index] >>= 1;
        if (index + 1 < magnitude.size()) magnitude[index] |= magnitude[index + 1] << 63;
    }
    Trim_nid_no_patch(magnitude);
}

void Divide_nid_no_patch(const Magnitude& dividend, const Magnitude& divisor, Magnitude& quotient, Magnitude& remainder) {
    remainder = dividend;
    quotient.clear();
    if (divisor.empty() || Compare_nid_no_patch(remainder, divisor) < 0) return;
    const std::size_t shift = BitLength_nid_no_patch(remainder) - BitLength_nid_no_patch(divisor);
    Magnitude shifted = ShiftLeft_nid_no_patch(divisor, shift);
    quotient.assign(shift / 64 + 1, 0);
    for (std::size_t bit = shift + 1; bit-- > 0;) {
        if (Compare_nid_no_patch(remainder, shifted) >= 0) {
            remainder = Subtract_nid_no_patch(remainder, shifted);
            quotient[bit / 64] |= std::uint64_t{1} << (bit % 64);
        }
        ShiftRightOne_nid_no_patch(shifted);
    }
    Trim_nid_no_patch(quotient);
    Trim_nid_no_patch(remainder);
}

bool EnsureCapacity_nid_no_patch(Bignum* value, std::size_t limbs) {
    if (limbs == 0) return true;
    if (value->d && value->dmax >= 0 && static_cast<std::size_t>(value->dmax) >= limbs) return true;
    if (value->flags & StaticData) return false;
    if (limbs > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
    auto* grown = static_cast<std::uint64_t*>(calloc_nid_postfix(limbs, sizeof(std::uint64_t)));
    if (!grown) return false;
    if (value->d) free_nid_postfix(value->d);
    value->d = grown;
    value->dmax = static_cast<int>(limbs);
    return true;
}

bool Store_nid_no_patch(Bignum* value, const Magnitude& magnitude, bool negative, bool keepZeroSign) {
    if (value->d && value->dmax < 0) throw std::invalid_argument("BIGNUM has a negative limb capacity");
    if (!EnsureCapacity_nid_no_patch(value, magnitude.size())) return false;
    const std::size_t capacity = value->d ? static_cast<std::size_t>(value->dmax) : 0;
    for (std::size_t index = 0; index < magnitude.size(); ++index) value->d[index] = magnitude[index];
    for (std::size_t index = magnitude.size(); index < capacity; ++index) value->d[index] = 0;
    value->top = static_cast<int>(magnitude.size());
    value->neg = (keepZeroSign ? negative : (negative && !magnitude.empty())) ? 1 : 0;
    return true;
}

void Free_nid_no_patch(Bignum* value) {
    if (!value) return;
    if (value->d && !(value->flags & StaticData)) free_nid_postfix(value->d);
    free_nid_postfix(value);
}

}

extern "C" {

Bignum* APS5_VABI BN_dup_nid_postfix(const Bignum* value) {
    if (!value) return nullptr;
    Check_nid_no_patch(value, __func__);
    auto* result = static_cast<Bignum*>(calloc_nid_postfix(1, sizeof(Bignum)));
    if (!result) return nullptr;
    result->flags = Allocated;
    Magnitude magnitude;
    if (value->top > 0) magnitude.assign(value->d, value->d + value->top);
    if (!Store_nid_no_patch(result, magnitude, value->neg != 0, true)) {
        Free_nid_no_patch(result);
        return nullptr;
    }
    return result;
}

int APS5_VABI BN_div_nid_postfix(Bignum* quotient, Bignum* remainder, const Bignum* dividend, const Bignum* divisor, BignumContext*) {
    Check_nid_no_patch(dividend, __func__);
    Check_nid_no_patch(divisor, __func__);
    if (dividend->top > 0 && dividend->d[dividend->top - 1] == 0) {
        PutError(NotInitialized);
        return 0;
    }
    Magnitude numerator;
    if (dividend->top > 0) numerator.assign(dividend->d, dividend->d + dividend->top);
    Trim_nid_no_patch(numerator);
    Magnitude denominator;
    if (divisor->top > 0) denominator.assign(divisor->d, divisor->d + divisor->top);
    Trim_nid_no_patch(denominator);
    if (denominator.empty()) {
        PutError(DivisionByZero);
        return 0;
    }
    Magnitude quotientMagnitude;
    Magnitude remainderMagnitude;
    Divide_nid_no_patch(numerator, denominator, quotientMagnitude, remainderMagnitude);
    const bool quotientNegative = (dividend->neg != 0) != (divisor->neg != 0);
    const bool remainderNegative = dividend->neg != 0;
    if (quotient && !Store_nid_no_patch(quotient, quotientMagnitude, quotientNegative, false)) return 0;
    if (remainder && !Store_nid_no_patch(remainder, remainderMagnitude, remainderNegative, false)) return 0;
    return 1;
}

}

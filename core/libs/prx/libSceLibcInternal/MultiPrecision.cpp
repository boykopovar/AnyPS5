#include "prx/libSceLibcInternal/include/MultiPrecision.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace {

constexpr int ValueWords = 5;
constexpr int DigitBits = 32;
constexpr unsigned long long DigitMask = 0xFFFFFFFFull;
constexpr unsigned long long DigitBase = DigitMask + 1;

void RequireValue(const unsigned long long* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + ": null value");
}

void AddDigits(unsigned long long* value, int valueWords, const unsigned long long* addend, int addendWords) {
    unsigned long long carry = 0;
    int index = 0;
    for (; index < addendWords; ++index) {
        value[index] += addend[index] + carry;
        carry = value[index] >> DigitBits;
        value[index] &= DigitMask;
    }
    for (; carry != 0 && index < valueWords; ++index) {
        value[index] += carry;
        carry = value[index] >> DigitBits;
        value[index] &= DigitMask;
    }
}

void ScaleDigits(unsigned long long* value, int words, unsigned long long factor) {
    unsigned long long carry = 0;
    for (int index = 0; index < words; ++index) {
        value[index] = value[index] * factor + carry;
        carry = value[index] >> DigitBits;
        value[index] &= DigitMask;
    }
}

void UnscaleDigits(unsigned long long* value, unsigned long long factor) {
    unsigned long long remainder = 0;
    for (int index = ValueWords - 1; index >= 0; --index) {
        const unsigned long long current = (remainder << DigitBits) + value[index];
        value[index] = current / factor;
        remainder = current % factor;
    }
}

unsigned long long TakeBorrow(unsigned long long& word) {
    const unsigned long long high = word >> DigitBits;
    word &= DigitMask;
    return high == 0 ? 0 : DigitBase - high;
}

void Reduce(unsigned long long* value, unsigned long long divisor) {
    if (divisor == 0) throw std::invalid_argument("_MP_Rem: zero divisor");
    if (value[ValueWords - 1] != 0) throw std::invalid_argument("_MP_Rem: nonzero fifth word");
    int words = ValueWords - 1;
    while (words > 0 && value[words - 1] == 0) --words;
    if (words == 0) throw std::invalid_argument("_MP_Rem: zero value");
    unsigned long long digits[2] = {divisor & DigitMask, divisor >> DigitBits};
    const int divisorWords = digits[1] == 0 ? 1 : 2;
    unsigned long long work[ValueWords];
    std::copy_n(value, ValueWords, work);
    const unsigned long long scale = DigitBase / (digits[divisorWords - 1] + 1);
    if (scale != 1) {
        ScaleDigits(work, ValueWords, scale);
        ScaleDigits(digits, divisorWords, scale);
    }
    const unsigned long long leading = digits[divisorWords - 1];
    for (int position = words - divisorWords; position >= 0; --position) {
        const unsigned long long top = (work[position + divisorWords] << DigitBits) + work[position + divisorWords - 1];
        unsigned long long quotient = top / leading;
        if (quotient == 0) continue;
        unsigned long long remainder = top % leading;
        for (;;) {
            if (quotient < DigitBase) {
                if (divisorWords == 1) throw std::invalid_argument("_MP_Rem: nonzero quotient digit for divisor " + std::to_string(divisor));
                if (quotient * digits[0] <= (remainder << DigitBits) + work[position]) break;
            }
            --quotient;
            remainder += leading;
            if (remainder >= DigitBase) break;
        }
        unsigned long long borrow = 0;
        int index = position;
        for (; index < position + divisorWords; ++index) {
            work[index] -= quotient * digits[index - position] + borrow;
            borrow = TakeBorrow(work[index]);
        }
        for (; borrow != 0 && index < ValueWords; ++index) {
            work[index] -= borrow;
            borrow = TakeBorrow(work[index]);
        }
        if (borrow != 0) AddDigits(work + position, divisorWords + 1, digits, divisorWords);
    }
    if (scale != 1) UnscaleDigits(work, scale);
    std::copy_n(work, ValueWords, value);
}

}

extern "C" {

unsigned long long APS5_VABI sceLibcInternalMultiPrecisionGet_nid_no_patch(unsigned long long* value) {
    RequireValue(value, "_MP_Get");
    return (value[1] << DigitBits) + value[0];
}
APS5_EXPORT("lrQSsTRMMr4", sceLibcInternalMultiPrecisionGet_nid_no_patch);

void APS5_VABI sceLibcInternalMultiPrecisionAdd_nid_no_patch(unsigned long long* value, unsigned long long addend) {
    RequireValue(value, "_MP_Add");
    const unsigned long long digits[2] = {addend & DigitMask, addend >> DigitBits};
    AddDigits(value, ValueWords, digits, 2);
}
APS5_EXPORT("QfPuSqhub7o", sceLibcInternalMultiPrecisionAdd_nid_no_patch);

void APS5_VABI sceLibcInternalMultiPrecisionMultiply_nid_no_patch(unsigned long long* product, unsigned long long left, unsigned long long right) {
    RequireValue(product, "_MP_Mul");
    const unsigned long long leftDigits[2] = {left & DigitMask, left >> DigitBits};
    const unsigned long long rightDigits[2] = {right & DigitMask, right >> DigitBits};
    for (int index = 0; index < ValueWords; ++index) product[index] = 0;
    for (int column = 0; column < 2; ++column) {
        unsigned long long carry = 0;
        for (int row = 0; row < 2; ++row) {
            unsigned long long& word = product[row + column];
            word = leftDigits[row] * rightDigits[column] + word + carry;
            carry = word >> DigitBits;
            word &= DigitMask;
        }
        product[column + 2] = carry;
    }
}
APS5_EXPORT("Gav1Xt1Ce+c", sceLibcInternalMultiPrecisionMultiply_nid_no_patch);

void APS5_VABI sceLibcInternalMultiPrecisionRemainder_nid_no_patch(unsigned long long* value, unsigned long long divisor) {
    RequireValue(value, "_MP_Rem");
    Reduce(value, divisor);
}
APS5_EXPORT("Ozk+Z6QnlTY", sceLibcInternalMultiPrecisionRemainder_nid_no_patch);

}

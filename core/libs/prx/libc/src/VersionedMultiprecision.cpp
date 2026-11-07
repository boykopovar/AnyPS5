#include "prx/libc/include/Api.hpp"

#include <cstdint>

namespace {

constexpr int Words = 5;
constexpr std::uint64_t Base = std::uint64_t{1} << 32;
constexpr std::uint64_t Mask = Base - 1;

int Length(const std::uint64_t* words, int count) {
    while (count > 0 && words[count - 1] == 0) --count;
    return count;
}

void Scale(std::uint64_t* words, int count, std::uint64_t factor) {
    std::uint64_t carry = 0;
    for (int index = 0; index < count; ++index) {
        const std::uint64_t value = words[index] * factor + carry;
        words[index] = value & Mask;
        carry = value >> 32;
    }
}

void Unscale(std::uint64_t* words, std::uint64_t factor) {
    std::uint64_t remainder = 0;
    for (int index = Words - 1; index >= 0; --index) {
        const std::uint64_t value = (remainder << 32) + words[index];
        words[index] = value / factor;
        remainder = value % factor;
    }
}

void Reduce(std::uint64_t* u, int at, const std::uint64_t* v, int n) {
    const std::uint64_t top = ((at + n < Words ? u[at + n] : 0) << 32) + u[at + n - 1];
    if (top < v[n - 1]) return;
    std::uint64_t quotient = top / v[n - 1];
    std::uint64_t rest = top % v[n - 1];
    while (quotient >= Base || (n > 1 && v[n - 2] * quotient > (rest << 32) + u[at + n - 2])) {
        --quotient;
        rest += v[n - 1];
        if (rest >= Base) break;
    }
    std::uint64_t borrow = 0;
    std::uint64_t value = 0;
    for (int index = 0; index < n; ++index) {
        value = u[at + index] - (v[index] * quotient + borrow);
        u[at + index] = value & Mask;
        borrow = (value >> 32) == 0 ? 0 : Base - (value >> 32);
    }
    if ((value >> 32) == 0) return;
    for (int index = n; at + index < Words; ++index) {
        value = u[at + index] - borrow;
        u[at + index] = value & Mask;
        if ((value >> 32) == 0) return;
        borrow = Base - (value >> 32);
    }
    std::uint64_t carry = 0;
    for (int index = 0; index < n; ++index) {
        value = carry + v[index] + u[at + index];
        u[at + index] = value & Mask;
        carry = value >> 32;
    }
    if (carry != 0 && at + n < Words) u[at + n] = (u[at + n] + carry) & Mask;
}

}

extern "C" {

std::uint64_t APS5_VABI _ZNSt8__sce_v27_MP_GetEPy_nid_postfix(std::uint64_t* u) { return (u[1] << 32) + u[0]; }

void APS5_VABI _ZNSt8__sce_v27_MP_AddEPyy_nid_postfix(std::uint64_t* u, std::uint64_t value) {
    std::uint64_t sum = (value & Mask) + u[0];
    u[0] = sum & Mask;
    sum = (sum >> 32) + (value >> 32) + u[1];
    u[1] = sum & Mask;
    for (int index = 2; index < Words && (sum >> 32) != 0; ++index) {
        sum = (sum >> 32) + u[index];
        u[index] = sum & Mask;
    }
}

void APS5_VABI _ZNSt8__sce_v27_MP_MulEPyyy_nid_postfix(std::uint64_t* w, std::uint64_t left, std::uint64_t right) {
    const std::uint64_t leftLow = left & Mask;
    const std::uint64_t leftHigh = left >> 32;
    const std::uint64_t rightLow = right & Mask;
    const std::uint64_t rightHigh = right >> 32;
    w[0] = 0;
    w[1] = 0;
    w[4] = 0;
    std::uint64_t carry = 0;
    if (rightLow != 0) {
        const std::uint64_t low = leftLow * rightLow;
        const std::uint64_t middle = leftHigh * rightLow + (low >> 32);
        w[0] = low & Mask;
        w[1] = middle & Mask;
        carry = middle >> 32;
    }
    w[2] = carry;
    if (rightHigh == 0) {
        w[3] = 0;
        return;
    }
    const std::uint64_t middle = w[1] + leftLow * rightHigh;
    w[1] = middle & Mask;
    const std::uint64_t high = carry + leftHigh * rightHigh + (middle >> 32);
    w[2] = high & Mask;
    w[3] = high >> 32;
}

void APS5_VABI _ZNSt8__sce_v27_MP_RemEPyy_nid_postfix(std::uint64_t* u, std::uint64_t value) {
    std::uint64_t v[2] {value & Mask, value >> 32};
    const int n = Length(v, 2);
    if (n == 0) return;
    const int length = Length(u, Words);
    const std::uint64_t factor = Base / (v[n - 1] + 1);
    if (factor != 1) {
        Scale(u, Words, factor);
        Scale(v, n, factor);
    }
    for (int at = length - n; at >= 0; --at) Reduce(u, at, v, n);
    if (factor != 1) Unscale(u, factor);
}

}

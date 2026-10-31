#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <bit>
#include <climits>
#include <cstdint>
#include <stdexcept>

namespace {

int BitCount(const LibreSsl3Bignum* number) {
    if (!number)
        throw std::invalid_argument("RSA_bits requires a modulus");
    if (number->top == 0)
        return 0;
    if (number->top < 0)
        throw std::invalid_argument("RSA_bits requires a nonnegative modulus word count");
    if (!number->d)
        throw std::invalid_argument("RSA_bits requires modulus words");
    const std::int64_t bits = static_cast<std::int64_t>(number->top - 1) * 64 + std::bit_width(number->d[number->top - 1]);
    if (bits > INT_MAX)
        throw std::invalid_argument("RSA_bits modulus bit count overflows int");
    return static_cast<int>(bits);
}

} // namespace

extern "C" {

int APS5_VABI RSA_bits_nid_postfix(const LibreSsl3Rsa* rsa) {
    if (!rsa)
        throw std::invalid_argument("RSA_bits requires a key");
    return BitCount(static_cast<const LibreSsl3Bignum*>(rsa->n));
}

}

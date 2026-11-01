#include "prx/libSceLibreSsl/include/Api.hpp"
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

struct LibreSslBignum {
    std::uint64_t* d;
    int top;
    int dmax;
    int neg;
    int flags;
};

static_assert(sizeof(LibreSslBignum) == 24);
static_assert(offsetof(LibreSslBignum, top) == 8);
static_assert(sizeof(LibreSslRsaMethod) == 112);
static_assert(offsetof(LibreSslRsaMethod, flags) == 72);
static_assert(sizeof(LibreSslRsa) == 152);
static_assert(offsetof(LibreSslRsa, meth) == 16);
static_assert(offsetof(LibreSslRsa, n) == 32);
static_assert(offsetof(LibreSslRsa, flags) == 108);

namespace {

int ByteCount(const LibreSslBignum* number, const char* function) {
    if (!number) throw std::invalid_argument(std::string(function) + ": null modulus");
    if (number->top == 0) return 0;
    if (number->top < 0) throw std::invalid_argument(std::string(function) + ": negative modulus word count");
    if (!number->d) throw std::invalid_argument(std::string(function) + ": null modulus words");
    const std::int64_t bits = static_cast<std::int64_t>(number->top - 1) * 64 + std::bit_width(number->d[number->top - 1]);
    if (bits > INT_MAX - 7) throw std::invalid_argument(std::string(function) + ": modulus bit count overflows int");
    return static_cast<int>((bits + 7) / 8);
}

}

extern "C" {

int APS5_VABI RSA_flags_nid_postfix(const LibreSslRsa* rsa) {
    if (!rsa) return 0;
    if (!rsa->meth) throw std::invalid_argument(std::string(__func__) + ": null method");
    return rsa->meth->flags;
}

int APS5_VABI RSA_size_nid_postfix(const LibreSslRsa* rsa) {
    if (!rsa) throw std::invalid_argument(std::string(__func__) + ": null key");
    return ByteCount(static_cast<const LibreSslBignum*>(rsa->n), __func__);
}

}

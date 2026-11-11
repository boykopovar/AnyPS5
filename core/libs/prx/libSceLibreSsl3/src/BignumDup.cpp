#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"

#include <climits>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int BignumLibrary = 3;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int BignumTooLong = 114;
constexpr int FlagMalloced = 0x01;
constexpr int FlagConstantTime = 0x04;
constexpr int MaxWords = INT_MAX / (4 * 64);

Bignum* Fail(Bignum* copy, int reason) {
    LibreSslPutError_nid_no_patch(BignumLibrary, AnyFunction, reason);
    if (copy) BN_free_nid_postfix(copy);
    return nullptr;
}

}

extern "C" Bignum* APS5_VABI BN_dup_nid_postfix(const Bignum* source) {
    if (!source) return nullptr;
    if (source->top > 0 && !source->d) throw std::invalid_argument("BN_dup: BIGNUM with words but no storage");
    auto* copy = static_cast<Bignum*>(calloc_nid_postfix(1, sizeof(Bignum)));
    if (!copy) return Fail(nullptr, MallocFailure);
    copy->flags = FlagMalloced;
    if (source->top < 0) {
        BN_free_nid_postfix(copy);
        return nullptr;
    }
    if (source->top > MaxWords) return Fail(copy, BignumTooLong);
    if (source->top > 0) {
        auto* words = static_cast<std::uint64_t*>(calloc_nid_postfix(static_cast<std::size_t>(source->top), sizeof(std::uint64_t)));
        if (!words) return Fail(copy, MallocFailure);
        std::memcpy(words, source->d, static_cast<std::size_t>(source->top) * sizeof(std::uint64_t));
        copy->d = words;
        copy->dmax = source->top;
    }
    copy->top = source->top;
    copy->neg = source->neg;
    copy->flags |= source->flags & FlagConstantTime;
    return copy;
}

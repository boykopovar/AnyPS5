#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <climits>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr int FlagConstantTime = 0x04;
constexpr int MaxWords = INT_MAX / (4 * 64);

}

extern "C" Bignum* APS5_VABI BN_dup_nid_postfix(const Bignum* source) {
    if (!source) return nullptr;
    if (source->top < 0 || (source->top > 0 && !source->d)) throw std::invalid_argument("BN_dup: malformed BIGNUM");
    if (source->top > MaxWords) NotImplemented_nid_no_patch("BN_dup of a BIGNUM over BN_R_BIGNUM_TOO_LONG words, which queues an error");
    Bignum* copy = BN_new_nid_postfix();
    if (!copy) NotImplemented_nid_no_patch("BN_dup allocation failure, which queues ERR_R_MALLOC_FAILURE");
    if (source->top > 0) {
        auto* words = static_cast<std::uint64_t*>(calloc_nid_postfix(static_cast<std::size_t>(source->top), sizeof(std::uint64_t)));
        if (!words) {
            BN_free_nid_postfix(copy);
            NotImplemented_nid_no_patch("BN_dup allocation failure, which queues ERR_R_MALLOC_FAILURE");
        }
        std::memcpy(words, source->d, static_cast<std::size_t>(source->top) * sizeof(std::uint64_t));
        copy->d = words;
        copy->dmax = source->top;
    }
    copy->top = source->top;
    copy->neg = source->neg;
    copy->flags |= source->flags & FlagConstantTime;
    return copy;
}

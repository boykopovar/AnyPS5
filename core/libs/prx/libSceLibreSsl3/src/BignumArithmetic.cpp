#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int Allocated = 1;

void Check_nid_no_patch(const Bignum* value) {
    if (!value) throw std::invalid_argument("BN_dup: null BIGNUM");
    if (value->top < 0) throw std::invalid_argument("BN_dup: negative BIGNUM top");
    if (value->top > 0 && (!value->d || value->dmax < value->top))
        throw std::invalid_argument("BN_dup: BIGNUM has fewer limbs than its top");
}

}

extern "C" {

Bignum* APS5_VABI BN_dup_nid_postfix(const Bignum* value) {
    if (!value) return nullptr;
    Check_nid_no_patch(value);
    auto* result = static_cast<Bignum*>(calloc_nid_postfix(1, sizeof(Bignum)));
    if (!result) return nullptr;
    result->flags = Allocated;
    result->top = value->top;
    result->neg = value->neg;
    if (value->top > 0) {
        auto* limbs = static_cast<std::uint64_t*>(calloc_nid_postfix(static_cast<std::size_t>(value->top), sizeof(std::uint64_t)));
        if (!limbs) {
            free_nid_postfix(result);
            return nullptr;
        }
        for (std::size_t index = 0; index < static_cast<std::size_t>(value->top); ++index) limbs[index] = value->d[index];
        result->d = limbs;
        result->dmax = value->top;
    }
    return result;
}

}

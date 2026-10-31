#pragma once

#include <cstddef>
#include <cstdint>

struct LibreSsl3Bignum {
    std::uint64_t* d;
    std::int32_t top;
    std::int32_t dmax;
    std::int32_t neg;
    std::int32_t flags;
};

static_assert(sizeof(LibreSsl3Bignum) == 24);
static_assert(offsetof(LibreSsl3Bignum, top) == 8);
static_assert(offsetof(LibreSsl3Bignum, neg) == 16);

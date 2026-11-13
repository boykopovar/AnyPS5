#pragma once

#include <cstddef>

struct LibreSsl3EcdsaSig {
    void* r;
    void* s;
};

static_assert(sizeof(LibreSsl3EcdsaSig) == 16);
static_assert(offsetof(LibreSsl3EcdsaSig, r) == 0);
static_assert(offsetof(LibreSsl3EcdsaSig, s) == 8);

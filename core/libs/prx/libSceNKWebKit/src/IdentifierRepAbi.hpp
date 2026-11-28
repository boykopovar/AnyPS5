#pragma once

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct IdentifierRep {
    union {
        const char* string;
        int number;
    } value;
    bool isString;
};

static_assert(sizeof(IdentifierRep) == 16);
static_assert(alignof(IdentifierRep) == 8);
static_assert(offsetof(IdentifierRep, value) == 0);
static_assert(offsetof(IdentifierRep, isString) == 8);

}

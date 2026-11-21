#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

struct StringPiece {
    const char* data;
    std::int32_t length;
};

static_assert(offsetof(StringPiece, length) == 8);
static_assert(sizeof(StringPiece) == 16);

}

extern "C" std::int8_t APS5_VABI _ZN6icu_68eqERKNS_11StringPieceES2__nid_postfix(const Icu::StringPiece* left, const Icu::StringPiece* right);

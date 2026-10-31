// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu::Abi {

struct StringPiece {
    const char* pointer;
    std::int32_t length;
};

static_assert(sizeof(StringPiece) == 16);
static_assert(alignof(StringPiece) == 8);
static_assert(offsetof(StringPiece, pointer) == 0);
static_assert(offsetof(StringPiece, length) == 8);

}

extern "C" {
void APS5_VABI _ZN6icu_6811StringPieceC1EPKc_nid_postfix(Icu::Abi::StringPiece*, const char*);
void APS5_VABI _ZN6icu_6811StringPieceC2EPKc_nid_postfix(Icu::Abi::StringPiece*, const char*);
void APS5_VABI _ZN6icu_6811StringPieceC1ERKS0_i_nid_postfix(Icu::Abi::StringPiece*, const Icu::Abi::StringPiece*, std::int32_t);
void APS5_VABI _ZN6icu_6811StringPieceC2ERKS0_i_nid_postfix(Icu::Abi::StringPiece*, const Icu::Abi::StringPiece*, std::int32_t);
void APS5_VABI _ZN6icu_6811StringPieceC1ERKS0_ii_nid_postfix(Icu::Abi::StringPiece*, const Icu::Abi::StringPiece*, std::int32_t, std::int32_t);
void APS5_VABI _ZN6icu_6811StringPieceC2ERKS0_ii_nid_postfix(Icu::Abi::StringPiece*, const Icu::Abi::StringPiece*, std::int32_t, std::int32_t);
void APS5_VABI _ZN6icu_6811StringPiece3setEPKc_nid_postfix(Icu::Abi::StringPiece*, const char*);
std::int32_t APS5_VABI _ZN6icu_6811StringPiece7compareES0__nid_postfix(const Icu::Abi::StringPiece*, Icu::Abi::StringPiece);
}

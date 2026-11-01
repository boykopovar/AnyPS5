#pragma once

#include <cstddef>

using FcChar8 = unsigned char;
using FcBool = int;

struct FcLangSet;
struct FcPattern;
struct FcCharSet;
struct FcRange;

enum FcType {
    FcTypeUnknown = -1,
    FcTypeVoid,
    FcTypeInteger,
    FcTypeDouble,
    FcTypeString,
    FcTypeBool,
    FcTypeMatrix,
    FcTypeCharSet,
    FcTypeFTFace,
    FcTypeLangSet,
    FcTypeRange
};

enum FcResult {
    FcResultMatch,
    FcResultNoMatch,
    FcResultTypeMismatch,
    FcResultNoId,
    FcResultOutOfMemory
};

enum FcLangResult {
    FcLangEqual,
    FcLangDifferentCountry,
    FcLangDifferentTerritory = FcLangDifferentCountry,
    FcLangDifferentLang
};

struct FcMatrix {
    double xx;
    double xy;
    double yx;
    double yy;
};

struct FcValue {
    FcType type;
    union {
        const FcChar8* s;
        int i;
        FcBool b;
        double d;
        const FcMatrix* m;
        const FcCharSet* c;
        void* f;
        const FcLangSet* l;
        const FcRange* r;
    } u;
};

static_assert(sizeof(FcMatrix) == 32);
static_assert(sizeof(FcValue) == 16);
static_assert(offsetof(FcValue, u) == 8);

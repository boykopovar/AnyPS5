#pragma once

#include "SceTypes.hpp"
#include <cstdint>

struct FcCharSet;

struct FcMatrix {
    double xx;
    double xy;
    double yx;
    double yy;
};

extern "C" {

int APS5_VABI FcLangSetDestroy();
int APS5_VABI FcPatternRemove();
int APS5_VABI FcPatternGetInteger();
int APS5_VABI FcConfigGetCurrent();
int APS5_VABI FcPatternGetBool();
int APS5_VABI FcPatternAddBool();
int APS5_VABI FcFontList();
int APS5_VABI FcFontSetMatch();
int APS5_VABI FcConfigSubstitute();
int APS5_VABI FcPatternGetMatrix();
int APS5_VABI FcInit();
int APS5_VABI FcInitBringUptoDate();
int APS5_VABI FcPatternGetString();
int APS5_VABI FcFontRenderPrepare();
int APS5_VABI FcPatternAddDouble();
int APS5_VABI FcPatternDuplicate();
int APS5_VABI FcFontMatch();
int APS5_VABI FcPatternFilter();
int APS5_VABI FcConfigCreate();
int APS5_VABI FcPatternGetFTFace();
int APS5_VABI FcPatternEqual();
uint32_t APS5_VABI FcCharSetSubtractCount(const FcCharSet* first, const FcCharSet* second);
int APS5_VABI FcPatternAddInteger();
int APS5_VABI FcLangSetAdd();
int APS5_VABI FcFontSort();
int APS5_VABI FcCharSetMerge(FcCharSet* target, const FcCharSet* source, int* changed);
int APS5_VABI FcPatternGetCharSet();
void APS5_VABI FcCharSetDestroy(FcCharSet* set);
int APS5_VABI FcPatternDel();
int APS5_VABI FcConfigDestroy();
int APS5_VABI FcPatternReference();
int APS5_VABI FcObjectSetDestroy();
int APS5_VABI FcPatternGet();
int APS5_VABI FcPatternAddMatrix();
int APS5_VABI FcPatternAddString();
int APS5_VABI FcFontSetCreate();
int APS5_VABI FcLangSetHasLang();
FcCharSet* APS5_VABI FcCharSetCreate();
int APS5_VABI FcLangSetCreate();
int APS5_VABI FcFreeTypeCharIndex();
int APS5_VABI FcFontSetAdd();
int APS5_VABI FcPatternDestroy();
int APS5_VABI FcCharSetAddChar(FcCharSet* set, uint32_t character);
void APS5_VABI FcMatrixMultiply(FcMatrix* result, const FcMatrix* first, const FcMatrix* second);
int APS5_VABI FcPatternAddCharSet();
int APS5_VABI FcDefaultSubstitute();
int APS5_VABI FcObjectSetBuild();
int APS5_VABI FcPatternAddLangSet();
int APS5_VABI FcFontSetDestroy();
int APS5_VABI FcPatternGetLangSet();
int APS5_VABI FcPatternCreate();
int APS5_VABI FcConfigReference();
int APS5_VABI FcPatternFormat();

}

#pragma once

#include "SceTypes.hpp"
#include "prx/libfontconfig/include/Types.hpp"

extern "C" {

void APS5_VABI FcLangSetDestroy(FcLangSet* set);
FcBool APS5_VABI FcPatternRemove(FcPattern* pattern, const char* object, int id);
FcResult APS5_VABI FcPatternGetInteger(const FcPattern* pattern, const char* object, int id, int* value);
int APS5_VABI FcConfigGetCurrent();
FcResult APS5_VABI FcPatternGetBool(const FcPattern* pattern, const char* object, int id, FcBool* value);
FcBool APS5_VABI FcPatternAddBool(FcPattern* pattern, const char* object, FcBool value);
int APS5_VABI FcFontList();
int APS5_VABI FcFontSetMatch();
int APS5_VABI FcConfigSubstitute();
FcResult APS5_VABI FcPatternGetMatrix(const FcPattern* pattern, const char* object, int id, FcMatrix** value);
int APS5_VABI FcInit();
int APS5_VABI FcInitBringUptoDate();
FcResult APS5_VABI FcPatternGetString(const FcPattern* pattern, const char* object, int id, FcChar8** value);
int APS5_VABI FcFontRenderPrepare();
FcBool APS5_VABI FcPatternAddDouble(FcPattern* pattern, const char* object, double value);
FcPattern* APS5_VABI FcPatternDuplicate(const FcPattern* pattern);
int APS5_VABI FcFontMatch();
int APS5_VABI FcPatternFilter();
int APS5_VABI FcConfigCreate();
int APS5_VABI FcPatternGetFTFace();
FcBool APS5_VABI FcPatternEqual(const FcPattern* first, const FcPattern* second);
int APS5_VABI FcCharSetSubtractCount();
FcBool APS5_VABI FcPatternAddInteger(FcPattern* pattern, const char* object, int value);
FcBool APS5_VABI FcLangSetAdd(FcLangSet* set, const FcChar8* language);
int APS5_VABI FcFontSort();
int APS5_VABI FcCharSetMerge();
int APS5_VABI FcPatternGetCharSet();
int APS5_VABI FcCharSetDestroy();
FcBool APS5_VABI FcPatternDel(FcPattern* pattern, const char* object);
int APS5_VABI FcConfigDestroy();
void APS5_VABI FcPatternReference(FcPattern* pattern);
int APS5_VABI FcObjectSetDestroy();
FcResult APS5_VABI FcPatternGet(const FcPattern* pattern, const char* object, int id, FcValue* value);
FcBool APS5_VABI FcPatternAddMatrix(FcPattern* pattern, const char* object, const FcMatrix* value);
FcBool APS5_VABI FcPatternAddString(FcPattern* pattern, const char* object, const FcChar8* value);
int APS5_VABI FcFontSetCreate();
FcLangResult APS5_VABI FcLangSetHasLang(const FcLangSet* set, const FcChar8* language);
int APS5_VABI FcCharSetCreate();
FcLangSet* APS5_VABI FcLangSetCreate();
int APS5_VABI FcFreeTypeCharIndex();
int APS5_VABI FcFontSetAdd();
void APS5_VABI FcPatternDestroy(FcPattern* pattern);
int APS5_VABI FcCharSetAddChar();
int APS5_VABI FcMatrixMultiply();
int APS5_VABI FcPatternAddCharSet();
int APS5_VABI FcDefaultSubstitute();
int APS5_VABI FcObjectSetBuild();
int APS5_VABI FcPatternAddLangSet();
int APS5_VABI FcFontSetDestroy();
int APS5_VABI FcPatternGetLangSet();
FcPattern* APS5_VABI FcPatternCreate();
int APS5_VABI FcConfigReference();
int APS5_VABI FcPatternFormat();

}

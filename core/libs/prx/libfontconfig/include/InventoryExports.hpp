#pragma once

#include "SceTypes.hpp"
#include "prx/libfontconfig/include/Types.hpp"

namespace FreeType { struct GuestFaceRec; }

extern "C" {

void APS5_VABI FcLangSetDestroy(FcLangSet* set);
FcBool APS5_VABI FcPatternRemove(FcPattern* pattern, const char* object, int id);
FcResult APS5_VABI FcPatternGetInteger(const FcPattern* pattern, const char* object, int id, int* value);
FcResult APS5_VABI FcPatternGetBool(const FcPattern* pattern, const char* object, int id, FcBool* value);
FcBool APS5_VABI FcPatternAddBool(FcPattern* pattern, const char* object, FcBool value);
int APS5_VABI FcFontList();
int APS5_VABI FcFontSetMatch();
int APS5_VABI FcConfigSubstitute();
FcResult APS5_VABI FcPatternGetMatrix(const FcPattern* pattern, const char* object, int id, FcMatrix** value);
int APS5_VABI FcInit();
FcResult APS5_VABI FcPatternGetString(const FcPattern* pattern, const char* object, int id, FcChar8** value);
int APS5_VABI FcFontRenderPrepare();
FcBool APS5_VABI FcPatternAddDouble(FcPattern* pattern, const char* object, double value);
FcPattern* APS5_VABI FcPatternDuplicate(const FcPattern* pattern);
int APS5_VABI FcFontMatch();
int APS5_VABI FcPatternFilter();
int APS5_VABI FcPatternGetFTFace();
FcBool APS5_VABI FcPatternEqual(const FcPattern* first, const FcPattern* second);
uint32_t APS5_VABI FcCharSetSubtractCount(const FcCharSet* first, const FcCharSet* second);
FcBool APS5_VABI FcPatternAddInteger(FcPattern* pattern, const char* object, int value);
FcBool APS5_VABI FcLangSetAdd(FcLangSet* set, const FcChar8* language);
int APS5_VABI FcFontSort();
int APS5_VABI FcCharSetMerge(FcCharSet* target, const FcCharSet* source, int* changed);
int APS5_VABI FcPatternGetCharSet();
void APS5_VABI FcCharSetDestroy(FcCharSet* set);
FcBool APS5_VABI FcPatternDel(FcPattern* pattern, const char* object);
void APS5_VABI FcPatternReference(FcPattern* pattern);
FcResult APS5_VABI FcPatternGet(const FcPattern* pattern, const char* object, int id, FcValue* value);
FcBool APS5_VABI FcPatternAddMatrix(FcPattern* pattern, const char* object, const FcMatrix* value);
FcBool APS5_VABI FcPatternAddString(FcPattern* pattern, const char* object, const FcChar8* value);
FcLangResult APS5_VABI FcLangSetHasLang(const FcLangSet* set, const FcChar8* language);
FcCharSet* APS5_VABI FcCharSetCreate();
FcLangSet* APS5_VABI FcLangSetCreate();
std::uint32_t APS5_VABI FcFreeTypeCharIndex(FreeType::GuestFaceRec* face, std::uint32_t character);
void APS5_VABI FcPatternDestroy(FcPattern* pattern);
int APS5_VABI FcCharSetAddChar(FcCharSet* set, uint32_t character);
void APS5_VABI FcMatrixMultiply(FcMatrix* result, const FcMatrix* first, const FcMatrix* second);
int APS5_VABI FcPatternAddCharSet();
int APS5_VABI FcDefaultSubstitute();
int APS5_VABI FcPatternAddLangSet();
int APS5_VABI FcPatternGetLangSet();
FcPattern* APS5_VABI FcPatternCreate();
int APS5_VABI FcPatternFormat();

}

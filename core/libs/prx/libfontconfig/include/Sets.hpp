#pragma once

#include "SceTypes.hpp"

struct FcPattern;

struct FcFontSet {
    int nfont;
    int sfont;
    FcPattern** fonts;
};

struct FcObjectSet {
    int nobject;
    int sobject;
    const char** objects;
};

extern "C" {

FcFontSet* APS5_VABI FcFontSetCreate();
int APS5_VABI FcFontSetAdd(FcFontSet* set, FcPattern* pattern);
void APS5_VABI FcFontSetDestroy(FcFontSet* set);
FcObjectSet* APS5_VABI FcObjectSetBuild(const char* first, ...);
void APS5_VABI FcObjectSetDestroy(FcObjectSet* set);
void APS5_VABI FcPatternDestroy(FcPattern* pattern);
FcPattern* APS5_VABI FcPatternDuplicate(const FcPattern* pattern);

}

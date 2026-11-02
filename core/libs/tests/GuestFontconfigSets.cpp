#include "prx/libfontconfig/include/Sets.hpp"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <type_traits>

static_assert(sizeof(FcFontSet) == 16);
static_assert(sizeof(FcObjectSet) == 16);
static_assert(offsetof(FcFontSet, fonts) == 8);
static_assert(offsetof(FcObjectSet, objects) == 8);
static_assert(std::is_same_v<decltype(FcFontSetCreate()), FcFontSet*>);
static_assert(std::is_same_v<decltype(FcFontSetAdd(nullptr, nullptr)), int>);
static_assert(std::is_same_v<decltype(FcObjectSetBuild(nullptr)), FcObjectSet*>);

namespace {

int destroyed;
int destroyedSum;

void CheckFonts() {
    FcFontSetDestroy(nullptr);
    FcFontSet* set = FcFontSetCreate();
    assert(set && set->nfont == 0 && set->sfont == 0 && set->fonts == nullptr);
    for (int j = 0; j < 100; ++j) {
        auto* pattern = reinterpret_cast<FcPattern*>(new int(j + 1));
        assert(FcFontSetAdd(set, pattern) == 1);
        assert(set->nfont == j + 1);
        assert(set->sfont == ((j / 32) + 1) * 32);
        assert(set->fonts[j] == pattern);
        for (int k = 0; k <= j; ++k)
            assert(*reinterpret_cast<int*>(set->fonts[k]) == k + 1);
    }
    assert(destroyed == 0);
    FcFontSetDestroy(set);
    assert(destroyed == 100 && destroyedSum == 5050);
    assert(FcFontSetAdd(nullptr, nullptr) == 0);
}

void CheckObjects() {
    FcObjectSetDestroy(nullptr);
    FcObjectSet* empty = FcObjectSetBuild(nullptr);
    assert(empty && empty->nobject == 0 && empty->sobject == 0 && empty->objects == nullptr);
    FcObjectSetDestroy(empty);

    char mutableName[] = "family";
    FcObjectSet* objects = FcObjectSetBuild(mutableName, "style", "weight", "size",
                                          "lang", "spacing", "family", "file", nullptr);
    assert(objects && objects->nobject == 8 && objects->sobject == 8);
    mutableName[0] = 'X';
    int families = 0;
    int styles = 0;
    for (int j = 0; j < objects->nobject; ++j) {
        assert(objects->objects[j]);
        assert(objects->objects[j] != mutableName);
        if (std::strcmp(objects->objects[j], "family") == 0) ++families;
        if (std::strcmp(objects->objects[j], "style") == 0) ++styles;
    }
    assert(families == 2 && styles == 1);
    FcObjectSetDestroy(objects);

    FcObjectSet* one = FcObjectSetBuild("family", nullptr);
    assert(one && one->nobject == 1 && one->sobject == 4);
    assert(std::strcmp(one->objects[0], "family") == 0);
    FcObjectSetDestroy(one);
}

}

extern "C" void APS5_VABI FcPatternDestroy(FcPattern* pattern) {
    ++destroyed;
    destroyedSum += *reinterpret_cast<int*>(pattern);
    delete reinterpret_cast<int*>(pattern);
}

int main() {
    CheckFonts();
    CheckObjects();
}

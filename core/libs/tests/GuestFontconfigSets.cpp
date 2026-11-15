#include "prx/libfontconfig/include/Sets.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
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
int originalDestroyed;

struct PatternState {
    int value;
    bool isDuplicate;
};

PatternState* Inspect(FcPattern* pattern) {
    return reinterpret_cast<PatternState*>(pattern);
}

void CheckFonts() {
    FcFontSetDestroy(nullptr);
    FcFontSet* set = FcFontSetCreate();
    assert(set && set->nfont == 0 && set->sfont == 0 && set->fonts == nullptr);
    for (int j = 0; j < 1024; ++j) {
        auto* pattern = reinterpret_cast<FcPattern*>(new PatternState{j + 1, false});
        const int added = FcFontSetAdd(set, pattern);
        assert(added == 1);
        assert(set->nfont == j + 1);
        assert(set->sfont == ((j / 16) + 1) * 128);
        assert(set->fonts[j] != pattern);
        assert(Inspect(set->fonts[j])->isDuplicate);
        FcPatternDestroy(pattern);
        for (int k = 0; k <= j; ++k)
            assert(Inspect(set->fonts[k])->value == k + 1);
    }
    assert(set->sfont == 8192);
    auto* beyondLimit = reinterpret_cast<FcPattern*>(new PatternState{9999, false});
    assert(FcFontSetAdd(set, beyondLimit) == 0);
    FcPatternDestroy(beyondLimit);
    assert(set->nfont == 1024);
    assert(FcFontSetAdd(set, nullptr) == 0);
    assert(destroyed == 0);
    FcFontSetDestroy(set);
    assert(destroyed == 1024 && destroyedSum == 524800);
    assert(originalDestroyed == 1025);
    const int invalidAdd = FcFontSetAdd(nullptr, nullptr);
    assert(invalidAdd == 0);
}

void CheckObjects() {
    FcObjectSetDestroy(nullptr);
    FcObjectSet* empty = FcObjectSetBuild(nullptr);
    assert(empty && empty->nobject == 0 && empty->sobject == 0);
    assert(empty->objects == reinterpret_cast<const char**>(empty + 1));
    FcObjectSetDestroy(empty);

    char mutableName[] = "family";
    FcObjectSet* objects = FcObjectSetBuild(mutableName, "style", "weight", "size",
                                          "lang", "spacing", "family", "file", nullptr);
    assert(objects && objects->nobject == 8 && objects->sobject == 64);
    assert(objects->objects == reinterpret_cast<const char**>(objects + 1));
    mutableName[0] = 'X';
    const char* expected[]{"family", "style", "weight", "size", "lang", "spacing", "family", "file"};
    for (int j = 0; j < objects->nobject; ++j) {
        assert(objects->objects[j]);
        assert(objects->objects[j] != mutableName);
        assert(std::strcmp(objects->objects[j], expected[j]) == 0);
    }
    FcObjectSetDestroy(objects);

    FcObjectSet* one = FcObjectSetBuild("family", nullptr);
    assert(one && one->nobject == 1 && one->sobject == 8);
    assert(std::strcmp(one->objects[0], "family") == 0);
    FcObjectSetDestroy(one);
}

}

extern "C" void APS5_VABI FcPatternDestroy(FcPattern* pattern) {
    if (!pattern) return;
    auto* value = Inspect(pattern);
    if (value->isDuplicate) {
        ++destroyed;
        destroyedSum += value->value;
    } else {
        ++originalDestroyed;
    }
    delete value;
}

extern "C" FcPattern* APS5_VABI FcPatternDuplicate(const FcPattern* pattern) {
    if (!pattern) return nullptr;
    const auto* source = reinterpret_cast<const PatternState*>(pattern);
    return reinterpret_cast<FcPattern*>(new PatternState{source->value, true});
}

int main() {
    CheckFonts();
    CheckObjects();
}

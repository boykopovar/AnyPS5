#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libfontconfig/libfontconfig.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "fontconfig check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

static bool Same(const FcMatrix& matrix, double xx, double xy, double yx, double yy) {
    return matrix.xx == xx && matrix.xy == xy && matrix.yx == yx && matrix.yy == yy;
}

static void CheckMatrix() {
    const FcMatrix first = {1, 2, 3, 4};
    const FcMatrix second = {5, 6, 7, 8};
    FcMatrix result = {};
    FcMatrixMultiply(&result, &first, &second);
    Check(Same(result, 19, 22, 43, 50));
    FcMatrixMultiply(&result, &second, &first);
    Check(Same(result, 23, 34, 31, 46));

    FcMatrix left = first;
    FcMatrixMultiply(&left, &left, &second);
    Check(Same(left, 19, 22, 43, 50));
    FcMatrix right = second;
    FcMatrixMultiply(&right, &first, &right);
    Check(Same(right, 19, 22, 43, 50));
    FcMatrix both = first;
    FcMatrixMultiply(&both, &both, &both);
    Check(Same(both, 7, 10, 15, 22));
}

static void CheckNullSets() {
    FcCharSetDestroy(nullptr);
    FcCharSet* set = FcCharSetCreate();
    Check(set != nullptr);
    int changed = 7;
    Check(FcCharSetAddChar(nullptr, 'a') == 0);
    Check(FcCharSetMerge(nullptr, set, &changed) == 0);
    Check(FcCharSetMerge(set, nullptr, &changed) == 0);
    Check(changed == 7);
    Check(FcCharSetSubtractCount(nullptr, set) == 0);
    Check(FcCharSetSubtractCount(set, nullptr) == 0);
    FcCharSetDestroy(set);
}

static void CheckSubtract() {
    FcCharSet* first = FcCharSetCreate();
    FcCharSet* second = FcCharSetCreate();
    Check(FcCharSetSubtractCount(first, second) == 0);
    for (uint32_t character : {0x0u, 0x1Fu, 0x20u, 0xFFu, 0x100u, 0x10FFFFu}) {
        Check(FcCharSetAddChar(first, character) == 1);
    }
    Check(FcCharSetAddChar(first, 0x20) == 1);
    Check(FcCharSetSubtractCount(first, second) == 6);
    Check(FcCharSetSubtractCount(second, first) == 0);
    Check(FcCharSetSubtractCount(first, first) == 0);
    Check(FcCharSetAddChar(second, 0x1F) == 1);
    Check(FcCharSetAddChar(second, 0x100) == 1);
    Check(FcCharSetAddChar(second, 'a') == 1);
    Check(FcCharSetSubtractCount(first, second) == 4);
    Check(FcCharSetSubtractCount(second, first) == 1);
    FcCharSetDestroy(second);
    FcCharSetDestroy(first);
}

static void CheckCharacterRange() {
    FcCharSet* set = FcCharSetCreate();
    FcCharSet* empty = FcCharSetCreate();
    Check(FcCharSetAddChar(set, 0x01000000) == 0);
    Check(FcCharSetAddChar(set, 0x01000041) == 0);
    Check(FcCharSetAddChar(set, 0xFFFFFFFF) == 0);
    Check(FcCharSetSubtractCount(set, empty) == 0);
    Check(FcCharSetAddChar(set, 0x00FFFFFF) == 1);
    Check(FcCharSetSubtractCount(set, empty) == 1);
    Check(FcCharSetAddChar(set, 0x01000041) == 0);
    Check(FcCharSetSubtractCount(set, empty) == 1);
    Check(FcCharSetAddChar(set, 0x41) == 1);
    Check(FcCharSetSubtractCount(set, empty) == 2);
    FcCharSetDestroy(empty);
    FcCharSetDestroy(set);
}

static void CheckCharacterAliasesExistingLeaf() {
    FcCharSet* set = FcCharSetCreate();
    FcCharSet* low = FcCharSetCreate();
    Check(FcCharSetAddChar(set, 0x42) == 1);
    Check(FcCharSetAddChar(set, 0x01000041) == 1);
    Check(FcCharSetAddChar(low, 0x41) == 1);
    Check(FcCharSetAddChar(low, 0x42) == 1);
    Check(FcCharSetSubtractCount(set, low) == 0);
    Check(FcCharSetSubtractCount(low, set) == 0);
    FcCharSetDestroy(low);
    FcCharSetDestroy(set);
}

static void CheckMerge() {
    FcCharSet* target = FcCharSetCreate();
    FcCharSet* source = FcCharSetCreate();
    Check(FcCharSetAddChar(target, 'a') == 1);
    Check(FcCharSetAddChar(source, 'a') == 1);
    Check(FcCharSetAddChar(source, 0x3042) == 1);

    int changed = 0;
    Check(FcCharSetMerge(target, source, &changed) == 1);
    Check(changed == 1);
    Check(FcCharSetSubtractCount(source, target) == 0);
    Check(FcCharSetSubtractCount(target, source) == 0);

    changed = 7;
    Check(FcCharSetMerge(target, source, &changed) == 1);
    Check(changed == 0);
    Check(FcCharSetMerge(target, target, &changed) == 1);
    Check(changed == 0);

    Check(FcCharSetAddChar(source, 'b') == 1);
    Check(FcCharSetMerge(target, source, nullptr) == 1);
    Check(FcCharSetSubtractCount(source, target) == 0);
    Check(FcCharSetMerge(target, target, nullptr) == 1);
    Check(FcCharSetSubtractCount(target, source) == 0);
    FcCharSetDestroy(source);
    FcCharSetDestroy(target);
}

static bool Throws(void (*action)()) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static FcPattern* queryPattern = nullptr;

static void CheckConfigurationQueries() {
    FcPattern* pattern = FcPatternCreate();
    queryPattern = pattern;
    Check(pattern != nullptr);
    FcObjectSet* objects = FcObjectSetBuild("family", "style", nullptr);
    Check(objects != nullptr);
    Check(FcFontList(nullptr, nullptr, nullptr) == nullptr);
    Check(FcFontList(nullptr, pattern, objects) == nullptr);
    Check(FcFontList(FcConfigCreate(), pattern, nullptr) == nullptr);
    FcFontSet* set = FcFontSetCreate();
    Check(set != nullptr);
    FcFontSet* sets[] = {set};
    for (const int count : {0, 1}) {
        FcResult result = FcResultMatch;
        Check(FcFontSetMatch(nullptr, sets, count, pattern, &result) == nullptr);
        Check(result == FcResultNoMatch);
    }
    FcResult result = FcResultOutOfMemory;
    Check(FcFontSetMatch(FcConfigGetCurrent(), sets, 1, pattern, &result) == nullptr && result == FcResultNoMatch);
    Check(Throws([] { FcFontSet* none[] = {nullptr}; FcFontSetMatch(nullptr, none, 1, queryPattern, nullptr); }));
    Check(Throws([] { FcResult unused; FcFontSetMatch(nullptr, nullptr, 0, queryPattern, &unused); }));
    Check(Throws([] { FcFontSet* none[] = {nullptr}; FcResult unused; FcFontSetMatch(nullptr, none, 1, nullptr, &unused); }));
    for (FcConfig* config : {static_cast<FcConfig*>(nullptr), FcConfigCreate()}) {
        FcResult matched = FcResultMatch;
        Check(FcFontMatch(config, pattern, &matched) == nullptr && matched == FcResultNoMatch);
        FcCharSet* coverage = reinterpret_cast<FcCharSet*>(&matched);
        for (const FcBool trim : {0, 1}) {
            FcResult sorted = FcResultTypeMismatch;
            Check(FcFontSort(config, pattern, trim, &coverage, &sorted) == nullptr && sorted == FcResultNoMatch);
            Check(coverage == reinterpret_cast<FcCharSet*>(&matched));
            sorted = FcResultMatch;
            Check(FcFontSort(config, pattern, trim, nullptr, &sorted) == nullptr && sorted == FcResultNoMatch);
        }
    }
    Check(Throws([] { FcFontMatch(nullptr, queryPattern, nullptr); }));
    Check(Throws([] { FcResult unused; FcFontMatch(nullptr, nullptr, &unused); }));
    Check(Throws([] { FcFontSort(nullptr, queryPattern, 1, nullptr, nullptr); }));
    Check(Throws([] { FcResult unused; FcFontSort(nullptr, nullptr, 1, nullptr, &unused); }));
    for (const int kind : {-1, 0, 1, 2, 3, 0x7fffffff}) {
        Check(FcConfigSubstitute(nullptr, pattern, kind) == 0);
        Check(FcConfigSubstitute(nullptr, nullptr, kind) == 0);
    }
    FcFontSetDestroy(set);
    FcObjectSetDestroy(objects);
    FcPatternDestroy(pattern);
}

int main() {
    CheckMatrix();
    CheckNullSets();
    CheckSubtract();
    CheckCharacterRange();
    CheckCharacterAliasesExistingLeaf();
    CheckMerge();
    CheckConfigurationQueries();
    return 0;
}

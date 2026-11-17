#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libfontconfig/libfontconfig.hpp"
#include "Testing.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static bool Same(const FcMatrix& matrix, double xx, double xy, double yx, double yy) {
    return matrix.xx == xx && matrix.xy == xy && matrix.yx == yx && matrix.yy == yy;
}

static void CheckMatrix() {
    const FcMatrix first = {1, 2, 3, 4};
    const FcMatrix second = {5, 6, 7, 8};
    FcMatrix result = {};
    FcMatrixMultiply(&result, &first, &second);
    Require(Same(result, 19, 22, 43, 50));
    FcMatrixMultiply(&result, &second, &first);
    Require(Same(result, 23, 34, 31, 46));

    FcMatrix left = first;
    FcMatrixMultiply(&left, &left, &second);
    Require(Same(left, 19, 22, 43, 50));
    FcMatrix right = second;
    FcMatrixMultiply(&right, &first, &right);
    Require(Same(right, 19, 22, 43, 50));
    FcMatrix both = first;
    FcMatrixMultiply(&both, &both, &both);
    Require(Same(both, 7, 10, 15, 22));

    errno = 71;
    FcMatrixMultiply(nullptr, &first, &second);
    FcMatrixMultiply(&result, nullptr, &second);
    FcMatrixMultiply(&result, &first, nullptr);
    FcMatrixMultiply(reinterpret_cast<FcMatrix*>(0x10), &first, &second);
    FcMatrixMultiply(&result, reinterpret_cast<const FcMatrix*>(0x10), &second);
    FcMatrixMultiply(&result, &first, reinterpret_cast<const FcMatrix*>(0x10));
    Require(errno == 71);
}

static void CheckNullSets() {
    FcCharSetDestroy(nullptr);
    FcCharSet* set = FcCharSetCreate();
    Require(set != nullptr);
    int changed = 7;
    Require(FcCharSetAddChar(nullptr, 'a') == 0);
    Require(FcCharSetMerge(nullptr, set, &changed) == 0);
    Require(FcCharSetMerge(set, nullptr, &changed) == 0);
    Require(changed == 7);
    Require(FcCharSetSubtractCount(nullptr, set) == 0);
    Require(FcCharSetSubtractCount(set, nullptr) == 0);
    FcCharSetDestroy(set);
}

static void CheckSubtract() {
    FcCharSet* first = FcCharSetCreate();
    FcCharSet* second = FcCharSetCreate();
    Require(FcCharSetSubtractCount(first, second) == 0);
    for (uint32_t character : {0x0u, 0x1Fu, 0x20u, 0xFFu, 0x100u, 0x10FFFFu}) {
        Require(FcCharSetAddChar(first, character) == 1);
    }
    Require(FcCharSetAddChar(first, 0x20) == 1);
    Require(FcCharSetSubtractCount(first, second) == 6);
    Require(FcCharSetSubtractCount(second, first) == 0);
    Require(FcCharSetSubtractCount(first, first) == 0);
    Require(FcCharSetAddChar(second, 0x1F) == 1);
    Require(FcCharSetAddChar(second, 0x100) == 1);
    Require(FcCharSetAddChar(second, 'a') == 1);
    Require(FcCharSetSubtractCount(first, second) == 4);
    Require(FcCharSetSubtractCount(second, first) == 1);
    FcCharSetDestroy(second);
    FcCharSetDestroy(first);
}

static void CheckCharacterRange() {
    FcCharSet* set = FcCharSetCreate();
    FcCharSet* empty = FcCharSetCreate();
    Require(FcCharSetAddChar(set, 0x01000000) == 0);
    Require(FcCharSetAddChar(set, 0x01000041) == 0);
    Require(FcCharSetAddChar(set, 0xFFFFFFFF) == 0);
    Require(FcCharSetSubtractCount(set, empty) == 0);
    Require(FcCharSetAddChar(set, 0x00FFFFFF) == 1);
    Require(FcCharSetSubtractCount(set, empty) == 1);
    Require(FcCharSetAddChar(set, 0x01000041) == 0);
    Require(FcCharSetSubtractCount(set, empty) == 1);
    Require(FcCharSetAddChar(set, 0x41) == 1);
    Require(FcCharSetSubtractCount(set, empty) == 2);
    FcCharSetDestroy(empty);
    FcCharSetDestroy(set);
}

static void CheckCharacterAliasesExistingLeaf() {
    FcCharSet* set = FcCharSetCreate();
    FcCharSet* low = FcCharSetCreate();
    Require(FcCharSetAddChar(set, 0x42) == 1);
    Require(FcCharSetAddChar(set, 0x01000041) == 1);
    Require(FcCharSetAddChar(low, 0x41) == 1);
    Require(FcCharSetAddChar(low, 0x42) == 1);
    Require(FcCharSetSubtractCount(set, low) == 0);
    Require(FcCharSetSubtractCount(low, set) == 0);
    FcCharSetDestroy(low);
    FcCharSetDestroy(set);
}

static void CheckMerge() {
    FcCharSet* target = FcCharSetCreate();
    FcCharSet* source = FcCharSetCreate();
    Require(FcCharSetAddChar(target, 'a') == 1);
    Require(FcCharSetAddChar(source, 'a') == 1);
    Require(FcCharSetAddChar(source, 0x3042) == 1);

    int changed = 0;
    errno = 71;
    Require(FcCharSetMerge(target, source, reinterpret_cast<int*>(0x10)) == 0);
    Require(errno == 71);
    Require(FcCharSetMerge(target, source, &changed) == 1);
    Require(changed == 1);
    Require(FcCharSetSubtractCount(source, target) == 0);
    Require(FcCharSetSubtractCount(target, source) == 0);

    changed = 7;
    Require(FcCharSetMerge(target, source, &changed) == 1);
    Require(changed == 0);
    Require(FcCharSetMerge(target, target, &changed) == 1);
    Require(changed == 0);

    Require(FcCharSetAddChar(source, 'b') == 1);
    Require(FcCharSetMerge(target, source, nullptr) == 1);
    Require(FcCharSetSubtractCount(source, target) == 0);
    Require(FcCharSetMerge(target, target, nullptr) == 1);
    Require(FcCharSetSubtractCount(target, source) == 0);
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
    Require(pattern != nullptr);
    FcObjectSet* objects = FcObjectSetBuild("family", "style", nullptr);
    Require(objects != nullptr);
    Require(FcFontList(nullptr, nullptr, nullptr) == nullptr);
    Require(FcFontList(nullptr, pattern, objects) == nullptr);
    Require(FcFontList(FcConfigCreate(), pattern, nullptr) == nullptr);
    FcFontSet* set = FcFontSetCreate();
    Require(set != nullptr);
    FcFontSet* sets[] = {set};
    for (const int count : {0, 1}) {
        FcResult result = FcResultMatch;
        Require(FcFontSetMatch(nullptr, sets, count, pattern, &result) == nullptr);
        Require(result == FcResultNoMatch);
    }
    FcResult result = FcResultOutOfMemory;
    Require(FcFontSetMatch(FcConfigGetCurrent(), sets, 1, pattern, &result) == nullptr && result == FcResultNoMatch);
    Require(Throws([] { FcFontSet* none[] = {nullptr}; FcFontSetMatch(nullptr, none, 1, queryPattern, nullptr); }));
    Require(Throws([] { FcResult unused; FcFontSetMatch(nullptr, nullptr, 0, queryPattern, &unused); }));
    Require(Throws([] { FcFontSet* none[] = {nullptr}; FcResult unused; FcFontSetMatch(nullptr, none, 1, nullptr, &unused); }));
    {
        FcResult initial = FcResultMatch;
        void* page = nullptr;
        std::size_t pageSize = 0;
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        page = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(page != nullptr);
        std::memcpy(page, &initial, sizeof(initial));
        DWORD previous = 0;
        Require(VirtualProtect(page, pageSize, PAGE_READONLY, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        page = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(page != MAP_FAILED);
        std::memcpy(page, &initial, sizeof(initial));
        Require(mprotect(page, pageSize, PROT_READ) == 0);
#endif
        auto* roResult = static_cast<FcResult*>(page);
        auto* badResult = reinterpret_cast<FcResult*>(std::uintptr_t{0x10});
        for (FcResult* target : {badResult, roResult}) {
            errno = 71;
            bool threw = false;
            try { FcFontSetMatch(nullptr, sets, 1, queryPattern, target); } catch (const std::runtime_error&) { threw = true; }
            Require(threw && errno == 71);
            errno = 71;
            threw = false;
            try { FcFontMatch(nullptr, queryPattern, target); } catch (const std::runtime_error&) { threw = true; }
            Require(threw && errno == 71);
            errno = 71;
            threw = false;
            try { FcFontSort(nullptr, queryPattern, 1, nullptr, target); } catch (const std::runtime_error&) { threw = true; }
            Require(threw && errno == 71);
        }
#ifdef _WIN32
        VirtualFree(page, 0, MEM_RELEASE);
#else
        munmap(page, pageSize);
#endif
    }
    for (FcConfig* config : {static_cast<FcConfig*>(nullptr), FcConfigCreate()}) {
        FcResult matched = FcResultMatch;
        Require(FcFontMatch(config, pattern, &matched) == nullptr && matched == FcResultNoMatch);
        FcCharSet* coverage = reinterpret_cast<FcCharSet*>(&matched);
        for (const FcBool trim : {0, 1}) {
            FcResult sorted = FcResultTypeMismatch;
            Require(FcFontSort(config, pattern, trim, &coverage, &sorted) == nullptr && sorted == FcResultNoMatch);
            Require(coverage == reinterpret_cast<FcCharSet*>(&matched));
            sorted = FcResultMatch;
            Require(FcFontSort(config, pattern, trim, nullptr, &sorted) == nullptr && sorted == FcResultNoMatch);
        }
    }
    Require(Throws([] { FcFontMatch(nullptr, queryPattern, nullptr); }));
    Require(Throws([] { FcResult unused; FcFontMatch(nullptr, nullptr, &unused); }));
    Require(Throws([] { FcFontSort(nullptr, queryPattern, 1, nullptr, nullptr); }));
    Require(Throws([] { FcResult unused; FcFontSort(nullptr, nullptr, 1, nullptr, &unused); }));
    for (const int kind : {-1, 0, 1, 2, 3, 0x7fffffff}) {
        Require(FcConfigSubstitute(nullptr, pattern, kind) == 0);
        Require(FcConfigSubstitute(nullptr, nullptr, kind) == 0);
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

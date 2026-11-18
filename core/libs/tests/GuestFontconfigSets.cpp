#include "prx/libfontconfig/include/Sets.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static_assert(sizeof(FcFontSet) == 16);
static_assert(sizeof(FcObjectSet) == 16);
static_assert(offsetof(FcFontSet, fonts) == 8);
static_assert(offsetof(FcObjectSet, objects) == 8);
static_assert(std::is_same_v<decltype(FcFontSetCreate()), FcFontSet*>);
static_assert(std::is_same_v<decltype(FcFontSetAdd(nullptr, nullptr)), int>);
static_assert(std::is_same_v<decltype(FcObjectSetBuild(nullptr)), FcObjectSet*>);

namespace {

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "fontconfig sets check failed at line %d\n", line);
        std::abort();
    }
}

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
        FcPatternDestroy_nid_postfix(pattern);
        for (int k = 0; k <= j; ++k)
            assert(Inspect(set->fonts[k])->value == k + 1);
    }
    assert(set->sfont == 8192);
    auto* beyondLimit = reinterpret_cast<FcPattern*>(new PatternState{9999, false});
    assert(FcFontSetAdd(set, beyondLimit) == 0);
    FcPatternDestroy_nid_postfix(beyondLimit);
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

template <typename Fn> static void ExpectThrowsPreservingErrno(Fn&& fn, int line) {
    errno = 71;
    bool threw = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == 71, line);
}

#define EXPECT_REJECTED(...) ExpectThrowsPreservingErrno([&] { __VA_ARGS__; }, __LINE__)

void CheckGuestPointerSafety() {
    const int duplicatesBefore = destroyed;
    const int duplicatesSumBefore = destroyedSum;
    auto* pattern = reinterpret_cast<FcPattern*>(new PatternState{4242, false});

    auto* unmappedSet = reinterpret_cast<FcFontSet*>(std::uintptr_t{0x10});
    EXPECT_REJECTED(FcFontSetAdd(unmappedSet, pattern));
    EXPECT_REJECTED(FcFontSetDestroy(unmappedSet));
    EXPECT_REJECTED(FcObjectSetDestroy(reinterpret_cast<FcObjectSet*>(std::uintptr_t{0x10})));
    assert(destroyed == duplicatesBefore);

    auto* setWithUnmappedArray = FcFontSetCreate();
    assert(setWithUnmappedArray);
    setWithUnmappedArray->nfont = 1;
    setWithUnmappedArray->sfont = 8;
    setWithUnmappedArray->fonts = reinterpret_cast<FcPattern**>(std::uintptr_t{0x10});
    EXPECT_REJECTED(FcFontSetDestroy(setWithUnmappedArray));
    assert(destroyed == duplicatesBefore);

    std::size_t pageSize = 0;
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    pageSize = info.dwPageSize;
    void* page = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(page);
#else
    const long size = sysconf(_SC_PAGESIZE);
    assert(size > 0);
    pageSize = static_cast<std::size_t>(size);
    void* page = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(page != MAP_FAILED);
#endif
    auto* readOnlySet = reinterpret_cast<FcFontSet*>(page);
    auto* array = static_cast<FcPattern**>(std::malloc(8));
    assert(array);
    readOnlySet->nfont = 0;
    readOnlySet->sfont = 8;
    readOnlySet->fonts = array;
#ifdef _WIN32
    DWORD previous = 0;
    assert(VirtualProtect(page, pageSize, PAGE_READONLY, &previous) != 0);
#else
    assert(mprotect(page, pageSize, PROT_READ) == 0);
#endif
    EXPECT_REJECTED(FcFontSetAdd(readOnlySet, pattern));
    assert(destroyed == duplicatesBefore + 1);
    assert(destroyedSum == duplicatesSumBefore + 4242);
    assert(readOnlySet->nfont == 0);
    std::free(array);
#ifdef _WIN32
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, pageSize);
#endif

    delete reinterpret_cast<PatternState*>(pattern);
}

}

extern "C" void APS5_VABI FcPatternDestroy_nid_postfix(FcPattern* pattern) {
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

extern "C" FcPattern* APS5_VABI FcPatternDuplicate_nid_postfix(const FcPattern* pattern) {
    if (!pattern) return nullptr;
    const auto* source = reinterpret_cast<const PatternState*>(pattern);
    return reinterpret_cast<FcPattern*>(new PatternState{source->value, true});
}

int main() {
    CheckFonts();
    CheckObjects();
    CheckGuestPointerSafety();
}

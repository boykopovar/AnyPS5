#include "prx/libfontconfig/include/Sets.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

char* CopyObject(const char* object) {
    const std::size_t size = std::strlen(object) + 1;
    char* copy = static_cast<char*>(std::malloc(size));
    if (!copy) return nullptr;
    std::memcpy(copy, object, size);
    return copy;
}

}

extern "C" {

FcFontSet* APS5_VABI FcFontSetCreate() {
    return static_cast<FcFontSet*>(std::calloc(1, sizeof(FcFontSet)));
}

int APS5_VABI FcFontSetAdd(FcFontSet* set, FcPattern* pattern) {
    if (!set || !pattern || set->nfont < 0 || set->sfont < 0 || set->nfont >= 1024)
        return 0;
    if (set->nfont >= set->sfont / static_cast<int>(sizeof(FcPattern*))) {
        if (set->sfont >= 8192) return 0;
        constexpr int increment = 16 * sizeof(FcPattern*);
        if (set->sfont > std::numeric_limits<int>::max() - increment) return 0;
        const auto next = static_cast<std::size_t>(set->sfont + increment);
        void* storage = std::realloc(set->fonts, next);
        if (!storage) return 0;
        set->fonts = static_cast<FcPattern**>(storage);
        set->sfont = static_cast<int>(next);
    }
    FcPattern* duplicate = FcPatternDuplicate(pattern);
    if (!duplicate) return 0;
    set->fonts[set->nfont++] = duplicate;
    return 1;
}

void APS5_VABI FcFontSetDestroy(FcFontSet* set) {
    if (!set) return;
    for (int i = 0; i < set->nfont; ++i) FcPatternDestroy(set->fonts[i]);
    std::free(set->fonts);
    std::free(set);
}

FcObjectSet* APS5_VABI FcObjectSetBuild(const char* first, ...) {
    std::size_t count = first ? 1 : 0;
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, first);
#else
    va_list args;
    va_start(args, first);
#endif
    if (first) {
        for (const char* object = va_arg(args, const char*); object;
             object = va_arg(args, const char*)) {
            if (count >= static_cast<std::size_t>(std::numeric_limits<int>::max() / 8)) {
#ifdef _WIN32
                __builtin_sysv_va_end(args);
#else
                va_end(args);
#endif
                return nullptr;
            }
            ++count;
        }
    }
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    if (count > (std::numeric_limits<std::size_t>::max() - sizeof(FcObjectSet)) / sizeof(const char*))
        return nullptr;
    auto* set = static_cast<FcObjectSet*>(std::malloc(sizeof(FcObjectSet) + count * sizeof(const char*)));
    if (!set) return nullptr;
    set->nobject = 0;
    set->sobject = static_cast<int>(count * sizeof(const char*));
    set->objects = reinterpret_cast<const char**>(set + 1);
    if (!first) return set;

#ifdef _WIN32
    __builtin_sysv_va_start(args, first);
#else
    va_start(args, first);
#endif
    bool ok = true;
    for (const char* object = first; object; object = va_arg(args, const char*)) {
        char* copy = CopyObject(object);
        if (!copy) {
            ok = false;
            break;
        }
        set->objects[set->nobject++] = copy;
    }
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    if (!ok) {
        FcObjectSetDestroy(set);
        return nullptr;
    }
    return set;
}

void APS5_VABI FcObjectSetDestroy(FcObjectSet* set) {
    if (!set) return;
    for (int i = 0; i < set->nobject; ++i)
        std::free(const_cast<char*>(set->objects[i]));
    std::free(set);
}

}

#include "prx/libfontconfig/include/Sets.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

template <typename T>
bool Grow(T*** entries, int& capacity, int count, int increment) {
    if (count < capacity) return true;
    if (capacity > std::numeric_limits<int>::max() - increment) return false;
    const int next = capacity + increment;
    if (static_cast<std::size_t>(next) > std::numeric_limits<std::size_t>::max() / sizeof(T*))
        return false;
    void* allocated = std::realloc(*entries, static_cast<std::size_t>(next) * sizeof(T*));
    if (!allocated) return false;
    *entries = static_cast<T**>(allocated);
    capacity = next;
    return true;
}

bool AddObject(FcObjectSet* set, const char* object) {
    if (!Grow(&set->objects, set->sobject, set->nobject, 4)) return false;
    const std::size_t size = std::strlen(object) + 1;
    char* copy = static_cast<char*>(std::malloc(size));
    if (!copy) return false;
    std::memcpy(copy, object, size);

    int left = 0;
    int right = set->nobject;
    const auto key = reinterpret_cast<std::uintptr_t>(copy);
    while (left < right) {
        const int middle = left + (right - left) / 2;
        if (reinterpret_cast<std::uintptr_t>(set->objects[middle]) < key)
            left = middle + 1;
        else
            right = middle;
    }
    const auto offset = static_cast<std::size_t>(set->nobject - left);
    std::memmove(set->objects + left + 1, set->objects + left, offset * sizeof(const char*));
    set->objects[left] = copy;
    ++set->nobject;
    return true;
}

}

extern "C" {

FcFontSet* APS5_VABI FcFontSetCreate() {
    return static_cast<FcFontSet*>(std::calloc(1, sizeof(FcFontSet)));
}

int APS5_VABI FcFontSetAdd(FcFontSet* set, FcPattern* pattern) {
    if (!set || !Grow(&set->fonts, set->sfont, set->nfont, 32)) return 0;
    set->fonts[set->nfont++] = pattern;
    return 1;
}

void APS5_VABI FcFontSetDestroy(FcFontSet* set) {
    if (!set) return;
    for (int i = 0; i < set->nfont; ++i) FcPatternDestroy(set->fonts[i]);
    std::free(set->fonts);
    std::free(set);
}

FcObjectSet* APS5_VABI FcObjectSetBuild(const char* first, ...) {
    FcObjectSet* set = static_cast<FcObjectSet*>(std::calloc(1, sizeof(FcObjectSet)));
    if (!set) return nullptr;
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, first);
#else
    va_list args;
    va_start(args, first);
#endif
    bool ok = true;
    for (const char* object = first; object; object = va_arg(args, const char*)) {
        if (!AddObject(set, object)) {
            ok = false;
            break;
        }
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
    std::free(set->objects);
    std::free(set);
}

}

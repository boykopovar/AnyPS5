#include "prx/libfontconfig/include/Sets.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdarg>
#include <cerrno>
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

bool ReadGuestBytes(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

bool WriteGuestBytes(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return ok;
}

}

extern "C" {

FcFontSet* APS5_VABI FcFontSetCreate() {
    return static_cast<FcFontSet*>(std::calloc(1, sizeof(FcFontSet)));
}

int APS5_VABI FcFontSetAdd(FcFontSet* set, FcPattern* pattern) {
    if (!set || !pattern) return 0;
    FcFontSet record{};
    if (!ReadGuestBytes(&record, set, sizeof(record))) NotImplemented_nid_no_patch(__func__);
    if (record.nfont < 0 || record.sfont < 0 || record.nfont >= 1024)
        return 0;
    const bool grows = record.nfont >= record.sfont / static_cast<int>(sizeof(FcPattern*));
    FcFontSet updated = record;
    void* grown = nullptr;
    if (grows) {
        if (record.sfont >= 8192) return 0;
        constexpr int increment = 16 * sizeof(FcPattern*);
        if (record.sfont > std::numeric_limits<int>::max() - increment) return 0;
        const auto next = static_cast<std::size_t>(record.sfont + increment);
        grown = std::malloc(next);
        if (!grown) return 0;
        const auto existing = static_cast<std::size_t>(record.nfont) * sizeof(FcPattern*);
        if (existing && !ReadGuestBytes(grown, record.fonts, existing)) {
            std::free(grown);
            NotImplemented_nid_no_patch(__func__);
        }
        updated.fonts = static_cast<FcPattern**>(grown);
        updated.sfont = static_cast<int>(next);
    }
    FcPattern* duplicate = FcPatternDuplicate_nid_postfix(pattern);
    if (!duplicate) {
        if (grown) std::free(grown);
        return 0;
    }
    if (!WriteGuestBytes(&updated.fonts[updated.nfont], &duplicate, sizeof(duplicate))) {
        if (grown) std::free(grown);
        FcPatternDestroy_nid_postfix(duplicate);
        NotImplemented_nid_no_patch(__func__);
    }
    ++updated.nfont;
    if (!WriteGuestBytes(set, &updated, sizeof(updated))) {
        if (grown) std::free(grown);
        FcPatternDestroy_nid_postfix(duplicate);
        NotImplemented_nid_no_patch(__func__);
    }
    if (grown) std::free(record.fonts);
    return 1;
}

void APS5_VABI FcFontSetDestroy(FcFontSet* set) {
    if (!set) return;
    FcFontSet record{};
    if (!ReadGuestBytes(&record, set, sizeof(record))) NotImplemented_nid_no_patch(__func__);
    for (int i = 0; i < record.nfont; ++i) {
        FcPattern* pattern = nullptr;
        if (!ReadGuestBytes(&pattern, &record.fonts[i], sizeof(pattern))) NotImplemented_nid_no_patch(__func__);
        FcPatternDestroy_nid_postfix(pattern);
    }
    std::free(record.fonts);
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
    FcObjectSet record{};
    if (!ReadGuestBytes(&record, set, sizeof(record))) NotImplemented_nid_no_patch(__func__);
    for (int i = 0; i < record.nobject; ++i) {
        const char* object = nullptr;
        if (!ReadGuestBytes(&object, &record.objects[i], sizeof(object))) NotImplemented_nid_no_patch(__func__);
        std::free(const_cast<char*>(object));
    }
    std::free(set);
}

}

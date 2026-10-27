#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <new>

extern "C" {

char* APS5_VABI strndup_nid_postfix(const char* source, std::size_t limit) {
    if (!source) { errno = 22; return nullptr; }
    std::size_t length = 0;
    while (length < limit && source[length] != '\0') ++length;
    try {
        auto* result = static_cast<char*>(ApplicationHeapAllocate_nid_no_patch(length + 1));
        if (!result) { errno = 12; return nullptr; }
        std::memcpy(result, source, length);
        result[length] = '\0';
        return result;
    } catch (const std::bad_alloc&) {
        errno = 12;
        return nullptr;
    }
}

char* APS5_VABI strdup_nid_postfix(const char* source) {
    return strndup_nid_postfix(source, static_cast<std::size_t>(-1));
}

int APS5_VABI asprintf_nid_postfix(char** destination, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vasprintf_nid_postfix(destination, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

}

#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>

#ifdef _WIN32
#include "prx/libc/include/WindowsFormatting.hpp"
#endif

extern "C" {

char* APS5_VABI strndup_nid_postfix(const char* source, std::size_t limit) {
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
    if (!destination) { errno = 22; return -1; }
    *destination = nullptr;
    if (!format) { errno = 22; return -1; }
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    int result = -1;
    try {
#ifdef _WIN32
        std::string text;
        const int count = LibcDetail::FormatWindows(nullptr, 0, format, args, &text);
        const char* source = text.c_str();
#else
        char* text = nullptr;
        const int count = ::vasprintf(&text, format, args);
        std::unique_ptr<char, decltype(&std::free)> owner(text, std::free);
        const char* source = text;
#endif
        if (count >= 0) {
            auto* output = static_cast<char*>(ApplicationHeapAllocate_nid_no_patch(static_cast<std::size_t>(count) + 1));
            if (output) {
                std::memcpy(output, source, static_cast<std::size_t>(count) + 1);
                *destination = output;
                result = count;
            } else errno = 12;
        }
    } catch (const std::bad_alloc&) {
        errno = 12;
    } catch (...) {
#ifdef _WIN32
        __builtin_sysv_va_end(args);
#else
        va_end(args);
#endif
        throw;
    }
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

}

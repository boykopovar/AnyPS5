#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

#ifdef _WIN32
#include "prx/libc/include/WindowsFormatting.hpp"
#endif

namespace {

using GuestCompare = int (APS5_VABI*)(const void*, const void*);
using GuestNewHandler = void (APS5_VABI*)();

GuestNewHandler g_newHandler = nullptr;

#ifdef _WIN32
// Formats with a guest (SysV) va_list into a host string.
std::string FormatGuest(const char* format, const void* args) {
    std::string text;
    LibcDetail::FormatWindows(nullptr, 0, format, args, &text);
    return text;
}
#endif

}

extern "C" {

void* APS5_VABI bsearch_nid_postfix(const void* key, const void* base, size_t count, size_t size, GuestCompare compare) {
    const auto* bytes = static_cast<const unsigned char*>(base);
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        const size_t middle = low + (high - low) / 2;
        const void* element = bytes + middle * size;
        const int order = compare(key, element);
        if (order == 0) return const_cast<void*>(element);
        if (order < 0) high = middle;
        else low = middle + 1;
    }
    return nullptr;
}

void APS5_VABI clearerr_nid_postfix(FileStream* stream) {
    std::clearerr(GetNativeStream(stream));
}

int APS5_VABI feof_nid_postfix(FileStream* stream) {
    return std::feof(GetNativeStream(stream));
}

int APS5_VABI ferror_nid_postfix(FileStream* stream) {
    return std::ferror(GetNativeStream(stream));
}

int APS5_VABI fputc_nid_postfix(int character, FileStream* stream) {
    return std::fputc(character, GetNativeStream(stream));
}

int APS5_VABI getc_nid_postfix(FileStream* stream) {
    return std::getc(GetNativeStream(stream));
}

int APS5_VABI ungetc_nid_postfix(int character, FileStream* stream) {
    return std::ungetc(character, GetNativeStream(stream));
}

char* APS5_VABI fgets_nid_postfix(char* buffer, int count, FileStream* stream) {
    return std::fgets(buffer, count, GetNativeStream(stream));
}

int APS5_VABI putchar_nid_postfix(int character) {
    return std::putchar(character);
}

void APS5_VABI perror_nid_postfix(const char* prefix) {
    if (prefix && *prefix) std::fprintf(stderr, "%s: %s\n", prefix, std::strerror(errno));
    else std::fprintf(stderr, "%s\n", std::strerror(errno));
}

int APS5_VABI remove_nid_postfix(const char* path) {
    std::error_code error;
    return std::filesystem::remove(ResolvePath_nid_no_patch(path), error) ? 0 : -1;
}

// FreeBSD and the host CRT agree on the classic errno values the messages are looked up by.
char* APS5_VABI strerror_nid_postfix(int error) {
    return std::strerror(error);
}

#ifdef _WIN32

int APS5_VABI fprintf_nid_postfix(FileStream* stream, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const auto text = FormatGuest(format, args);
    __builtin_sysv_va_end(args);
    return static_cast<int>(std::fwrite(text.data(), 1, text.size(), GetNativeStream(stream)));
}

int APS5_VABI vsprintf_nid_postfix(char* buffer, const char* format, VaList* args) {
    return LibcDetail::FormatWindows(buffer, SIZE_MAX, format, args);
}

int APS5_VABI vsprintf_s_nid_postfix(char* buffer, size_t size, const char* format, VaList* args) {
    return LibcDetail::FormatWindows(buffer, size, format, args);
}

int APS5_VABI sprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::FormatWindows(buffer, size, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

#endif

[[noreturn]] void APS5_VABI _Assert_nid_postfix(const char* message, const char* location) {
    std::fprintf(stderr, "[libc] guest assertion failed: %s (%s)\n", message ? message : "?", location ? location : "?");
    std::fflush(stderr);
    std::abort();
}

[[noreturn]] void APS5_VABI _Exit_nid_postfix(int status) {
    static const bool trace = std::getenv("APS5_TRACE_EXIT") != nullptr;
    if (trace) std::fprintf(stderr, "[libc] _Exit(%d) called from %p\n", status, __builtin_return_address(0));
    std::fflush(nullptr);
    std::_Exit(status);
}

// Only the "C" locale exists.
const char* APS5_VABI setlocale_nid_postfix(int category, const char* locale) {
    (void)category;
    if (locale == nullptr || locale[0] == 0 || std::strcmp(locale, "C") == 0 || std::strcmp(locale, "POSIX") == 0) return "C";
    return nullptr;
}

GuestNewHandler APS5_VABI _ZSt15set_new_handlerPFvvE_nid_postfix(GuestNewHandler handler) {
    const auto previous = g_newHandler;
    g_newHandler = handler;
    return previous;
}

void APS5_VABI _ZdlPvSt11align_val_t_nid_postfix(void* pointer, std::size_t alignment) {
    (void)alignment;
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
}

}

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

#ifdef _WIN32
#include "prx/libc/include/WindowsFormatting.hpp"
#endif

namespace {

using GuestNewHandler = void (APS5_VABI*)();

GuestNewHandler g_newHandler = nullptr;

void* Allocate(std::size_t size) {
    return ApplicationHeapAllocate_nid_no_patch(size == 0 ? 1 : size);
}

}

extern "C" {

#ifdef _WIN32

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

// Dinkumware lconv as the title's libc lays it out: monetary strings, CHAR_MAX monetary flags, then numeric and message strings.
struct GuestLconv {
    const char* monetary[7];
    char monetaryFlags[14];
    const char* decimalPoint;
    const char* numeric[4];
    const char* falseName;
    const char* trueName;
    const char* no;
    const char* yes;
};
static_assert(sizeof(GuestLconv) == 0x90);

const GuestLconv* APS5_VABI localeconv_nid_postfix() {
    static const GuestLconv lconv{
        {"", "", "", "", "", "", ""},
        {CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX},
        ".", {"", "", "", ""}, "false", "true", "", ""};
    return &lconv;
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

GuestNewHandler APS5_VABI _ZSt15get_new_handlerv_nid_postfix() {
    return g_newHandler;
}

unsigned char _ZSt7nothrow_nid_postfix = 0;

void* APS5_VABI _Znwm_nid_postfix(std::size_t size) {
    return Allocate(size);
}

void* APS5_VABI _Znam_nid_postfix(std::size_t size) {
    return Allocate(size);
}

void* APS5_VABI _ZnwmRKSt9nothrow_t_nid_postfix(std::size_t size, const void*) {
    return Allocate(size);
}

void* APS5_VABI _ZnamRKSt9nothrow_t_nid_postfix(std::size_t size, const void*) {
    return Allocate(size);
}

void APS5_VABI _ZdlPv_nid_postfix(void* pointer) {
    if (pointer != nullptr) ApplicationHeapFree_nid_no_patch(pointer);
}

void APS5_VABI _ZdaPv_nid_postfix(void* pointer) {
    if (pointer != nullptr) ApplicationHeapFree_nid_no_patch(pointer);
}

void APS5_VABI _ZdlPvm_nid_postfix(void* pointer, std::size_t) {
    if (pointer != nullptr) ApplicationHeapFree_nid_no_patch(pointer);
}

void APS5_VABI _ZdaPvm_nid_postfix(void* pointer, std::size_t) {
    if (pointer != nullptr) ApplicationHeapFree_nid_no_patch(pointer);
}

void* APS5_VABI _ZnwmSt11align_val_t_nid_postfix(std::size_t size, std::size_t alignment) {
    return GuestHeap::GuestHeapAlign_nid_postfix(alignment, size);
}

void APS5_VABI _ZdlPvSt11align_val_t_nid_postfix(void* pointer, std::size_t alignment) {
    (void)alignment;
    GuestHeap::GuestHeapFree_nid_postfix(pointer);
}

}

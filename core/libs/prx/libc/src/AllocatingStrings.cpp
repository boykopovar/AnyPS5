#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cstring>
#include <new>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
#else
#include <unistd.h>
#endif

namespace {
std::atomic<GuestWriteHook_nid_no_patch> g_guestWriteHook{nullptr};
}

extern "C" {

void LibcSetGuestWriteHook_nid_no_patch(GuestWriteHook_nid_no_patch hook) {
    g_guestWriteHook.store(hook, std::memory_order_release);
}

GuestWriteHook_nid_no_patch LibcGetGuestWriteHook_nid_no_patch() {
    return g_guestWriteHook.load(std::memory_order_acquire);
}

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

int APS5_VABI vdprintf_nid_postfix(int descriptor, const char* format, VaList* args) {
    if (descriptor > SHRT_MAX) {
        errno = 24;
        return -1;
    }
    if (descriptor < 0) {
        errno = 9;
        return -1;
    }
    char* text = nullptr;
    const int count = vasprintf_nid_postfix(&text, format, args);
    if (count < 0) return -1;
    std::size_t remaining = static_cast<std::size_t>(count);
    const char* ptr = text;
    int writeError = 0;
    while (remaining > 0) {
        std::int64_t written = 0;
        if (const auto hook = g_guestWriteHook.load(std::memory_order_acquire)) {
            written = hook(descriptor, ptr, remaining);
        } else {
#ifdef _WIN32
            const auto ignore = +[](const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {};
            const auto previous = _set_thread_local_invalid_parameter_handler(ignore);
            written = ::_write(descriptor, ptr, static_cast<unsigned int>(remaining));
            _set_thread_local_invalid_parameter_handler(previous);
#else
            written = ::write(descriptor, ptr, remaining);
#endif
        }
        if (written <= 0) {
            writeError = errno;
            break;
        }
        ptr += written;
        remaining -= static_cast<std::size_t>(written);
    }
    ApplicationHeapFree_nid_no_patch(text);
    if (remaining > 0) {
        if (writeError != 0) errno = writeError;
        return -1;
    }
    return count;
}

int APS5_VABI dprintf_nid_postfix(int descriptor, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vdprintf_nid_postfix(descriptor, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

}

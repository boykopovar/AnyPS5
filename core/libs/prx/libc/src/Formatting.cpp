#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>
#include <new>
#include <stdexcept>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/VarArgsAbi.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
extern "C" int* APS5_VABI __error_nid_postfix();

#ifdef _WIN32
#include "prx/libc/include/WindowsFormatting.hpp"

namespace {
int ScanWindowsSafe(const char* input, const char* format, const void* args) noexcept {
    try {
        return LibcDetail::ScanWindows(input, format, args);
    } catch (const std::bad_alloc&) {
        *__error_nid_postfix() = 12; // Guest ENOMEM.
    } catch (const std::invalid_argument&) {
        *__error_nid_postfix() = 22; // Guest EINVAL.
    } catch (const std::out_of_range&) {
        *__error_nid_postfix() = 22;
    } catch (...) {
        *__error_nid_postfix() = 5; // Guest EIO for unexpected CRT failures.
    }
    return -1;
}
}
#endif

extern "C" {

int APS5_VABI vfprintf_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    auto* native = GetNativeStream(stream);
#ifdef _WIN32
    std::string buffer;
    const int count = LibcDetail::FormatWindows(nullptr, 0, format, args, &buffer);
    const int result = std::fwrite(buffer.data(), 1, static_cast<size_t>(count), native) ==
        static_cast<size_t>(count) ? count : -1;
#else
    const int result = std::vfprintf(native, format, *reinterpret_cast<std::va_list*>(args));
#endif
    stream->SyncStatus();
    return result;
}

int APS5_VABI fprintf_nid_postfix(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfprintf_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

#ifdef _WIN32

int APS5_VABI printf_nid_postfix(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI libc_printf_nid_postfix(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI snprintf_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::FormatWindows(buffer, size, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI sprintf_nid_postfix(char* buffer, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = LibcDetail::FormatWindows(buffer, SIZE_MAX, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

#else

int APS5_VABI printf_nid_postfix(const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = std::vprintf(format, args);
    va_end(args);
    return result;
}

int APS5_VABI libc_printf_nid_postfix(VA_ARGS) {
    (void)rcx; (void)r8; (void)r9;
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rsi, rdx, rcx, r8, r9, 0,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 0u,
        reinterpret_cast<void*>(overflow_arg_area));
    return std::vprintf(reinterpret_cast<const char*>(rdi), *va);
}

int APS5_VABI snprintf_nid_postfix(VA_ARGS) {
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rcx, r8, r9, 0, 0, 0,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 0u,
        reinterpret_cast<void*>(overflow_arg_area));
    return std::vsnprintf(
        reinterpret_cast<char*>(rdi),
        static_cast<size_t>(rsi),
        reinterpret_cast<const char*>(rdx),
        *va
    );
}

int APS5_VABI sprintf_nid_postfix(VA_ARGS) {
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rdx, rcx, r8, r9, 0, 0,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 0u,
        reinterpret_cast<void*>(overflow_arg_area));
    return std::vsprintf(
        reinterpret_cast<char*>(rdi),
        reinterpret_cast<const char*>(rsi),
        *va
    );
}

#endif

#ifdef _WIN32
int APS5_VABI sscanf_nid_postfix(const char* input, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = ScanWindowsSafe(input, format, args);
    __builtin_sysv_va_end(args);
    return result;
}
#else
int APS5_VABI sscanf_nid_postfix(VA_ARGS) {
    LibcDetail::RegSaveArea regs;
    LibcDetail::FillRegSaveArea(regs, rdx, rcx, r8, r9, 0, 0,
        xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7);
    LibcDetail::VaListLayout layout;
    std::va_list* va = LibcDetail::BuildVaList(layout, regs, 0u,
        reinterpret_cast<void*>(overflow_arg_area));
    return std::vsscanf(
        reinterpret_cast<const char*>(rdi),
        reinterpret_cast<const char*>(rsi),
        *va
    );
}
#endif

int APS5_VABI vsscanf_nid_postfix(const char* input, const char* format, VaList* args) {
#ifdef _WIN32
    return ScanWindowsSafe(input, format, args);
#else
    return std::vsscanf(input, format, *reinterpret_cast<std::va_list*>(args));
#endif
}

int APS5_VABI vprintf_nid_postfix(const char* str, VaList* c) {
#ifdef _WIN32
    return LibcDetail::PrintWindows(str, c);
#else
    std::va_list* va = reinterpret_cast<std::va_list*>(c);
    return std::vprintf(str, *va);
#endif
}

int APS5_VABI vsprintf_nid_postfix(char* str, const char* format, VaList* args) {
#ifdef _WIN32
    return LibcDetail::FormatWindows(str, SIZE_MAX, format, args);
#else
    return std::vsprintf(str, format, *reinterpret_cast<std::va_list*>(args));
#endif
}

int APS5_VABI vsnprintf_nid_postfix(char* str, size_t size, const char* format, VaList* c) {
#ifdef _WIN32
    return LibcDetail::FormatWindows(str, size, format, c);
#else
    std::va_list* va = reinterpret_cast<std::va_list*>(c);
    return std::vsnprintf(str, size, format, *va);
#endif
}

int APS5_VABI vasprintf_nid_postfix(char** output, const char* format, VaList* args) noexcept {
    if (output == nullptr) { *__error_nid_postfix() = 22; return -1; }
    *output = nullptr;
    if (format == nullptr || args == nullptr) { *__error_nid_postfix() = 22; return -1; }
    try {
#ifdef _WIN32
        // The guest's SysV va_list cannot be passed to the Windows CRT.
        std::string formatted;
        const int count = LibcDetail::FormatWindows(nullptr, 0, format, args, &formatted);
        char* result = static_cast<char*>(ApplicationHeapAllocate_nid_no_patch(formatted.size() + 1));
        std::memcpy(result, formatted.data(), formatted.size());
        result[formatted.size()] = '\0';
#else
        auto* guestArgs = reinterpret_cast<std::va_list*>(args);
        std::va_list measure;
        va_copy(measure, *guestArgs);
        const int count = std::vsnprintf(nullptr, 0, format, measure);
        va_end(measure);
        if (count < 0) { *__error_nid_postfix() = 22; return -1; }
        char* result = static_cast<char*>(ApplicationHeapAllocate_nid_no_patch(static_cast<std::size_t>(count) + 1));
        std::va_list render;
        va_copy(render, *guestArgs);
        const int written = std::vsnprintf(result, static_cast<std::size_t>(count) + 1, format, render);
        va_end(render);
        if (written != count) {
            ApplicationHeapFree_nid_no_patch(result);
            *__error_nid_postfix() = 5;
            return -1;
        }
#endif
        *output = result;
        return count;
    } catch (const std::bad_alloc&) {
        *__error_nid_postfix() = 12;
    } catch (const std::invalid_argument&) {
        *__error_nid_postfix() = 22;
    } catch (const std::overflow_error&) {
        *__error_nid_postfix() = 84;
    } catch (...) {
        *__error_nid_postfix() = 5;
    }
    return -1;
}

int APS5_VABI puts_nid_postfix(const char* s) {
    return std::puts(s);
}

}

#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <algorithm>
#include <cstdarg>
#include <cctype>
#include <cstring>
#include <string>
#include <cerrno>
#include <cstdlib>
#include <memory>
#include <climits>
#include <stdexcept>
#include <cmath>
#include <limits>

#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/SonyCrt.hpp"

#include "prx/libc/include/WindowsFormatting.hpp"
#include "prx/libc/include/ScanWideField.hpp"
#include "prx/libc/include/ScanQuadLength.hpp"

#ifdef _WIN32
#include "prx/libc/include/WindowsScanning.hpp"
#endif

#if defined(__GLIBC__)
int __isoc99_vsscanf(const char*, const char*, std::va_list) noexcept;
int __isoc99_vfscanf(std::FILE*, const char*, std::va_list);
}
#endif

namespace {
int ScanGuest(const char* buffer, const char* format, bool secure, NextPointer nextPointer, NextCapacity nextCapacity, bool consoleSizes = false) {
    if (buffer == nullptr) return EOF;
    return ScanSource(StringSource(buffer), format, secure, nextPointer, nextCapacity, consoleSizes);
}

template <typename NextPointer, typename NextCapacity>
int ScanGuestStream(FileStream* stream, const char* format, NextPointer nextPointer, NextCapacity nextCapacity, bool secure = true) {
    if (stream == nullptr || format == nullptr) return EOF;
    if (!stream->IsOpen()) { errno = 9; return EOF; }
    flockfile_nid_postfix(stream);
    struct Unlock {
        FileStream* stream;
        ~Unlock() { funlockfile_nid_postfix(stream); }
    } unlock{stream};
    auto* native = GetNativeStream(stream);
    stream->Orientation(-1);
    if (!stream->SwitchToReading()) return EOF;
    const int result = ScanSource(StreamSource(native), format, secure, nextPointer, nextCapacity);
    stream->SyncStatus();
    stream->KeepDirectoryReadError(result == -1);
    return result;
}

}

extern "C" {

int APS5_VABI vasprintf_nid_postfix(char** destination, const char* format, VaList* args) {
    if (!destination) { errno = 22; return -1; }
    *destination = nullptr;
    if (!format || !args) { errno = 22; return -1; }
    try {
#ifdef _WIN32
        std::string text;
        const int count = LibcDetail::FormatWindows(nullptr, 0, format, args, &text);
        const char* source = text.c_str();
#else
        std::string guestText;
        const bool guestFormatting = RequiresGuestFormatting(format);
        char* text = nullptr;
        std::va_list copy;
        va_copy(copy, *reinterpret_cast<std::va_list*>(args));
        const int count = guestFormatting ? LibcDetail::FormatWindows(nullptr, 0, format, args, &guestText) : ::vasprintf(&text, format, copy);
        va_end(copy);
        std::unique_ptr<char, decltype(&std::free)> owner(text, std::free);
        if (count < 0) return -1;
        const char* source = guestFormatting ? guestText.c_str() : text;
#endif
        auto* output = static_cast<char*>(ApplicationHeapAllocate_nid_no_patch(static_cast<size_t>(count) + 1));
        if (!output) {
            errno = 12;
            return -1;
        }
        std::memcpy(output, source, static_cast<size_t>(count) + 1);
        *destination = output;
        return count;
    } catch (const std::bad_alloc&) {
        errno = 12;
        return -1;
    }
}

int APS5_VABI vfprintf_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(stream);
    stream->Orientation(-1);
    if (!stream->SwitchToWriting()) return -1;
#ifdef _WIN32
    std::string buffer;
    const int count = LibcDetail::FormatWindows(nullptr, 0, format, args, &buffer);
    const int result = std::fwrite(buffer.data(), 1, static_cast<size_t>(count), native) ==
        static_cast<size_t>(count) ? count : -1;
#else
    if (RequiresGuestFormatting(format)) {
        std::string buffer;
        const int count = LibcDetail::FormatWindows(nullptr, 0, format, args, &buffer);
        const int result = std::fwrite(buffer.data(), 1, static_cast<size_t>(count), native) == static_cast<size_t>(count) ? count : -1;
        stream->SyncStatus();
        return result;
    }
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = std::vfprintf(native, format, copy);
    va_end(copy);
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

int APS5_VABI fscanf_nid_postfix(FileStream* stream, const char* format, ...) {
    std::string quadHead;
    format = CutAtQuadScanDirective(format, quadHead);
#ifndef _WIN32
    if (HasNarrowCharacterScan(format)) {
        std::va_list args;
        va_start(args, format);
        const int result = ScanGuestStream(stream, format, [&] { return va_arg(args, void*); }, [] { return 0u; }, false);
        va_end(args);
        return result;
    }
#endif
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(stream);
    stream->Orientation(-1);
    if (!stream->SwitchToReading()) return EOF;
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = HasGuestWideScan(format) ? ScanSource(StreamSource(native), format, false, [&] { return __builtin_va_arg(args, void*); }, [] { return 0u; })
                                                : LibcDetail::ScanFileWindows_nid_no_patch(native, format, args);
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, format);
    const int result = HasGuestWideScan(format) ? ScanSource(StreamSource(native), format, false, [&] { return va_arg(args, void*); }, [] { return 0u; })
                                                : HostVfscanf(native, format, args);
    va_end(args);
#endif
    stream->SyncStatus();
    stream->KeepDirectoryReadError(result == -1);
    return result;
}

#ifdef _WIN32

int APS5_VABI printf_nid_postfix(const char* format, ...) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    _Stdout_nid_postfix.Orientation(-1);
    const int result = LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI libc_printf_nid_postfix(const char* format, ...) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    _Stdout_nid_postfix.Orientation(-1);
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
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    std::va_list args;
    va_start(args, format);
    _Stdout_nid_postfix.Orientation(-1);
    const int result = RequiresGuestFormatting(format) ? LibcDetail::PrintWindows(format, args) : std::vfprintf(GuestStandardOutputFile_nid_no_patch(), format, args);
    va_end(args);
    return result;
}

int APS5_VABI libc_printf_nid_postfix(const char* format, ...) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    std::va_list args;
    va_start(args, format);
    _Stdout_nid_postfix.Orientation(-1);
    const int result = RequiresGuestFormatting(format) ? LibcDetail::PrintWindows(format, args) : std::vfprintf(GuestStandardOutputFile_nid_no_patch(), format, args);
    va_end(args);
    return result;
}

int APS5_VABI snprintf_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = RequiresGuestFormatting(format) ? LibcDetail::FormatWindows(buffer, size, format, args) : std::vsnprintf(buffer, HostFormatSize(size), format, args);
    va_end(args);
    return result;
}

int APS5_VABI sprintf_nid_postfix(char* buffer, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = RequiresGuestFormatting(format) ? LibcDetail::FormatWindows(buffer, SIZE_MAX, format, args) : std::vsprintf(buffer, format, args);
    va_end(args);
    return result;
}

#endif

int APS5_VABI vsscanf_nid_postfix(const char* input, const char* format, VaList* args) {
    std::string head;
    format = CutAtQuadScanDirective(format, head);
    if (ScansInProcess(format)) {
        LibcDetail::FormatArguments arguments(args);
        return ScanGuest(input, format, false, [&] { return arguments.Next<void*>(); }, [] { return 0u; });
    }
#ifdef _WIN32
    return LibcDetail::ScanWindows(input, format, args);
#else
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = HostVsscanf(input, format, copy);
    va_end(copy);
    return result;
#endif
}

#ifdef _WIN32
int APS5_VABI sscanf_nid_postfix(const char* input, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = vsscanf_nid_postfix(input, format, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
    return result;
}
#else
int APS5_VABI sscanf_nid_postfix(const char* input, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    std::string head;
    format = CutAtQuadScanDirective(format, head);
    const int result = ScansInProcess(format) ? ScanGuest(input, format, false, [&] { return va_arg(args, void*); }, [] { return 0u; })
                                                  : HostVsscanf(input, format, args);
    va_end(args);
    return result;
}
#endif

#ifdef _WIN32

int APS5_VABI sscanf_s_nid_postfix(const char* buffer, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = ScanGuest(buffer, format, true, [&] { return __builtin_va_arg(args, void*); }, [&] { return __builtin_va_arg(args, unsigned int); });
    __builtin_sysv_va_end(args);
    return result;
}

#else

int APS5_VABI sscanf_s_nid_postfix(const char* buffer, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = ScanGuest(buffer, format, true, [&] { return va_arg(args, void*); }, [&] { return va_arg(args, unsigned int); });
    va_end(args);
    return result;
}

#endif

int APS5_VABI vprintf_nid_postfix(const char* str, VaList* c) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
#ifdef _WIN32
    _Stdout_nid_postfix.Orientation(-1);
    return LibcDetail::PrintWindows(str, c);
#else
    _Stdout_nid_postfix.Orientation(-1);
    if (RequiresGuestFormatting(str)) return LibcDetail::PrintWindows(str, c);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(c));
    const int result = std::vfprintf(GuestStandardOutputFile_nid_no_patch(), str, copy);
    va_end(copy);
    return result;
#endif
}

int APS5_VABI vsprintf_nid_postfix(char* str, const char* format, VaList* args) {
#ifdef _WIN32
    return LibcDetail::FormatWindows(str, SIZE_MAX, format, args);
#else
    if (RequiresGuestFormatting(format)) return LibcDetail::FormatWindows(str, SIZE_MAX, format, args);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int result = std::vsprintf(str, format, copy);
    va_end(copy);
    return result;
#endif
}

int APS5_VABI vsnprintf_nid_postfix(char* str, size_t size, const char* format, VaList* c) {
#ifdef _WIN32
    return LibcDetail::FormatWindows(str, size, format, c);
#else
    if (RequiresGuestFormatting(format)) return LibcDetail::FormatWindows(str, size, format, c);
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(c));
    const int result = std::vsnprintf(str, HostFormatSize(size), format, copy);
    va_end(copy);
    return result;
#endif
}

static int FormatTerminated(const char* badArgument, const char* badFormat, const char* overflow, char* buffer, size_t size, const char* format, const void* args) {
    if (buffer == nullptr || size == 0 || size > SonyRsizeMax) {
        ReportConstraint(badArgument);
        return 0;
    }
    if (format == nullptr) {
        buffer[0] = 0;
        ReportConstraint(badFormat);
        return 0;
    }
    std::string text;
    bool violated = false;
    try {
        LibcDetail::FormatWindows(nullptr, 0, format, args, &text, true);
    } catch (const LibcDetail::FormatConstraintViolation& violation) {
        std::memcpy(buffer, text.data(), std::min(text.size(), size - 1));
        buffer[0] = 0;
        ReportConstraint(violation.what());
        violated = true;
    }
    if (!violated && text.size() < size) {
        std::memcpy(buffer, text.c_str(), text.size() + 1);
        return static_cast<int>(text.size());
    }
    if (!violated) {
        std::memcpy(buffer, text.data(), size - 1);
        buffer[0] = 0;
    }
    if (!violated || text.size() >= size) SonyInvokeConstraintHandler(overflow, nullptr, guestErange);
    return 0;
}

static int FormatSecure(char* buffer, size_t size, const char* format, const void* args) {
    constexpr size_t RsizeMax = SIZE_MAX >> 1;
    int result = -1;
    if (buffer != nullptr && format != nullptr && args != nullptr && size != 0 && size <= RsizeMax) {
        try {
            result = LibcDetail::FormatWindows(buffer, size, format, args, nullptr, true);
        } catch (const LibcDetail::FormatConstraintViolation&) {
        }
    }
    if (result < 0 && buffer != nullptr && size != 0 && size <= RsizeMax) buffer[0] = 0;
    return result;
}

int APS5_VABI vsnprintf_s_nid_postfix(char* buffer, size_t size, const char* format, VaList* args) {
    return FormatSecure(buffer, size, format, args);
}

static int WriteSecure(FileStream* stream, const char* format, const void* args) {
    if (stream == nullptr || format == nullptr || args == nullptr) return -1;
    if (!stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(stream);
    stream->Orientation(-1);
    if (!stream->SwitchToWriting()) return -1;
    std::string text;
    int count;
    try {
        count = LibcDetail::FormatWindows(nullptr, 0, format, args, &text, true);
    } catch (const LibcDetail::FormatConstraintViolation&) {
        return -1;
    }
    const int result = std::fwrite(text.data(), 1, static_cast<size_t>(count), native) == static_cast<size_t>(count) ? count : -1;
    stream->SyncStatus();
    return result;
}

int APS5_VABI vfprintf_s_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    return WriteSecure(stream, format, args);
}

int APS5_VABI vprintf_s_nid_postfix(const char* format, VaList* args) {
    return WriteSecure(&_Stdout_nid_postfix, format, args);
}

int APS5_VABI fprintf_s_nid_postfix(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfprintf_s_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

#ifdef _WIN32


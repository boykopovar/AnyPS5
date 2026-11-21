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

int APS5_VABI vsscanf_s_nid_postfix(const char* buffer, const char* format, VaList* args) {
    LibcDetail::FormatArguments arguments(args);
    return ScanGuest(buffer, format, true, [&] { return arguments.Next<void*>(); }, [&] { return arguments.Next<unsigned long long>(); }, true);
}

int APS5_VABI vsprintf_s_nid_postfix(char* buffer, size_t size, const char* format, VaList* args) {
    return FormatTerminated("vsprintf_s: bad argument", "vsprintf_s: bad format argument", "vsprintf_s: buffer overflow", buffer, size, format, args);
}

int APS5_VABI sprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = FormatTerminated("sprintf_s: bad argument", "sprintf_s: bad format argument", "sprintf_s: buffer overflow", buffer, size, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI snprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = FormatSecure(buffer, size, format, args);
    __builtin_sysv_va_end(args);
    return result;
}

int APS5_VABI printf_s_nid_postfix(const char* format, ...) {
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const int result = WriteSecure(&_Stdout_nid_postfix, format, reinterpret_cast<VaList*>(args));
    __builtin_sysv_va_end(args);
    return result;
}

#else

int APS5_VABI vsscanf_s_nid_postfix(const char* buffer, const char* format, VaList* args) {
    LibcDetail::FormatArguments arguments(args);
    return ScanGuest(buffer, format, true, [&] { return arguments.Next<void*>(); }, [&] { return arguments.Next<unsigned long long>(); }, true);
}

int APS5_VABI vsprintf_s_nid_postfix(char* buffer, size_t size, const char* format, VaList* args) {
    return FormatTerminated("vsprintf_s: bad argument", "vsprintf_s: bad format argument", "vsprintf_s: buffer overflow", buffer, size, format, args);
}

int APS5_VABI sprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = FormatTerminated("sprintf_s: bad argument", "sprintf_s: bad format argument", "sprintf_s: buffer overflow", buffer, size, format, args);
    va_end(args);
    return result;
}

int APS5_VABI snprintf_s_nid_postfix(char* buffer, size_t size, const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = FormatSecure(buffer, size, format, args);
    va_end(args);
    return result;
}

int APS5_VABI printf_s_nid_postfix(const char* format, ...) {
    std::va_list args;
    va_start(args, format);
    const int result = WriteSecure(&_Stdout_nid_postfix, format, reinterpret_cast<VaList*>(args));
    va_end(args);
    return result;
}

#endif

int APS5_VABI puts_nid_postfix(const char* s) {
    if (!_Stdout_nid_postfix.IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(&_Stdout_nid_postfix);
    _Stdout_nid_postfix.Orientation(-1);
    if (!_Stdout_nid_postfix.SwitchToWriting()) return EOF;
    if (std::fputs(s, native) < 0 || std::fputc('\n', native) == EOF) return EOF;
    return 0;
}

extern FileStream* __stdinp_nid_postfix;

int APS5_VABI vfscanf_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    std::string quadHead;
    format = CutAtQuadScanDirective(format, quadHead);
#ifndef _WIN32
    if (HasNarrowCharacterScan(format)) {
        LibcDetail::FormatArguments arguments(args);
        return ScanGuestStream(stream, format, [&] { return arguments.Next<void*>(); }, [] { return 0u; }, false);
    }
#endif
    if (stream && !stream->IsOpen()) { errno = 9; return -1; }
    auto* native = GetNativeStream(stream);
    stream->Orientation(-1);
    if (!stream->SwitchToReading()) return EOF;
    int result = 0;
    if (HasGuestWideScan(format)) {
        LibcDetail::FormatArguments arguments(args);
        result = ScanSource(StreamSource(native), format, false, [&] { return arguments.Next<void*>(); }, [] { return 0u; });
    } else {
#ifdef _WIN32
        result = LibcDetail::ScanFileWindows_nid_no_patch(native, format, args);
#else
        std::va_list copy;
        va_copy(copy, *reinterpret_cast<std::va_list*>(args));
        result = HostVfscanf(native, format, copy);
        va_end(copy);
#endif
    }
    stream->SyncStatus();
    stream->KeepDirectoryReadError(result == -1);
    return result;
}

int APS5_VABI vscanf_nid_postfix(const char* format, VaList* args) {
    return vfscanf_nid_postfix(__stdinp_nid_postfix, format, args);
}

int APS5_VABI scanf_nid_postfix(const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfscanf_nid_postfix(__stdinp_nid_postfix, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

int APS5_VABI vfscanf_s_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    LibcDetail::FormatArguments arguments(args);
    return ScanGuestStream(stream, format, [&] { return arguments.Next<void*>(); }, [&] { return arguments.Next<unsigned int>(); });
}

int APS5_VABI vscanf_s_nid_postfix(const char* format, VaList* args) {
    return vfscanf_s_nid_postfix(__stdinp_nid_postfix, format, args);
}

int APS5_VABI fscanf_s_nid_postfix(FileStream* stream, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfscanf_s_nid_postfix(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;
}

int APS5_VABI scanf_s_nid_postfix(const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    const int result = vfscanf_s_nid_postfix(__stdinp_nid_postfix, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
    return result;


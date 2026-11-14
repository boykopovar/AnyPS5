#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/WindowsFormatting.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#define StrCaseCmp _stricmp
#else
#define StrCaseCmp strcasecmp
#endif

struct WTFLogChannel {
    std::uint8_t state;
    const char* name;
    std::uint8_t level;
    const char* subsystem;
};

extern "C" {
[[noreturn]] void APS5_VABI WTFCrash() {
    std::fprintf(stderr, "WTFCrash\n");
    std::abort();
}

[[noreturn]] void APS5_VABI WTFCrashWithSecurityImplication() {
    std::fprintf(stderr, "WTFCrashWithSecurityImplication\n");
    std::abort();
}

[[noreturn]] void APS5_VABI WTFLogAlwaysAndCrash(const char* format, ...) {
    if (format) {
#ifdef _WIN32
        __builtin_sysv_va_list args;
        __builtin_sysv_va_start(args, format);
        LibcDetail::PrintWindows(format, args);
        __builtin_sysv_va_end(args);
#else
        std::va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        va_end(args);
#endif
        std::fprintf(stderr, "\n");
    }
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_im(int line, const char* file, const char* function, int counter, std::uint64_t reason) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", file ? file : "", line, function ? function : "", counter, static_cast<unsigned long long>(reason));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_imm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu misc1=%llu\n", file ? file : "", line, function ? function : "", counter, static_cast<unsigned long long>(reason), static_cast<unsigned long long>(misc1));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_immm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1, std::uint64_t misc2) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", file ? file : "", line, function ? function : "", counter, static_cast<unsigned long long>(reason));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_immmm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1, std::uint64_t misc2, std::uint64_t misc3) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", file ? file : "", line, function ? function : "", counter, static_cast<unsigned long long>(reason));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_immmmm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1, std::uint64_t misc2, std::uint64_t misc3, std::uint64_t misc4) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", file ? file : "", line, function ? function : "", counter, static_cast<unsigned long long>(reason));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_immmmmm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1, std::uint64_t misc2, std::uint64_t misc3, std::uint64_t misc4, std::uint64_t misc5) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", file ? file : "", line, function ? function : "", counter, static_cast<unsigned long long>(reason));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_immmmmmm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1, std::uint64_t misc2, std::uint64_t misc3, std::uint64_t misc4, std::uint64_t misc5, std::uint64_t misc6) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", file ? file : "", line, function ? function : "", counter, static_cast<unsigned long long>(reason));
    std::abort();
}

void APS5_VABI WTFReportAssertionFailure(const char* file, int line, const char* function, const char* assertion) {
    if (assertion) {
        std::fprintf(stderr, "ASSERTION FAILED: %s\n", assertion);
    } else {
        std::fprintf(stderr, "SHOULD NEVER BE REACHED\n");
    }
    std::fprintf(stderr, "(%s:%d %s)\n", file ? file : "", line, function ? function : "");
}

void APS5_VABI WTFReportAssertionFailureWithMessage(const char* file, int line, const char* function, const char* assertion, const char* format, ...) {
    std::fprintf(stderr, "ASSERTION FAILED: ");
    if (format) {
#ifdef _WIN32
        __builtin_sysv_va_list args;
        __builtin_sysv_va_start(args, format);
        LibcDetail::PrintWindows(format, args);
        __builtin_sysv_va_end(args);
#else
        std::va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        va_end(args);
#endif
    }
    std::fprintf(stderr, "\n%s\n(%s:%d %s)\n", assertion ? assertion : "", file ? file : "", line, function ? function : "");
}

void APS5_VABI WTFReportArgumentAssertionFailure(const char* file, int line, const char* function, const char* argName, const char* assertion) {
    std::fprintf(stderr, "ARGUMENT BAD: %s, %s\n(%s:%d %s)\n", argName ? argName : "", assertion ? assertion : "", file ? file : "", line, function ? function : "");
}

void APS5_VABI WTFReportFatalError(const char* file, int line, const char* function, const char* format, ...) {
    std::fprintf(stderr, "FATAL ERROR: ");
    if (format) {
#ifdef _WIN32
        __builtin_sysv_va_list args;
        __builtin_sysv_va_start(args, format);
        LibcDetail::PrintWindows(format, args);
        __builtin_sysv_va_end(args);
#else
        std::va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        va_end(args);
#endif
    }
    std::fprintf(stderr, "\n(%s:%d %s)\n", file ? file : "", line, function ? function : "");
}

void APS5_VABI WTFReportError(const char* file, int line, const char* function, const char* format, ...) {
    std::fprintf(stderr, "ERROR: ");
    if (format) {
#ifdef _WIN32
        __builtin_sysv_va_list args;
        __builtin_sysv_va_start(args, format);
        LibcDetail::PrintWindows(format, args);
        __builtin_sysv_va_end(args);
#else
        std::va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        va_end(args);
#endif
    }
    std::fprintf(stderr, "\n(%s:%d %s)\n", file ? file : "", line, function ? function : "");
}

void APS5_VABI WTFReportNotImplementedYet(const char* file, int line, const char* function) {
    std::fprintf(stderr, "NOT IMPLEMENTED YET\n(%s:%d %s)\n", file ? file : "", line, function ? function : "");
}

void APS5_VABI WTFLogAlways(const char* format, ...) {
    if (!format) {
        return;
    }
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
#endif
    std::fprintf(stderr, "\n");
}

void APS5_VABI WTFLog(void* channel, const char* format, ...) {
    if (!channel || !format) {
        return;
    }
    const auto* ch = static_cast<const WTFLogChannel*>(channel);
    if (ch->state == 0) {
        return;
    }
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
#endif
    std::fprintf(stderr, "\n");
}

void APS5_VABI WTFLogVerbose(const char* file, int line, const char* function, void* channel, const char* format, ...) {
    if (!channel || !format) {
        return;
    }
    const auto* ch = static_cast<const WTFLogChannel*>(channel);
    if (ch->state == 0) {
        return;
    }
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
#endif
    std::fprintf(stderr, "\n(%s:%d %s)\n", file ? file : "", line, function ? function : "");
}

void APS5_VABI WTFLogWithLevel(void* channel, std::uint32_t level, const char* format, ...) {
    if (!channel || !format) {
        return;
    }
    const auto* ch = static_cast<const WTFLogChannel*>(channel);
    if (ch->state == 0 || ch->level < level) {
        return;
    }
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
#endif
    std::fprintf(stderr, "\n");
}

void APS5_VABI WTFSetLogChannelLevel(void* channel, std::uint32_t level) {
    if (channel) {
        auto* ch = static_cast<WTFLogChannel*>(channel);
        ch->level = static_cast<std::uint8_t>(level);
    }
}

bool APS5_VABI WTFWillLogWithLevel(void* channel, std::uint32_t level) {
    if (!channel) {
        return false;
    }
    const auto* ch = static_cast<const WTFLogChannel*>(channel);
    return ch->state != 0 && ch->level >= level;
}

void* APS5_VABI WTFLogChannelByName(void** channels, std::size_t count, const char* name) {
    if (!channels || !name) {
        return nullptr;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (!channels[i]) {
            continue;
        }
        auto* ch = static_cast<WTFLogChannel*>(channels[i]);
        if (ch->name && StrCaseCmp(ch->name, name) == 0) {
            return ch;
        }
    }
    return nullptr;
}

void APS5_VABI WTFInitializeLogChannelStatesFromString(void** channels, std::size_t count, const char* logLevel) {
    if (!channels || !logLevel) {
        return;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (!channels[i]) {
            continue;
        }
        auto* ch = static_cast<WTFLogChannel*>(channels[i]);
        if (StrCaseCmp(logLevel, "all") == 0 || (ch->name && StrCaseCmp(logLevel, ch->name) == 0)) {
            ch->state = 1;
        }
    }
}

void APS5_VABI WTFGetBacktrace(void** stack, int* size) {
    if (!stack || !size || *size <= 0) {
        return;
    }
#ifdef _WIN32
    *size = static_cast<int>(CaptureStackBackTrace(0, static_cast<DWORD>(*size), stack, nullptr));
#else
    *size = 0;
#endif
}

void APS5_VABI WTFPrintBacktrace(void** stack, int size) {
    if (!stack || size <= 0) {
        return;
    }
    for (int i = 0; i < size; ++i) {
        std::fprintf(stderr, "%-3d %p\n", i, stack[i]);
    }
}

void APS5_VABI WTFReportBacktrace() {
    void* frames[32];
    int size = 32;
    WTFGetBacktrace(frames, &size);
    WTFPrintBacktrace(frames, size);
}

bool APS5_VABI WTFIsDebuggerAttached() {
    return false;
}

void APS5_VABI _ZN3WTF8dataLogFEPKcz(const char* format, ...) {
    if (!format) {
        return;
    }
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    LibcDetail::PrintWindows(format, args);
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
#endif
}

void APS5_VABI _ZN3WTF9dataLogFVEPKcP13__va_list_tag(const char* format, void* argList) {
    if (!format || !argList) {
        return;
    }
#ifdef _WIN32
    LibcDetail::PrintWindows(format, argList);
#else
    std::vfprintf(stderr, format, *reinterpret_cast<std::va_list*>(argList));
#endif
}

}

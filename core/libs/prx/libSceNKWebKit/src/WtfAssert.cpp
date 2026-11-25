#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/WindowsFormatting.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define StrCaseCmp _stricmp
#else
#define StrCaseCmp strcasecmp
#endif

// Derived from Source/WTF/wtf/Assertions.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2003-2023 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2011 University of Szeged. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// Derived from Source/WTF/wtf/DataLog.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2012-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifdef _WIN32
namespace {

void PrintWindowsToStandardError(const char* format, const void* args) {
    std::string buffer;
    const int size = LibcDetail::FormatWindows(nullptr, 0, format, args, &buffer);
    if (size > 0) std::fwrite(buffer.data(), 1, static_cast<std::size_t>(size), stderr);
}

}
#endif

struct WTFLogChannel {
    std::uint8_t state;
    const char* name;
    std::uint8_t level;
    const char* subsystem;
};

static_assert(sizeof(WTFLogChannel) == 32);
static_assert(offsetof(WTFLogChannel, state) == 0);
static_assert(offsetof(WTFLogChannel, name) == 8);
static_assert(offsetof(WTFLogChannel, level) == 16);
static_assert(offsetof(WTFLogChannel, subsystem) == 24);

namespace {

constexpr std::uint8_t LogChannelStateOn = 1;
constexpr std::size_t ReportTextLimit = 65536;

std::string GuestText(const char* text) {
    std::string copied;
    if (text == nullptr) return copied;
    if (CopyGuestString_nid_no_patch(text, ReportTextLimit, copied) != 0) copied.clear();
    return copied;
}

std::mutex& AccumulatorLock() {
    static auto* lock = new std::mutex;
    return *lock;
}

std::string& Accumulator() {
    static auto* text = new std::string;
    return *text;
}

#ifdef _WIN32
std::string FormatText(const char* format, const void* args) {
    std::string text;
    LibcDetail::FormatWindows(nullptr, 0, format, args, &text);
    return text;
}
#else
std::string FormatText(const char* format, std::va_list args) {
    std::va_list copy;
    va_copy(copy, args);
    const int length = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    if (length <= 0) {
        return {};
    }
    std::string text(static_cast<std::size_t>(length) + 1, '\0');
    std::vsnprintf(text.data(), text.size(), format, args);
    text.resize(static_cast<std::size_t>(length));
    return text;
}
#endif

void AccumulateAndLog(std::string text) {
    if (text.empty() || text.back() != '\n') {
        text.push_back('\n');
    }
    {
        std::lock_guard lock(AccumulatorLock());
        auto& accumulator = Accumulator();
        if (text.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) - accumulator.size()) {
            throw std::length_error("WTF accumulated logs exceed the maximum string length");
        }
        accumulator += text;
    }
    std::string utf8;
    utf8.reserve(text.size());
    for (const unsigned char character : text) {
        if (character < 0x80) {
            utf8.push_back(static_cast<char>(character));
        } else {
            utf8.push_back(static_cast<char>(0xC0 | (character >> 6)));
            utf8.push_back(static_cast<char>(0x80 | (character & 0x3F)));
        }
    }
    std::fputs(utf8.c_str(), stderr);
}

}  // namespace

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
        PrintWindowsToStandardError(format, args);
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
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", GuestText(file).c_str(), line, GuestText(function).c_str(), counter, static_cast<unsigned long long>(reason));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_imm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu misc1=%llu\n", GuestText(file).c_str(), line, GuestText(function).c_str(), counter, static_cast<unsigned long long>(reason), static_cast<unsigned long long>(misc1));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_immm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1, std::uint64_t misc2) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", GuestText(file).c_str(), line, GuestText(function).c_str(), counter, static_cast<unsigned long long>(reason));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_immmm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1, std::uint64_t misc2, std::uint64_t misc3) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", GuestText(file).c_str(), line, GuestText(function).c_str(), counter, static_cast<unsigned long long>(reason));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_immmmm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1, std::uint64_t misc2, std::uint64_t misc3, std::uint64_t misc4) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", GuestText(file).c_str(), line, GuestText(function).c_str(), counter, static_cast<unsigned long long>(reason));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_immmmmm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1, std::uint64_t misc2, std::uint64_t misc3, std::uint64_t misc4, std::uint64_t misc5) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", GuestText(file).c_str(), line, GuestText(function).c_str(), counter, static_cast<unsigned long long>(reason));
    std::abort();
}

[[noreturn]] void APS5_VABI _Z20WTFCrashWithInfoImpliPKcS0_immmmmmm(int line, const char* file, const char* function, int counter, std::uint64_t reason, std::uint64_t misc1, std::uint64_t misc2, std::uint64_t misc3, std::uint64_t misc4, std::uint64_t misc5, std::uint64_t misc6) {
    std::fprintf(stderr, "CRASH WITH INFO: %s:%d %s counter=%d reason=%llu\n", GuestText(file).c_str(), line, GuestText(function).c_str(), counter, static_cast<unsigned long long>(reason));
    std::abort();
}

void APS5_VABI WTFReportAssertionFailure(const char* file, int line, const char* function, const char* assertion) {
    if (assertion) {
        std::fprintf(stderr, "ASSERTION FAILED: %s\n", GuestText(assertion).c_str());
    } else {
        std::fprintf(stderr, "SHOULD NEVER BE REACHED\n");
    }
    std::fprintf(stderr, "(%s:%d %s)\n", GuestText(file).c_str(), line, GuestText(function).c_str());
}

void APS5_VABI WTFReportAssertionFailureWithMessage(const char* file, int line, const char* function, const char* assertion, const char* format, ...) {
    std::fprintf(stderr, "ASSERTION FAILED: ");
    if (format) {
#ifdef _WIN32
        __builtin_sysv_va_list args;
        __builtin_sysv_va_start(args, format);
        PrintWindowsToStandardError(format, args);
        __builtin_sysv_va_end(args);
#else
        std::va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        va_end(args);
#endif
    }
    std::fprintf(stderr, "\n%s\n(%s:%d %s)\n", GuestText(assertion).c_str(), GuestText(file).c_str(), line, GuestText(function).c_str());
}

void APS5_VABI WTFReportArgumentAssertionFailure(const char* file, int line, const char* function, const char* argName, const char* assertion) {
    std::fprintf(stderr, "ARGUMENT BAD: %s, %s\n(%s:%d %s)\n", GuestText(argName).c_str(), GuestText(assertion).c_str(), GuestText(file).c_str(), line, GuestText(function).c_str());
}

void APS5_VABI WTFReportFatalError(const char* file, int line, const char* function, const char* format, ...) {
    std::fprintf(stderr, "FATAL ERROR: ");
    if (format) {
#ifdef _WIN32
        __builtin_sysv_va_list args;
        __builtin_sysv_va_start(args, format);
        PrintWindowsToStandardError(format, args);
        __builtin_sysv_va_end(args);
#else
        std::va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        va_end(args);
#endif
    }
    std::fprintf(stderr, "\n(%s:%d %s)\n", GuestText(file).c_str(), line, GuestText(function).c_str());
}

void APS5_VABI WTFReportError(const char* file, int line, const char* function, const char* format, ...) {
    std::fprintf(stderr, "ERROR: ");
    if (format) {
#ifdef _WIN32
        __builtin_sysv_va_list args;
        __builtin_sysv_va_start(args, format);
        PrintWindowsToStandardError(format, args);
        __builtin_sysv_va_end(args);
#else
        std::va_list args;
        va_start(args, format);
        std::vfprintf(stderr, format, args);
        va_end(args);
#endif
    }
    std::fprintf(stderr, "\n(%s:%d %s)\n", GuestText(file).c_str(), line, GuestText(function).c_str());
}

void APS5_VABI WTFReportNotImplementedYet(const char* file, int line, const char* function) {
    std::fprintf(stderr, "NOT IMPLEMENTED YET\n(%s:%d %s)\n", GuestText(file).c_str(), line, GuestText(function).c_str());
}

void APS5_VABI WTFLogAlways(const char* format, ...) {
    if (!format) {
        return;
    }
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    PrintWindowsToStandardError(format, args);
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
    if (ch->state != LogChannelStateOn) {
#ifdef _WIN32
        __builtin_sysv_va_list args;
        __builtin_sysv_va_start(args, format);
        std::string text = FormatText(format, args);
        __builtin_sysv_va_end(args);
#else
        std::va_list args;
        va_start(args, format);
        std::string text = FormatText(format, args);
        va_end(args);
#endif
        AccumulateAndLog(std::move(text));
        return;
    }
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    PrintWindowsToStandardError(format, args);
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
    PrintWindowsToStandardError(format, args);
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
#endif
    std::fprintf(stderr, "\n(%s:%d %s)\n", GuestText(file).c_str(), line, GuestText(function).c_str());
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
    PrintWindowsToStandardError(format, args);
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
    if (!stack || !size) {
        return;
    }
    int requested = 0;
    bool copied = GuestCopyIn_nid_no_patch(&requested, size, sizeof(requested));
    if (!copied) throw std::invalid_argument("WTFGetBacktrace: unreadable size");
    if (requested <= 0) {
        return;
    }
    int captured = 0;
#ifdef _WIN32
    std::vector<void*> frames(std::min<std::size_t>(static_cast<std::size_t>(requested), std::numeric_limits<USHORT>::max()));
    captured = static_cast<int>(CaptureStackBackTrace(0, static_cast<DWORD>(frames.size()), frames.data(), nullptr));
    copied = GuestCopyOut_nid_no_patch(stack, frames.data(), static_cast<std::size_t>(captured) * sizeof(void*));
    if (!copied) throw std::invalid_argument("WTFGetBacktrace: unwritable stack");
#endif
    copied = GuestCopyOut_nid_no_patch(size, &captured, sizeof(captured));
    if (!copied) throw std::invalid_argument("WTFGetBacktrace: unwritable size");
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
    PrintWindowsToStandardError(format, args);
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
    PrintWindowsToStandardError(format, argList);
#else
    std::vfprintf(stderr, format, *reinterpret_cast<std::va_list*>(argList));
#endif
}

void APS5_VABI _ZN3WTF14dataLogFStringEPKc(const char* str) {
    _ZN3WTF8dataLogFEPKcz("%s", str);
}

void APS5_VABI _ZN3WTF20resetAccumulatedLogsEv() {
    std::lock_guard lock(AccumulatorLock());
    Accumulator().clear();
}

NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF26getAndResetAccumulatedLogsEv() {
    std::string text;
    {
        std::lock_guard lock(AccumulatorLock());
        text.swap(Accumulator());
    }
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

}

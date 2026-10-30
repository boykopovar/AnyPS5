#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string>
#ifdef _WIN32
#include "prx/libc/include/WindowsFormatting.hpp"
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {
constexpr int priorityMask = 0x07;
constexpr int facilityMask = 0x3f8;
constexpr int userFacility = 1 << 3;
constexpr int internalLogPriority = 3 | 0x02 | 0x20 | 0x01;
constexpr int logPid = 0x01;
int logMask = 0xff;
int logOptions = 0;
int logFacility = userFacility;
std::optional<std::string> logIdentity;

std::mutex& OutputMutex() {
    static std::mutex mutex;
    return mutex;
}

void Emit(int priority, const std::string& message) {
    std::fprintf(stderr, "[syslog:%d] ", priority);
    if (logIdentity) std::fputs(logIdentity->c_str(), stderr);
    if (logOptions & logPid) {
#ifdef _WIN32
        std::fprintf(stderr, "[%d]", ::_getpid());
#else
        std::fprintf(stderr, "[%d]", static_cast<int>(::getpid()));
#endif
    }
    if (logIdentity) std::fputs(": ", stderr);
    std::fputs(message.c_str(), stderr);
    if (message.empty() || message.back() != '\n') std::fputc('\n', stderr);
    std::fflush(stderr);
}

std::string ExpandErrorText(const char* format, int error) {
    std::string expanded;
    for (const char* cursor = format; *cursor; ++cursor) {
        if (cursor[0] == '%' && cursor[1] == 'm') {
            char text[128];
            strerror_r_nid_postfix(error, text, sizeof(text));

            for (const char* character = text; *character; ++character) {
                expanded += *character;
                if (*character == '%') expanded += '%';
            }
            ++cursor;
        } else if (cursor[0] == '%' && cursor[1] == '%') {
            expanded += "%%";
            ++cursor;
        } else {
            expanded += *cursor;
        }
    }
    return expanded;
}

std::string Format(const char* format, VaList* args) {
#ifdef _WIN32
    std::string buffer;
    LibcDetail::FormatWindows(nullptr, 0, format, args, &buffer);
    return buffer;
#else
    const int size = vsnprintf_nid_postfix(nullptr, 0, format, args);
    if (size <= 0) return {};
    std::string buffer(static_cast<std::size_t>(size) + 1, '\0');
    vsnprintf_nid_postfix(buffer.data(), buffer.size(), format, args);

    buffer.resize(static_cast<std::size_t>(size));
    return buffer;
#endif
}
}

extern "C" {
void APS5_VABI openlog_nid_postfix(const char* identity, int options, int facility) {
    std::lock_guard lock(OutputMutex());
    if (identity) logIdentity = identity;
    logOptions = options;
    if (facility != 0 && (facility & ~facilityMask) == 0) logFacility = facility;
}

void APS5_VABI closelog_nid_postfix() {
    std::lock_guard lock(OutputMutex());
    logIdentity.reset();
}

int APS5_VABI setlogmask_nid_postfix(int mask) {
    std::lock_guard lock(OutputMutex());
    const int previous = logMask;
    if (mask != 0) logMask = mask;
    return previous;
}

void APS5_VABI vsyslog_nid_postfix(int priority, const char* format, VaList* args) {
    const int saved = *__error_nid_postfix();
    std::lock_guard lock(OutputMutex());
    if (priority & ~(priorityMask | facilityMask)) {
        if (logMask & (1 << (internalLogPriority & priorityMask))) {
            char complaint[64];
            std::snprintf(complaint, sizeof(complaint), "syslog: unknown facility/priority: %x", priority);
            Emit(internalLogPriority, complaint);
        }
        priority &= priorityMask | facilityMask;
    }
    if (logMask & (1 << (priority & priorityMask))) {
        if ((priority & facilityMask) == 0) priority |= logFacility;
        Emit(priority, format ? Format(ExpandErrorText(format, saved).c_str(), args) : std::string());
    }
    *__error_nid_postfix() = saved;
}

void APS5_VABI syslog_nid_postfix(int priority, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    vsyslog_nid_postfix(priority, format, reinterpret_cast<VaList*>(args));

#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
}
}

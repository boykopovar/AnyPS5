#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <string>
#ifdef _WIN32
#include "prx/libc/include/WindowsFormatting.hpp"
#endif

extern "C" {
int* APS5_VABI __error_nid_postfix();
int APS5_VABI strerror_r_nid_postfix(int error, char* buffer, std::size_t length);
}

namespace {
constexpr int priorityMask = 0x07;
constexpr int facilityMask = 0x3f8;
constexpr int userFacility = 1 << 3;
constexpr int internalLogPriority = 3 | 0x02 | 0x20 | 0x01;

std::mutex& OutputMutex() {
    static std::mutex mutex;
    return mutex;
}

void Emit(int priority, const std::string& message) {
    std::lock_guard lock(OutputMutex());
    std::fprintf(stderr, "[syslog:%d] %s", priority, message.c_str());
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
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    const int size = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    if (size <= 0) return {};
    std::string buffer(static_cast<std::size_t>(size) + 1, '\0');
    va_copy(copy, *reinterpret_cast<std::va_list*>(args));
    std::vsnprintf(buffer.data(), buffer.size(), format, copy);
    va_end(copy);
    buffer.resize(static_cast<std::size_t>(size));
    return buffer;
#endif
}
}

extern "C" {
void APS5_VABI vsyslog_nid_postfix(int priority, const char* format, VaList* args) {
    const int saved = *__error_nid_postfix();
    if (priority & ~(priorityMask | facilityMask)) {
        char complaint[64];
        std::snprintf(complaint, sizeof(complaint), "syslog: unknown facility/priority: %x", priority);
        Emit(internalLogPriority, complaint);
        priority &= priorityMask | facilityMask;
    }
    if ((priority & facilityMask) == 0) priority |= userFacility;
    Emit(priority, format ? Format(ExpandErrorText(format, saved).c_str(), args) : std::string());
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

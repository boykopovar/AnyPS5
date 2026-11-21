#include "prx/libSceAvPlayer.native/include/Api.hpp"
#include "prx/libSceAvPlayer/include/Api.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include "prx/libc/include/WindowsFormatting.hpp"
#endif

static void Require(bool value) { if (!value) std::abort(); }

struct CapturedLog {
    void* userData;
    int calls;
    char message[128];
};

static CapturedLog Captured;

static int APS5_VABI CaptureLog(void* user_data, const char* format, VaList* arguments) {
    CapturedLog* log = static_cast<CapturedLog*>(user_data);
    log->userData = user_data;
    ++log->calls;
#ifdef _WIN32
    LibcDetail::FormatWindows(log->message, sizeof(log->message), format, arguments);
#else
    std::va_list copy;
    va_copy(copy, *reinterpret_cast<std::va_list*>(arguments));
    std::vsnprintf(log->message, sizeof(log->message), format, copy);
    va_end(copy);
#endif
    return 42;
}

static int APS5_VABI PrintFormatted(const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    const int result = sceAvPlayerVprintf(format, reinterpret_cast<VaList*>(arguments));
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    const int result = sceAvPlayerVprintf(format, reinterpret_cast<VaList*>(arguments));
    va_end(arguments);
#endif
    return result;
}

int main() {
    Require(sceAvPlayerSetLogCallback(nullptr, nullptr) == 0);
    Require(sceAvPlayerPrintf("fallback %d\n", 1) > 0);

    Captured = {};
    Require(sceAvPlayerSetLogCallback(reinterpret_cast<void*>(&CaptureLog), &Captured) == 0);
    Require(sceAvPlayerPrintf("callback %d", 7) == 42);
    Require(Captured.calls == 1);
    Require(Captured.userData == &Captured);
    Require(std::strcmp(Captured.message, "callback 7") == 0);

    Require(PrintFormatted("vcallback %s", "ok") == 42);
    Require(Captured.calls == 2);
    Require(Captured.userData == &Captured);
    Require(std::strcmp(Captured.message, "vcallback ok") == 0);

    Require(sceAvPlayerSetLogCallback(nullptr, nullptr) == 0);
    Require(sceAvPlayerPrintf("fallback %d\n", 2) > 0);
    Require(PrintFormatted("fallback %s\n", "ok") > 0);

    Require(sceAvPlayerPrintf(nullptr) == -1);
    Require(PrintFormatted(nullptr) == -1);
}

#include "prx/libSceAvPlayer.native/include/Api.hpp"
#include "prx/libSceAvPlayer/include/Api.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

struct CapturedLog {
    void* userData;
    int calls;
    char message[128];
};

static CapturedLog Captured;

static int CaptureLog(void* user_data, const char* format, va_list arguments) {
    CapturedLog* log = static_cast<CapturedLog*>(user_data);
    log->userData = user_data;
    ++log->calls;
    std::vsnprintf(log->message, sizeof(log->message), format, arguments);
    return 42;
}

static int PrintFormatted(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = sceAvPlayerVprintf(format, arguments);
    va_end(arguments);
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

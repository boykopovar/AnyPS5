#include "prx/libSceAvPlayer.native/include/Api.hpp"
#include <cstdarg>
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

static int PrintFormatted(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = sceAvPlayerVprintf(format, arguments);
    va_end(arguments);
    return result;
}

int main() {
    Require(sceAvPlayerPrintf("sceAvPlayerPrintf %d\n", 7) > 0);
    Require(PrintFormatted("sceAvPlayerVprintf %s\n", "ok") > 0);
    Require(sceAvPlayerPrintf(nullptr) == -1);
    Require(PrintFormatted(nullptr) == -1);
}

#include "prx/libSceNpCommon/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

int main() {
    if (sceNpGetSystemClockUsec(nullptr) != 0x80550003) {
        std::fputs("null result bad\n", stderr);
        return 1;
    }
    char version[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    std::fprintf(stderr, "before version\n");
    sceNpGetSdkVersion(version);
    std::fprintf(stderr, "SDK={%s}\n", version);
    std::fflush(stderr);
    return 0;
}

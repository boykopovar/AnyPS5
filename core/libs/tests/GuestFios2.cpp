#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceFiosFileGetStatSync(const void* attr, const char* path, void* stat);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    char statBuffer[128]{};

    int retNullPath = sceFiosFileGetStatSync(nullptr, nullptr, statBuffer);
    Require(retNullPath != 0, "sceFiosFileGetStatSync accepted null path");

    int retNullStat = sceFiosFileGetStatSync(nullptr, "/app0/test", nullptr);
    Require(retNullStat != 0, "sceFiosFileGetStatSync accepted null stat buffer pointer");

    int retBothNull = sceFiosFileGetStatSync(nullptr, nullptr, nullptr);
    Require(retBothNull != 0, "sceFiosFileGetStatSync accepted null path and null stat buffer pointer");

    std::fprintf(stderr, "GuestFios2 tests passed\n");
    return 0;
}

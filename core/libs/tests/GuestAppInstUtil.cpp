#include "prx/libSceAppInstUtil/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "AppInstUtil: %s\n", message);
        std::abort();
    }
}

}

int main() {
    Require(sceAppInstUtilInitialize() == 0, "initialization failed");

    std::int32_t exists = 0;
    Require(sceAppInstUtilAppExists("PPSA00000", &exists) == 0 && exists == 1, "the running title is not installed");
    exists = 7;
    Require(sceAppInstUtilAppExists("PPSA00001", &exists) == 0 && exists == 0, "another title is installed");
    exists = 7;
    Require(sceAppInstUtilAppExists("PPSA0000", &exists) == 0 && exists == 0, "a title id prefix matched");
    exists = 7;
    Require(sceAppInstUtilAppExists("", &exists) == 0 && exists == 0, "an empty title id matched");

    Require(sceAppInstUtilTerminate() == 0, "termination failed");
    Require(sceAppInstUtilInitialize() == 0, "initialization after termination failed");
    exists = 0;
    Require(sceAppInstUtilAppExists("PPSA00000", &exists) == 0 && exists == 1, "the running title is not installed after a restart");
    Require(sceAppInstUtilTerminate() == 0, "the second termination failed");
    std::puts("AppInstUtil tests passed");
    return 0;
}

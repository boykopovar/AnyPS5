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
    constexpr int nullArgument = static_cast<int>(0x80A30003u);
    constexpr int notInitialized = static_cast<int>(0x80A31000u);
    std::int32_t untouched = 7;
    Require(sceAppInstUtilAppExists("PPSA00000", &untouched) == notInitialized && untouched == 7, "a query before initialization was answered");
    Require(sceAppInstUtilAppExists(nullptr, &untouched) == nullArgument && untouched == 7, "a null title id was not refused before the initialization check");
    Require(sceAppInstUtilAppExists("PPSA00000", nullptr) == nullArgument, "a null output was not refused before the initialization check");

    Require(sceAppInstUtilTerminate() == 0, "an unbalanced termination failed");
    Require(sceAppInstUtilInitialize() == 0, "initialization after an unbalanced termination failed");
    Require(sceAppInstUtilAppExists("PPSA00000", &untouched) == notInitialized && untouched == 7, "one initialization undid an unbalanced termination");
    Require(sceAppInstUtilInitialize() == 0, "initialization failed");
    Require(sceAppInstUtilInitialize() == 0, "a nested initialization failed");
    Require(sceAppInstUtilTerminate() == 0, "the nested termination failed");
    Require(sceAppInstUtilAppExists(nullptr, &untouched) == nullArgument && untouched == 7, "a null title id was not refused");

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

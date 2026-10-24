#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceAppInstUtilInitialize(void);
int APS5_VABI sceAppInstUtilTerminate(void);
int APS5_VABI sceAppInstUtilAppExists(const char* titleId, std::int32_t* exists);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "AppInstUtil: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void RequireStopped() {
    std::int32_t value = 7;
    Require(Throws([&] { sceAppInstUtilAppExists("PPSA00000", &value); }), "a title was looked up before initialization");
    Require(value == 7, "a stopped service wrote a result");
    Require(Throws([] { sceAppInstUtilTerminate(); }), "a stopped service was terminated");
}

}

int main() {
    RequireStopped();
    Require(sceAppInstUtilInitialize() == 0, "initialization failed");
    Require(Throws([] { sceAppInstUtilInitialize(); }), "a second initialization was accepted");


    std::int32_t exists = 0;
    Require(sceAppInstUtilAppExists("PPSA00000", &exists) == 0 && exists == 1, "the running title is not installed");
    exists = 7;
    Require(sceAppInstUtilAppExists("PPSA00001", &exists) == 0 && exists == 0, "another title is installed");
    exists = 7;
    Require(sceAppInstUtilAppExists("PPSA0000", &exists) == 0 && exists == 0, "a title id prefix matched");
    exists = 7;
    Require(sceAppInstUtilAppExists("", &exists) == 0 && exists == 0, "an empty title id matched");
    Require(Throws([] { sceAppInstUtilAppExists(nullptr, nullptr); }), "a null title id was accepted");
    exists = 7;
    Require(Throws([&] { sceAppInstUtilAppExists(nullptr, &exists); }) && exists == 7, "a null title id wrote a result");
    Require(Throws([] { sceAppInstUtilAppExists("PPSA00000", nullptr); }), "a null result was accepted");

    Require(sceAppInstUtilTerminate() == 0, "termination failed");
    RequireStopped();
    Require(sceAppInstUtilInitialize() == 0, "initialization after termination failed");
    exists = 0;
    Require(sceAppInstUtilAppExists("PPSA00000", &exists) == 0 && exists == 1, "the running title is not installed after a restart");
    Require(sceAppInstUtilTerminate() == 0, "the second termination failed");
    std::puts("AppInstUtil tests passed");
    return 0;
}

#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "SceTypes.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#define Require(condition) \
    do { \
        if (!(condition)) { \
            std::fprintf(stderr, "%s:%d: Require failed: %s\n", __FILE__, __LINE__, #condition); \
            std::fflush(stderr); \
            std::abort(); \
        } \
    } while (false)

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;

constexpr int ErrorInvalidValue = static_cast<int>(0x80290001u);
constexpr int ErrorInvalidHandle = static_cast<int>(0x8029000Bu);
constexpr int ErrorFreshMasterState = static_cast<int>(0x8029001Cu);

void TestUnopenedHandleGates(int openHandle) {
    for (const int badHandle : {-100, -1, 0, openHandle + 1, openHandle + 2, 8, 100, 9999}) {
        if (badHandle == openHandle) continue;

        Require(sceVideoOutSetFlipMaster(badHandle, 0) == ErrorInvalidHandle);
        Require(sceVideoOutSetFlipMaster(badHandle, 1) == ErrorInvalidHandle);
        Require(sceVideoOutSetFlipMaster(badHandle, 2) == ErrorInvalidHandle);
        Require(sceVideoOutSetFlipMaster(badHandle, -1) == ErrorInvalidHandle);

        Require(sceVideoOutClearFlipMaster(badHandle) == ErrorInvalidHandle);

        if (badHandle == -1) {
            Require(sceVideoOutVrrPegToFixedRate(badHandle, 0, 0) == ErrorInvalidHandle);
            Require(sceVideoOutVrrPegToFixedRate(badHandle, 1, 0) == ErrorInvalidHandle);
            Require(sceVideoOutVrrPegToFixedRate(badHandle, 0, 1) == ErrorInvalidHandle);
        }
    }
}

void TestOpenHandleOperations(int handle) {
    for (const int validRate : {0, 1, 2}) {
        Require(sceVideoOutSetFlipRate(handle, validRate) == 0);
    }

    for (const int invalidRate : {-100, -1, 3, 4, 15, 60, 120, 1000}) {
        Require(sceVideoOutSetFlipRate(handle, invalidRate) == ErrorInvalidValue);
    }

    Require(sceVideoOutSetFlipRate(handle, 0) == 0);

    Require(sceVideoOutSetFlipMaster(handle, 0) == ErrorInvalidHandle);
    Require(sceVideoOutSetFlipMaster(handle, 1) == ErrorInvalidHandle);

    Require(sceVideoOutClearFlipMaster(handle) == ErrorFreshMasterState);

    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);

    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0);
}

void TestPostCloseGates(int handle) {
    Require(sceVideoOutSetFlipMaster(handle, 0) == ErrorInvalidHandle);
    Require(sceVideoOutSetFlipMaster(handle, 1) == ErrorInvalidHandle);
    Require(sceVideoOutSetFlipMaster(handle, 2) == ErrorInvalidHandle);

    Require(sceVideoOutClearFlipMaster(handle) == ErrorInvalidHandle);

    Require(sceVideoOutClose(handle) == ErrorInvalidHandle);
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    int handle = -1;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error&) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        return 77;
    }

    Require(handle >= 0);

    TestUnopenedHandleGates(handle);

    TestOpenHandleOperations(handle);

    Require(sceVideoOutClose(handle) == 0);

    TestPostCloseGates(handle);

    return 0;
}

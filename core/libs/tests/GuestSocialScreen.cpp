#include "prx/libSceSocialScreen/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrNotInitialized = static_cast<int>(0x81440001);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x81440002);
constexpr int kErrInvalidArg = static_cast<int>(0x81440003);

void TestLifecycle() {
    Require(sceSocialScreenTerminate() == kErrNotInitialized);
    Require(sceSocialScreenCloseSeparateMode() == kErrNotInitialized);
    Require(sceSocialScreenConfigureSeparateMode() == kErrNotInitialized);
    Require(sceSocialScreenCrashDaemon() == kErrNotInitialized);
    Require(sceSocialScreenGetIgnoreSeparateMode() == kErrNotInitialized);
    Require(sceSocialScreenOpenSeparateMode() == kErrNotInitialized);
    Require(sceSocialScreenReserveHighFrameRateVideoCaptureRequest() == kErrNotInitialized);
    Require(sceSocialScreenSetIgnoreSeparateMode() == kErrNotInitialized);
    Require(sceSocialScreenSetMode() == kErrNotInitialized);

    Require(sceSocialScreenInitialize() == 0);
    Require(sceSocialScreenInitialize() == kErrAlreadyInitialized);

    Require(sceSocialScreenTerminate() == 0);
    Require(sceSocialScreenTerminate() == kErrNotInitialized);
    Require(sceSocialScreenCloseSeparateMode() == kErrNotInitialized);

    Require(sceSocialScreenInitialize() == 0);
    Require(sceSocialScreenTerminate() == 0);
}

}

int main() {
    TestLifecycle();
    Require(sceSocialScreenInitializeSeparateModeParameter(nullptr) == kErrInvalidArg);
    Require(sceSocialScreenInitializeSeparateModeParameter(reinterpret_cast<void*>(0x10)) == kErrInvalidArg);
    std::array<std::uint8_t, 0x40> parameter{};
    parameter.fill(0xaa);
    Require(sceSocialScreenInitializeSeparateModeParameter(parameter.data()) == 0);
    Require(parameter[0] == 1 && parameter[1] == 0 && parameter[2] == 0 && parameter[3] == 0);
    for (std::size_t i = 4; i < 0x20; ++i) Require(parameter[i] == 0);
    for (std::size_t i = 0x20; i < parameter.size(); ++i) Require(parameter[i] == 0xaa);
    return 0;
}

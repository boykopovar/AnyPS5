#include "prx/libSceSocialScreen/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrNotInit = static_cast<int>(0x81440001);
constexpr int kErrAlreadyInit = static_cast<int>(0x81440002);
constexpr int kErrInvalidArg = static_cast<int>(0x81440003);

void CheckLifecycle() {
    Require(sceSocialScreenTerminate() == kErrNotInit);
    Require(sceSocialScreenCloseSeparateMode() == kErrNotInit);
    Require(sceSocialScreenConfigureSeparateMode() == kErrNotInit);
    Require(sceSocialScreenGetIgnoreSeparateMode() == kErrNotInit);
    Require(sceSocialScreenOpenSeparateMode() == kErrNotInit);
    Require(sceSocialScreenReserveHighFrameRateVideoCaptureRequest() == kErrNotInit);
    Require(sceSocialScreenSetIgnoreSeparateMode() == kErrNotInit);
    Require(sceSocialScreenSetMode() == kErrNotInit);

    Require(sceSocialScreenInitialize() == 0);
    Require(sceSocialScreenInitialize() == kErrAlreadyInit);

    Require(sceSocialScreenTerminate() == 0);
    Require(sceSocialScreenTerminate() == kErrNotInit);
}

}

int main() {
    CheckLifecycle();
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

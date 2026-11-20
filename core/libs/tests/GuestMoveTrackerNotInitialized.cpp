#include "prx/libSceMoveTracker/libSceMoveTracker.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int NotInitialized = static_cast<int>(0x80ef0001);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

} // namespace

int main() {
    const float direction[3] = {0.0f, 0.0f, 1.0f};
    Require(sceMoveTrackerCalibrateReset() == NotInitialized, "CalibrateReset answers 0x80ef0001 before sceMoveTrackerInit");
    Require(sceMoveTrackerCalibrateYaw(0, direction) == NotInitialized, "CalibrateYaw answers 0x80ef0001 before sceMoveTrackerInit");
    Require(sceMoveTrackerCalibrateYaw(-1, nullptr) == NotInitialized, "CalibrateYaw answers 0x80ef0001 for any argument");
    Require(sceMoveTrackerCameraUpdate(nullptr) == NotInitialized, "CameraUpdate answers 0x80ef0001 before sceMoveTrackerInit");
    Require(sceMoveTrackerControllersUpdate(nullptr) == NotInitialized, "ControllersUpdate answers 0x80ef0001 before sceMoveTrackerInit");
    Require(sceMoveTrackerGetState(0, 0, nullptr) == NotInitialized, "GetState answers 0x80ef0001 before sceMoveTrackerInit");
    Require(sceMoveTrackerGetState(3, ~std::uint64_t{0}, nullptr) == NotInitialized, "GetState answers 0x80ef0001 for any argument");
    Require(sceMoveTrackerTerm() == NotInitialized, "Term answers 0x80ef0001 before sceMoveTrackerInit");
    Require(sceMoveTrackerTerm() == NotInitialized, "a second Term answers 0x80ef0001");
    return 0;
}

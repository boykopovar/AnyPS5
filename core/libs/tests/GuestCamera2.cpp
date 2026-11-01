#include "prx/libSceCamera/libSceCamera.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

constexpr int ErrorParam = static_cast<int>(0x8A760000);
constexpr int ErrorCameraOpen = static_cast<int>(0x8A760015);

using NoArgumentFn = int (APS5_VABI*)();
using HandleFn = int (APS5_VABI*)(std::int32_t);
using OpenFn = int (APS5_VABI*)(std::int32_t, std::int32_t, std::int32_t, void*);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "Camera2: %s\n", message);
    std::abort();
}

template <typename Function, typename Export>
Function As(Export function) {
    return reinterpret_cast<Function>(function);
}

}

int main() {
    const auto original = std::filesystem::current_path();
    const auto directory = original / ("camera2-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory), "the test directory could not be created");
    std::filesystem::current_path(directory);
    std::filesystem::create_directories("app0/sce_sys");
    {
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}}})";
        Require(static_cast<bool>(param), "param.json could not be written");
    }

    const auto initialize = As<NoArgumentFn>(&sceCamera2Initialize);
    const auto finalize = As<NoArgumentFn>(&sceCamera2Finalize);
    const auto open = As<OpenFn>(&sceCamera2Open);
    const auto isAttached = As<HandleFn>(&sceCamera2IsAttached);
    const HandleFn handleCalls[] = {
        As<HandleFn>(&sceCamera2Close), As<HandleFn>(&sceCamera2GetAttribute), As<HandleFn>(&sceCamera2SetAttribute),
        As<HandleFn>(&sceCamera2GetAutoExposureGain), As<HandleFn>(&sceCamera2SetAutoExposureGain), As<HandleFn>(&sceCamera2GetAutoWhiteBalance),
        As<HandleFn>(&sceCamera2SetAutoWhiteBalance), As<HandleFn>(&sceCamera2GetConfig), As<HandleFn>(&sceCamera2SetConfig),
        As<HandleFn>(&sceCamera2GetContrast), As<HandleFn>(&sceCamera2SetContrast), As<HandleFn>(&sceCamera2GetExposureGain),
        As<HandleFn>(&sceCamera2SetExposureGain), As<HandleFn>(&sceCamera2GetGamma), As<HandleFn>(&sceCamera2SetGamma),
        As<HandleFn>(&sceCamera2GetHue), As<HandleFn>(&sceCamera2SetHue), As<HandleFn>(&sceCamera2GetSaturation),
        As<HandleFn>(&sceCamera2SetSaturation), As<HandleFn>(&sceCamera2GetSharpness), As<HandleFn>(&sceCamera2SetSharpness),
        As<HandleFn>(&sceCamera2GetWhiteBalance), As<HandleFn>(&sceCamera2SetWhiteBalance), As<HandleFn>(&sceCamera2GetFieldOfView),
        As<HandleFn>(&sceCamera2GetFrameData), As<HandleFn>(&sceCamera2IsValidFrameData), As<HandleFn>(&sceCamera2SetVideoSync),
        As<HandleFn>(&sceCamera2Start), As<HandleFn>(&sceCamera2Stop),
    };

    for (const std::int32_t handle : {0, 1, 2, -1}) {
        for (const auto call : handleCalls) Require(call(handle) == ErrorParam, "a handle that sceCamera2Open never returned was accepted");
    }
    Require(open(0xFE, 0, 0, nullptr) == ErrorParam, "sceCamera2Open took a user other than the system user");
    Require(open(0xFF, 1, 0, nullptr) == ErrorParam && open(0xFF, 0, 1, nullptr) == ErrorParam, "sceCamera2Open took a nonzero type or index");
    Require(isAttached(0) == ErrorParam && isAttached(2) == ErrorParam, "sceCamera2IsAttached took a device other than 1");

    Require(initialize() == 0 && initialize() == 0, "sceCamera2Initialize failed");
    for (const auto call : handleCalls) Require(call(1) == ErrorParam, "an initialized Camera2 accepted a handle it never opened");
    Require(finalize() == 0, "sceCamera2Finalize failed with no camera open");

    const int camera = sceCameraOpen(0xFF, 0, 0, nullptr);
    Require(camera > 0, "sceCameraOpen failed");
    Require(finalize() == ErrorCameraOpen, "sceCamera2Finalize ran while a camera was open");
    Require(sceCameraClose(camera) == 0, "sceCameraClose failed");
    Require(finalize() == 0, "sceCamera2Finalize failed after the camera closed");

    std::filesystem::current_path(original);
    std::filesystem::remove_all(directory);
    std::puts("Camera2 tests passed");
    return 0;
}

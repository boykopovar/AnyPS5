#include "prx/libSceDepth2/libSceDepth2.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int invalidValue = static_cast<int>(0x80BB0000u);
constexpr int nonZeroArgument = static_cast<int>(0x80BB0004u);
constexpr int invalidHandle = static_cast<int>(0x80BB0005u);
constexpr int nullPointer = static_cast<int>(0x80BB000Au);

}

int main() {
    std::array<std::uint8_t, 64> buffer{};
    for (const std::int32_t handle : std::array<std::int32_t, 7>{-2147483647 - 1, -1, 0, 1, 3, 4, 2147483647}) {
        Require(sceDepth2Submit(handle) == invalidHandle);
        Require(sceDepth2SaveInternalBuffers(handle) == invalidHandle);
        Require(sceDepth2SetCameraFactoryCalibrationData(handle) == invalidHandle);
        Require(sceDepth2WaitAndExecutePostProcess(handle) == invalidHandle);
        Require(sceDepth2GetImage(handle, 0, buffer.data(), 0) == invalidHandle);
        Require(sceDepth2GetImage(handle, 1, nullptr, 0) == nonZeroArgument);
        Require(sceDepth2GetImage(handle, 0, nullptr, 0) == nullPointer);
        Require(libSceDepth2Unknown_tLM3AwtzaHs_nid_no_patch(handle, buffer.data(), 0) == invalidHandle);
        Require(libSceDepth2Unknown_tLM3AwtzaHs_nid_no_patch(handle, nullptr, -1) == nullPointer);
        Require(libSceDepth2Unknown_tLM3AwtzaHs_nid_no_patch(handle, buffer.data(), -1) == invalidValue);
    }
    return 0;
}

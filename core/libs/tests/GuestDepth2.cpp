#include "prx/libSceDepth2/libSceDepth2.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int invalidValue = static_cast<int>(0x80BB0000u);
constexpr int invalidOutputProperty = static_cast<int>(0x80BB0002u);
constexpr int invalidInputProperty = static_cast<int>(0x80BB0003u);
constexpr int nonZeroArgument = static_cast<int>(0x80BB0004u);
constexpr int invalidHandle = static_cast<int>(0x80BB0005u);
constexpr int invalidSize = static_cast<int>(0x80BB0008u);
constexpr int reservedNotEmpty = static_cast<int>(0x80BB0009u);
constexpr int nullPointer = static_cast<int>(0x80BB000Au);
constexpr int invalidMode = static_cast<int>(0x80BB000Bu);
constexpr int invalidInputResolutionMode1 = static_cast<int>(0x80BB000Cu);
constexpr int invalidType = static_cast<int>(0x80BB0012u);

void TestQueryMemory() {
    SceDepth2Memory mem{};
    Require(sceDepth2QueryMemory(nullptr, &mem) == nullPointer);

    SceDepth2Param param{};
    Require(sceDepth2QueryMemory(&param, nullptr) == nullPointer);

    param.size = 0x30;
    Require(sceDepth2QueryMemory(&param, &mem) == invalidSize);

    param.size = 0x40;
    param.type = 1;
    Require(sceDepth2QueryMemory(&param, &mem) == invalidType);

    param.type = 0;
    param.mode = 2;
    Require(sceDepth2QueryMemory(&param, &mem) == invalidMode);

    param.mode = 0;
    param.inputWidth = 100;
    param.inputHeight = 100;
    Require(sceDepth2QueryMemory(&param, &mem) == invalidMode);

    param.inputWidth = 1280;
    param.inputHeight = 800;
    Require(sceDepth2QueryMemory(&param, &mem) == invalidMode);

    param.mode = 1;
    Require(sceDepth2QueryMemory(&param, &mem) == invalidInputResolutionMode1);

    param.mode = 0;
    param.type = 3;
    param.inputWidth = 1280;
    param.inputHeight = 800;
    param.unknown0c = 2;
    Require(sceDepth2QueryMemory(&param, &mem) == invalidInputProperty);

    param.unknown0c = 0;
    param.outputWidth = 100;
    param.outputHeight = 100;
    Require(sceDepth2QueryMemory(&param, &mem) == invalidMode);

    param.outputWidth = 320;
    param.outputHeight = 200;
    param.unknown18 = 2;
    Require(sceDepth2QueryMemory(&param, &mem) == invalidOutputProperty);

    param.unknown18 = 1;
    param.reserved20 = 1;
    Require(sceDepth2QueryMemory(&param, &mem) == reservedNotEmpty);

    param.reserved20 = 0;
    param.reserved24 = 1;
    Require(sceDepth2QueryMemory(&param, &mem) == reservedNotEmpty);

    param.reserved24 = 0;
    param.unknown30 = 7;
    Require(sceDepth2QueryMemory(&param, &mem) == invalidValue);

    param.unknown30 = 6;
    param.unknown34 = 8;
    Require(sceDepth2QueryMemory(&param, &mem) == invalidValue);

    param.unknown34 = 0;
    mem.size = 0xdeadbeef;
    mem.alignment = 0xdeadbeef;
    Require(sceDepth2QueryMemory(&param, &mem) == 0);
    Require(mem.size == 0x8000000);
    Require(mem.alignment == 0x40000);

    param.type = 0;
    param.inputWidth = 320;
    param.inputHeight = 200;
    param.outputWidth = 320;
    param.outputHeight = 200;
    Require(sceDepth2QueryMemory(&param, &mem) == 0);
    Require(mem.size == 0xc00000);
    Require(mem.alignment == 0x40000);

    param.type = 2;
    param.inputWidth = 640;
    param.inputHeight = 400;
    param.outputWidth = 640;
    param.outputHeight = 400;
    Require(sceDepth2QueryMemory(&param, &mem) == 0);
    Require(mem.size == 0x2000000);
    Require(mem.alignment == 0x40000);

    param.mode = 1;
    param.type = 0;
    param.inputWidth = 480;
    param.inputHeight = 270;
    param.outputWidth = 480;
    param.outputHeight = 270;
    Require(sceDepth2QueryMemory(&param, &mem) == 0);
    Require(mem.size == 0xc00000);
    Require(mem.alignment == 0x40000);

    param.type = 2;
    param.inputWidth = 960;
    param.inputHeight = 540;
    param.outputWidth = 960;
    param.outputHeight = 540;
    Require(sceDepth2QueryMemory(&param, &mem) == 0);
    Require(mem.size == 0x2000000);
    Require(mem.alignment == 0x40000);

    param.type = 4;
    param.inputWidth = 1920;
    param.inputHeight = 1080;
    param.outputWidth = 1920;
    param.outputHeight = 1080;
    Require(sceDepth2QueryMemory(&param, &mem) == 0);
    Require(mem.size == 0x8000000);
    Require(mem.alignment == 0x40000);
}

}

int main() {
    TestQueryMemory();
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

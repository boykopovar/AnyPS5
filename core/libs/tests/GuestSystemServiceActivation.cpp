#include "prx/libSceSystemService/include/SystemService.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr int invalidArgument = static_cast<int>(0x80A10003u);
constexpr int shellInvalidArgument = static_cast<int>(0x80E30005u);
constexpr std::uintptr_t untouchedHandle = 0x5A5A5A5A5A5A5A5Au;
constexpr std::uint32_t untouchedStatus = 0x5A5A5A5Au;
constexpr std::int32_t untouchedResult = 0x5A5A5A5A;
constexpr std::uint8_t untouchedFlag = 0x5A;

void Require(bool value) { if (!value) std::abort(); }

struct Codec {
    int (APS5_VABI* init)(std::uintptr_t*);
    int (APS5_VABI* isActivated)(std::uint8_t*);
    int (APS5_VABI* start)(std::uintptr_t);
    int (APS5_VABI* getStatus)(std::uintptr_t, std::uint32_t*, std::int32_t*);
    int (APS5_VABI* abort)(std::uintptr_t);
    int (APS5_VABI* term)(std::uintptr_t);
};

void TestShellCalls(std::intptr_t expectedHandle) {
    Require(sceShellCoreUtilActivateInit(7, nullptr) == shellInvalidArgument);
    std::intptr_t handle = 0;
    Require(sceShellCoreUtilActivateInit(7, &handle) == 0 && handle == expectedHandle);
    std::int32_t status = static_cast<std::int32_t>(untouchedStatus);
    std::int32_t result = untouchedResult;
    Require(sceShellCoreUtilActivateGetStatus(0, nullptr, &result) == shellInvalidArgument && result == untouchedResult);
    Require(sceShellCoreUtilActivateGetStatus(0, &status, nullptr) == shellInvalidArgument && status == static_cast<std::int32_t>(untouchedStatus));
    Require(sceShellCoreUtilActivateGetStatus(0, &status, &result) == 0 && status == 2 && result == 0);
    Require(sceShellCoreUtilActivateIsActivated(7, nullptr) == shellInvalidArgument);
    bool activated = false;
    Require(sceShellCoreUtilActivateIsActivated(-1, &activated) == 0 && activated);
    for (std::intptr_t value : {std::intptr_t{0}, handle, std::intptr_t{-1}}) {
        Require(sceShellCoreUtilActivateStart(value) == 0);
        Require(sceShellCoreUtilActivateStartAsync(value) == 0);
        Require(sceShellCoreUtilActivateAbort(value) == 0);
        Require(sceShellCoreUtilActivateTerm(value) == 0);
    }
    Require(sceShellCoreUtilActivateRecordActivation() == 0);
}

void TestCodec(const Codec& codec, std::uintptr_t expectedHandle) {
    Require(codec.init(nullptr) == invalidArgument);
    std::uintptr_t handle = untouchedHandle;
    Require(codec.init(&handle) == 0 && handle == expectedHandle);
    Require(codec.isActivated(nullptr) == invalidArgument);
    std::uint8_t activated = untouchedFlag;
    Require(codec.isActivated(&activated) == 0 && activated == 1);
    std::uint32_t status = untouchedStatus;
    std::int32_t result = untouchedResult;
    Require(codec.getStatus(0, &status, &result) == invalidArgument);
    Require(codec.getStatus(handle, nullptr, &result) == invalidArgument);
    Require(codec.getStatus(handle, &status, nullptr) == invalidArgument);
    Require(status == untouchedStatus && result == untouchedResult);
    for (auto call : {codec.start, codec.abort, codec.term}) {
        Require(call(0) == invalidArgument);
        Require(call(handle) == 0);
        Require(call(untouchedHandle) == 0);
    }
    Require(codec.getStatus(handle, &status, &result) == 0 && status == 2 && result == 0);
    Require(codec.getStatus(untouchedHandle, &status, &result) == 0 && status == 2 && result == 0);
}

}

int main() {
    TestCodec({sceSystemServiceActivateHevcInit, sceSystemServiceActivateHevcIsActivated, sceSystemServiceActivateHevcStart,
               sceSystemServiceActivateHevcGetStatus, sceSystemServiceActivateHevcAbort, sceSystemServiceActivateHevcTerm}, 0x1000);
    TestCodec({sceSystemServiceActivateHevcSoftInit, sceSystemServiceActivateHevcSoftIsActivated, sceSystemServiceActivateHevcSoftStart,
               sceSystemServiceActivateHevcSoftGetStatus, sceSystemServiceActivateHevcSoftAbort, sceSystemServiceActivateHevcSoftTerm}, 0x1004);
    TestCodec({sceSystemServiceActivateMpeg2Init, sceSystemServiceActivateMpeg2IsActivated, sceSystemServiceActivateMpeg2Start,
               sceSystemServiceActivateMpeg2GetStatus, sceSystemServiceActivateMpeg2Abort, sceSystemServiceActivateMpeg2Term}, 0x1008);
    TestShellCalls(0x100C);
    Require(sceSystemServiceActivateHevc() == 0);
    Require(sceSystemServiceActivateHevcSoft() == 0);
    return 0;
}

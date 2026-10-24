#include "ProcessParametersFixture.hpp"
#include "prx/libSceHmd/Hmd.hpp"
#include "prx/libSceHmd2/Hmd2.hpp"
#include "prx/libSceUserService/UserService.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace Hmd2 {
struct ReprojectionOutput;
}

extern "C" {
std::int32_t APS5_VABI Hmd2WriteStateTwo(void* a0);
std::int32_t APS5_VABI Hmd2WriteSize0x2000Align0x100(std::uint64_t* a0, std::uint64_t* a1);
std::int32_t APS5_VABI Hmd2WriteFourConstants(void* a0);
std::int32_t APS5_VABI Hmd2WriteSixHalvesThenInvalidHandle(std::uint64_t a0, void* a1);
std::int32_t APS5_VABI Hmd2WriteTwelveHalvesThenInvalidHandle(std::uint64_t a0, void* a1);
std::int32_t APS5_VABI Hmd2SlotUpTo3CheckSecondThirdThenZero(std::uint64_t a0, const void* a1, const void* a2);
std::int32_t APS5_VABI Hmd2SlotUpTo1ThenError1002(std::uint64_t a0);
std::int32_t APS5_VABI Hmd2SlotUpTo1CheckSecondThenError1002(std::uint64_t a0, const void* a1);
std::int32_t APS5_VABI Hmd2SlotUpTo1ThenMinusOne(std::uint64_t a0);
std::int32_t APS5_VABI Hmd2SlotUpTo2ThenMinusOne(std::uint64_t a0);
std::int32_t APS5_VABI Hmd2SlotUpTo2ThenError1002(std::uint64_t a0);
std::int32_t APS5_VABI Hmd2SlotUpTo2ThenError1003(std::uint64_t a0);
std::int32_t APS5_VABI Hmd2SlotUpTo2CheckSecondThenError002C(std::uint64_t a0, const void* a1);
std::int32_t APS5_VABI Hmd2IndexZeroCheckSecondThenZero(std::uint64_t a0, const void* a1);
std::int32_t APS5_VABI Hmd2CheckFirstThenError0004(const void*);
std::int32_t APS5_VABI Hmd2CheckFirstThenZero(const void*);
std::int32_t APS5_VABI Hmd2CheckFirstThenError1002(const void*);
std::int32_t APS5_VABI Hmd2CheckFirstThenMinusOne(const void*);
std::int32_t APS5_VABI Hmd2CheckFirstTwoThenParameterInvalid(const void*, const void*);
std::int32_t APS5_VABI Hmd2CheckSecondThenError002C(const void*, const void*);
std::int32_t APS5_VABI Hmd2CheckFirstThenParameterInvalid(const void*);
std::int32_t APS5_VABI Hmd2CheckAllSixThenZero(const void*, const void*, const void*, const void*, const void*, const void*);
std::int32_t APS5_VABI Hmd2CheckSecondThenParameterInvalid(const void*, const void*);
std::int32_t APS5_VABI Hmd2CheckFirstThenError800d0203(const void*);
std::int32_t APS5_VABI Hmd2CheckFirstThenError1003(const void*);
std::int32_t APS5_VABI Hmd2CheckFirstThenError1010(const void*);
std::int32_t APS5_VABI Hmd2CheckFirstTwoThenError1002(const void*, const void*);
std::int32_t APS5_VABI Hmd2CheckFirstTwoThenError1003(const void*, const void*);
std::int32_t APS5_VABI Hmd2CheckFirstAndThirdThenZero(const void*, const void*, const void*);
std::int32_t APS5_VABI Hmd2CheckFirstAndThirdThenError1003(const void*, const void*, const void*);
std::int32_t APS5_VABI Hmd2CheckSecondThenInvalidHandle(const void*, const void*);
std::int32_t APS5_VABI Hmd2CheckSecondThenError1002(const void*, const void*);
std::int32_t APS5_VABI Hmd2BeforeInitializeZero();
std::int32_t APS5_VABI Hmd2BeforeInitializeNotInitialized();
std::int32_t APS5_VABI Hmd2BeforeInitializeError0004();
std::int32_t APS5_VABI Hmd2BeforeInitializeParameterNull();
std::int32_t APS5_VABI Hmd2BeforeInitializeParameterInvalid();
std::int32_t APS5_VABI Hmd2BeforeInitializeReprojectionNotInitialized();
std::int32_t APS5_VABI Hmd2BeforeInitializeError002C();
std::int32_t APS5_VABI Hmd2BeforeInitializeError1002();
std::int32_t APS5_VABI Hmd2BeforeInitializeError1003();
std::int32_t APS5_VABI Hmd2BeforeInitializeMinusOne();
std::int32_t APS5_VABI Hmd2BeforeInitializeValue0400();
std::int32_t APS5_VABI Hmd2BeforeInitializeValue20000();
int APS5_VABI sceHmd2GazeActivateGazeTracking(std::int32_t mode);
int APS5_VABI sceHmd2ReprojectionGetCurrentOutput(Hmd2::ReprojectionOutput* output);
int APS5_VABI sceHmd2SetFieldOfView(const Hmd2::FieldOfView* fieldOfView);
}

namespace {

alignas(16) std::array<std::byte, 0x40> process{};

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "HMD2: %s\n", message);
        std::abort();
    }
}

void InstallProcessParameters() {
    const std::uint64_t size = process.size();
    const std::uint32_t magic = 0x4942524f;
    std::memcpy(process.data(), &size, sizeof(size));
    std::memcpy(process.data() + 8, &magic, sizeof(magic));
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "process parameters were not installed");
}

void TestBeforeInitialize() {
    Hmd2::DeviceInformation info;
    std::memset(&info, 0xcc, sizeof(info));
    Require(sceHmd2GetDeviceInformation(&info) == Hmd2::NotInitialized, "device information before initialization");
    const Hmd2::DeviceInformation cleared{};
    Require(std::memcmp(&info, &cleared, sizeof(info)) == 0, "device information was not cleared before the initialization check");
    Require(sceHmd2GetDeviceInformation(nullptr) == Hmd2::ParameterNull, "null device information before initialization");
    Hmd2::FieldOfView fieldOfView{};
    Require(sceHmd2GetFieldOfViewWithoutHandle(&fieldOfView) == Hmd2::NotInitialized, "field of view before initialization");
    Require(sceHmd2GetFieldOfView(0, &fieldOfView) == Hmd2::InvalidHandle, "field of view of handle 0 before initialization");
    Require(sceHmd2GetDeviceInformationByHandle(0, &info) == Hmd2::NotInitialized, "device information of handle 0 before initialization");
    Require(sceHmd2Open(USER_SERVICE_INITIAL_USER_ID, 0, 0, nullptr) == Hmd2::NotInitialized, "open before initialization");
    Require(sceHmd2Close(0) == Hmd2::NotInitialized, "close before initialization");
    Require(sceHmd2Terminate() == Hmd2::NotInitialized, "terminate before initialization");
    {
        std::uint8_t out[64];
        std::uint64_t words[2];
        Require(Hmd2WriteStateTwo(nullptr) == Hmd2::ParameterNull, "Hmd2WriteStateTwo null before initialization");
        std::memset(out, 0xaa, sizeof out);
        Require(Hmd2WriteStateTwo(out) == 0 && out[0] == 2 && out[1] == 0 && out[4] == 0 && out[5] == 0xaa, "Hmd2WriteStateTwo bytes before initialization");
        Require(Hmd2WriteSize0x2000Align0x100(nullptr, words) == Hmd2::ParameterNull && Hmd2WriteSize0x2000Align0x100(words, nullptr) == Hmd2::ParameterNull, "Hmd2WriteSize0x2000Align0x100 null before initialization");
        words[0] = words[1] = ~0ull;
        Require(Hmd2WriteSize0x2000Align0x100(words, words + 1) == 0 && words[0] == 0x2000 && words[1] == 0x100, "Hmd2WriteSize0x2000Align0x100 values before initialization");
        Require(Hmd2WriteFourConstants(nullptr) == Hmd2::ParameterNull, "Hmd2WriteFourConstants null before initialization");
        std::memset(out, 0xaa, sizeof out);
        Require(Hmd2WriteFourConstants(out) == 0 && out[0] == 0xe1 && out[15] == 0x3c && out[16] == 0xaa, "Hmd2WriteFourConstants bytes before initialization");
        Require(Hmd2WriteSixHalvesThenInvalidHandle(0, nullptr) == Hmd2::ParameterNull, "Hmd2WriteSixHalvesThenInvalidHandle null before initialization");
        std::memset(out, 0xaa, sizeof out);
        Require(Hmd2WriteSixHalvesThenInvalidHandle(0, out) == Hmd2::InvalidHandle && out[7] == 0x3f && out[20] == 0x00 && out[23] == 0x3f && out[24] == 0xaa, "Hmd2WriteSixHalvesThenInvalidHandle bytes before initialization");
        Require(Hmd2WriteTwelveHalvesThenInvalidHandle(0, nullptr) == Hmd2::ParameterNull, "Hmd2WriteTwelveHalvesThenInvalidHandle null before initialization");
        std::memset(out, 0xaa, sizeof out);
        Require(Hmd2WriteTwelveHalvesThenInvalidHandle(0, out) == Hmd2::InvalidHandle && out[7] == 0x3f && out[31] == 0x3f && out[47] == 0x00 && out[48] == 0xaa, "Hmd2WriteTwelveHalvesThenInvalidHandle bytes before initialization");
        Require(Hmd2SlotUpTo3CheckSecondThirdThenZero(9, nullptr, out) == Hmd2::ParameterNull && Hmd2SlotUpTo3CheckSecondThirdThenZero(0, out, nullptr) == Hmd2::ParameterNull, "Hmd2SlotUpTo3CheckSecondThirdThenZero null before initialization");
        Require(Hmd2SlotUpTo3CheckSecondThirdThenZero(3, out, out) == 0 && Hmd2SlotUpTo3CheckSecondThirdThenZero(4, out, out) == Hmd2::ParameterInvalid && Hmd2SlotUpTo3CheckSecondThirdThenZero(~0ull, out, out) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo3CheckSecondThirdThenZero slots before initialization");
        Require(Hmd2SlotUpTo1ThenError1002(1) == Hmd2::Error1002 && Hmd2SlotUpTo1ThenError1002(2) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo1ThenError1002 before initialization");
        Require(Hmd2SlotUpTo1CheckSecondThenError1002(5, nullptr) == Hmd2::ParameterNull && Hmd2SlotUpTo1CheckSecondThenError1002(0, out) == Hmd2::Error1002 && Hmd2SlotUpTo1CheckSecondThenError1002(2, out) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo1CheckSecondThenError1002 before initialization");
        Require(Hmd2SlotUpTo1ThenMinusOne(1) == -1 && Hmd2SlotUpTo1ThenMinusOne(2) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo1ThenMinusOne before initialization");
        Require(Hmd2SlotUpTo2ThenMinusOne(2) == -1 && Hmd2SlotUpTo2ThenMinusOne(3) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo2ThenMinusOne before initialization");
        Require(Hmd2SlotUpTo2ThenError1002(2) == Hmd2::Error1002 && Hmd2SlotUpTo2ThenError1002(3) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo2ThenError1002 before initialization");
        Require(Hmd2SlotUpTo2ThenError1003(0) == Hmd2::Error1003 && Hmd2SlotUpTo2ThenError1003(16) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo2ThenError1003 before initialization");
        Require(Hmd2SlotUpTo2CheckSecondThenError002C(8, nullptr) == Hmd2::ParameterNull && Hmd2SlotUpTo2CheckSecondThenError002C(2, out) == Hmd2::Error002C && Hmd2SlotUpTo2CheckSecondThenError002C(3, out) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo2CheckSecondThenError002C before initialization");
        Require(Hmd2IndexZeroCheckSecondThenZero(1, nullptr) == Hmd2::ParameterNull && Hmd2IndexZeroCheckSecondThenZero(0, out) == 0 && Hmd2IndexZeroCheckSecondThenZero(1, out) == Hmd2::Error100F, "Hmd2IndexZeroCheckSecondThenZero before initialization");
    }
    const std::uint8_t dummyBytes[16]{};
    const void* dummy = dummyBytes;
    Require(Hmd2CheckFirstThenError0004(dummy) == Hmd2::Error0004, "Hmd2CheckFirstThenError0004 with every pointer present before initialization");
    Require(Hmd2CheckFirstThenError0004(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenError0004 with pointer 0 null before initialization");
    Require(Hmd2CheckFirstThenZero(dummy) == 0, "Hmd2CheckFirstThenZero with every pointer present before initialization");
    Require(Hmd2CheckFirstThenZero(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenZero with pointer 0 null before initialization");
    Require(Hmd2CheckFirstThenError1002(dummy) == Hmd2::Error1002, "Hmd2CheckFirstThenError1002 with every pointer present before initialization");
    Require(Hmd2CheckFirstThenError1002(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenError1002 with pointer 0 null before initialization");
    Require(Hmd2CheckFirstThenMinusOne(dummy) == -1, "Hmd2CheckFirstThenMinusOne with every pointer present before initialization");
    Require(Hmd2CheckFirstThenMinusOne(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenMinusOne with pointer 0 null before initialization");
    Require(Hmd2CheckFirstTwoThenParameterInvalid(dummy, dummy) == Hmd2::ParameterInvalid, "Hmd2CheckFirstTwoThenParameterInvalid with every pointer present before initialization");
    Require(Hmd2CheckFirstTwoThenParameterInvalid(nullptr, dummy) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenParameterInvalid with pointer 0 null before initialization");
    Require(Hmd2CheckFirstTwoThenParameterInvalid(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenParameterInvalid with pointer 1 null before initialization");
    Require(Hmd2CheckSecondThenError002C(dummy, dummy) == Hmd2::Error002C, "Hmd2CheckSecondThenError002C with every pointer present before initialization");
    Require(Hmd2CheckSecondThenError002C(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckSecondThenError002C with pointer 1 null before initialization");
    Require(Hmd2CheckFirstThenParameterInvalid(dummy) == Hmd2::ParameterInvalid, "Hmd2CheckFirstThenParameterInvalid with every pointer present before initialization");
    Require(Hmd2CheckFirstThenParameterInvalid(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenParameterInvalid with pointer 0 null before initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, dummy, dummy, dummy, dummy, dummy) == 0, "Hmd2CheckAllSixThenZero with every pointer present before initialization");
    Require(Hmd2CheckAllSixThenZero(nullptr, dummy, dummy, dummy, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 0 null before initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, nullptr, dummy, dummy, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 1 null before initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, dummy, nullptr, dummy, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 2 null before initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, dummy, dummy, nullptr, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 3 null before initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, dummy, dummy, dummy, nullptr, dummy) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 4 null before initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, dummy, dummy, dummy, dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 5 null before initialization");
    Require(Hmd2CheckSecondThenParameterInvalid(dummy, dummy) == Hmd2::ParameterInvalid, "Hmd2CheckSecondThenParameterInvalid with every pointer present before initialization");
    Require(Hmd2CheckSecondThenParameterInvalid(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckSecondThenParameterInvalid with pointer 1 null before initialization");
    Require(Hmd2CheckFirstThenError800d0203(dummy) == Hmd2::Error800d0203, "Hmd2CheckFirstThenError800d0203 with every pointer present before initialization");
    Require(Hmd2CheckFirstThenError800d0203(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenError800d0203 with pointer 0 null before initialization");
    Require(Hmd2CheckFirstThenError1003(dummy) == Hmd2::Error1003, "Hmd2CheckFirstThenError1003 with every pointer present before initialization");
    Require(Hmd2CheckFirstThenError1003(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenError1003 with pointer 0 null before initialization");
    Require(Hmd2CheckFirstThenError1010(dummy) == Hmd2::Error1010, "Hmd2CheckFirstThenError1010 with every pointer present before initialization");
    Require(Hmd2CheckFirstThenError1010(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenError1010 with pointer 0 null before initialization");
    Require(Hmd2CheckFirstTwoThenError1002(dummy, dummy) == Hmd2::Error1002, "Hmd2CheckFirstTwoThenError1002 with every pointer present before initialization");
    Require(Hmd2CheckFirstTwoThenError1002(nullptr, dummy) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenError1002 with pointer 0 null before initialization");
    Require(Hmd2CheckFirstTwoThenError1002(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenError1002 with pointer 1 null before initialization");
    Require(Hmd2CheckFirstTwoThenError1003(dummy, dummy) == Hmd2::Error1003, "Hmd2CheckFirstTwoThenError1003 with every pointer present before initialization");
    Require(Hmd2CheckFirstTwoThenError1003(nullptr, dummy) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenError1003 with pointer 0 null before initialization");
    Require(Hmd2CheckFirstTwoThenError1003(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenError1003 with pointer 1 null before initialization");
    Require(Hmd2CheckFirstAndThirdThenZero(dummy, dummy, dummy) == 0, "Hmd2CheckFirstAndThirdThenZero with every pointer present before initialization");
    Require(Hmd2CheckFirstAndThirdThenZero(nullptr, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckFirstAndThirdThenZero with pointer 0 null before initialization");
    Require(Hmd2CheckFirstAndThirdThenZero(dummy, dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstAndThirdThenZero with pointer 2 null before initialization");
    Require(Hmd2CheckFirstAndThirdThenError1003(dummy, dummy, dummy) == Hmd2::Error1003, "Hmd2CheckFirstAndThirdThenError1003 with every pointer present before initialization");
    Require(Hmd2CheckFirstAndThirdThenError1003(nullptr, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckFirstAndThirdThenError1003 with pointer 0 null before initialization");
    Require(Hmd2CheckFirstAndThirdThenError1003(dummy, dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstAndThirdThenError1003 with pointer 2 null before initialization");
    Require(Hmd2CheckSecondThenInvalidHandle(dummy, dummy) == Hmd2::InvalidHandle, "Hmd2CheckSecondThenInvalidHandle with every pointer present before initialization");
    Require(Hmd2CheckSecondThenInvalidHandle(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckSecondThenInvalidHandle with pointer 1 null before initialization");
    Require(Hmd2CheckSecondThenError1002(dummy, dummy) == Hmd2::Error1002, "Hmd2CheckSecondThenError1002 with every pointer present before initialization");
    Require(Hmd2CheckSecondThenError1002(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckSecondThenError1002 with pointer 1 null before initialization");
    Require(Hmd2BeforeInitializeZero() == 0, "Hmd2BeforeInitializeZero before initialization");
    Require(Hmd2BeforeInitializeNotInitialized() == Hmd2::NotInitialized, "Hmd2BeforeInitializeNotInitialized before initialization");
    Require(Hmd2BeforeInitializeError0004() == Hmd2::Error0004, "Hmd2BeforeInitializeError0004 before initialization");
    Require(Hmd2BeforeInitializeParameterNull() == Hmd2::ParameterNull, "Hmd2BeforeInitializeParameterNull before initialization");
    Require(Hmd2BeforeInitializeParameterInvalid() == Hmd2::ParameterInvalid, "Hmd2BeforeInitializeParameterInvalid before initialization");
    Require(Hmd2BeforeInitializeReprojectionNotInitialized() == Hmd2::ReprojectionNotInitialized, "Hmd2BeforeInitializeReprojectionNotInitialized before initialization");
    Require(Hmd2BeforeInitializeError002C() == Hmd2::Error002C, "Hmd2BeforeInitializeError002C before initialization");
    Require(Hmd2BeforeInitializeError1002() == Hmd2::Error1002, "Hmd2BeforeInitializeError1002 before initialization");
    Require(Hmd2BeforeInitializeError1003() == Hmd2::Error1003, "Hmd2BeforeInitializeError1003 before initialization");
    Require(Hmd2BeforeInitializeMinusOne() == -1, "Hmd2BeforeInitializeMinusOne before initialization");
    Require(Hmd2BeforeInitializeValue0400() == 0x400, "Hmd2BeforeInitializeValue0400 before initialization");
    Require(Hmd2BeforeInitializeValue20000() == 0x20000, "Hmd2BeforeInitializeValue20000 before initialization");
    Require(sceHmd2SetVibration(0, 0) == 0, "vibration of the unopened handle 0 failed");
}

void TestWithoutHeadset() {
    const std::uint8_t param[16]{};
    Require(sceHmd2Initialize(param) == Hmd::UnsupportedFeature, "initialization without a headset did not fail");
    Require(sceHmd2Initialize(nullptr) == Hmd::UnsupportedFeature, "initialization without a param did not fail as unsupported");
    Require(sceHmd2Open(USER_SERVICE_INITIAL_USER_ID, 0, 0, nullptr) == Hmd2::NotInitialized, "open without a headset did not fail");
    Hmd2::DeviceInformation info;
    Require(sceHmd2GetDeviceInformation(&info) == Hmd2::NotInitialized, "device information without a headset");
    Require(sceHmd2Terminate() == Hmd2::NotInitialized, "terminate after the failed initialization");
    {
        std::uint8_t out[64];
        std::uint64_t words[2];
        Require(Hmd2WriteStateTwo(nullptr) == Hmd2::ParameterNull, "Hmd2WriteStateTwo null after the failed initialization");
        std::memset(out, 0xaa, sizeof out);
        Require(Hmd2WriteStateTwo(out) == 0 && out[0] == 2 && out[1] == 0 && out[4] == 0 && out[5] == 0xaa, "Hmd2WriteStateTwo bytes after the failed initialization");
        Require(Hmd2WriteSize0x2000Align0x100(nullptr, words) == Hmd2::ParameterNull && Hmd2WriteSize0x2000Align0x100(words, nullptr) == Hmd2::ParameterNull, "Hmd2WriteSize0x2000Align0x100 null after the failed initialization");
        words[0] = words[1] = ~0ull;
        Require(Hmd2WriteSize0x2000Align0x100(words, words + 1) == 0 && words[0] == 0x2000 && words[1] == 0x100, "Hmd2WriteSize0x2000Align0x100 values after the failed initialization");
        Require(Hmd2WriteFourConstants(nullptr) == Hmd2::ParameterNull, "Hmd2WriteFourConstants null after the failed initialization");
        std::memset(out, 0xaa, sizeof out);
        Require(Hmd2WriteFourConstants(out) == 0 && out[0] == 0xe1 && out[15] == 0x3c && out[16] == 0xaa, "Hmd2WriteFourConstants bytes after the failed initialization");
        Require(Hmd2WriteSixHalvesThenInvalidHandle(0, nullptr) == Hmd2::ParameterNull, "Hmd2WriteSixHalvesThenInvalidHandle null after the failed initialization");
        std::memset(out, 0xaa, sizeof out);
        Require(Hmd2WriteSixHalvesThenInvalidHandle(0, out) == Hmd2::InvalidHandle && out[7] == 0x3f && out[20] == 0x00 && out[23] == 0x3f && out[24] == 0xaa, "Hmd2WriteSixHalvesThenInvalidHandle bytes after the failed initialization");
        Require(Hmd2WriteTwelveHalvesThenInvalidHandle(0, nullptr) == Hmd2::ParameterNull, "Hmd2WriteTwelveHalvesThenInvalidHandle null after the failed initialization");
        std::memset(out, 0xaa, sizeof out);
        Require(Hmd2WriteTwelveHalvesThenInvalidHandle(0, out) == Hmd2::InvalidHandle && out[7] == 0x3f && out[31] == 0x3f && out[47] == 0x00 && out[48] == 0xaa, "Hmd2WriteTwelveHalvesThenInvalidHandle bytes after the failed initialization");
        Require(Hmd2SlotUpTo3CheckSecondThirdThenZero(9, nullptr, out) == Hmd2::ParameterNull && Hmd2SlotUpTo3CheckSecondThirdThenZero(0, out, nullptr) == Hmd2::ParameterNull, "Hmd2SlotUpTo3CheckSecondThirdThenZero null after the failed initialization");
        Require(Hmd2SlotUpTo3CheckSecondThirdThenZero(3, out, out) == 0 && Hmd2SlotUpTo3CheckSecondThirdThenZero(4, out, out) == Hmd2::ParameterInvalid && Hmd2SlotUpTo3CheckSecondThirdThenZero(~0ull, out, out) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo3CheckSecondThirdThenZero slots after the failed initialization");
        Require(Hmd2SlotUpTo1ThenError1002(1) == Hmd2::Error1002 && Hmd2SlotUpTo1ThenError1002(2) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo1ThenError1002 after the failed initialization");
        Require(Hmd2SlotUpTo1CheckSecondThenError1002(5, nullptr) == Hmd2::ParameterNull && Hmd2SlotUpTo1CheckSecondThenError1002(0, out) == Hmd2::Error1002 && Hmd2SlotUpTo1CheckSecondThenError1002(2, out) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo1CheckSecondThenError1002 after the failed initialization");
        Require(Hmd2SlotUpTo1ThenMinusOne(1) == -1 && Hmd2SlotUpTo1ThenMinusOne(2) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo1ThenMinusOne after the failed initialization");
        Require(Hmd2SlotUpTo2ThenMinusOne(2) == -1 && Hmd2SlotUpTo2ThenMinusOne(3) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo2ThenMinusOne after the failed initialization");
        Require(Hmd2SlotUpTo2ThenError1002(2) == Hmd2::Error1002 && Hmd2SlotUpTo2ThenError1002(3) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo2ThenError1002 after the failed initialization");
        Require(Hmd2SlotUpTo2ThenError1003(0) == Hmd2::Error1003 && Hmd2SlotUpTo2ThenError1003(16) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo2ThenError1003 after the failed initialization");
        Require(Hmd2SlotUpTo2CheckSecondThenError002C(8, nullptr) == Hmd2::ParameterNull && Hmd2SlotUpTo2CheckSecondThenError002C(2, out) == Hmd2::Error002C && Hmd2SlotUpTo2CheckSecondThenError002C(3, out) == Hmd2::ParameterInvalid, "Hmd2SlotUpTo2CheckSecondThenError002C after the failed initialization");
        Require(Hmd2IndexZeroCheckSecondThenZero(1, nullptr) == Hmd2::ParameterNull && Hmd2IndexZeroCheckSecondThenZero(0, out) == 0 && Hmd2IndexZeroCheckSecondThenZero(1, out) == Hmd2::Error100F, "Hmd2IndexZeroCheckSecondThenZero after the failed initialization");
    }
    const std::uint8_t dummyBytes[16]{};
    const void* dummy = dummyBytes;
    Require(Hmd2CheckFirstThenError0004(dummy) == Hmd2::Error0004, "Hmd2CheckFirstThenError0004 with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstThenError0004(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenError0004 with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstThenZero(dummy) == 0, "Hmd2CheckFirstThenZero with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstThenZero(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenZero with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstThenError1002(dummy) == Hmd2::Error1002, "Hmd2CheckFirstThenError1002 with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstThenError1002(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenError1002 with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstThenMinusOne(dummy) == -1, "Hmd2CheckFirstThenMinusOne with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstThenMinusOne(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenMinusOne with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstTwoThenParameterInvalid(dummy, dummy) == Hmd2::ParameterInvalid, "Hmd2CheckFirstTwoThenParameterInvalid with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstTwoThenParameterInvalid(nullptr, dummy) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenParameterInvalid with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstTwoThenParameterInvalid(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenParameterInvalid with pointer 1 null after the failed initialization");
    Require(Hmd2CheckSecondThenError002C(dummy, dummy) == Hmd2::Error002C, "Hmd2CheckSecondThenError002C with every pointer present after the failed initialization");
    Require(Hmd2CheckSecondThenError002C(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckSecondThenError002C with pointer 1 null after the failed initialization");
    Require(Hmd2CheckFirstThenParameterInvalid(dummy) == Hmd2::ParameterInvalid, "Hmd2CheckFirstThenParameterInvalid with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstThenParameterInvalid(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenParameterInvalid with pointer 0 null after the failed initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, dummy, dummy, dummy, dummy, dummy) == 0, "Hmd2CheckAllSixThenZero with every pointer present after the failed initialization");
    Require(Hmd2CheckAllSixThenZero(nullptr, dummy, dummy, dummy, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 0 null after the failed initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, nullptr, dummy, dummy, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 1 null after the failed initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, dummy, nullptr, dummy, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 2 null after the failed initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, dummy, dummy, nullptr, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 3 null after the failed initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, dummy, dummy, dummy, nullptr, dummy) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 4 null after the failed initialization");
    Require(Hmd2CheckAllSixThenZero(dummy, dummy, dummy, dummy, dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckAllSixThenZero with pointer 5 null after the failed initialization");
    Require(Hmd2CheckSecondThenParameterInvalid(dummy, dummy) == Hmd2::ParameterInvalid, "Hmd2CheckSecondThenParameterInvalid with every pointer present after the failed initialization");
    Require(Hmd2CheckSecondThenParameterInvalid(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckSecondThenParameterInvalid with pointer 1 null after the failed initialization");
    Require(Hmd2CheckFirstThenError800d0203(dummy) == Hmd2::Error800d0203, "Hmd2CheckFirstThenError800d0203 with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstThenError800d0203(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenError800d0203 with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstThenError1003(dummy) == Hmd2::Error1003, "Hmd2CheckFirstThenError1003 with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstThenError1003(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenError1003 with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstThenError1010(dummy) == Hmd2::Error1010, "Hmd2CheckFirstThenError1010 with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstThenError1010(nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstThenError1010 with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstTwoThenError1002(dummy, dummy) == Hmd2::Error1002, "Hmd2CheckFirstTwoThenError1002 with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstTwoThenError1002(nullptr, dummy) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenError1002 with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstTwoThenError1002(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenError1002 with pointer 1 null after the failed initialization");
    Require(Hmd2CheckFirstTwoThenError1003(dummy, dummy) == Hmd2::Error1003, "Hmd2CheckFirstTwoThenError1003 with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstTwoThenError1003(nullptr, dummy) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenError1003 with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstTwoThenError1003(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstTwoThenError1003 with pointer 1 null after the failed initialization");
    Require(Hmd2CheckFirstAndThirdThenZero(dummy, dummy, dummy) == 0, "Hmd2CheckFirstAndThirdThenZero with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstAndThirdThenZero(nullptr, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckFirstAndThirdThenZero with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstAndThirdThenZero(dummy, dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstAndThirdThenZero with pointer 2 null after the failed initialization");
    Require(Hmd2CheckFirstAndThirdThenError1003(dummy, dummy, dummy) == Hmd2::Error1003, "Hmd2CheckFirstAndThirdThenError1003 with every pointer present after the failed initialization");
    Require(Hmd2CheckFirstAndThirdThenError1003(nullptr, dummy, dummy) == Hmd2::ParameterNull, "Hmd2CheckFirstAndThirdThenError1003 with pointer 0 null after the failed initialization");
    Require(Hmd2CheckFirstAndThirdThenError1003(dummy, dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckFirstAndThirdThenError1003 with pointer 2 null after the failed initialization");
    Require(Hmd2CheckSecondThenInvalidHandle(dummy, dummy) == Hmd2::InvalidHandle, "Hmd2CheckSecondThenInvalidHandle with every pointer present after the failed initialization");
    Require(Hmd2CheckSecondThenInvalidHandle(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckSecondThenInvalidHandle with pointer 1 null after the failed initialization");
    Require(Hmd2CheckSecondThenError1002(dummy, dummy) == Hmd2::Error1002, "Hmd2CheckSecondThenError1002 with every pointer present after the failed initialization");
    Require(Hmd2CheckSecondThenError1002(dummy, nullptr) == Hmd2::ParameterNull, "Hmd2CheckSecondThenError1002 with pointer 1 null after the failed initialization");
    Require(Hmd2BeforeInitializeZero() == 0, "Hmd2BeforeInitializeZero after the failed initialization");
    Require(Hmd2BeforeInitializeNotInitialized() == Hmd2::NotInitialized, "Hmd2BeforeInitializeNotInitialized after the failed initialization");
    Require(Hmd2BeforeInitializeError0004() == Hmd2::Error0004, "Hmd2BeforeInitializeError0004 after the failed initialization");
    Require(Hmd2BeforeInitializeParameterNull() == Hmd2::ParameterNull, "Hmd2BeforeInitializeParameterNull after the failed initialization");
    Require(Hmd2BeforeInitializeParameterInvalid() == Hmd2::ParameterInvalid, "Hmd2BeforeInitializeParameterInvalid after the failed initialization");
    Require(Hmd2BeforeInitializeReprojectionNotInitialized() == Hmd2::ReprojectionNotInitialized, "Hmd2BeforeInitializeReprojectionNotInitialized after the failed initialization");
    Require(Hmd2BeforeInitializeError002C() == Hmd2::Error002C, "Hmd2BeforeInitializeError002C after the failed initialization");
    Require(Hmd2BeforeInitializeError1002() == Hmd2::Error1002, "Hmd2BeforeInitializeError1002 after the failed initialization");
    Require(Hmd2BeforeInitializeError1003() == Hmd2::Error1003, "Hmd2BeforeInitializeError1003 after the failed initialization");
    Require(Hmd2BeforeInitializeMinusOne() == -1, "Hmd2BeforeInitializeMinusOne after the failed initialization");
    Require(Hmd2BeforeInitializeValue0400() == 0x400, "Hmd2BeforeInitializeValue0400 after the failed initialization");
    Require(Hmd2BeforeInitializeValue20000() == 0x20000, "Hmd2BeforeInitializeValue20000 after the failed initialization");
}

}

void SizeQueriesReturnTheConsoleSizes() {
    const auto display = sceHmd2ReprojectionQueryDisplayBufferSizeAlign();
    Require(display.size == 0x6000000 && display.align == 0x10000, "display buffer size");
    const auto seeThrough = sceHmd2ReprojectionQuerySeeThroughBufferSizeAlign();
    Require(seeThrough.size == 0x1106000 && seeThrough.align == 0x100, "see-through buffer size");
    const auto mirroring = sceHmd2ReprojectionGetMirroringDisplayBufferSizeAlign();
    Require(mirroring.size == 0x5FA0000 && mirroring.align == 0x10000, "mirroring display buffer size");
    const auto work = sceHmd2ReprojectionGetMirroringWorkMemorySizeAlign();
    Require(work.size == 0x4000 && work.align == 0x100, "mirroring work memory size");
}

int main() {
    SizeQueriesReturnTheConsoleSizes();
    InstallProcessParameters();
    TestBeforeInitialize();
    TestWithoutHeadset();
    bool threw = false;
    try {
        static_cast<void>(sceHmd2ReprojectionQueryBufferSizeAlign());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "buffer size query did not throw");
    Require(sceHmd2GazeActivateGazeTracking(0) == 0, "gaze activation before initialization");
    Require(sceHmd2ReprojectionGetCurrentOutput(nullptr) == Hmd2::ReprojectionNotInitialized, "reprojection before initialization");
    Require(sceHmd2SetFieldOfView(nullptr) == Hmd2::ParameterNull, "null field of view");
    std::puts("HMD2 tests passed");
    return 0;
}

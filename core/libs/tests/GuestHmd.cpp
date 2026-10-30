#include "ProcessParametersFixture.hpp"
#include "prx/libSceHmd/Hmd.hpp"
#include "prx/libSceHmd/include/Api.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace {

constexpr std::uint32_t LastSdkBeforeStartupState = 0x044fffff;
constexpr std::uint32_t FirstSdkWithStartupState = 0x04500000;

alignas(16) std::array<std::byte, 0x40> process{};

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "HMD: %s\n", message);
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

void SetCompiledSdkVersion(std::uint32_t version) {
    std::memcpy(process.data() + 0x10, &version, sizeof(version));
}

void CheckDisconnected() {
    Hmd::DeviceStatus status = Hmd::DeviceStatus::Ready;
    Require(sceHmdInternalGetDeviceStatus(&status) == 0, "status query failed");
    Require(status == Hmd::DeviceStatus::NotDetected, "headset reported connected");
    Require(sceHmdInternalGetDeviceStatus(nullptr) == Hmd::ParameterNull, "null status");
    Require(sceHmdInternalMmapIsConnect() == 0, "connection reported present");
    Require(!sceHmdInternalIsGameVr2d(), "game reported in VR2D without a headset");
    Require(!sceHmdInternalIsMiniAppVr2d(), "mini app reported in VR2D without a headset");
    Require(!sceHmdInternalIsCommonDlgVr2d(), "common dialog reported in VR2D without a headset");
    Require(!sceHmdInternalIsCommonDlgMiniAppVr2d(), "common dialog mini app reported in VR2D without a headset");
}

void CheckBufferQueries() {
    Require(sceHmdDistortionGetWorkMemoryAlign() == 0x400, "distortion work memory alignment");
    Require(sceHmdGetDistortionWorkMemoryAlign() == 0x400, "distortion work memory alignment query");
    Require(sceHmdDistortionGetWorkMemorySize() == 0x20000, "distortion work memory size");
    Require(sceHmdGetDistortionWorkMemorySize() == 0x20000, "distortion work memory size query");
    Require(sceHmdReprojectionQueryGarlicBuffAlign() == 0x100, "reprojection garlic buffer alignment");
    Require(sceHmdReprojectionQueryGarlicBuffSize() == 0x100000, "reprojection garlic buffer size");
    Require(sceHmdReprojectionQueryOnionBuffAlign() == 0x100, "reprojection onion buffer alignment");
    Require(sceHmdReprojectionQueryOnionBuffSize() == 0x810, "reprojection onion buffer size");
}

void CheckHandleQueries(std::int32_t expected) {
    unsigned char left[16];
    unsigned char right[16];
    unsigned char fieldOfView[16];
    unsigned char sensor[64];
    std::memset(left, 0x5a, sizeof(left));
    std::memset(right, 0x5a, sizeof(right));
    std::memset(fieldOfView, 0x5a, sizeof(fieldOfView));
    std::memset(sensor, 0x5a, sizeof(sensor));
    for (const auto handle : {0, -1, 1, 0x0f000000}) {
        Require(sceHmdGetFieldOfView(handle, nullptr) == Hmd::ParameterNull, "null field of view accepted");
        Require(sceHmdGetFieldOfView(handle, reinterpret_cast<HmdFieldOfView*>(fieldOfView)) == expected, "field of view without a headset");
        Require(sceHmdGet2DEyeOffset(handle, reinterpret_cast<HmdEyeOffset*>(left), reinterpret_cast<HmdEyeOffset*>(right)) == expected, "eye offset without a headset");
        Require(sceHmdGet2DEyeOffset(handle, nullptr, nullptr) == expected, "eye offset with null outputs");
        Require(sceHmdGetInertialSensorData(handle, reinterpret_cast<HmdInertialSensorData*>(sensor), 1) == expected, "sensor data without a headset");
    }
    for (const auto* bytes : {left, right, fieldOfView}) {
        for (std::size_t index = 0; index < 16; ++index) Require(bytes[index] == 0x5a, "a refused query wrote its output");
    }
    for (const auto byte : sensor) Require(byte == 0x5a, "a refused sensor query wrote its output");
}

template <typename Call>
bool Unimplemented(Call call) {
    try {
        static_cast<void>(call());
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void CheckCommandQueries(bool handleZeroMeasured) {
    unsigned char zeroed[64] = {};
    unsigned char vr2dBytes[64] = {};
    unsigned char commands[16];
    std::memset(commands, 0x5a, sizeof(commands));
    for (const std::size_t offset : {0x00u, 0x08u, 0x20u}) std::memset(vr2dBytes + offset, 0x11, 8);
    const auto* distortion = reinterpret_cast<const HmdDistortionCommandParameter*>(zeroed);
    const auto* wideNear = reinterpret_cast<const HmdWideNearDistortionCommandParameter*>(zeroed);
    const auto* zeroedVr2d = reinterpret_cast<const Hmd2dVrCommandParameter*>(zeroed);
    const auto* vr2d = reinterpret_cast<const Hmd2dVrCommandParameter*>(vr2dBytes);
    auto* commandBuffer = reinterpret_cast<HmdCommandBuffer*>(commands);
    const auto vr2dWith = [&](std::size_t offset, unsigned char value, std::size_t bytes, auto check) {
        unsigned char changed[64];
        std::memcpy(changed, vr2dBytes, sizeof(changed));
        std::memset(changed + offset, value, bytes);
        check(reinterpret_cast<const Hmd2dVrCommandParameter*>(changed));
    };
    for (const auto handle : {-1, 1, 0x0f000000}) {
        Require(sceHmdGetDistortionCorrectionCommand(handle, distortion, commandBuffer) == Hmd::InvalidHandle, "distortion command accepted a handle sceHmdOpen never issued");
        Require(sceHmdDistortionGetCorrectionCommand(handle, distortion, commandBuffer) == Hmd::InvalidHandle, "aliased distortion command accepted a handle sceHmdOpen never issued");
        Require(sceHmdGetWideNearDistortionCorrectionCommand(handle, wideNear, commandBuffer) == Hmd::InvalidHandle, "wide-near distortion command accepted a handle sceHmdOpen never issued");
        Require(sceHmdDistortionGetWideNearCorrectionCommand(handle, wideNear, commandBuffer) == Hmd::InvalidHandle, "aliased wide-near distortion command accepted a handle sceHmdOpen never issued");
        Require(sceHmdGet2dVrCommand(handle, vr2d, commandBuffer) == Hmd::InvalidHandle, "2D VR command accepted a handle sceHmdOpen never issued");
        Require(sceHmdDistortionGet2dVrCommand(handle, vr2d, commandBuffer) == Hmd::InvalidHandle, "aliased 2D VR command accepted a handle sceHmdOpen never issued");
    }
    for (const auto handle : {0, -1, 1, 0x0f000000}) {
        for (const auto query : {sceHmdGet2dVrCommand, sceHmdDistortionGet2dVrCommand}) {
            Require(query(handle, zeroedVr2d, commandBuffer) == Hmd::ParameterNull, "2D VR command took a parameter with null pointers");
            Require(query(handle, nullptr, commandBuffer) == Hmd::ParameterNull, "2D VR command took a null parameter");
            Require(query(handle, vr2d, nullptr) == Hmd::ParameterNull, "2D VR command took a null command buffer");
            for (const std::size_t offset : {0x00u, 0x08u, 0x20u}) {
                vr2dWith(offset, 0, 8, [&](const Hmd2dVrCommandParameter* changed) {
                    Require(query(handle, changed, commandBuffer) == Hmd::ParameterNull, "2D VR command took a null pointer in its parameter");
                });
            }
            vr2dWith(0x34, 0x01, 4, [&](const Hmd2dVrCommandParameter* changed) {
                Require(query(handle, changed, commandBuffer) == Hmd::ParameterInvalid, "2D VR command took a non-zero field at 0x34");
            });
            vr2dWith(0x38, 0x01, 8, [&](const Hmd2dVrCommandParameter* changed) {
                Require(query(handle, changed, commandBuffer) == Hmd::ParameterInvalid, "2D VR command took a non-zero field at 0x38");
            });
        }
    }
    if (!handleZeroMeasured) {
        Require(Unimplemented([&] { return sceHmdGetDistortionCorrectionCommand(0, distortion, commandBuffer); }), "distortion command answered handle 0 where its answer is not measured");
        Require(Unimplemented([&] { return sceHmdDistortionGetCorrectionCommand(0, distortion, commandBuffer); }), "aliased distortion command answered handle 0 where its answer is not measured");
        Require(Unimplemented([&] { return sceHmdGetWideNearDistortionCorrectionCommand(0, wideNear, commandBuffer); }), "wide-near distortion command answered handle 0 where its answer is not measured");
        Require(Unimplemented([&] { return sceHmdDistortionGetWideNearCorrectionCommand(0, wideNear, commandBuffer); }), "aliased wide-near distortion command answered handle 0 where its answer is not measured");
        Require(Unimplemented([&] { return sceHmdGet2dVrCommand(0, vr2d, commandBuffer); }), "2D VR command answered handle 0 where its answer is not measured");
        Require(Unimplemented([&] { return sceHmdDistortionGet2dVrCommand(0, vr2d, commandBuffer); }), "aliased 2D VR command answered handle 0 where its answer is not measured");
    } else {
        Require(sceHmdGetDistortionCorrectionCommand(0, distortion, commandBuffer) == Hmd::NotInitialized, "distortion command before initialize");
        Require(sceHmdDistortionGetCorrectionCommand(0, distortion, commandBuffer) == Hmd::NotInitialized, "aliased distortion command before initialize");
        Require(sceHmdGetWideNearDistortionCorrectionCommand(0, wideNear, commandBuffer) == Hmd::NotInitialized, "wide-near distortion command before initialize");
        Require(sceHmdDistortionGetWideNearCorrectionCommand(0, wideNear, commandBuffer) == Hmd::NotInitialized, "aliased wide-near distortion command before initialize");
        Require(sceHmdGet2dVrCommand(0, vr2d, commandBuffer) == Hmd::NotInitialized, "2D VR command before initialize");
        Require(sceHmdDistortionGet2dVrCommand(0, vr2d, commandBuffer) == Hmd::NotInitialized, "aliased 2D VR command before initialize");
    }
    for (const auto byte : commands) Require(byte == 0x5a, "a refused command query wrote its output");
}

void CheckAssyError(std::int32_t expected) {
    unsigned char data[32];
    std::memset(data, 0x5a, sizeof(data));
    Require(sceHmdGetAssyError(nullptr) == Hmd::ParameterNull, "null assembly error output accepted");
    Require(sceHmdGetAssyError(reinterpret_cast<HmdAssemblyError*>(data)) == expected, "assembly error query without a headset");
    for (const auto byte : data) Require(byte == 0x5a, "a refused assembly error query wrote its output");
}

}

int main() {
    Hmd::InitializeParam param{};
    Hmd::DeviceInformation info;
    std::memset(&info, 0xa5, sizeof(info));
    const auto untouched = info;

    InstallProcessParameters();
    CheckDisconnected();
    CheckBufferQueries();
    Require(sceHmdTerminate() == Hmd::NotInitialized, "terminate before initialize");
    Require(sceHmdOpen(1, 0, 0, nullptr) == Hmd::NotInitialized, "open before initialize");
    Require(sceHmdClose(0) == Hmd::NotInitialized, "close before initialize");
    Require(sceHmdGetDeviceInformation(nullptr) == Hmd::ParameterNull, "null information");
    Require(sceHmdGetDeviceInformation(&info) == Hmd::NotInitialized, "query before initialize");
    Require(std::memcmp(&info, &untouched, sizeof(info)) == 0, "failed query modified output");
    CheckHandleQueries(Hmd::NotInitialized);
    SetCompiledSdkVersion(LastSdkBeforeStartupState);
    CheckCommandQueries(true);
    SetCompiledSdkVersion(FirstSdkWithStartupState);
    CheckCommandQueries(false);
    CheckAssyError(Hmd::NotInitialized);
    Require(sceHmdGetDeviceInformationByHandle(0, &info) == Hmd::NotInitialized,
            "handle query before initialize");
    Require(sceHmdInitialize(nullptr) == Hmd::ParameterNull, "null initialize");
    Require(sceHmdInitialize315(nullptr) == Hmd::ParameterNull, "null initialize315");
    param.reserved0 = &param;
    Require(sceHmdInitialize(&param) == Hmd::UnsupportedFeature, "distortion unsupported");
    Require(sceHmdInitialize315(&param) == Hmd::UnsupportedFeature, "distortion315 unsupported");
    param.reserved0 = nullptr;

    for (const auto initialize : {sceHmdInitialize, sceHmdInitialize315}) {
        Require(initialize(&param) == 0, "initialize failed");
        Require(sceHmdInitialize(&param) == Hmd::AlreadyInitialized, "duplicate initialize");
        Require(sceHmdInitialize315(&param) == Hmd::AlreadyInitialized, "shared initialization");
        Require(sceHmdInitialize(nullptr) == Hmd::AlreadyInitialized, "duplicate precedence");
        CheckDisconnected();
        Require(sceHmdGetDeviceInformation(&info) == 0, "device query failed");
        Hmd::DeviceInformation expected{};
        expected.status = Hmd::DeviceStatus::NotDetected;
        expected.userId = -1;
        Require(std::memcmp(&info, &expected, sizeof(info)) == 0, "incorrect absent-device output");
        Require(sceHmdGetDeviceInformation(nullptr) == Hmd::ParameterNull, "null device query");

        Require(sceHmdOpen(-1, 0, 0, nullptr) == Hmd::ParameterInvalid, "invalid user");
        Require(sceHmdOpen(0xff, 0, 0, nullptr) == Hmd::ParameterInvalid, "system user");
        Require(sceHmdOpen(1, 1, 0, nullptr) == Hmd::ParameterInvalid, "invalid type");
        Require(sceHmdOpen(1, 0, 1, nullptr) == Hmd::ParameterInvalid, "invalid index");
        Require(sceHmdOpen(1, 0, 0, reinterpret_cast<Hmd::OpenParam*>(&param)) ==
                    Hmd::ParameterInvalid, "non-null reserved open parameter");
        Require(sceHmdOpen(1, 0, 0, nullptr) == Hmd::DeviceDisconnected, "open faked a handle");
        Require(sceHmdOpen(1, 0, 0, nullptr) == Hmd::DeviceDisconnected, "failed open created state");
        for (const auto handle : {0, -1, 1, 0x0f000000}) {
            Require(sceHmdClose(handle) == Hmd::InvalidHandle, "close accepted invalid handle");
            Require(sceHmdGetDeviceInformationByHandle(handle, &info) == Hmd::InvalidHandle,
                    "query accepted invalid handle");
            Require(std::memcmp(&info, &expected, sizeof(info)) == 0, "invalid handle modified output");
        }
        CheckHandleQueries(Hmd::InvalidHandle);
        CheckCommandQueries(false);
        CheckAssyError(Hmd::DeviceDisconnected);
        Require(sceHmdGetDeviceInformationByHandle(0, nullptr) == Hmd::InvalidHandle,
                "null query accepted invalid handle");
        Require(sceHmdTerminate() == 0, "terminate failed");
        Require(sceHmdTerminate() == Hmd::NotInitialized, "duplicate terminate");
        Require(sceHmdGetDeviceInformation(&info) == Hmd::NotInitialized, "query after terminate");
        CheckDisconnected();
    }
}

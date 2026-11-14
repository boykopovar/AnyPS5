#include "prx/libSceHidControl/libSceHidControl.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x803b3001);
constexpr int ErrorNotInitialized = static_cast<int>(0x803b3002);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "HID CONTROL: %s\n", message);
    std::abort();
}

bool AllBytes(const std::uint8_t* data, std::size_t size, std::uint8_t value) {
    for (std::size_t index = 0; index < size; ++index) {
        if (data[index] != value) return false;
    }
    return true;
}

void CheckBeforeInit() {
    std::array<std::uint8_t, 0x40> output{};
    output.fill(0x5a);
    std::uint8_t flag = 0x5a;
    std::uint16_t first = 0x5a5a;
    std::uint16_t second = 0x5a5a;
    std::uint32_t state = 0x5a5a5a5a;
    Require(sceHidControlConnectPort(0, 0) == ErrorNotInitialized, "ConnectPort before init");
    Require(sceHidControlDisconnectPort(0, 0) == ErrorNotInitialized, "DisconnectPort before init");
    Require(sceHidControlConnectHandle(0, 0, 0) == ErrorNotInitialized, "ConnectHandle before init");
    Require(sceHidControlDisconnectHandle(0, 0) == ErrorNotInitialized, "DisconnectHandle before init");
    Require(sceHidControlDisconnectDevice(0) == ErrorNotInitialized, "DisconnectDevice before init");
    Require(sceHidControlGetDeviceInfo(0, nullptr) == ErrorNotInitialized, "GetDeviceInfo before init");
    Require(sceHidControlSetProcessFocus(0, nullptr) == ErrorNotInitialized, "SetProcessFocus before init");
    Require(sceHidControlSetApplicationFocus(0, nullptr) == ErrorNotInitialized, "SetApplicationFocus before init");
    Require(sceHidControlGetBatteryState(0, nullptr) == ErrorNotInitialized, "GetBatteryState before init");
    Require(sceHidControlGetDeviceName(0, nullptr) == ErrorNotInitialized, "GetDeviceName before init");
    Require(libSceHidControlUnknown_HZJRMD3BeKk_nid_no_patch(0, nullptr) == ErrorNotInitialized, "HZJRMD3BeKk before init");
    Require(sceHidControlGetJediState(0, nullptr) == ErrorNotInitialized, "GetJediState before init");
    Require(sceHidControlSetJediVolumeGain(0, nullptr) == ErrorNotInitialized, "SetJediVolumeGain before init");
    Require(sceHidControlSetJediAudioOutPath(0, 0) == ErrorNotInitialized, "SetJediAudioOutPath before init");
    Require(sceHidControlGetJediExtensionUnitInfo(0, nullptr) == ErrorNotInitialized, "GetJediExtensionUnitInfo before init");
    Require(sceHidControlGetSparkState(0, nullptr) == ErrorNotInitialized, "GetSparkState before init");
    Require(sceHidControlSetHandleState(0, 0, 0) == ErrorNotInitialized, "SetHandleState before init");
    Require(sceHidControlSetUserColor(0, 0) == ErrorNotInitialized, "SetUserColor before init");
    Require(libSceHidControlUnknown_tcDE01gUoN0_nid_no_patch(0, 0, 0) == ErrorNotInitialized, "tcDE01gUoN0 before init");
    Require(libSceHidControlUnknown_iVF5VrBHJzk_nid_no_patch(0, 0) == ErrorNotInitialized, "iVF5VrBHJzk before init");
    Require(sceHidControlSetVibrationTriggerEffectWeakForce(0, 0) == ErrorNotInitialized, "SetVibrationTriggerEffectWeakForce before init");
    Require(sceHidControlSetAllVibrationMute(0) == ErrorNotInitialized, "SetAllVibrationMute before init");
    Require(libSceHidControlUnknown_C05jP9w4PCc_nid_no_patch(0, 0) == ErrorNotInitialized, "C05jP9w4PCc before init");
    Require(libSceHidControlUnknown_yJJq5Arw8EI_nid_no_patch(0, nullptr) == ErrorNotInitialized, "yJJq5Arw8EI before init");
    Require(sceHidControlEnableMicBeamForming(0, 0) == ErrorNotInitialized, "EnableMicBeamForming before init");
    Require(libSceHidControlUnknown_w2MEBm2Tvm8_nid_no_patch(0, 0) == ErrorNotInitialized, "w2MEBm2Tvm8 before init");
    Require(sceHidControlGetPswBatteryState(0, nullptr) == ErrorNotInitialized, "GetPswBatteryState before init");
    Require(sceHidControlGetPswState(0, nullptr) == ErrorNotInitialized, "GetPswState before init");
    Require(sceHidControlSetPswDeviceName(0, nullptr) == ErrorNotInitialized, "SetPswDeviceName before init");
    Require(sceHidControlGetPswDeviceName(0, nullptr) == ErrorNotInitialized, "GetPswDeviceName before init");
    Require(libSceHidControlUnknown_WikCB91EU0w_nid_no_patch(0, nullptr) == ErrorNotInitialized, "WikCB91EU0w before init");
    Require(sceHidControlGetPswNearbyDeviceInfo(0, 0, nullptr) == ErrorNotInitialized, "GetPswNearbyDeviceInfo before init");
    Require(sceHidControlGetPswDeviceNameForNearby(0, 0, nullptr) == ErrorNotInitialized, "GetPswDeviceNameForNearby before init");
    Require(sceHidControlSetPswConsoleMute(0, 0) == ErrorNotInitialized, "SetPswConsoleMute before init");
    Require(sceHidControlSetPswSpeakerVolume(0, 0xff) == ErrorNotInitialized, "SetPswSpeakerVolume before init");
    Require(sceHidControlSetPswSidetoneLevel(0, 0xff) == ErrorNotInitialized, "SetPswSidetoneLevel before init");
    Require(sceHidControlGetPswSidetoneLevel(0, nullptr) == ErrorNotInitialized, "GetPswSidetoneLevel before init");
    Require(sceHidControlSetPswEqPresetSelection(0, 6) == ErrorNotInitialized, "SetPswEqPresetSelection before init");
    Require(sceHidControlGetPswEqPresetSelection(0, nullptr) == ErrorNotInitialized, "GetPswEqPresetSelection before init");
    Require(sceHidControlSetPswEqPresetName(0, 0, nullptr) == ErrorNotInitialized, "SetPswEqPresetName before init");
    Require(sceHidControlGetPswEqPresetName(0, 6, nullptr) == ErrorNotInitialized, "GetPswEqPresetName before init");
    Require(sceHidControlSetPswEqBandValues(0, 0, nullptr) == ErrorNotInitialized, "SetPswEqBandValues before init");
    Require(sceHidControlGetPswEqBandValues(0, 6, nullptr) == ErrorNotInitialized, "GetPswEqBandValues before init");
    Require(sceHidControlGetPswAllEqBandValues(0, nullptr) == ErrorNotInitialized, "GetPswAllEqBandValues before init");
    Require(sceHidControlSetPswResetSettings(0) == ErrorNotInitialized, "SetPswResetSettings before init");
    Require(sceHidControlSetPswNetworkLocale(0, 0) == ErrorNotInitialized, "SetPswNetworkLocale before init");
    Require(sceHidControlGetPswNetworkLocale(0, nullptr) == ErrorNotInitialized, "GetPswNetworkLocale before init");
    Require(sceHidControlConnectPswDeviceForNearby(0, 0) == ErrorNotInitialized, "ConnectPswDeviceForNearby before init");
    Require(libSceHidControlUnknown_i5ALHbvjRi8_nid_no_patch(0, nullptr) == ErrorNotInitialized, "i5ALHbvjRi8 before init");
    Require(sceHidControlSetPswCommand(0, nullptr) == ErrorNotInitialized, "SetPswCommand before init");
    Require(sceHidControlGetPswCommand(0, nullptr) == ErrorNotInitialized, "GetPswCommand before init");
    Require(sceHidControlGetPswDeviceIdFromIdxAdd(0, 0, nullptr) == ErrorNotInitialized, "GetPswDeviceIdFromIdxAdd before init");
    Require(libSceHidControlUnknown_OFyJjudLKlA_nid_no_patch(nullptr, nullptr, 0) == ErrorNotInitialized, "OFyJjudLKlA before init");
    Require(libSceHidControlUnknown_8fIm7UR4VDc_nid_no_patch(0, 0, nullptr) == ErrorNotInitialized, "8fIm7UR4VDc before init");
    Require(sceHidControlGetAuthInformation(1, 0, 1, output.data()) == ErrorNotInitialized, "GetAuthInformation before init");
    Require(AllBytes(output.data(), output.size(), 0x5a), "GetAuthInformation before init writes nothing");
    Require(sceHidControlGetDeviceId(0, nullptr) == ErrorNotInitialized, "GetDeviceId before init");
    Require(sceHidControlGetVersionInfo(0, nullptr) == ErrorNotInitialized, "GetVersionInfo before init");
    Require(sceHidControlGetPswDeviceInfo(0, nullptr) == ErrorNotInitialized, "GetPswDeviceInfo before init");
    Require(libSceHidControlUnknown_AjBZjowWaFI_nid_no_patch(0, nullptr) == ErrorNotInitialized, "AjBZjowWaFI before init");
    Require(sceHidControlSetRevokeList2(0, 0, nullptr) == ErrorNotInitialized, "SetRevokeList2 before init");
    Require(sceHidControlIsForceUpdateNeeded(0, 1, &flag) == ErrorNotInitialized && flag == 0x5a, "IsForceUpdateNeeded before init");
    Require(sceHidControlSetRemoteMergeProcess(0, 0) == ErrorNotInitialized, "SetRemoteMergeProcess before init");
    Require(sceHidControlSetCollaboInputSettingOverrideProcess(0) == ErrorNotInitialized, "SetCollaboInputSettingOverrideProcess before init");
    Require(sceHidControlSetForceAdvancedVibrationMode(0) == ErrorNotInitialized, "SetForceAdvancedVibrationMode before init");
    Require(sceHidControlPswUpdateIsUpdateNeeded(0, nullptr, &first) == ErrorNotInitialized && first == 0x5a5a, "PswUpdateIsUpdateNeeded before init");
    Require(sceHidControlPswUpdateIsUpdateNeededForMultiDevice(0, nullptr, &first, &second) == ErrorNotInitialized && first == 0x5a5a && second == 0x5a5a,
            "PswUpdateIsUpdateNeededForMultiDevice before init");
    Require(libSceHidControlUnknown_00qes5NswlQ_nid_no_patch(0, output.data(), &flag) == ErrorNotInitialized && flag == 0x5a, "00qes5NswlQ before init");
    Require(sceHidControlPswUpdateInit() == ErrorNotInitialized, "PswUpdateInit before init");
    Require(sceHidControlPswUpdateIsReady(0, &state) == ErrorNotInitialized && state == 0x5a5a5a5a, "PswUpdateIsReady before init");
    Require(sceHidControlPswUpdateStart(0, nullptr) == ErrorNotInitialized, "PswUpdateStart before init");
    Require(sceHidControlPswUpdateGetStatus(0, &state) == ErrorNotInitialized && state == 0x5a5a5a5a, "PswUpdateGetStatus before init");
    Require(sceHidControlPswUpdateSendData(0, nullptr) == ErrorNotInitialized, "PswUpdateSendData before init");
    Require(sceHidControlPswUpdateSendAppendixData(0, nullptr) == ErrorNotInitialized, "PswUpdateSendAppendixData before init");
    Require(libSceHidControlUnknown_TKax3WNHtEg_nid_no_patch(0) == ErrorNotInitialized, "TKax3WNHtEg before init");
    Require(sceHidControlPswUpdateGetReconnectStatus(&state) == ErrorNotInitialized && state == 0x5a5a5a5a, "PswUpdateGetReconnectStatus before init");
    Require(sceHidControlPswUpdateTerm() == ErrorNotInitialized, "PswUpdateTerm before init");
    Require(sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(0, output.data()) == ErrorNotInitialized, "telemetry before init");
    Require(libSceHidControlUnknown_DUGI7AXFtV0_nid_no_patch(nullptr) == ErrorNotInitialized, "DUGI7AXFtV0 before init");
    Require(sceHidControlGetControllerSerialNumberInformation(0, output.data()) == ErrorNotInitialized, "serial number before init");
    Require(AllBytes(output.data(), output.size(), 0x5a), "queries before init write nothing");
    Require(sceHidControlSetPswControllerCommunicationMethod(0) == ErrorNotInitialized, "SetPswControllerCommunicationMethod before init");
}

void CheckArguments() {
    std::array<std::uint8_t, 0x40> buffer{};
    buffer.fill(0x5a);
    std::uint8_t flag = 0x5a;
    std::uint16_t first = 0x5a5a;
    std::uint16_t second = 0x5a5a;
    Require(sceHidControlGetDeviceInfo(0, nullptr) == ErrorInvalidArgument, "GetDeviceInfo null output");
    Require(sceHidControlSetProcessFocus(0, nullptr) == ErrorInvalidArgument, "SetProcessFocus null data");
    Require(sceHidControlSetApplicationFocus(0, nullptr) == ErrorInvalidArgument, "SetApplicationFocus null data");
    Require(sceHidControlGetBatteryState(0, nullptr) == ErrorInvalidArgument, "GetBatteryState null output");
    Require(sceHidControlGetDeviceName(0, nullptr) == ErrorInvalidArgument, "GetDeviceName null output");
    Require(libSceHidControlUnknown_HZJRMD3BeKk_nid_no_patch(0, nullptr) == ErrorInvalidArgument, "HZJRMD3BeKk null output");
    Require(sceHidControlGetJediState(0, nullptr) == ErrorInvalidArgument, "GetJediState null output");
    Require(sceHidControlSetJediVolumeGain(0, nullptr) == ErrorInvalidArgument, "SetJediVolumeGain null gain");
    Require(sceHidControlGetSparkState(0, nullptr) == ErrorInvalidArgument, "GetSparkState null output");
    Require(sceHidControlSetUserColor(0, 0) == ErrorInvalidArgument, "SetUserColor zero color");
    Require(sceHidControlGetPswBatteryState(0, nullptr) == ErrorInvalidArgument, "GetPswBatteryState null output");
    Require(sceHidControlGetPswState(0, nullptr) == ErrorInvalidArgument, "GetPswState null output");
    Require(sceHidControlSetPswDeviceName(0, nullptr) == ErrorInvalidArgument, "SetPswDeviceName null name");
    Require(sceHidControlGetPswDeviceName(0, nullptr) == ErrorInvalidArgument, "GetPswDeviceName null output");
    Require(libSceHidControlUnknown_WikCB91EU0w_nid_no_patch(0, nullptr) == ErrorInvalidArgument, "WikCB91EU0w null output");
    Require(sceHidControlGetPswNearbyDeviceInfo(0, 0, nullptr) == ErrorInvalidArgument, "GetPswNearbyDeviceInfo null output");
    Require(sceHidControlGetPswDeviceNameForNearby(0, 0, nullptr) == ErrorInvalidArgument, "GetPswDeviceNameForNearby null output");
    Require(sceHidControlSetPswSpeakerVolume(0, 16) == ErrorInvalidArgument, "SetPswSpeakerVolume 16");
    Require(sceHidControlSetPswSpeakerVolume(0, 0xff) == ErrorInvalidArgument, "SetPswSpeakerVolume 255");
    Require(sceHidControlSetPswSidetoneLevel(0, 16) == ErrorInvalidArgument, "SetPswSidetoneLevel 16");
    Require(sceHidControlGetPswSidetoneLevel(0, nullptr) == ErrorInvalidArgument, "GetPswSidetoneLevel null output");
    Require(sceHidControlSetPswEqPresetSelection(0, 6) == ErrorInvalidArgument, "SetPswEqPresetSelection 6");
    Require(sceHidControlGetPswEqPresetSelection(0, nullptr) == ErrorInvalidArgument, "GetPswEqPresetSelection null output");
    for (const std::uint32_t preset : {0u, 1u, 2u, 6u, 0xffffffffu}) {
        Require(sceHidControlSetPswEqPresetName(0, preset, buffer.data()) == ErrorInvalidArgument, "SetPswEqPresetName preset outside 3-5");
        Require(sceHidControlSetPswEqBandValues(0, preset, buffer.data()) == ErrorInvalidArgument, "SetPswEqBandValues preset outside 3-5");
    }
    Require(sceHidControlSetPswEqPresetName(0, 3, nullptr) == ErrorInvalidArgument, "SetPswEqPresetName null name");
    Require(sceHidControlSetPswEqBandValues(0, 5, nullptr) == ErrorInvalidArgument, "SetPswEqBandValues null values");
    Require(sceHidControlGetPswEqPresetName(0, 6, buffer.data()) == ErrorInvalidArgument, "GetPswEqPresetName preset 6");
    Require(sceHidControlGetPswEqPresetName(0, 0, nullptr) == ErrorInvalidArgument, "GetPswEqPresetName null output");
    Require(sceHidControlGetPswEqBandValues(0, 6, buffer.data()) == ErrorInvalidArgument, "GetPswEqBandValues preset 6");
    Require(sceHidControlGetPswEqBandValues(0, 0, nullptr) == ErrorInvalidArgument, "GetPswEqBandValues null output");
    Require(sceHidControlGetPswAllEqBandValues(0, nullptr) == ErrorInvalidArgument, "GetPswAllEqBandValues null output");
    Require(sceHidControlSetPswNetworkLocale(0, 0) == ErrorInvalidArgument, "SetPswNetworkLocale 0");
    Require(sceHidControlSetPswNetworkLocale(0, 32) == ErrorInvalidArgument, "SetPswNetworkLocale 32");
    Require(sceHidControlGetPswNetworkLocale(0, nullptr) == ErrorInvalidArgument, "GetPswNetworkLocale null output");
    Require(libSceHidControlUnknown_i5ALHbvjRi8_nid_no_patch(0, nullptr) == ErrorInvalidArgument, "i5ALHbvjRi8 null output");
    Require(sceHidControlSetPswCommand(0, nullptr) == ErrorInvalidArgument, "SetPswCommand null command");
    Require(sceHidControlGetPswCommand(0, nullptr) == ErrorInvalidArgument, "GetPswCommand null output");
    Require(sceHidControlGetPswDeviceIdFromIdxAdd(0, 0, nullptr) == ErrorInvalidArgument, "GetPswDeviceIdFromIdxAdd null output");
    Require(libSceHidControlUnknown_OFyJjudLKlA_nid_no_patch(nullptr, buffer.data(), 0) == ErrorInvalidArgument, "OFyJjudLKlA null first");
    Require(libSceHidControlUnknown_OFyJjudLKlA_nid_no_patch(buffer.data(), nullptr, 0) == ErrorInvalidArgument, "OFyJjudLKlA null second");
    Require(libSceHidControlUnknown_8fIm7UR4VDc_nid_no_patch(0, 0, nullptr) == ErrorInvalidArgument, "8fIm7UR4VDc null data");
    for (const std::uint32_t kind : {0u, 9u, 0xffffffffu}) {
        Require(sceHidControlGetAuthInformation(1, 0, kind, buffer.data()) == ErrorInvalidArgument, "GetAuthInformation kind outside 1-8");
    }
    Require(sceHidControlGetAuthInformation(0, 0, 1, buffer.data()) == ErrorInvalidArgument, "GetAuthInformation zero first");
    Require(sceHidControlGetAuthInformation(1, 0, 8, nullptr) == ErrorInvalidArgument, "GetAuthInformation null output");
    Require(AllBytes(buffer.data(), buffer.size(), 0x5a), "rejected calls write nothing");
    Require(sceHidControlGetDeviceId(0, nullptr) == ErrorInvalidArgument, "GetDeviceId null output");
    Require(sceHidControlGetVersionInfo(0, nullptr) == ErrorInvalidArgument, "GetVersionInfo null output");
    Require(sceHidControlGetPswDeviceInfo(0, nullptr) == ErrorInvalidArgument, "GetPswDeviceInfo null output");
    Require(libSceHidControlUnknown_AjBZjowWaFI_nid_no_patch(0, nullptr) == ErrorInvalidArgument, "AjBZjowWaFI null output");
    Require(sceHidControlSetRevokeList2(0, 0, nullptr) == 0, "SetRevokeList2 empty list");
    Require(sceHidControlSetRevokeList2(0, 1, nullptr) == ErrorInvalidArgument, "SetRevokeList2 null list");
    Require(sceHidControlIsForceUpdateNeeded(0, 0, &flag) == ErrorInvalidArgument && flag == 0x5a, "IsForceUpdateNeeded zero value");
    Require(sceHidControlIsForceUpdateNeeded(0, 1, nullptr) == ErrorInvalidArgument, "IsForceUpdateNeeded null result");
    errno = 71;
    Require(sceHidControlIsForceUpdateNeeded(0, 1, reinterpret_cast<std::uint8_t*>(0x10)) == ErrorInvalidArgument && errno == 71,
            "IsForceUpdateNeeded unwritable result");
    Require(sceHidControlPswUpdateIsUpdateNeeded(0, buffer.data(), nullptr) == ErrorInvalidArgument, "PswUpdateIsUpdateNeeded null result");
    Require(sceHidControlPswUpdateIsUpdateNeeded(0, nullptr, &first) == ErrorInvalidArgument && first == 0, "PswUpdateIsUpdateNeeded clears result before the image check");
    first = 0x5a5a;
    Require(sceHidControlPswUpdateIsUpdateNeededForMultiDevice(0, &first, nullptr, &second) == ErrorInvalidArgument && second == 0x5a5a,
            "PswUpdateIsUpdateNeededForMultiDevice null first result");
    Require(sceHidControlPswUpdateIsUpdateNeededForMultiDevice(0, nullptr, &first, &second) == ErrorInvalidArgument && first == 0 && second == 0x5a5a,
            "PswUpdateIsUpdateNeededForMultiDevice clears only the first result before the version check");
    Require(libSceHidControlUnknown_00qes5NswlQ_nid_no_patch(0, nullptr, &flag) == ErrorInvalidArgument && flag == 0x5a, "00qes5NswlQ null data");
    Require(libSceHidControlUnknown_00qes5NswlQ_nid_no_patch(0, buffer.data(), nullptr) == ErrorInvalidArgument, "00qes5NswlQ null result");
    Require(sceHidControlPswUpdateIsReady(0, nullptr) == ErrorInvalidArgument, "PswUpdateIsReady null state");
    Require(sceHidControlPswUpdateStart(0, nullptr) == ErrorInvalidArgument, "PswUpdateStart null image");
    Require(sceHidControlPswUpdateGetStatus(0, nullptr) == ErrorInvalidArgument, "PswUpdateGetStatus null status");
    std::array<std::uint8_t, 16> chunk{};
    const std::uint64_t oversized = 0x7fd6;
    std::memcpy(chunk.data(), &oversized, sizeof(oversized));
    Require(sceHidControlPswUpdateSendData(0, nullptr) == ErrorInvalidArgument, "PswUpdateSendData null chunk");
    Require(sceHidControlPswUpdateSendData(0, chunk.data()) == ErrorInvalidArgument, "PswUpdateSendData chunk over 0x7fd5 bytes");
    Require(sceHidControlPswUpdateSendAppendixData(0, nullptr) == ErrorInvalidArgument, "PswUpdateSendAppendixData null chunk");
    Require(sceHidControlPswUpdateSendAppendixData(0, chunk.data()) == ErrorInvalidArgument, "PswUpdateSendAppendixData chunk over 0x7fd5 bytes");
    Require(sceHidControlPswUpdateGetReconnectStatus(nullptr) == ErrorInvalidArgument, "PswUpdateGetReconnectStatus null status");
    Require(sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(0, nullptr) == ErrorInvalidArgument, "telemetry null output");
    Require(libSceHidControlUnknown_DUGI7AXFtV0_nid_no_patch(nullptr) == ErrorInvalidArgument, "DUGI7AXFtV0 null data");
    Require(sceHidControlGetControllerSerialNumberInformation(0, nullptr) == ErrorInvalidArgument, "serial number null output");
    Require(sceHidControlSetPswControllerCommunicationMethod(0) == 0, "SetPswControllerCommunicationMethod 0");
    Require(sceHidControlSetPswControllerCommunicationMethod(1) == 0, "SetPswControllerCommunicationMethod 1");
    Require(sceHidControlSetPswControllerCommunicationMethod(2) == ErrorInvalidArgument, "SetPswControllerCommunicationMethod 2");
}

void CheckGuestPointerFaults() {
    std::uint16_t version = 0x1234;
    std::uint16_t first = 0x5a5a;
    std::uint16_t second = 0x5a5a;
    const auto unmapped16 = reinterpret_cast<std::uint16_t*>(0x10);
    const auto unmappedVersion = reinterpret_cast<const std::uint16_t*>(0x10);
    errno = 71;
    Require(sceHidControlPswUpdateIsUpdateNeededForMultiDevice(0, &version, unmapped16, &second) == ErrorInvalidArgument && errno == 71,
            "PswUpdateIsUpdateNeededForMultiDevice unmapped first result");
    Require(sceHidControlPswUpdateIsUpdateNeededForMultiDevice(0, &version, &first, unmapped16) == ErrorInvalidArgument && errno == 71,
            "PswUpdateIsUpdateNeededForMultiDevice unmapped second result");
    Require(sceHidControlPswUpdateIsUpdateNeededForMultiDevice(0, unmappedVersion, &first, &second) == ErrorInvalidArgument && errno == 71,
            "PswUpdateIsUpdateNeededForMultiDevice unmapped version");
    Require(first == 0 && second == 0, "multi-device zeroes the outputs before the version read fails");
}

}

int main() {
    CheckBeforeInit();
    const auto original = std::filesystem::current_path();
    const auto directory = original / ("hidcontrol-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory), "create the test directory");
    std::filesystem::current_path(directory);
    Require(sceHidControlInit() < 0, "Init without /dev/hid fails");
    Require(sceHidControlGetDeviceInfo(0, nullptr) == ErrorNotInitialized, "a failed Init leaves the library uninitialized");
    Require(sceHidControlSetPswControllerCommunicationMethod(0) == ErrorNotInitialized, "a failed Init leaves the method check uninitialized");
    std::filesystem::create_directory("dev");
    {
        std::ofstream device("dev/hid", std::ios::binary);
        Require(static_cast<bool>(device), "create dev/hid");
    }
    Require(sceHidControlInit() == 0, "Init opens /dev/hid");
    Require(sceHidControlInit() == 0, "Init again returns 0");
    CheckArguments();
    CheckGuestPointerFaults();
    std::filesystem::current_path(original);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::puts("HidControl tests passed");
    return 0;
}

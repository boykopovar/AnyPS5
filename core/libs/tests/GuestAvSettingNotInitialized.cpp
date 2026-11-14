#include "prx/libSceAvSetting/libSceAvSetting.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int Error0001 = static_cast<int>(0x802A0001u);
constexpr int Error0002 = static_cast<int>(0x802A0002u);
constexpr int Error0007 = static_cast<int>(0x802A0007u);
constexpr int NotInitialized = static_cast<int>(0x802A001Au);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "AVSETTING NOT INITIALIZED: %s\n", message);
    std::abort();
}

std::uint32_t* Word(std::uint32_t value) {
    auto* memory = static_cast<std::uint32_t*>(GuestHeap::GuestHeapAllocate_nid_postfix(sizeof(std::uint32_t)));
    Require(memory != nullptr, "guest fixture allocation failed");
    const GuestArena::HostWrite access(memory, sizeof(value));
    Require(access.Open(), "guest fixture memory was not writable");
    std::memcpy(memory, &value, sizeof(value));
    return memory;
}

void Expect(int result, int expected, const char* message) {
    Require(result == expected, message);
}

void CheckGetters(void* buffer) {
    Expect(sceAvControlGetRawMonitorInfo(0x7001, nullptr), Error0007, "GetRawMonitorInfo checks the first argument first");
    Expect(sceAvControlGetRawMonitorInfo(0x7000, nullptr), Error0002, "GetRawMonitorInfo checks the output pointer");
    Expect(sceAvControlGetRawMonitorInfo(0x7000, buffer), NotInitialized, "GetRawMonitorInfo reports not initialized");
    Expect(sceAvControlGetRawEdid(0, buffer), Error0007, "GetRawEdid checks the first argument");
    Expect(sceAvControlGetRawEdid(0x7000, nullptr), Error0002, "GetRawEdid checks the output pointer");
    Expect(sceAvControlGetRawEdid(0x7000, buffer), NotInitialized, "GetRawEdid reports not initialized");
    Expect(sceAvControlGetKsvList(5, nullptr), Error0001, "GetKsvList checks the output pointer");
    Expect(sceAvControlGetKsvList(5, buffer), NotInitialized, "GetKsvList reports not initialized");
    Expect(sceAvControlGetCurrentDeviceId(0, nullptr), Error0002, "GetCurrentDeviceId checks the selector first");
    Expect(sceAvControlGetCurrentDeviceId(4, buffer), Error0002, "GetCurrentDeviceId refuses selector 4");
    Expect(sceAvControlGetCurrentDeviceId(3, nullptr), Error0001, "GetCurrentDeviceId checks the output pointer");
    Expect(sceAvControlGetCurrentDeviceId(1, buffer), NotInitialized, "GetCurrentDeviceId reports not initialized");
    Expect(sceAvControlGetDetailHdcpStatus(0, buffer), Error0002, "GetDetailHdcpStatus checks the selector");
    Expect(sceAvControlGetDetailHdcpStatus(2, nullptr), Error0002, "GetDetailHdcpStatus checks the output pointer");
    Expect(sceAvControlGetDetailHdcpStatus(3, buffer), NotInitialized, "GetDetailHdcpStatus reports not initialized");
    Expect(sceAvControlGetDeviceInfo(1, nullptr), Error0002, "GetDeviceInfo checks the output pointer");
    Expect(sceAvControlGetDeviceInfo(1, buffer), NotInitialized, "GetDeviceInfo reports not initialized");
    Expect(sceAvControlGetHdcpStatus(2, buffer), Error0002, "GetHdcpStatus accepts selector 1 only");
    Expect(sceAvControlGetHdcpStatus(1, nullptr), Error0002, "GetHdcpStatus checks the output pointer");
    Expect(sceAvControlGetHdcpStatus(1, buffer), NotInitialized, "GetHdcpStatus reports not initialized");
    Expect(sceAvControlGetMonitorInfo(1, nullptr), Error0001, "GetMonitorInfo checks the output pointer");
    Expect(sceAvControlGetMonitorInfo(1, buffer), NotInitialized, "GetMonitorInfo reports not initialized");
    Expect(sceAvControlGetNativeMonitorInfo(1, nullptr), Error0001, "GetNativeMonitorInfo checks the output pointer");
    Expect(sceAvControlGetNativeMonitorInfo(1, buffer), NotInitialized, "GetNativeMonitorInfo reports not initialized");
    Expect(sceAvControlGetPuStatus(nullptr), Error0002, "GetPuStatus checks the output pointer");
    Expect(sceAvControlGetPuStatus(buffer), NotInitialized, "GetPuStatus reports not initialized");
    Expect(sceAvControlGetPuType(nullptr), Error0002, "GetPuType checks the output pointer");
    Expect(sceAvControlGetPuType(buffer), NotInitialized, "GetPuType reports not initialized");
}

void CheckControlSetters() {
    Expect(sceAvControlSetDitherModeForDiag(4), Error0002, "SetDitherModeForDiag accepts modes up to 3");
    Expect(sceAvControlSetDitherModeForDiag(3), NotInitialized, "SetDitherModeForDiag reports not initialized");
    Expect(sceAvControlSet2dVrMode(2), Error0002, "Set2dVrMode accepts 0 and 1");
    Expect(sceAvControlSet2dVrMode(1), NotInitialized, "Set2dVrMode reports not initialized");
    Expect(sceAvControlSetAudioMuteForShutdown(2), Error0002, "SetAudioMuteForShutdown accepts 0 and 1");
    Expect(sceAvControlSetAudioMuteForShutdown(0), NotInitialized, "SetAudioMuteForShutdown reports not initialized");
    Expect(sceAvControlSetAvOutputMode(2, nullptr), Error0002, "SetAvOutputMode accepts modes 0 and 1");
    Expect(sceAvControlSetAvOutputMode(0, Word(0)), Error0002, "SetAvOutputMode refuses a second argument");
    Expect(sceAvControlSetAvOutputMode(1, nullptr), NotInitialized, "SetAvOutputMode reports not initialized");
    Expect(sceAvControlSetDispclk(), NotInitialized, "SetDispclk reports not initialized");
    Expect(sceAvControlSetGamutMetadata(0x6FFF), Error0007, "SetGamutMetadata checks the first argument");
    Expect(sceAvControlSetGamutMetadata(0x7000), NotInitialized, "SetGamutMetadata reports not initialized");
    Expect(sceAvControlSetHdmiGamutMetadata(), NotInitialized, "SetHdmiGamutMetadata reports not initialized");
    Expect(sceAvControlSetHdcpEncryption(0, 2), Error0007, "SetHdcpEncryption checks the first argument first");
    Expect(sceAvControlSetHdcpEncryption(0x7000, 2), Error0002, "SetHdcpEncryption accepts 0 and 1");
    Expect(sceAvControlSetHdcpEncryption(0x7000, 1), NotInitialized, "SetHdcpEncryption reports not initialized");
    Expect(sceAvControlSetHdcpStatus(2, 0), Error0002, "SetHdcpStatus accepts selector 1 only");
    Expect(sceAvControlSetHdcpStatus(1, 2), Error0002, "SetHdcpStatus accepts 0 and 1");
    Expect(sceAvControlSetHdcpStatus(1, 1), NotInitialized, "SetHdcpStatus reports not initialized");
    Expect(sceAvControlSimulateProcessOutputModeArbitration(), NotInitialized,
           "SimulateProcessOutputModeArbitration reports not initialized");
}

void CheckSettingCalls() {
    Expect(sceAvSettingSet2dVrMode(2), Error0002, "sceAvSettingSet2dVrMode accepts 0 and 1");
    Expect(sceAvSettingSet2dVrMode(0), NotInitialized, "sceAvSettingSet2dVrMode reports not initialized");
    Expect(sceAvSettingEnterAudioMuteForShutdown(2), Error0002, "EnterAudioMuteForShutdown accepts 0 and 1");
    Expect(sceAvSettingEnterAudioMuteForShutdown(1), NotInitialized, "EnterAudioMuteForShutdown reports not initialized");
    Expect(sceAvSettingSetAvOutputMode(0, Word(1)), Error0002, "sceAvSettingSetAvOutputMode refuses a second argument");
    Expect(sceAvSettingSetAvOutputMode(0, nullptr), NotInitialized, "sceAvSettingSetAvOutputMode reports not initialized");
    Expect(sceAvSettingSetDispclk(), NotInitialized, "sceAvSettingSetDispclk reports not initialized");
    Expect(sceAvSettingControlHdcpEncryption(0, 0, 2), Error0002, "ControlHdcpEncryption accepts 0 and 1");
    Expect(sceAvSettingControlHdcpEncryption(0, 0, 1), NotInitialized, "ControlHdcpEncryption reports not initialized");
    Expect(sceAvSettingSetHdcpStatus(2), Error0002, "sceAvSettingSetHdcpStatus accepts 0 and 1");
    Expect(sceAvSettingSetHdcpStatus(1), NotInitialized, "sceAvSettingSetHdcpStatus reports not initialized");
    Expect(sceAvSettingDebugSetProcessAttribute(0, 0x10000000), Error0002, "DebugSetProcessAttribute checks its range");
    Expect(sceAvSettingDebugSetProcessAttribute(0, 0x0FFFFFFF), NotInitialized,
           "DebugSetProcessAttribute reports not initialized");
    Expect(sceAvSettingNotifyAudioOutMode(0, 2), Error0002, "NotifyAudioOutMode accepts 0 and 1");
    Expect(sceAvSettingNotifyAudioOutMode(0, 1), NotInitialized, "NotifyAudioOutMode reports not initialized");
    Expect(sceAvSettingNotifyVideoOutMode(0, 0, 2), Error0002, "NotifyVideoOutMode accepts 0 and 1");
    Expect(sceAvSettingNotifyVideoOutMode(0, 0, 0), NotInitialized, "NotifyVideoOutMode reports not initialized");
}

void CheckNidBound(void* buffer) {
    Expect(libSceAvSettingUnknown_V_minus_IcK9eokeE_nid_no_patch(0, 0x10000000), Error0002, "V-IcK9eokeE checks its range");
    Expect(libSceAvSettingUnknown_V_minus_IcK9eokeE_nid_no_patch(0, 0), NotInitialized, "V-IcK9eokeE reports not initialized");
    Expect(libSceAvSettingUnknown_xVRs9kzSP7I_nid_no_patch(0, 2), Error0002, "xVRs9kzSP7I accepts 0 and 1");
    Expect(libSceAvSettingUnknown_xVRs9kzSP7I_nid_no_patch(0, 0), NotInitialized, "xVRs9kzSP7I reports not initialized");
    Expect(libSceAvSettingUnknown_m1Wi0Yp38v0_nid_no_patch(0, 0, 2), Error0002, "m1Wi0Yp38v0 accepts 0 and 1");
    Expect(libSceAvSettingUnknown_m1Wi0Yp38v0_nid_no_patch(0, 0, 1), NotInitialized, "m1Wi0Yp38v0 reports not initialized");
    Expect(libSceAvSettingUnknown_7kywfNahIbw_nid_no_patch(nullptr), Error0002, "7kywfNahIbw checks its pointer");
    Expect(libSceAvSettingUnknown_7kywfNahIbw_nid_no_patch(buffer), NotInitialized, "7kywfNahIbw reports not initialized");
    Expect(libSceAvSettingUnknown_Dxe4ybR_minus_NaM_nid_no_patch(0, nullptr), Error0001, "Dxe4ybR-NaM checks its pointer");
    Expect(libSceAvSettingUnknown_Dxe4ybR_minus_NaM_nid_no_patch(9, buffer), NotInitialized,
           "Dxe4ybR-NaM ignores its first argument and reports not initialized");
    Expect(libSceAvSettingUnknown_JUxonNYB9xk_nid_no_patch(nullptr), Error0002, "JUxonNYB9xk checks its pointer");
    Expect(libSceAvSettingUnknown_JUxonNYB9xk_nid_no_patch(Word(3)), Error0002, "JUxonNYB9xk checks the value it reads");
    Expect(libSceAvSettingUnknown_JUxonNYB9xk_nid_no_patch(Word(2)), NotInitialized, "JUxonNYB9xk reports not initialized");
    Expect(libSceAvSettingUnknown_bXmd6MGBaNI_nid_no_patch(3), Error0002, "bXmd6MGBaNI accepts up to 2");
    Expect(libSceAvSettingUnknown_bXmd6MGBaNI_nid_no_patch(2), NotInitialized, "bXmd6MGBaNI reports not initialized");
    Expect(libSceAvSettingUnknown_ftQASJeHi6M_nid_no_patch(1, nullptr), Error0002, "ftQASJeHi6M checks its pointer");
    Expect(libSceAvSettingUnknown_ftQASJeHi6M_nid_no_patch(0, Word(0)), Error0002, "ftQASJeHi6M refuses selector 0");
    Expect(libSceAvSettingUnknown_ftQASJeHi6M_nid_no_patch(3, Word(0)), Error0002, "ftQASJeHi6M refuses selector 3");
    Expect(libSceAvSettingUnknown_ftQASJeHi6M_nid_no_patch(2, Word(3)), Error0002, "ftQASJeHi6M checks the value it reads");
    Expect(libSceAvSettingUnknown_ftQASJeHi6M_nid_no_patch(1, Word(2)), NotInitialized, "ftQASJeHi6M reports not initialized");
}

} // namespace

int main() {
    void* buffer = Word(0);
    for (int pass = 0; pass < 2; ++pass) {
        CheckGetters(buffer);
        CheckControlSetters();
        CheckSettingCalls();
        CheckNidBound(buffer);
    }
}

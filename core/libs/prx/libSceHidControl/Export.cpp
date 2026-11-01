#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceHidControl/include/InventoryExports.hpp"
#include "prx/libSceHidControl/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>

namespace {
constexpr std::uint32_t CMD_c0084888 = 0xc0084888u;
constexpr std::uint32_t CMD_c010487d = 0xc010487du;
constexpr std::uint32_t CMD_c0104882 = 0xc0104882u;
constexpr std::uint32_t CMD_c0104883 = 0xc0104883u;
constexpr std::uint32_t CMD_c0104884 = 0xc0104884u;
constexpr std::uint32_t CMD_c0104885 = 0xc0104885u;
constexpr std::uint32_t CMD_c0104886 = 0xc0104886u;
constexpr std::uint32_t CMD_c0104889 = 0xc0104889u;
constexpr std::uint32_t CMD_c0184852 = 0xc0184852u;
constexpr std::uint32_t CMD_c0184853 = 0xc0184853u;
constexpr std::uint32_t CMD_c0184883 = 0xc0184883u;
constexpr std::uint32_t CMD_c0184884 = 0xc0184884u;
constexpr std::uint32_t CMD_c018488a = 0xc018488au;
constexpr std::uint32_t CMD_80044807 = 0x80044807u;
constexpr std::uint32_t CMD_80084805 = 0x80084805u;
constexpr std::uint32_t CMD_80084806 = 0x80084806u;
constexpr std::uint32_t CMD_80084807 = 0x80084807u;
constexpr std::uint32_t CMD_8008480a = 0x8008480au;
constexpr std::uint32_t CMD_800c480a = 0x800c480au;
constexpr std::uint32_t CMD_800c480b = 0x800c480bu;
constexpr std::uint32_t CMD_8010483d = 0x8010483du;
constexpr std::uint32_t CMD_8010484a = 0x8010484au;
constexpr std::uint32_t CMD_80104842 = 0x80104842u;
constexpr std::uint32_t CMD_8010484b = 0x8010484bu;
constexpr std::uint32_t CMD_8010484c = 0x8010484cu;
constexpr std::uint32_t CMD_80104850 = 0x80104850u;
constexpr std::uint32_t CMD_8010487a = 0x8010487au;
constexpr std::uint32_t CMD_8010487b = 0x8010487bu;
constexpr std::uint32_t CMD_8010487e = 0x8010487eu;
constexpr std::uint32_t CMD_80184808 = 0x80184808u;
constexpr std::uint32_t CMD_80184866 = 0x80184866u;
constexpr std::uint32_t CMD_80184868 = 0x80184868u;
constexpr std::uint32_t CMD_80184869 = 0x80184869u;
constexpr std::uint32_t CMD_8018486c = 0x8018486cu;
constexpr std::uint32_t CMD_8018486d = 0x8018486du;
constexpr std::uint32_t CMD_8018487f = 0x8018487fu;
constexpr std::uint32_t CMD_8020483f = 0x8020483fu;
constexpr std::uint32_t CMD_8028487e = 0x8028487eu;

constexpr std::int32_t kStatus3001 = static_cast<std::int32_t>(0x803b3001u);
constexpr std::int32_t kNotReady = static_cast<std::int32_t>(0x803b3002u);

bool g_initialized = false;

std::int32_t hidSendCommand(std::uint32_t, const void*, std::size_t)
{
    NotImplemented_nid_no_patch(__func__);
}

inline void putU32(std::uint8_t* dst, std::uint32_t v)
{
    std::copy(reinterpret_cast<const std::uint8_t*>(&v),
              reinterpret_cast<const std::uint8_t*>(&v) + 4, dst);
}

inline void putU64(std::uint8_t* dst, std::int64_t v)
{
    std::copy(reinterpret_cast<const std::uint8_t*>(&v),
              reinterpret_cast<const std::uint8_t*>(&v) + 8, dst);
}

template <std::size_t N>
std::int32_t runCmd(std::uint32_t code, std::array<std::uint8_t, N>& cmd)
{
    return hidSendCommand(code, cmd.data(), N);
}
}

extern "C" {

int APS5_VABI libSceHidControlUnknown_00qes5NswlQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_8fIm7UR4VDc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_AjBZjowWaFI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_C05jP9w4PCc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_DUGI7AXFtV0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_HZJRMD3BeKk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_i5ALHbvjRi8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_iVF5VrBHJzk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_OFyJjudLKlA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_tcDE01gUoN0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_TKax3WNHtEg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_w2MEBm2Tvm8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_WikCB91EU0w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceHidControlUnknown_yJJq5Arw8EI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceHidControlConnectHandle(std::uint32_t param_1, std::uint32_t param_2, std::uint32_t param_3) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    putU32(cmd.data() + 8, param_3);
    return runCmd(CMD_800c480a, cmd);
}

int APS5_VABI sceHidControlConnectPort(std::uint32_t param_1, std::uint32_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 8> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    return runCmd(CMD_80084805, cmd);
}

int APS5_VABI sceHidControlConnectPswDeviceForNearby(std::uint32_t param_1, std::uint32_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 8> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    return runCmd(CMD_8028487e, cmd);
}

int APS5_VABI sceHidControlDisconnectDevice(std::uint32_t param_1) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    return runCmd(CMD_80044807, cmd);
}

int APS5_VABI sceHidControlDisconnectHandle(std::uint32_t param_1, std::uint32_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    putU32(cmd.data() + 8, 0xffffffffu);
    return runCmd(CMD_800c480b, cmd);
}

int APS5_VABI sceHidControlDisconnectPort(std::uint32_t param_1, std::uint32_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 8> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    return runCmd(CMD_80084806, cmd);
}

int APS5_VABI sceHidControlEnableMicBeamForming() {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 8> cmd{};
    putU32(cmd.data() + 0, 1);
    putU32(cmd.data() + 4, 1);
    return runCmd(CMD_80184808, cmd);
}

int APS5_VABI sceHidControlGetAuthInformation(std::uint16_t param_1, std::uint16_t param_2, std::uint32_t param_3, std::uint8_t* param_4) {
    if (!g_initialized) return kNotReady;
    (void)param_1;
    (void)param_2;
    (void)param_4;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_3);
    return runCmd(CMD_c0104883, cmd);
}

int APS5_VABI sceHidControlGetBatteryState() {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    return runCmd(CMD_8008480a, cmd);
}

int APS5_VABI sceHidControlGetControllerSerialNumberInformation(std::uint32_t param_1, std::uint8_t* param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 == nullptr) NotImplemented_nid_no_patch(__func__);
    (void)param_1;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    return runCmd(CMD_8018487f, cmd);
}

int APS5_VABI sceHidControlGetDeviceId(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    return runCmd(CMD_c0104885, cmd);
}

int APS5_VABI sceHidControlGetDeviceInfo(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    return runCmd(CMD_c0184852, cmd);
}

int APS5_VABI sceHidControlGetDeviceName() {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    return runCmd(CMD_8010484a, cmd);
}

int APS5_VABI sceHidControlGetJediExtensionUnitInfo(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    return runCmd(CMD_8010484c, cmd);
}

int APS5_VABI sceHidControlGetJediState() {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    return runCmd(CMD_8010483d, cmd);
}

int APS5_VABI sceHidControlGetPswAllEqBandValues(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    return runCmd(CMD_c0104882, cmd);
}

int APS5_VABI sceHidControlGetPswBatteryState() {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    return runCmd(CMD_80104850, cmd);
}

int APS5_VABI sceHidControlGetPswCommand(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 == 0) NotImplemented_nid_no_patch(__func__);
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    return runCmd(CMD_c010487d, cmd);
}

int APS5_VABI sceHidControlGetPswDeviceIdFromIdxAdd(std::uint32_t param_1, std::uint32_t param_2, std::int64_t param_3) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 16> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    putU64(cmd.data() + 8, param_3);
    return runCmd(CMD_c0104884, cmd);
}

int APS5_VABI sceHidControlGetPswDeviceInfo() {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    return runCmd(CMD_c0104889, cmd);
}

int APS5_VABI sceHidControlGetPswDeviceName(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 24> cmd{};
    putU64(cmd.data() + 16, param_2);
    putU32(cmd.data() + 4, param_1);
    return runCmd(CMD_80084807, cmd);
}

int APS5_VABI sceHidControlGetPswDeviceNameForNearby() {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    return runCmd(CMD_8010484c, cmd);
}

int APS5_VABI sceHidControlGetPswEqBandValues(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    if (!g_initialized) return kStatus3001;
    std::array<std::uint8_t, 16> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    putU64(cmd.data() + 8, param_3);
    return runCmd(CMD_c0184884, cmd);
}

int APS5_VABI sceHidControlGetPswEqPresetName(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    if (!g_initialized) return kStatus3001;
    std::array<std::uint8_t, 16> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    putU64(cmd.data() + 8, param_3);
    return runCmd(CMD_c0184883, cmd);
}

int APS5_VABI sceHidControlGetPswEqPresetSelection(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 == 0) NotImplemented_nid_no_patch(__func__);
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    cmd[10] = 0x0du;
    return runCmd(CMD_c0184853, cmd);
}

int APS5_VABI sceHidControlGetPswNearbyDeviceInfo(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    return runCmd(CMD_c0104886, cmd);
}

int APS5_VABI sceHidControlGetPswNetworkLocale(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 == 0) NotImplemented_nid_no_patch(__func__);
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    cmd[10] = 0x09u;
    return runCmd(CMD_c0184852, cmd);
}

int APS5_VABI sceHidControlGetPswSidetoneLevel(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 == 0) NotImplemented_nid_no_patch(__func__);
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    cmd[10] = 0x0bu;
    return runCmd(CMD_c0184853, cmd);
}

int APS5_VABI sceHidControlGetPswState(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    return runCmd(CMD_c0184852, cmd);
}

int APS5_VABI sceHidControlGetSparkState() {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    return runCmd(CMD_8010484b, cmd);
}

int APS5_VABI sceHidControlGetVersionInfo(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    return runCmd(CMD_8010483d, cmd);
}

int APS5_VABI sceHidControlInit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceHidControlIsForceUpdateNeeded(std::uint32_t param_1, std::uint32_t param_2, std::uint8_t* param_3) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    (void)param_2;
    (void)param_3;
    return runCmd(CMD_8018486d, cmd);
}

int APS5_VABI sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData(std::uint32_t param_1, std::uint32_t* param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 == nullptr) NotImplemented_nid_no_patch(__func__);
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    return runCmd(CMD_8028487e, cmd);
}

int APS5_VABI sceHidControlPswUpdateGetReconnectStatus(std::uint32_t param_1) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    return runCmd(CMD_8018486c, cmd);
}

int APS5_VABI sceHidControlPswUpdateGetStatus(std::uint32_t param_1, std::uint32_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    (void)param_2;
    return runCmd(CMD_8028487e, cmd);
}

int APS5_VABI sceHidControlPswUpdateInit() {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, 0);
    return runCmd(CMD_80104850, cmd);
}

int APS5_VABI sceHidControlPswUpdateIsReady(std::uint32_t param_1, std::uint32_t* param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 == nullptr) NotImplemented_nid_no_patch(__func__);
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    return runCmd(CMD_8020483f, cmd);
}

int APS5_VABI sceHidControlPswUpdateIsUpdateNeeded(std::uint32_t param_1, std::int64_t param_2, std::uint16_t* param_3) {
    if (!g_initialized) return kNotReady;
    if (param_2 == 0) NotImplemented_nid_no_patch(__func__);
    if (param_3 == nullptr) NotImplemented_nid_no_patch(__func__);
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    return runCmd(CMD_8018486d, cmd);
}

int APS5_VABI sceHidControlPswUpdateIsUpdateNeededForMultiDevice(std::uint32_t param_1, std::uint16_t* param_2, std::uint16_t* param_3, std::uint16_t* param_4) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    (void)param_2;
    (void)param_3;
    (void)param_4;
    return runCmd(CMD_8018486d, cmd);
}

int APS5_VABI sceHidControlPswUpdateSendAppendixData(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    (void)param_2;
    return runCmd(CMD_8028487e, cmd);
}

int APS5_VABI sceHidControlPswUpdateSendData(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    (void)param_2;
    return runCmd(CMD_8028487e, cmd);
}

int APS5_VABI sceHidControlPswUpdateStart(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU64(cmd.data() + 4, param_2);
    return runCmd(CMD_8028487e, cmd);
}

int APS5_VABI sceHidControlPswUpdateTerm() {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, 0);
    return runCmd(CMD_80104850, cmd);
}

int APS5_VABI sceHidControlSetAllVibrationMute(std::uint32_t param_1) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    return runCmd(CMD_80184869, cmd);
}

int APS5_VABI sceHidControlSetApplicationFocus(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 24> cmd{};
    putU64(cmd.data() + 8, param_2);
    putU32(cmd.data() + 12, 1);
    (void)param_1;
    return runCmd(CMD_80184808, cmd);
}

int APS5_VABI sceHidControlSetCollaboInputSettingOverrideProcess() {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, 0);
    return runCmd(CMD_8028487e, cmd);
}

int APS5_VABI sceHidControlSetForceAdvancedVibrationMode(std::uint8_t param_1) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU32(cmd.data() + 0, 0);
    putU32(cmd.data() + 4, param_1);
    return runCmd(CMD_c0104883, cmd);
}

int APS5_VABI sceHidControlSetHandleState(std::uint32_t param_1) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    return runCmd(CMD_80184866, cmd);
}

int APS5_VABI sceHidControlSetJediAudioOutPath(std::uint32_t param_1) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    return runCmd(CMD_80104842, cmd);
}

int APS5_VABI sceHidControlSetJediVolumeGain(std::int64_t param_1, std::uint32_t param_2, std::uint32_t param_3) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_1);
    putU32(cmd.data() + 8, param_2);
    (void)param_3;
    return runCmd(CMD_c010487d, cmd);
}

int APS5_VABI sceHidControlSetProcessFocus(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 24> cmd{};
    putU64(cmd.data() + 8, param_2);
    putU32(cmd.data() + 12, 0);
    (void)param_1;
    return runCmd(CMD_80184808, cmd);
}

int APS5_VABI sceHidControlSetPswCommand(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 == 0) NotImplemented_nid_no_patch(__func__);
    std::array<std::uint8_t, 12> cmd{};
    putU64(cmd.data() + 0, param_2);
    putU32(cmd.data() + 8, param_1);
    return runCmd(CMD_c010487d, cmd);
}

int APS5_VABI sceHidControlSetPswConsoleMute(std::uint32_t param_1, std::uint8_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, static_cast<std::uint32_t>(param_2));
    cmd[8] = 0x2du;
    return runCmd(CMD_80104850, cmd);
}

int APS5_VABI sceHidControlSetPswControllerCommunicationMethod(std::uint32_t param_1) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 8> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, 1);
    return runCmd(CMD_8028487e, cmd);
}

int APS5_VABI sceHidControlSetPswDeviceName(std::uint32_t param_1, std::int64_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 24> cmd{};
    putU64(cmd.data() + 16, param_2);
    putU32(cmd.data() + 4, param_1);
    return runCmd(CMD_80084807, cmd);
}

int APS5_VABI sceHidControlSetPswEqBandValues(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    if (!g_initialized) return kStatus3001;
    std::array<std::uint8_t, 16> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    putU64(cmd.data() + 8, param_3);
    return runCmd(CMD_c0184884, cmd);
}

int APS5_VABI sceHidControlSetPswEqPresetName(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    if (!g_initialized) return kStatus3001;
    std::array<std::uint8_t, 16> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    putU64(cmd.data() + 8, param_3);
    return runCmd(CMD_c0184883, cmd);
}

int APS5_VABI sceHidControlSetPswEqPresetSelection(std::uint32_t param_1, std::uint32_t param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 >= 6) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    cmd[8] = 0x30u;
    return runCmd(CMD_80104850, cmd);
}

int APS5_VABI sceHidControlSetPswNetworkLocale(std::uint32_t param_1, std::int32_t param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 <= 0x24) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, static_cast<std::uint32_t>(param_2));
    cmd[8] = 0x28u;
    return runCmd(CMD_80104850, cmd);
}

int APS5_VABI sceHidControlSetPswResetSettings(std::uint32_t param_1) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    return runCmd(CMD_c0104883, cmd);
}

int APS5_VABI sceHidControlSetPswSidetoneLevel(std::uint32_t param_1, std::uint8_t param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 >= 0x10) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, static_cast<std::uint32_t>(param_2));
    cmd[8] = 0x2fu;
    return runCmd(CMD_80104850, cmd);
}

int APS5_VABI sceHidControlSetPswSpeakerVolume(std::uint32_t param_1, std::uint8_t param_2) {
    if (!g_initialized) return kNotReady;
    if (param_2 >= 0x10) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, static_cast<std::uint32_t>(param_2));
    cmd[8] = 0x2eu;
    return runCmd(CMD_80104850, cmd);
}

int APS5_VABI sceHidControlSetRemoteMergeProcess(std::uint32_t param_1, std::uint32_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 8> cmd{};
    putU32(cmd.data() + 0, param_1);
    putU32(cmd.data() + 4, param_2);
    return runCmd(CMD_8010487a, cmd);
}

int APS5_VABI sceHidControlSetRevokeList(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 16> cmd{};
    putU32(cmd.data() + 12, param_1);
    putU32(cmd.data() + 8, static_cast<std::uint32_t>(param_2));
    putU64(cmd.data() + 0, param_3);
    return runCmd(CMD_8010484a, cmd);
}

int APS5_VABI sceHidControlSetRevokeList2(std::uint32_t param_1, std::int32_t param_2, std::int64_t param_3) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 12> cmd{};
    putU32(cmd.data() + 8, param_1);
    putU32(cmd.data() + 4, static_cast<std::uint32_t>(param_2));
    putU64(cmd.data() + 0, param_3);
    return runCmd(CMD_c0104885, cmd);
}

int APS5_VABI sceHidControlSetUserColor(std::uint32_t param_1) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    return runCmd(CMD_80104850, cmd);
}

int APS5_VABI sceHidControlSetVibrationTriggerEffectWeakForce(std::uint32_t param_1, std::uint32_t param_2) {
    if (!g_initialized) return kNotReady;
    std::array<std::uint8_t, 4> cmd{};
    putU32(cmd.data() + 0, param_1);
    (void)param_2;
    return runCmd(CMD_80184868, cmd);
}

}
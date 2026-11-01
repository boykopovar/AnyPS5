#include "prx/libSceHidControl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>

static void Require(bool condition, const char* label) {
    if (condition) return;
    std::fprintf(stderr, "%s: failed\n", label);
    std::abort();
}

template<typename TCallable>
static void RequireNotImplemented(TCallable callable, const char* label) {
    try {
        callable();
    } catch (const std::runtime_error&) {
        return;
    }
    std::fprintf(stderr, "%s: expected not-implemented exception\n", label);
    std::abort();
}

int main() {
    const std::int32_t kStatus3001 = static_cast<std::int32_t>(0x803b3001u);
    const std::int32_t kNotReady = static_cast<std::int32_t>(0x803b3002u);
    Require(sceHidControlConnectPort(1, 2) == kNotReady, "ConnectPort before init");
    Require(sceHidControlConnectPort(0, 0) == kNotReady, "ConnectPort zero args before init");
    Require(sceHidControlConnectHandle(1, 2, 3) == kNotReady, "ConnectHandle before init");
    Require(sceHidControlDisconnectDevice(1) == kNotReady, "DisconnectDevice before init");
    Require(sceHidControlGetDeviceName() == kNotReady, "GetDeviceName before init");
    Require(sceHidControlGetPswCommand(1, 0) == kNotReady, "GetPswCommand before init");
    Require(sceHidControlSetPswCommand(1, 1) == kNotReady, "SetPswCommand before init");
    Require(sceHidControlGetPswState(1, 1) == kNotReady, "GetPswState before init");
    Require(sceHidControlGetBatteryState() == kNotReady, "GetBatteryState before init");
    Require(sceHidControlGetPswAllEqBandValues(1, 1) == kNotReady, "GetPswAllEqBandValues before init");
    Require(sceHidControlGetPswEqBandValues(1, 2, 3) == kStatus3001, "GetPswEqBandValues before init");
    Require(sceHidControlSetPswEqBandValues(1, 2, 3) == kStatus3001, "SetPswEqBandValues before init");
    Require(sceHidControlGetPswEqPresetName(1, 2, 3) == kStatus3001, "GetPswEqPresetName before init");
    Require(sceHidControlSetPswEqPresetName(1, 2, 3) == kStatus3001, "SetPswEqPresetName before init");
    RequireNotImplemented([] { sceHidControlInit(); }, "Init without HID transport");
    Require(sceHidControlConnectPort(1, 2) == kNotReady, "ConnectPort after failed init");
    RequireNotImplemented([] { libSceHidControlUnknown_00qes5NswlQ_nid_no_patch(); }, "libSceHidControlUnknown_00qes5NswlQ_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_8fIm7UR4VDc_nid_no_patch(); }, "libSceHidControlUnknown_8fIm7UR4VDc_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_AjBZjowWaFI_nid_no_patch(); }, "libSceHidControlUnknown_AjBZjowWaFI_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_C05jP9w4PCc_nid_no_patch(); }, "libSceHidControlUnknown_C05jP9w4PCc_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_DUGI7AXFtV0_nid_no_patch(); }, "libSceHidControlUnknown_DUGI7AXFtV0_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_HZJRMD3BeKk_nid_no_patch(); }, "libSceHidControlUnknown_HZJRMD3BeKk_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_i5ALHbvjRi8_nid_no_patch(); }, "libSceHidControlUnknown_i5ALHbvjRi8_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_iVF5VrBHJzk_nid_no_patch(); }, "libSceHidControlUnknown_iVF5VrBHJzk_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_OFyJjudLKlA_nid_no_patch(); }, "libSceHidControlUnknown_OFyJjudLKlA_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_tcDE01gUoN0_nid_no_patch(); }, "libSceHidControlUnknown_tcDE01gUoN0_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_TKax3WNHtEg_nid_no_patch(); }, "libSceHidControlUnknown_TKax3WNHtEg_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_w2MEBm2Tvm8_nid_no_patch(); }, "libSceHidControlUnknown_w2MEBm2Tvm8_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_WikCB91EU0w_nid_no_patch(); }, "libSceHidControlUnknown_WikCB91EU0w_nid_no_patch");
    RequireNotImplemented([] { libSceHidControlUnknown_yJJq5Arw8EI_nid_no_patch(); }, "libSceHidControlUnknown_yJJq5Arw8EI_nid_no_patch");
    return 0;
}

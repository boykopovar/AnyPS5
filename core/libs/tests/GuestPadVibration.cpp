#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static void RequireAt(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) RequireAt((value), __LINE__)

int main() {
    for (int handle : {-9999, -100, -1, 0, 1, 2, 5, 12345}) {
        const PadVibrationParam dummy{50, 50};
        Require(scePadSetVibration(handle, &dummy) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-9999, -100, -1, 0, 5, 12345}) {
        const PadVibrationParam dummy{10, 20};
        Require(scePadSetVibration(handle, &dummy) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibration(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
    }

    constexpr int user0 = 0x10000000;
    const int handle0 = scePadOpen_nid_postfix(user0, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle0 > 0);

    Require(scePadSetVibration(handle0, nullptr) == PAD_ERROR_INVALID_ARG);

    std::uint32_t seen0 = 0;
    PadOutputState out0{};

    const PadVibrationParam pMax{255, 255};
    Require(scePadSetVibration(handle0, &pMax) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.vibrationLarge == 255 && out0.vibrationSmall == 255);

    const PadVibrationParam pLargeOnly{255, 0};
    Require(scePadSetVibration(handle0, &pLargeOnly) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.vibrationLarge == 255 && out0.vibrationSmall == 0);

    const PadVibrationParam pSmallOnly{0, 255};
    Require(scePadSetVibration(handle0, &pSmallOnly) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.vibrationLarge == 0 && out0.vibrationSmall == 255);

    const PadVibrationParam pMinNonZero{1, 1};
    Require(scePadSetVibration(handle0, &pMinNonZero) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.vibrationLarge == 1 && out0.vibrationSmall == 1);

    const PadVibrationParam pMid{128, 64};
    Require(scePadSetVibration(handle0, &pMid) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.vibrationLarge == 128 && out0.vibrationSmall == 64);

    const PadVibrationParam pUpperBoundary{254, 254};
    Require(scePadSetVibration(handle0, &pUpperBoundary) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.vibrationLarge == 254 && out0.vibrationSmall == 254);

    const PadVibrationParam pReset{0, 0};
    Require(scePadSetVibration(handle0, &pReset) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.vibrationLarge == 0 && out0.vibrationSmall == 0);

    Require(scePadSetVibration(handle0, &pReset) == PAD_OK);
    Require(!PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.vibrationLarge == 0 && out0.vibrationSmall == 0);

    constexpr int user1 = 0x10000001;
    const int handle1 = scePadOpen_nid_postfix(user1, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle1 > 0);
    Require(handle1 != handle0);

    std::uint32_t seen1 = 0;
    PadOutputState out1{};

    const PadVibrationParam pPort1{180, 90};
    Require(scePadSetVibration(handle1, &pPort1) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(1, &seen1, &out1));
    Require(out1.vibrationLarge == 180 && out1.vibrationSmall == 90);
    Require(!PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.vibrationLarge == 0 && out0.vibrationSmall == 0);

    const PadVibrationParam pPort0{75, 125};
    Require(scePadSetVibration(handle0, &pPort0) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.vibrationLarge == 75 && out0.vibrationSmall == 125);
    Require(!PadFetchOutputForPort_nid_postfix(1, &seen1, &out1));
    Require(out1.vibrationLarge == 180 && out1.vibrationSmall == 90);

    Require(scePadSetVibration(handle1, &pReset) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(1, &seen1, &out1));
    Require(out1.vibrationLarge == 0 && out1.vibrationSmall == 0);
    Require(out0.vibrationLarge == 75 && out0.vibrationSmall == 125);

    Require(scePadSetVibration(handle0, &pReset) == PAD_OK);
    Require(PadFetchOutputForPort_nid_postfix(0, &seen0, &out0));
    Require(out0.vibrationLarge == 0 && out0.vibrationSmall == 0);

    const int special = scePadOpen_nid_postfix(user0, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadSetVibration(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibration(special, &pMax) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetVibration(special, &pMax) == PAD_ERROR_INVALID_HANDLE);

    Require(scePadClose_nid_postfix(handle1) == PAD_OK);
    Require(scePadClose_nid_postfix(handle0) == PAD_OK);

    for (int invalid : {-9999, -1, 0, 12345}) {
        Require(scePadClose_nid_postfix(invalid) == PAD_ERROR_INVALID_HANDLE);
    }
}

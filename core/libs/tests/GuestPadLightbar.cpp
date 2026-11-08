#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

using ScePadColor = PadLightBarParam;

constexpr int SCE_PAD_ERROR_INVALID_HANDLE = PAD_ERROR_INVALID_HANDLE;
constexpr int SCE_PAD_ERROR_INVALID_ARG = PAD_ERROR_INVALID_ARG;

static void Require(bool value) {
    if (!value) std::abort();
}

static PadOutputState OutputOf(int port) {
    std::uint32_t seen = 0xffffffffu;
    PadOutputState output{};
    Require(PadFetchOutputForPort_nid_postfix(port, &seen, &output));
    return output;
}

static bool LightBarIs(int port, bool valid, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    const PadOutputState output = OutputOf(port);
    return output.lightBarValid == valid && output.lightBar[0] == r && output.lightBar[1] == g && output.lightBar[2] == b;
}

static void CheckUninitialized() {
    for (int handle : {-1, 0, 1, 55, 12345}) {
        const ScePadColor color{255, 0, 0};
        Require(scePadSetLightBar(handle, &color) == SCE_PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, nullptr) == SCE_PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(handle) == SCE_PAD_ERROR_INVALID_HANDLE);
    }
}

static void CheckInvalidArgumentsAndHandles(int handle) {
    Require(scePadSetLightBar(handle, nullptr) == SCE_PAD_ERROR_INVALID_ARG);
    for (int invalid : {-9999, -1, 0, 55, 9999}) {
        const ScePadColor mixed{128, 64, 192};
        Require(scePadSetLightBar(invalid, &mixed) == SCE_PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(invalid, nullptr) == SCE_PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(invalid) == SCE_PAD_ERROR_INVALID_HANDLE);
    }
}

static void CheckColorBounds(int handle) {
    const ScePadColor off{0, 0, 0};
    Require(scePadSetLightBar(handle, &off) == PAD_OK);
    Require(LightBarIs(0, true, 0, 0, 0));

    const ScePadColor red{255, 0, 0};
    Require(scePadSetLightBar(handle, &red) == PAD_OK);
    Require(LightBarIs(0, true, 255, 0, 0));

    const ScePadColor green{0, 255, 0};
    Require(scePadSetLightBar(handle, &green) == PAD_OK);
    Require(LightBarIs(0, true, 0, 255, 0));

    const ScePadColor blue{0, 0, 255};
    Require(scePadSetLightBar(handle, &blue) == PAD_OK);
    Require(LightBarIs(0, true, 0, 0, 255));

    const ScePadColor full{255, 255, 255};
    Require(scePadSetLightBar(handle, &full) == PAD_OK);
    Require(LightBarIs(0, true, 255, 255, 255));

    const ScePadColor mid{128, 128, 128};
    Require(scePadSetLightBar(handle, &mid) == PAD_OK);
    Require(LightBarIs(0, true, 128, 128, 128));

    const ScePadColor mixed{128, 64, 192};
    Require(scePadSetLightBar(handle, &mixed) == PAD_OK);
    Require(LightBarIs(0, true, 128, 64, 192));

    const ScePadColor orange{255, 128, 0};
    Require(scePadSetLightBar(handle, &orange) == PAD_OK);
    Require(LightBarIs(0, true, 255, 128, 0));

    const ScePadColor purple{128, 0, 255};
    Require(scePadSetLightBar(handle, &purple) == PAD_OK);
    Require(LightBarIs(0, true, 128, 0, 255));
}

static void CheckTransitionsAndFading(int handle) {
    for (int step = 0; step <= 255; step += 17) {
        const auto val = static_cast<std::uint8_t>(step);
        const ScePadColor color{val, val, val};
        Require(scePadSetLightBar(handle, &color) == PAD_OK);
        Require(LightBarIs(0, true, val, val, val));
    }
    for (int step = 255; step >= 0; step -= 17) {
        const auto val = static_cast<std::uint8_t>(step);
        const ScePadColor color{val, val, val};
        Require(scePadSetLightBar(handle, &color) == PAD_OK);
        Require(LightBarIs(0, true, val, val, val));
    }
    for (int step = 0; step <= 255; step += 51) {
        const auto r = static_cast<std::uint8_t>(255 - step);
        const auto g = static_cast<std::uint8_t>(step);
        const ScePadColor color{r, g, 0};
        Require(scePadSetLightBar(handle, &color) == PAD_OK);
        Require(LightBarIs(0, true, r, g, 0));
    }
    for (int step = 0; step <= 255; step += 51) {
        const auto g = static_cast<std::uint8_t>(255 - step);
        const auto b = static_cast<std::uint8_t>(step);
        const ScePadColor color{0, g, b};
        Require(scePadSetLightBar(handle, &color) == PAD_OK);
        Require(LightBarIs(0, true, 0, g, b));
    }
}

static void CheckResetAndClear(int handle) {
    const ScePadColor color{64, 128, 255};
    Require(scePadSetLightBar(handle, &color) == PAD_OK);
    Require(LightBarIs(0, true, 64, 128, 255));

    Require(scePadResetLightBar(handle) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));

    Require(scePadResetLightBar(handle) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));

    Require(scePadSetLightBar(handle, &color) == PAD_OK);
    Require(LightBarIs(0, true, 64, 128, 255));

    Require(scePadResetLightBarAll() == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));

    Require(scePadSetLightBar(handle, &color) == PAD_OK);
    Require(LightBarIs(0, true, 64, 128, 255));

    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_STANDARD) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));

    for (std::uint32_t type : {3u, 4u, 16u, 99u}) {
        Require(scePadResetLightBarAllByPortType(type) == PAD_ERROR_INVALID_ARG);
    }
}

static void CheckMultipleControllers() {
    const int h0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    const int h1 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 1, nullptr);
    Require(h0 > 0 && h1 > 0 && h0 != h1);

    const ScePadColor c0{255, 0, 128};
    const ScePadColor c1{0, 255, 128};
    Require(scePadSetLightBar(h0, &c0) == PAD_OK);
    Require(scePadSetLightBar(h1, &c1) == PAD_OK);
    Require(LightBarIs(0, true, 255, 0, 128));
    Require(LightBarIs(1, true, 0, 255, 128));

    Require(scePadResetLightBar(h0) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));
    Require(LightBarIs(1, true, 0, 255, 128));

    Require(scePadResetLightBar(h1) == PAD_OK);
    Require(LightBarIs(1, false, 0, 0, 0));

    Require(scePadClose_nid_postfix(h1) == PAD_OK);
    Require(scePadClose_nid_postfix(h0) == PAD_OK);
}

static void CheckSpecialHandle() {
    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special >= PAD_SPECIAL_HANDLE);

    const ScePadColor color{100, 150, 200};
    Require(scePadSetLightBar(special, &color) == PAD_OK);
    Require(scePadSetLightBar(special, nullptr) == SCE_PAD_ERROR_INVALID_ARG);
    Require(scePadResetLightBar(special) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetLightBar(special, &color) == SCE_PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetLightBar(special) == SCE_PAD_ERROR_INVALID_HANDLE);
}

int main() {
    CheckUninitialized();

    Require(scePadInit_nid_postfix() == PAD_OK);

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    CheckInvalidArgumentsAndHandles(handle);
    CheckColorBounds(handle);
    CheckTransitionsAndFading(handle);
    CheckResetAndClear(handle);

    Require(scePadClose_nid_postfix(handle) == PAD_OK);

    const ScePadColor off{0, 0, 0};
    Require(scePadSetLightBar(handle, &off) == SCE_PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetLightBar(handle) == SCE_PAD_ERROR_INVALID_HANDLE);

    CheckMultipleControllers();
    CheckSpecialHandle();

    return 0;
}

#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

#include <cstdint>
#include <cstdlib>
#include <initializer_list>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static PadOutputState OutputOf(int port) {
    std::uint32_t seen = 0xffffffffu;
    PadOutputState output{};
    Require(PadFetchOutputForPort_nid_postfix(port, &seen, &output));
    return output;
}

static bool LightBarIs(int port, bool valid, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    const PadOutputState output = OutputOf(port);
    return output.lightBarValid == valid &&
           output.lightBar[0] == r &&
           output.lightBar[1] == g &&
           output.lightBar[2] == b;
}

int main() {
    const PadLightBarParam probeColor{123, 200, 50};

    for (int handle : {-10000, -1000, -500, -100, -2, -1, 0, 1, 2, 3, 4, 5, 15, 16, 31, 32, 100, 1000, 99999, 0x7fffffff}) {
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, &probeColor) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(handle) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-1000, -100, -1, 0, 1, 2, 3, 4, 15, 16, 31, 32, 100}) {
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, &probeColor) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(handle) == PAD_ERROR_INVALID_HANDLE);
    }

    const int h0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(h0 > 0);

    for (int handle : {-100, -1, 0, h0 + 1, h0 + 2, h0 + 3, 16, 32, 100}) {
        Require(scePadSetLightBar(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(handle, &probeColor) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(handle) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadSetLightBar(h0, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadResetLightBar(h0) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));

    const PadLightBarParam off{0, 0, 0};
    Require(scePadSetLightBar(h0, &off) == PAD_OK);
    Require(LightBarIs(0, true, 0, 0, 0));
    Require(scePadResetLightBar(h0) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));

    const PadLightBarParam white{255, 255, 255};
    Require(scePadSetLightBar(h0, &white) == PAD_OK);
    Require(LightBarIs(0, true, 255, 255, 255));
    Require(scePadResetLightBar(h0) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));

    const PadLightBarParam red{255, 0, 0};
    Require(scePadSetLightBar(h0, &red) == PAD_OK);
    Require(LightBarIs(0, true, 255, 0, 0));

    const PadLightBarParam green{0, 255, 0};
    Require(scePadSetLightBar(h0, &green) == PAD_OK);
    Require(LightBarIs(0, true, 0, 255, 0));

    const PadLightBarParam blue{0, 0, 255};
    Require(scePadSetLightBar(h0, &blue) == PAD_OK);
    Require(LightBarIs(0, true, 0, 0, 255));

    const PadLightBarParam cyan{0, 255, 255};
    Require(scePadSetLightBar(h0, &cyan) == PAD_OK);
    Require(LightBarIs(0, true, 0, 255, 255));

    const PadLightBarParam magenta{255, 0, 255};
    Require(scePadSetLightBar(h0, &magenta) == PAD_OK);
    Require(LightBarIs(0, true, 255, 0, 255));

    const PadLightBarParam yellow{255, 255, 0};
    Require(scePadSetLightBar(h0, &yellow) == PAD_OK);
    Require(LightBarIs(0, true, 255, 255, 0));

    for (std::uint8_t r : {0, 1, 63, 64, 127, 128, 191, 192, 254, 255}) {
        const PadLightBarParam param{r, 0, 0};
        Require(scePadSetLightBar(h0, &param) == PAD_OK);
        Require(LightBarIs(0, true, r, 0, 0));
    }

    for (std::uint8_t g : {0, 1, 63, 64, 127, 128, 191, 192, 254, 255}) {
        const PadLightBarParam param{0, g, 0};
        Require(scePadSetLightBar(h0, &param) == PAD_OK);
        Require(LightBarIs(0, true, 0, g, 0));
    }

    for (std::uint8_t b : {0, 1, 63, 64, 127, 128, 191, 192, 254, 255}) {
        const PadLightBarParam param{0, 0, b};
        Require(scePadSetLightBar(h0, &param) == PAD_OK);
        Require(LightBarIs(0, true, 0, 0, b));
    }

    for (std::uint8_t val : {1, 2, 42, 85, 128, 170, 200, 254}) {
        const PadLightBarParam gray{val, val, val};
        Require(scePadSetLightBar(h0, &gray) == PAD_OK);
        Require(LightBarIs(0, true, val, val, val));
    }

    Require(scePadResetLightBar(h0) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));

    const int h1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 1, nullptr);
    const int h2 = scePadOpen_nid_postfix(0x10000002, PAD_PORT_TYPE_STANDARD, 2, nullptr);
    const int h3 = scePadOpen_nid_postfix(0x10000003, PAD_PORT_TYPE_STANDARD, 3, nullptr);
    Require(h1 > 0 && h2 > 0 && h3 > 0);

    const PadLightBarParam color0{255, 10, 20};
    const PadLightBarParam color1{30, 255, 40};
    const PadLightBarParam color2{50, 60, 255};
    const PadLightBarParam color3{200, 200, 50};

    Require(scePadSetLightBar(h0, &color0) == PAD_OK);
    Require(scePadSetLightBar(h1, &color1) == PAD_OK);
    Require(scePadSetLightBar(h2, &color2) == PAD_OK);
    Require(scePadSetLightBar(h3, &color3) == PAD_OK);

    Require(LightBarIs(0, true, 255, 10, 20));
    Require(LightBarIs(1, true, 30, 255, 40));
    Require(LightBarIs(2, true, 50, 60, 255));
    Require(LightBarIs(3, true, 200, 200, 50));

    Require(scePadResetLightBar(h1) == PAD_OK);
    Require(LightBarIs(1, false, 0, 0, 0));
    Require(LightBarIs(0, true, 255, 10, 20));
    Require(LightBarIs(2, true, 50, 60, 255));
    Require(LightBarIs(3, true, 200, 200, 50));

    Require(scePadResetLightBar(h0) == PAD_OK);
    Require(scePadResetLightBar(h2) == PAD_OK);
    Require(scePadResetLightBar(h3) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));
    Require(LightBarIs(2, false, 0, 0, 0));
    Require(LightBarIs(3, false, 0, 0, 0));

    Require(scePadSetLightBar(h0, &color0) == PAD_OK);
    Require(scePadSetLightBar(h1, &color1) == PAD_OK);
    Require(scePadSetLightBar(h2, &color2) == PAD_OK);
    Require(scePadSetLightBar(h3, &color3) == PAD_OK);

    for (std::uint32_t invalidType : {3u, 4u, 16u, 0xffu, 0xffffffffu}) {
        Require(scePadResetLightBarAllByPortType(invalidType) == PAD_ERROR_INVALID_ARG);
    }

    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_SPECIAL) == PAD_OK);
    Require(LightBarIs(0, true, 255, 10, 20));
    Require(LightBarIs(1, true, 30, 255, 40));
    Require(LightBarIs(2, true, 50, 60, 255));
    Require(LightBarIs(3, true, 200, 200, 50));

    Require(scePadResetLightBarAllByPortType(PAD_PORT_TYPE_STANDARD) == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));
    Require(LightBarIs(1, false, 0, 0, 0));
    Require(LightBarIs(2, false, 0, 0, 0));
    Require(LightBarIs(3, false, 0, 0, 0));

    Require(scePadSetLightBar(h0, &color0) == PAD_OK);
    Require(scePadSetLightBar(h1, &color1) == PAD_OK);
    Require(scePadSetLightBar(h2, &color2) == PAD_OK);
    Require(scePadSetLightBar(h3, &color3) == PAD_OK);

    Require(scePadResetLightBarAll() == PAD_OK);
    Require(LightBarIs(0, false, 0, 0, 0));
    Require(LightBarIs(1, false, 0, 0, 0));
    Require(LightBarIs(2, false, 0, 0, 0));
    Require(LightBarIs(3, false, 0, 0, 0));

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadSetLightBar(special, nullptr) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetLightBar(special, &probeColor) == PAD_OK);
    Require(scePadResetLightBar(special) == PAD_OK);
    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetLightBar(special, nullptr) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetLightBar(special, &probeColor) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadResetLightBar(special) == PAD_ERROR_INVALID_HANDLE);

    const int remote = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    if (remote > 0) {
        Require(scePadSetLightBar(remote, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(remote, &probeColor) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(remote) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadClose_nid_postfix(remote) == PAD_OK);
        Require(scePadSetLightBar(remote, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBar(remote, &probeColor) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadResetLightBar(remote) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose_nid_postfix(h0) == PAD_OK);
    Require(scePadClose_nid_postfix(h1) == PAD_OK);
    Require(scePadClose_nid_postfix(h2) == PAD_OK);
    Require(scePadClose_nid_postfix(h3) == PAD_OK);

    return 0;
}

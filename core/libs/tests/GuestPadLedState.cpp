#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

extern "C" {
int APS5_VABI scePadInit_nid_postfix(void);
int APS5_VABI scePadOpen_nid_postfix(int userId, int type, int index, const void* param);
int APS5_VABI scePadSetUserColor(std::int32_t handle, std::uint32_t color);
int APS5_VABI scePadSetLightBarBlinking(std::int32_t handle, const PadLightBarBlinkingParameter* parameter);
int APS5_VABI scePadSetMuteLed(std::int32_t handle, std::uint32_t mode);
}

static void Require(bool value) { if (!value) std::abort(); }

template <typename F>
static void RequireThrows(F&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (...) {
        threw = true;
    }
    Require(threw);
}

int main() {
    for (int handle : {-1, 0, 2, 12345}) {
        Require(scePadSetUserColor(handle, 0xff00ff) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetLightBarBlinking(handle, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMuteLed(handle, 1) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);
    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0);
    PadLightBarBlinkingParameter parameter{};
    for (std::uint32_t color : {0u, 0x0000ffu, 0xff00ffu})
        Require(scePadSetUserColor(handle, color) == PAD_OK);
    Require(scePadSetLightBarBlinking(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    RequireThrows([&] { (void)scePadSetLightBarBlinking(handle, &parameter); });
    RequireThrows([&] { (void)scePadSetMuteLed(handle, 0); });
    RequireThrows([&] { (void)scePadSetMuteLed(handle, 1); });
    return 0;
}

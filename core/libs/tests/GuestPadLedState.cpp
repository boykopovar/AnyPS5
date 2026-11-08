#include "prx/libScePad/include/Pad.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

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
    Require(scePadSetUserColor(PAD_HANDLE, 0) == PAD_ERROR_NOT_INITIALIZED);
    Require(scePadSetUserColor(PAD_HANDLE, 4) == PAD_ERROR_NOT_INITIALIZED);

    Require(scePadInit_nid_postfix() == PAD_OK);
    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0);
    for (int unopened : {-1, 0, 2, 12345}) {
        Require(scePadSetLightBarBlinking(unopened, nullptr) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetMuteLed(unopened, 1) == PAD_ERROR_INVALID_HANDLE);
    }
    PadLightBarBlinkingParameter parameter{};
    Require(scePadSetLightBarBlinking(handle, nullptr) == PAD_ERROR_INVALID_ARG);
    for (std::uint32_t preset : {0u, 1u, 2u, 3u})
        RequireThrows([&] { (void)scePadSetUserColor(handle, preset); });
    for (std::uint32_t color : {4u, 5u, 0xffu, 0x112233u, 0x332211u, 0xff00ffu, 0xffffffffu})
        Require(scePadSetUserColor(handle, color) == PAD_ERROR_INVALID_ARG);
    RequireThrows([&] { (void)scePadSetUserColor(-1, 0); });
    return 0;
}

#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"

#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <string_view>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename F>
static void RequireNotImplemented(const char* expectedName, F&& func) {
    try {
        func();
        std::abort();
    } catch (const std::runtime_error& err) {
        const std::string_view message = err.what();
        const std::string_view expected = expectedName;
        if (message.find(expected) == std::string_view::npos) {
            std::abort();
        }
    }
}

static int VibrationModeOf(int port) {
    std::uint32_t seen = 0xffffffffu;
    PadOutputState output{};
    Require(PadFetchOutputForPort_nid_postfix(port, &seen, &output));
    return output.vibrationMode;
}

int main() {
    for (int handle : {-1000, -100, -1, 0, 1, 2, 16, 32, 100, 12345}) {
        for (int mode : {-50, -1, 0, 1, 2, 3, 4, 5, 6, 10, 100}) {
            Require(scePadSetVibrationMode(handle, mode) == PAD_ERROR_NOT_INITIALIZED);
        }
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-500, -100, -2, -1, 0, 1, 2, 3, 4, 15, 16, 31, 32, 100, 99999}) {
        for (int mode : {-1000, -100, -10, -2, -1, 0}) {
            Require(scePadSetVibrationMode(handle, mode) == PAD_ERROR_INVALID_ARG);
        }
        for (int mode : {1, 2, 3, 4, 5, 6, 10, 100, 9999}) {
            Require(scePadSetVibrationMode(handle, mode) == PAD_ERROR_INVALID_HANDLE);
        }
    }

    const int h0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(h0 > 0);
    Require(VibrationModeOf(0) == 0);

    for (int handle : {-100, -1, 0, h0 + 1, h0 + 2, h0 + 3, 16, 32, 100}) {
        Require(scePadSetVibrationMode(handle, 0) == PAD_ERROR_INVALID_ARG);
        Require(scePadSetVibrationMode(handle, -1) == PAD_ERROR_INVALID_ARG);
        Require(scePadSetVibrationMode(handle, 1) == PAD_ERROR_INVALID_HANDLE);
        Require(scePadSetVibrationMode(handle, 6) == PAD_ERROR_INVALID_HANDLE);
    }

    for (int mode : {-500, -10, -2, -1, 0}) {
        Require(scePadSetVibrationMode(h0, mode) == PAD_ERROR_INVALID_ARG);
        Require(VibrationModeOf(0) == 0);
    }

    for (int mode : {6, 7, 8, 10, 50, 100, 1000, 0x7fffffff}) {
        Require(scePadSetVibrationMode(h0, mode) == PAD_ERROR_INVALID_ARG);
        Require(VibrationModeOf(0) == 0);
    }

    for (int mode : {1, 2, 3, 4, 5}) {
        Require(scePadSetVibrationMode(h0, mode) == PAD_OK);
        Require(VibrationModeOf(0) == mode);
    }

    Require(scePadSetVibrationMode(h0, 5) == PAD_OK);
    Require(VibrationModeOf(0) == 5);

    const int h1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 1, nullptr);
    const int h2 = scePadOpen_nid_postfix(0x10000002, PAD_PORT_TYPE_STANDARD, 2, nullptr);
    const int h3 = scePadOpen_nid_postfix(0x10000003, PAD_PORT_TYPE_STANDARD, 3, nullptr);
    Require(h1 > 0 && h2 > 0 && h3 > 0);

    Require(scePadSetVibrationMode(h0, 1) == PAD_OK);
    Require(scePadSetVibrationMode(h1, 2) == PAD_OK);
    Require(scePadSetVibrationMode(h2, 3) == PAD_OK);
    Require(scePadSetVibrationMode(h3, 4) == PAD_OK);

    Require(VibrationModeOf(0) == 1);
    Require(VibrationModeOf(1) == 2);
    Require(VibrationModeOf(2) == 3);
    Require(VibrationModeOf(3) == 4);

    Require(scePadSetVibrationMode(h1, 5) == PAD_OK);
    Require(VibrationModeOf(1) == 5);
    Require(VibrationModeOf(0) == 1);
    Require(VibrationModeOf(2) == 3);
    Require(VibrationModeOf(3) == 4);

    const int special = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special > 0);
    Require(scePadSetVibrationMode(special, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(special, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(special, 6) == PAD_ERROR_INVALID_ARG);
    for (int mode : {1, 2, 3, 4, 5}) {
        Require(scePadSetVibrationMode(special, mode) == PAD_OK);
    }
    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadSetVibrationMode(special, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(special, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(special, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetVibrationMode(special, 6) == PAD_ERROR_INVALID_HANDLE);

    const int remote = scePadOpen_nid_postfix(PAD_USER_ID_SYSTEM, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    Require(remote > 0);
    Require(scePadSetVibrationMode(remote, -1) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(remote, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(remote, 1) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadSetVibrationMode(remote, 6) == PAD_ERROR_INVALID_HANDLE);
    Require(scePadClose_nid_postfix(remote) == PAD_OK);

    Require(scePadClose_nid_postfix(h0) == PAD_OK);
    Require(scePadClose_nid_postfix(h1) == PAD_OK);
    Require(scePadClose_nid_postfix(h2) == PAD_OK);
    Require(scePadClose_nid_postfix(h3) == PAD_OK);

    Require(scePadSetVibrationMode(h0, 0) == PAD_ERROR_INVALID_ARG);
    Require(scePadSetVibrationMode(h0, 1) == PAD_OK);
    Require(scePadSetVibrationMode(h0, 6) == PAD_ERROR_INVALID_ARG);

    for (std::uint8_t mode : {0, 1, 2, 5, 255}) {
        RequireNotImplemented("scePadSetPS4BcVibrationMode", [mode] {
            return scePadSetPS4BcVibrationMode(mode);
        });
    }

    for (int handle : {-1, 0, 1, 100}) {
        for (std::uint32_t mode : {0u, 1u, 2u, 5u}) {
            RequireNotImplemented("scePadVrControllerSetVibrationMode", [handle, mode] {
                return scePadVrControllerSetVibrationMode(handle, mode);
            });
        }
    }

    return 0;
}

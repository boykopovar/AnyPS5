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
        if (message.find(expected) == std::string_view::npos ||
            message.find("not implemented") == std::string_view::npos) {
            std::abort();
        }
    }
}

static void VerifyAllAccessControllerDebtStubs(int handle) {
    int mode = -1;

    for (bool enabled : {false, true}) {
        for (std::uint32_t profile : {0u, 1u, 2u, 3u, 16u, 255u, 0xffffu, 0xffffffffu}) {
            RequireNotImplemented("scePadSetProfileEditMode", [handle, enabled, profile] {
                return scePadSetProfileEditMode(handle, enabled, profile);
            });
        }
    }

    RequireNotImplemented("scePadGetProfileEditMode", [handle, &mode] {
        return scePadGetProfileEditMode(handle, &mode);
    });
    RequireNotImplemented("scePadGetProfileEditMode", [handle] {
        return scePadGetProfileEditMode(handle, nullptr);
    });
    Require(mode == -1);

    RequireNotImplemented("scePadGetProfileEditModeLeonardo", [handle] {
        return scePadGetProfileEditModeLeonardo(handle);
    });

    for (bool enabled : {false, true}) {
        RequireNotImplemented("scePadSetSelectButtonCustomizeMode", [handle, enabled] {
            return scePadSetSelectButtonCustomizeMode(handle, enabled);
        });
    }

    RequireNotImplemented("scePadGetSelectButtonCustomizeMode", [handle] {
        return scePadGetSelectButtonCustomizeMode(handle);
    });

    RequireNotImplemented("scePadSetToggleAllReleaseLeonardo", [handle] {
        return scePadSetToggleAllReleaseLeonardo(handle);
    });

    RequireNotImplemented("scePadEnableControllerIndicator", [handle] {
        return scePadEnableControllerIndicator(handle);
    });

    RequireNotImplemented("scePadEnableProfileIndicator", [handle] {
        return scePadEnableProfileIndicator(handle);
    });

    for (std::uint8_t anim : {0x00, 0x01, 0x02, 0x03, 0x0f, 0x55, 0x7f, 0x80, 0xaa, 0xfe, 0xff}) {
        RequireNotImplemented("scePadSetProfileIndicatorAnimation", [handle, anim] {
            return scePadSetProfileIndicatorAnimation(handle, anim);
        });
    }
}

int main() {
    for (int handle : {-1000, -100, -1, 0, 1, 2, 15, 16, 32, 100, 1000, 0x7fffffff}) {
        VerifyAllAccessControllerDebtStubs(handle);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-500, -2, -1, 0, 1, 2, 3, 4, 15, 16, 31, 32, 100, 99999}) {
        VerifyAllAccessControllerDebtStubs(handle);
    }

    const int standardHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(standardHandle > 0);
    VerifyAllAccessControllerDebtStubs(standardHandle);

    PadSetPortConnected_nid_postfix(0, false);
    VerifyAllAccessControllerDebtStubs(standardHandle);
    PadSetPortConnected_nid_postfix(0, true);
    VerifyAllAccessControllerDebtStubs(standardHandle);

    Require(scePadClose_nid_postfix(standardHandle) == PAD_OK);
    VerifyAllAccessControllerDebtStubs(standardHandle);

    const int specialHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(specialHandle > 0);
    VerifyAllAccessControllerDebtStubs(specialHandle);
    Require(scePadClose_nid_postfix(specialHandle) == PAD_OK);
    VerifyAllAccessControllerDebtStubs(specialHandle);

    const int remoteHandle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_REMOTE, 0, nullptr);
    if (remoteHandle > 0) {
        VerifyAllAccessControllerDebtStubs(remoteHandle);
        Require(scePadClose_nid_postfix(remoteHandle) == PAD_OK);
        VerifyAllAccessControllerDebtStubs(remoteHandle);
    }

    const int h0 = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    const int h1 = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_STANDARD, 1, nullptr);
    const int h2 = scePadOpen_nid_postfix(0x10000002, PAD_PORT_TYPE_STANDARD, 2, nullptr);
    const int h3 = scePadOpen_nid_postfix(0x10000003, PAD_PORT_TYPE_STANDARD, 3, nullptr);
    Require(h0 > 0 && h1 > 0 && h2 > 0 && h3 > 0);

    VerifyAllAccessControllerDebtStubs(h0);
    VerifyAllAccessControllerDebtStubs(h1);
    VerifyAllAccessControllerDebtStubs(h2);
    VerifyAllAccessControllerDebtStubs(h3);

    Require(scePadClose_nid_postfix(h0) == PAD_OK);
    Require(scePadClose_nid_postfix(h1) == PAD_OK);
    Require(scePadClose_nid_postfix(h2) == PAD_OK);
    Require(scePadClose_nid_postfix(h3) == PAD_OK);

    VerifyAllAccessControllerDebtStubs(h0);
    VerifyAllAccessControllerDebtStubs(h1);
    VerifyAllAccessControllerDebtStubs(h2);
    VerifyAllAccessControllerDebtStubs(h3);

    return 0;
}

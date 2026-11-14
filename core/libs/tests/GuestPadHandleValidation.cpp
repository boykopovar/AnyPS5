#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    constexpr int noHandle = static_cast<int>(0x80920008);
    constexpr int invalidArg = static_cast<int>(0x80920002);

    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 0) == noHandle);
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 2) == invalidArg);
    Require(scePadGetHandle(0, PAD_PORT_TYPE_STANDARD, 0) == invalidArg);
    Require(scePadGetHandle(-1, PAD_PORT_TYPE_STANDARD, 0) == invalidArg);

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 0) == handle);
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 1) == noHandle);
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 2) == invalidArg);

    Require(scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_SPECIAL));
    Require(!scePadIsValidHandle(handle + 1, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(0, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(-1, PAD_PORT_TYPE_STANDARD));

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));
}

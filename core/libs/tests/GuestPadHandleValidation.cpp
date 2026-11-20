#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

static void Require(bool value, std::source_location loc = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Require failed at line %u\n", loc.line());
        std::fflush(stderr);
        std::abort();
    }
}

int main() {
    Require(scePadInit_nid_postfix() == PAD_OK);

    constexpr int noHandle = static_cast<int>(0x80920008);

    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 0) == noHandle);
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 2) == noHandle);
    Require(scePadGetHandle(0, PAD_PORT_TYPE_STANDARD, 0) == noHandle);
    Require(scePadGetHandle(-1, PAD_PORT_TYPE_STANDARD, 0) == noHandle);

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 0) == handle);
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 1) == noHandle);
    Require(scePadGetHandle(0x10000000, PAD_PORT_TYPE_STANDARD, 2) == noHandle);

    Require(scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_SPECIAL));
    Require(!scePadIsValidHandle(handle + 1, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(0, PAD_PORT_TYPE_STANDARD));
    Require(!scePadIsValidHandle(-1, PAD_PORT_TYPE_STANDARD));

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(!scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD));
}

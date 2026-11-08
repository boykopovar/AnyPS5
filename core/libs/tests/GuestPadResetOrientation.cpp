#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(scePadInit_nid_postfix() == PAD_OK);
    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);

    Require(scePadResetOrientation(handle) == PAD_OK);

    for (const int invalid : {-9999, -100, -2, -1, 0, handle + 1, 9999}) {
        Require(scePadResetOrientation(invalid) == PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadClose_nid_postfix(handle) == PAD_OK);
}

#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

static void ExpectQueriesReturn(int expected) {
    for (const int handle : {-1, 0, PAD_HANDLE, PAD_HANDLE + 1, PAD_SPECIAL_HANDLE, PAD_REMOTE_HANDLE, 12345}) {
        Require(scePadIsCoPilot(handle) == expected);
        Require(scePadIsMoveReproductionModel(handle) == expected);
    }
}

int main() {
    ExpectQueriesReturn(PAD_ERROR_NOT_INITIALIZED);
    Require(scePadInit_nid_postfix() == PAD_OK);
    ExpectQueriesReturn(0);

    const int handle = scePadOpen_nid_postfix(0x10000000, PAD_PORT_TYPE_STANDARD, 0, nullptr);
    Require(handle > 0);
    Require(scePadIsCoPilot(handle) == 0);
    Require(scePadIsMoveReproductionModel(handle) == 0);

    const int special = scePadOpen_nid_postfix(0x10000001, PAD_PORT_TYPE_SPECIAL, 0, nullptr);
    Require(special >= PAD_SPECIAL_HANDLE);
    Require(scePadIsCoPilot(special) == 0);
    Require(scePadIsMoveReproductionModel(special) == 0);

    Require(scePadInit_nid_postfix() == PAD_OK);
    Require(scePadIsCoPilot(handle) == 0);
    Require(scePadIsMoveReproductionModel(handle) == 0);

    Require(scePadClose_nid_postfix(special) == PAD_OK);
    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadIsCoPilot(handle) == 0);
    Require(scePadIsMoveReproductionModel(handle) == 0);
    return 0;
}

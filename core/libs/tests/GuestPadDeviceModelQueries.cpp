#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    for (int handle : {-1, 0, PAD_HANDLE, PAD_HANDLE + 1, PAD_SPECIAL_HANDLE, PAD_REMOTE_HANDLE, 12345}) {
        Require(scePadIsCoPilot(handle) == PAD_ERROR_NOT_INITIALIZED);
        Require(scePadIsMoveReproductionModel(handle) == PAD_ERROR_NOT_INITIALIZED);
    }
    Require(scePadInit_nid_postfix() == PAD_OK);
    const int handle = scePadOpen_nid_postfix(0x10000000, 0, 0, nullptr);
    Require(handle > 0);
    Require(scePadIsCoPilot(handle) == 0);
    Require(scePadIsMoveReproductionModel(handle) == 0);
    Require(scePadClose_nid_postfix(handle) == PAD_OK);
    Require(scePadIsCoPilot(handle) == 0);
    Require(scePadIsMoveReproductionModel(handle) == 0);
    return 0;
}

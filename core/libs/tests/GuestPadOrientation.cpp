#include "SceTypes.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include <cstdlib>
#include <initializer_list>

static void Require(bool value) { if (!value) std::abort(); }

constexpr int SCE_PAD_ERROR_INVALID_HANDLE = PAD_ERROR_INVALID_HANDLE;

int main() {
    for (int handle : {-1, -2, -100, -9999}) {
        Require(scePadResetOrientation(handle) == SCE_PAD_ERROR_INVALID_HANDLE);
    }

    Require(scePadInit_nid_postfix() == PAD_OK);

    for (int handle : {-1, -2, -100, -9999}) {
        Require(scePadResetOrientation(handle) == SCE_PAD_ERROR_INVALID_HANDLE);
    }

    return 0;
}

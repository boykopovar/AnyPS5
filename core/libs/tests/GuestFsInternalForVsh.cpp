#include "prx/libSceFsInternalForVsh/libSceFsInternalForVsh.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    Require(sceFsErriCtl() == static_cast<int>(0x8002002d));
    Require(sceFsErriGetState() == static_cast<int>(0x8002002d));
    for (int call = 0; call < 2; ++call) {
        Require(libSceFsInternalForVshUnknown_0XlyVxRNeIw_nid_no_patch() == 0);
        Require(libSceFsInternalForVshUnknown_W8CmA_plus_wZ9_plus_w_nid_no_patch() == 0);
        Require(libSceFsInternalForVshUnknown_lxyJjALNmp4_nid_no_patch() == 0);
    }
    return 0;
}

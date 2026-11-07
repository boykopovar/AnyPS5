#include "prx/libSceFsInternalForVsh/libSceFsInternalForVsh.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void CheckWorkspaceOptInit(decltype(&sceFsInitCreateWorkspaceOpt) init) {
    for (int call = 0; call < 2; ++call) {
        alignas(8) std::uint8_t opt[16];
        for (auto& byte : opt) {
            byte = 0xa5;
        }
        Require(init(opt) == 0);
        for (int i = 0; i < 4; ++i) {
            Require(opt[i] == 0);
        }
        for (int i = 4; i < 16; ++i) {
            Require(opt[i] == 0xa5);
        }
    }
}

}

int main() {
    Require(sceFsErriCtl() == static_cast<int>(0x8002002d));
    Require(sceFsErriGetState() == static_cast<int>(0x8002002d));
    CheckWorkspaceOptInit(sceFsInitCreateWorkspaceOpt);
    CheckWorkspaceOptInit(sceFsInitDeleteWorkspaceOpt);
    CheckWorkspaceOptInit(sceFsInitUmountWorkspaceOpt);
    return 0;
}

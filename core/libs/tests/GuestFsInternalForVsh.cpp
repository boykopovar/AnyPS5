#include "prx/libSceFsInternalForVsh/libSceFsInternalForVsh.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void CheckTrophyImageError() {
    const std::uint32_t accepted[] = {0x80020005u, 0x80020016u, 0x8002002du, 0x80020060u};
    for (std::uint32_t error : accepted) {
        Require(sceFsTrophyImageError(static_cast<int>(error)) == 0);
    }
    const std::uint32_t rejected[] = {0u,          1u,          5u,          0x16u,       0x80020004u, 0x80020006u,
                                      0x80020015u, 0x80020017u, 0x8002002cu, 0x8002002eu, 0x8002005fu, 0x80020061u,
                                      0x80000000u, 0x7fffffffu, 0xffffffffu};
    for (std::uint32_t error : rejected) {
        Require(sceFsTrophyImageError(static_cast<int>(error)) == static_cast<int>(0x80020016u));
    }
}

}

int main() {
    Require(sceFsErriCtl() == static_cast<int>(0x8002002d));
    Require(sceFsErriGetState() == static_cast<int>(0x8002002d));
    CheckTrophyImageError();
    return 0;
}

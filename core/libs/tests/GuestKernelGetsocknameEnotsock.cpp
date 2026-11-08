#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "prx/libSceNet/libSceNet.hpp"


static void Require(bool condition) {
    if (!condition) std::abort();
}

static void RequireError(int result, int error) {
    Require(result == static_cast<int>(0x80410100u | static_cast<std::uint32_t>(error)));
    Require(*sceNetErrnoLoc() == error);
}

int main() {
    std::array<std::uint8_t, 32> address;
    address.fill(0xa5);
    std::uint32_t size = 16;

    RequireError(sceNetGetsockname(-1, address.data(), &size), 9);
    RequireError(sceNetGetsockname(9999, address.data(), &size), 9);

    const int pipeFds[2] = {sceNetSocket("pipe-read", 2, 1, 6), sceNetSocket("pipe-write", 2, 1, 6)};
    Require(pipeFds[0] >= 0 && pipeFds[1] >= 0);
    Require(sceNetSocketClose(pipeFds[1]) == 0);

    RequireError(sceNetGetsockname(pipeFds[1], address.data(), &size), 9);
    Require(sceNetSocketClose(pipeFds[0]) == 0);

    return 0;
}
} // namespace

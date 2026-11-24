#include "prx/libSceVideoRecordingP/include/Api.hpp"

#include <cstdint>
#include <cstdio>

int main() {
    const std::uint32_t actual = static_cast<std::uint32_t>(sceVideoRecordingRestModeProcessGetStatus());
    if (actual != 0x80a80002u) {
        std::fprintf(stderr, "RestModeProcessGetStatus without a client: expected 0x80a80002, got 0x%08x\n", static_cast<unsigned>(actual));
        return 1;
    }
    return 0;
}

#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceVideoOutGetPortStatus(int port, void* status = nullptr);
}

static constexpr int SCE_VIDEO_OUT_ERROR_INVALID_PORT = static_cast<int>(0x80290001);

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

int main() {
    Require(sceVideoOutGetPortStatus(-1) == SCE_VIDEO_OUT_ERROR_INVALID_PORT);
    Require(sceVideoOutGetPortStatus(-100) == SCE_VIDEO_OUT_ERROR_INVALID_PORT);
    Require(sceVideoOutGetPortStatus(3) == SCE_VIDEO_OUT_ERROR_INVALID_PORT);
    Require(sceVideoOutGetPortStatus(4) == SCE_VIDEO_OUT_ERROR_INVALID_PORT);
    Require(sceVideoOutGetPortStatus(100) == SCE_VIDEO_OUT_ERROR_INVALID_PORT);

    Require(sceVideoOutGetPortStatus(-1, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_PORT);
    Require(sceVideoOutGetPortStatus(-100, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_PORT);
    Require(sceVideoOutGetPortStatus(3, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_PORT);
    Require(sceVideoOutGetPortStatus(4, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_PORT);
    Require(sceVideoOutGetPortStatus(100, nullptr) == SCE_VIDEO_OUT_ERROR_INVALID_PORT);

    LibcRunShutdown_nid_postfix();
    return 0;
}

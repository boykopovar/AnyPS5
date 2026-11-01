#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdlib>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int INVALID_VALUE = static_cast<int>(0x80290004u);

static void Require(bool value) {
    if (!value) std::abort();
}

static void ExpectFailure(auto&& action) {
    bool failed = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Require(failed);
}

int main() {
    VideoOutTestEnvironment environment;
    ExpectFailure([] { sceVideoOutSetFlipRate(-1, 0); });
    ExpectFailure([] { sceVideoOutSetFlipRate(0, 0); });
    ExpectFailure([] { sceVideoOutSetFlipRate(9999, 0); });

    Require(sceVideoOutSetFlipRate(-1, -1) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(0, -1) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(9999, -1) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(-1, 3) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(0, 3) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(9999, 3) == INVALID_VALUE);

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error&) {
        LibcRunShutdown_nid_postfix();
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutSetFlipRate(handle, 0) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0);

    Require(sceVideoOutSetFlipRate(handle, -1) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 3) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 100) == INVALID_VALUE);

    Require(sceVideoOutClose(handle) == 0);
    ExpectFailure([&] { sceVideoOutSetFlipRate(handle, 1); });

    LibcRunShutdown_nid_postfix();
    return 0;
}

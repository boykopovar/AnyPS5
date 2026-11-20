#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int NEVER_OPENED_HANDLE = 2;

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

static bool RejectsPeg(int handle, std::uint64_t arg1, std::uint64_t arg2) {
    try {
        sceVideoOutVrrPegToFixedRate(handle, arg1, arg2);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool RejectsUnpeg(int handle) {
    try {
        sceVideoOutVrrUnpegFromFixedRate(handle);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    VideoOutTestEnvironment environment;

    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0);

    Require(RejectsPeg(0, 0, 0));
    Require(RejectsPeg(NEVER_OPENED_HANDLE, 0, 0));
    Require(RejectsUnpeg(0));
    Require(RejectsUnpeg(NEVER_OPENED_HANDLE));

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) {
            throw;
        }
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);

    Require(RejectsPeg(handle, 1, 0));
    Require(RejectsPeg(handle, 0, 1));
    Require(RejectsPeg(handle, 1, 1));
    Require(RejectsPeg(handle, 0x100000000ull, 0));
    Require(RejectsPeg(handle, 0, 0x100000000ull));
    Require(RejectsPeg(handle, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull));

    Require(sceVideoOutClose(handle) == 0);

    Require(RejectsPeg(handle, 0, 0));
    Require(RejectsUnpeg(handle));

    LibcRunShutdown_nid_postfix();
    return 0;
}

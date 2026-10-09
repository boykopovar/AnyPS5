#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

extern "C" {
int APS5_VABI sceVideoOutOpen(int userId, int busType, int index, const void* param);
int APS5_VABI sceVideoOutClose(int handle);
int APS5_VABI sceVideoOutVrrPegToFixedRate(int handle, uint64_t arg1, uint64_t arg2);
int APS5_VABI sceVideoOutVrrUnpegFromFixedRate(int handle);
}

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int NEVER_OPENED_HANDLE = 2;

static void Require(bool value) { if (!value) std::abort(); }

static bool RejectsPeg(int handle) {
    try {
        sceVideoOutVrrPegToFixedRate(handle, 1, 2);
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
    std::filesystem::create_directories("app0/sce_sys");
    {
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}},"downloadDataSize":0})";
        Require(static_cast<bool>(param));
    }
    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0);

    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);
    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);
    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);

    Require(RejectsPeg(0));
    Require(RejectsPeg(-1));
    Require(RejectsPeg(NEVER_OPENED_HANDLE));
    Require(RejectsUnpeg(0));
    Require(RejectsUnpeg(-1));
    Require(RejectsUnpeg(NEVER_OPENED_HANDLE));

    Require(sceVideoOutClose(handle) == 0);
    Require(RejectsPeg(handle));
    Require(RejectsUnpeg(handle));
    LibcRunShutdown_nid_postfix();
}

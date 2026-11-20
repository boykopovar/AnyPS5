#include "SceTypes.hpp"
#include "prx/libSceVideoOut/include/InventoryExports.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int NO_DEVICE = static_cast<int>(0x80290018u);
static constexpr int INVALID_ADDRESS = static_cast<int>(0x80290002u);
static constexpr std::uint64_t MAIN_OUTPUT = 0x7000;

static void Require(bool value, const char* message) {
    if (!value) throw std::logic_error(message);
}

static bool Throws(std::uint64_t target, void* mode) {
    try {
        sceVideoOutGetCurrentOutputMode_(target, mode);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static bool Untouched(const std::array<std::uint8_t, 64>& mode) {
    for (const auto value : mode) {
        if (value != 0xAA) return false;
    }
    return true;
}

static int Run() {
    VideoOutTestEnvironment environment;
    std::array<std::uint8_t, 64> mode{};
    mode.fill(0xAA);
    Require(Throws(MAIN_OUTPUT, mode.data()), "the main output bus did not throw");
    Require(Throws(0x7FFFFFFF, mode.data()), "a value that is no VideoOut handle did not throw");
    Require(Throws(0x100000001ull, mode.data()), "a value above 32 bits did not throw");
    Require(Untouched(mode), "a call that threw wrote the mode");
    Require(sceVideoOutGetCurrentOutputMode_(MAIN_OUTPUT, nullptr) == INVALID_ADDRESS, "the main output bus with a null mode did not answer INVALID_ADDRESS");
    Require(sceVideoOutGetCurrentOutputMode_(0x7FFFFFFF, nullptr) == INVALID_ADDRESS, "a value that is no VideoOut handle with a null mode did not answer INVALID_ADDRESS");
    Require(sceVideoOutGetCurrentOutputMode_(0x100000001ull, nullptr) == INVALID_ADDRESS, "a value above 32 bits with a null mode did not answer INVALID_ADDRESS");

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0, "sceVideoOutOpen failed");
    Require(sceVideoOutGetCurrentOutputMode_(static_cast<std::uint64_t>(handle), mode.data()) == NO_DEVICE, "an open VideoOut handle did not answer NO_DEVICE");
    Require(Untouched(mode), "the NO_DEVICE answer wrote the mode");
    Require(sceVideoOutGetCurrentOutputMode_(static_cast<std::uint64_t>(handle), nullptr) == INVALID_ADDRESS, "an open VideoOut handle with a null mode did not answer INVALID_ADDRESS");
    Require(sceVideoOutClose(handle) == 0, "sceVideoOutClose failed");
    Require(Throws(static_cast<std::uint64_t>(handle), mode.data()), "a closed VideoOut handle did not throw");
    Require(Untouched(mode), "a closed handle wrote the mode");
    Require(sceVideoOutGetCurrentOutputMode_(static_cast<std::uint64_t>(handle), nullptr) == INVALID_ADDRESS, "a closed VideoOut handle with a null mode did not answer INVALID_ADDRESS");
    std::puts("sceVideoOutGetCurrentOutputMode_ tests passed");
    return 0;
}

int main() {
    try {
        return Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

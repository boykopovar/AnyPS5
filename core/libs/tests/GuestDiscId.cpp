#include "prx/libSceDiscId/include/Api.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81680016);
constexpr int ErrorDeviceUnavailable = static_cast<int>(0x816800ff);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "DISC ID: %s\n", message);
    std::abort();
}

}

int main() {
    const auto original = std::filesystem::current_path();
    const auto directory = original / ("discid-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory), "create the test directory");
    std::filesystem::current_path(directory);

    Require(sceDiscIdGet(nullptr) == ErrorInvalidArgument, "Get null output");
    Require(sceDiscIdGet2(nullptr, 0) == ErrorInvalidArgument, "Get2 null output");
    Require(sceDiscIdGet2(nullptr, 1) == ErrorInvalidArgument, "Get2 null output and nonzero flags");

    std::uint8_t buffer[64];
    std::memset(buffer, 0xAB, sizeof(buffer));
    auto* id = reinterpret_cast<DiscId*>(buffer);
    auto* id2 = reinterpret_cast<DiscId2*>(buffer);
    for (const std::uint32_t flags : {1u, 3u, 0x80000000u, 0xFFFFFFFFu}) {
        Require(sceDiscIdGet2(id2, flags) == ErrorInvalidArgument, "Get2 nonzero flags");
    }
    Require(sceDiscIdGet(id) == ErrorDeviceUnavailable, "Get without /dev/duid");
    Require(sceDiscIdGet2(id2, 0) == ErrorDeviceUnavailable, "Get2 without /dev/duid");
    for (const std::uint8_t byte : buffer) Require(byte == 0xAB, "failed calls write nothing");

    std::filesystem::current_path(original);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::puts("DiscId tests passed");
    return 0;
}

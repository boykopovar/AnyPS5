#include "prx/libSceGic/libSceGic.hpp"
#include <cerrno>
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
    std::fprintf(stderr, "GIC: %s\n", message);
    std::abort();
}

}

int main() {
    const auto original = std::filesystem::current_path();
    const auto directory = original / ("gic-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory), "create the test directory");
    std::filesystem::current_path(directory);

    Require(sceGicGetValue(nullptr) == ErrorInvalidArgument, "null output");
    errno = 71;
    Require(sceGicGetValue(reinterpret_cast<void*>(0x10)) == ErrorDeviceUnavailable, "unwritable output");
    Require(errno == 71, "preserves errno");
    std::uint8_t buffer[32];
    std::memset(buffer, 0xAB, sizeof(buffer));
    Require(sceGicGetValue(buffer) == ErrorDeviceUnavailable, "GetValue without /dev/gic");
    for (std::size_t i = 0; i < sizeof(buffer); ++i) Require(buffer[i] == 0xAB, "preserves buffer on unavailable");
    for (const std::uint8_t byte : buffer) Require(byte == 0xAB, "a failed call writes nothing");

    std::filesystem::current_path(original);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::puts("Gic tests passed");
    return 0;
}

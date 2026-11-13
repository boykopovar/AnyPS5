#include "prx/libSceGic/libSceGic.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
    Require(sceGicGetValue(nullptr) == ErrorInvalidArgument, "repeated null output");
    for (const std::uint8_t fill : {std::uint8_t{0x00}, std::uint8_t{0xAB}, std::uint8_t{0xFF}}) {
        std::array<std::uint8_t, 33> buffer{};
        buffer.fill(fill);
        Require(sceGicGetValue(buffer.data()) == ErrorDeviceUnavailable, "GetValue without /dev/gic");
        Require(sceGicGetValue(buffer.data() + 1) == ErrorDeviceUnavailable, "unaligned GetValue without /dev/gic");
        for (const std::uint8_t byte : buffer) Require(byte == fill, "a failed call writes nothing");
    }

    std::filesystem::current_path(original);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::puts("Gic tests passed");
    return 0;
}

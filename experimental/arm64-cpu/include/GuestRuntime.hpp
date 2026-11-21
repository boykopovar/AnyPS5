#pragma once
#include "GuestCpu.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace anyps5::experimental {
// Deliberately scoped, non-ELF test image. All offsets are little-endian.
class GuestRuntime {
public:
    // AG64 v1: 20-byte header, code and initialized data.
    // AG64 v2: 24-byte header, code, data, and relocation records.
    static GuestRuntime load(std::span<const std::uint8_t> image,
                             std::size_t stackSize = 65536);
    std::uint64_t run(std::size_t instructionLimit = 100000);
    GuestCpu& cpu() noexcept { return cpu_; }
    const GuestCpu& cpu() const noexcept { return cpu_; }
    static constexpr std::uint32_t dataBase = 4096;
private:
    GuestRuntime(std::vector<std::uint8_t> code, std::size_t memorySize,
                 std::size_t dataSize, std::size_t stackBase);
    GuestCpu cpu_;
};
}

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace SceVcodec::Internal {

inline constexpr std::size_t StatusRequestSize = 0x1b50;
inline constexpr std::size_t StatusOutputSize = 0x1308;

using StatusRequest = std::array<std::uint8_t, StatusRequestSize>;
using StatusOutput = std::array<std::uint8_t, StatusOutputSize>;

inline std::uint32_t Read32(const StatusRequest& request, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, request.data() + offset, sizeof(value));
    return value;
}

inline std::uint64_t Read64(const StatusRequest& request, std::size_t offset) {
    std::uint64_t value = 0;
    std::memcpy(&value, request.data() + offset, sizeof(value));
    return value;
}

inline void Write32(StatusOutput& output, std::size_t offset, std::uint32_t value) {
    std::memcpy(output.data() + offset, &value, sizeof(value));
}

inline void WriteFloat(StatusOutput& output, std::size_t offset, float value) {
    std::memcpy(output.data() + offset, &value, sizeof(value));
}

inline float ToFloat(std::uint64_t value) {
    return static_cast<float>(value);
}

inline void ConvertStatus(const StatusRequest& request, StatusOutput& output) {
    output.fill(0);
    const std::uint64_t duration = Read64(request, 0x1b48);

    for (std::size_t index = 0; index < 21; ++index) {
        const std::size_t source = index * 0x68;
        const std::size_t destination = index * 0x48;
        std::memcpy(output.data() + destination, request.data() + source, 0x20);
        std::memcpy(output.data() + destination + 0x20, request.data() + source + 0x20, 8);

        const std::uint32_t sampleCount = Read32(request, source + 0x24);
        if (duration != 0) {
            WriteFloat(output, destination + 0x28, ToFloat(Read64(request, source + 0x28)) / ToFloat(duration) * 100.0F);
            WriteFloat(output, destination + 0x2c, ToFloat(Read64(request, source + 0x30)) / ToFloat(duration) * 100.0F);
        }
        if (sampleCount != 0) {
            const float divisor = static_cast<float>(sampleCount);
            WriteFloat(output, destination + 0x30, ToFloat(Read64(request, source + 0x38)) / divisor / 1000.0F);
            WriteFloat(output, destination + 0x38, ToFloat(Read64(request, source + 0x48)) / divisor / 1000.0F);
            WriteFloat(output, destination + 0x3c, ToFloat(Read64(request, source + 0x50)) / divisor / 1000.0F);
            WriteFloat(output, destination + 0x40, ToFloat(Read64(request, source + 0x58)) / divisor / 1000.0F);
        }
        WriteFloat(output, destination + 0x34, ToFloat(Read64(request, source + 0x40)) / 1000.0F);
        WriteFloat(output, destination + 0x44, ToFloat(Read64(request, source + 0x60)) / 1000.0F);
    }

    for (std::size_t index = 0; index < 40; ++index) {
        const std::size_t source = 0x888 + index * 0x78;
        const std::size_t destination = 0x5e8 + index * 0x54;
        if ((Read32(request, source + 0x10) & 0x80000000u) != 0) {
            std::memset(output.data() + destination, 0xff, 0x18);
            std::memset(output.data() + destination + 0x3c, 0xff, 0x10);
            continue;
        }

        std::memcpy(output.data() + destination, request.data() + source, 0x18);
        if (duration != 0) {
            WriteFloat(output, destination + 0x18, static_cast<float>(Read32(request, source + 0x18)) * 1000000.0F / ToFloat(duration));
            WriteFloat(output, destination + 0x1c, ToFloat(Read64(request, source + 0x20)) / ToFloat(duration) * 100.0F);
            WriteFloat(output, destination + 0x20, ToFloat(Read64(request, source + 0x28)) / ToFloat(duration) * 100.0F);
        }
        const std::uint32_t sampleCount = Read32(request, source + 0x18);
        if (sampleCount != 0) {
            const float divisor = static_cast<float>(sampleCount);
            WriteFloat(output, destination + 0x24, ToFloat(Read64(request, source + 0x30)) / divisor / 1000.0F);
            WriteFloat(output, destination + 0x28, ToFloat(Read64(request, source + 0x38)) / 1000.0F);
            WriteFloat(output, destination + 0x2c, ToFloat(Read64(request, source + 0x40)) / divisor / 1000.0F);
            WriteFloat(output, destination + 0x30, ToFloat(Read64(request, source + 0x48)) / 1000.0F);
            WriteFloat(output, destination + 0x34, ToFloat(Read64(request, source + 0x50)) / divisor / 1000.0F);
            WriteFloat(output, destination + 0x38, ToFloat(Read64(request, source + 0x58)) / divisor / 1000.0F);
        }
        std::memcpy(output.data() + destination + 0x3c, request.data() + source + 0x60, 0x10);
        std::memcpy(output.data() + destination + 0x4c, request.data() + source + 0x70, 8);
    }
}

}

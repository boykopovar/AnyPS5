#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_BUFFERFILL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_BUFFERFILL_HPP

#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace AgcDriver::DriverDetail {

inline constexpr std::array<std::uint32_t, 18> PatternFillKernel{
    0xbfa00001u, 0xd7460000u, 0x04010c0cu, 0xf4201a84u, 0xfa000000u, 0xbf8cc07fu,
    0x7da8006au, 0xbf880009u, 0xf4201a84u, 0xfa000004u, 0xbf8cc07fu, 0x3602006au,
    0xe0002000u, 0x80000101u, 0xbf8c3f70u, 0xe0102000u, 0x80010100u, 0xbf810000u
};

inline bool MatchesPatternFillKernel(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::ShaderComputeStageInfo& compute) {
    if (userData.size() != 12 || compute.numThreads != std::array<std::uint32_t, 3>{64, 1, 1} || !compute.groupIdEnable[0] || compute.threadIdComponentCount != 1) return false;
    if (code.size() < PatternFillKernel.size() || !std::equal(PatternFillKernel.begin(), PatternFillKernel.end(), code.begin())) return false;
    for (const auto first : {0u, 4u, 8u}) {
        if ((userData[first + 1] & 0xffff0000u) != (first == 8 ? 0x00100000u : 0x00040000u)) return false;
        if (userData[first + 3] != (first == 8 ? 0x0004dfacu : 0x00014004u)) return false;
    }
    return userData[2] >= 1 && userData[10] >= 1;
}

struct PatternFillRange {
    std::uint64_t source, destination, control, invocations;
    std::uint32_t records;

    [[nodiscard]] std::optional<std::size_t> UniformBytes(std::uint32_t count, std::uint32_t mask) const {
        if (mask != 0) return std::nullopt;
        const auto bytes = std::min({invocations, std::uint64_t{count}, std::uint64_t{records}}) * 4u;
        if (destination % 16 != 0 || bytes % 16 != 0) return std::nullopt;
        if (bytes != 0 && ((source < destination + bytes && destination < source + 4) || (control < destination + bytes && destination < control + 8))) return std::nullopt;
        return static_cast<std::size_t>(bytes);
    }
};

inline constexpr std::array<std::uint32_t, 18> GcnPatternFillKernel{
    0xbeeb03ffu, 0x0000000bu, 0x8f008610u, 0x4a000000u, 0xc2400d00u, 0xbf8c007fu,
    0x7d880000u, 0xbe82246au, 0x36020001u, 0xbf880007u, 0xe0002000u, 0x80010101u,
    0x7d88000au, 0xbe80246au, 0xbf8c0f70u, 0xe0102000u, 0x80020100u, 0xbf810000u
};

inline bool MatchesGcnPatternFillKernel(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::ShaderComputeStageInfo& compute) {
    if (userData.size() != 16 || compute.numThreads != std::array<std::uint32_t, 3>{64, 1, 1} || !compute.groupIdEnable[0] || compute.threadIdComponentCount != 1) return false;
    if (code.size() < GcnPatternFillKernel.size() || !std::equal(GcnPatternFillKernel.begin(), GcnPatternFillKernel.end(), code.begin())) return false;
    for (const auto first : {4u, 8u}) {
        if ((userData[first + 1] & 0xbfff0000u) != 0x00040000u || (userData[first + 3] & 0xc087f000u) != 0x00024000u) return false;
    }
    return (userData[7] & 7u) == 4u && userData[6] >= 1 && userData[14] >= 1;
}

inline std::optional<PatternFillRange> DecodeGcnPatternFillRange(std::span<const std::uint32_t> userData, std::span<const std::uint32_t> packet) {
    if (userData.size() != 16 || packet.size() < 5 || packet[2] != 1 || packet[3] != 1) return std::nullopt;
    const std::uint64_t invocations = (packet[4] & 0x20u) != 0 ? packet[1] : std::uint64_t{packet[1]} * 64u;
    if (invocations > (std::uint64_t{1} << 32u)) return std::nullopt;
    const auto base = [&](std::size_t word) { return userData[word] | (static_cast<std::uint64_t>(userData[word + 1] & 0xfffu) << 32u); };
    if (base(4) % 4 != 0 || base(12) % 4 != 0) return std::nullopt;
    return PatternFillRange{base(4), base(8), base(12), invocations, userData[10]};
}

inline std::optional<PatternFillRange> DecodePatternFillRange(std::span<const std::uint32_t> userData, std::span<const std::uint32_t> packet) {
    if (userData.size() != 12 || packet.size() < 5 || packet[2] != 1 || packet[3] != 1) return std::nullopt;
    const std::uint64_t invocations = (packet[4] & 0x20u) != 0 ? packet[1] : std::uint64_t{packet[1]} * 64u;
    if (invocations > (std::uint64_t{1} << 32u)) return std::nullopt;
    const auto base = [&](std::size_t word) { return userData[word] | (static_cast<std::uint64_t>(userData[word + 1] & 0xffffu) << 32u); };
    if (base(0) % 4 != 0 || base(8) % 4 != 0) return std::nullopt;
    return PatternFillRange{base(0), base(4), base(8), invocations, userData[6]};
}

}

#endif

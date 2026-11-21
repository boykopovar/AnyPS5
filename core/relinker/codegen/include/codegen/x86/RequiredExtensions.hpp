#ifndef CODEGEN_X86_REQUIREDEXTENSIONS_HPP
#define CODEGEN_X86_REQUIREDEXTENSIONS_HPP

#include <domain/Types.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Codegen {

enum class CpuExtension : std::uint32_t {
    Lzcnt = 1u << 0,
    Bmi1 = 1u << 1,
    Bmi2 = 1u << 2,
    Movbe = 1u << 3,
    Avx2 = 1u << 4,
    Fma = 1u << 5,
    F16c = 1u << 6
};

inline constexpr std::uint32_t AllCpuExtensions =
    static_cast<std::uint32_t>(CpuExtension::Lzcnt) | static_cast<std::uint32_t>(CpuExtension::Bmi1) |
    static_cast<std::uint32_t>(CpuExtension::Bmi2) | static_cast<std::uint32_t>(CpuExtension::Movbe) |
    static_cast<std::uint32_t>(CpuExtension::Avx2) | static_cast<std::uint32_t>(CpuExtension::Fma) |
    static_cast<std::uint32_t>(CpuExtension::F16c);

std::uint32_t RequiredExtensions(const std::uint8_t* data, std::size_t size);
std::uint32_t RequiredExtensions(const std::uint8_t* data, std::size_t size, const std::vector<Domain::CodeRange>& codeRanges);

}

#endif

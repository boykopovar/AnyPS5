#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TOOLS_ORACLECODEOBJECT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TOOLS_ORACLECODEOBJECT_HPP

#include "Recompiler.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

struct OracleKernel {
    std::vector<std::uint32_t> text;
    std::uint32_t ldsBytes;
    std::uint32_t waveSize;
    ShaderRecompiler::ShaderFloatMode floatMode;
    std::array<bool, 3> groupIdEnable;
    bool tgSizeEnable;
    std::uint32_t threadIdComponentCount;
};

OracleKernel ParseOracleCodeObject(std::span<const std::byte> bytes);

std::uint32_t ParseOracleCount(std::string_view text, const char* what);

#endif

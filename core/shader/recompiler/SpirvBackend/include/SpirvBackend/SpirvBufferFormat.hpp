#ifndef CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVBUFFERFORMAT_HPP
#define CORE_SHADER_RECOMPILIER_SPIRVBACKEND_INCLUDE_SPIRVBACKEND_SPIRVBUFFERFORMAT_HPP

#include "IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include "SpirvBackend/SpirvEmitterState.hpp"
#include <cstdint>

namespace ShaderRecompiler {

enum class SpirvFormattedSourceKind {
    Memory,
    Zero,
    One
};

struct SpirvFormattedSource {
    SpirvFormattedSourceKind kind = SpirvFormattedSourceKind::Zero;
    std::uint32_t component = 0;
};

[[nodiscard]] IrBufferFormat DecodeTBufferFormat(std::uint32_t dataFormat, std::uint32_t numberFormat);
[[nodiscard]] SpirvFormatComponentType GetFormatComponentType(IrBufferFormat format);
[[nodiscard]] SpirvBufferFormatInfo GetFormatInfo(IrBufferFormat format);
[[nodiscard]] SpirvFormattedSource ResolveFormattedSource(const SpirvBufferFormatInfo& info, std::uint32_t selector);
[[nodiscard]] std::uint32_t FormattedConstantBits(const SpirvBufferFormatInfo& info, SpirvFormattedSourceKind kind);
[[nodiscard]] std::uint32_t GetFormatComponentByteOffset(const SpirvBufferFormatInfo& info, std::uint32_t component);

}

#endif

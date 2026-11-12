#ifndef CORE_SHADER_RECOMPILER_STORAGEIMAGEFORMAT_HPP
#define CORE_SHADER_RECOMPILER_STORAGEIMAGEFORMAT_HPP

#include <cstdint>

namespace ShaderRecompiler {

enum class StorageImageFormat : std::uint8_t {
    Unknown,
    R16ui,
    R32ui,
    R8,
    R8Snorm,
    Rg8,
    Rg8Snorm,
    Rgba8,
    Rgba8Snorm,
    R16,
    R16Snorm,
    Rg16,
    Rg16Snorm,
    Rgba16,
    Rgba16Snorm,
};

inline constexpr std::uint8_t StorageImageFormatCount = static_cast<std::uint8_t>(StorageImageFormat::Rgba16Snorm) + 1u;

}

#endif

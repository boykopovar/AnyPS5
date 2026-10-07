#include "SpirvBackend/SpirvBufferFormat.hpp"
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

IrBufferFormat DecodeTBufferFormat(std::uint32_t dataFormat, std::uint32_t numberFormat) {
    if (dataFormat > 0xfu || numberFormat > 0x7u) {
        throw std::runtime_error("DecodeTBufferFormat: invalid tbuffer format fields data=" +
                                 std::to_string(dataFormat) + " number=" + std::to_string(numberFormat));
    }
    return static_cast<IrBufferFormat>((numberFormat << 4u) | dataFormat);
}

SpirvFormatComponentType GetFormatComponentType(IrBufferFormat format) {
    switch (format) {
    case IrBufferFormat::Invalid:
        return SpirvFormatComponentType::Unknown;
    case IrBufferFormat::Format8UInt:
    case IrBufferFormat::Format16UInt:
    case IrBufferFormat::Format8_8UInt:
    case IrBufferFormat::Format32UInt:
    case IrBufferFormat::Format16_16UInt:
    case IrBufferFormat::Format11_11_10UInt:
    case IrBufferFormat::Format10_11_11UInt:
    case IrBufferFormat::Format2_10_10_10UInt:
    case IrBufferFormat::Format10_10_10_2UInt:
    case IrBufferFormat::Format8_8_8_8UInt:
    case IrBufferFormat::Format32_32UInt:
    case IrBufferFormat::Format16_16_16_16UInt:
    case IrBufferFormat::Format32_32_32UInt:
    case IrBufferFormat::Format32_32_32_32UInt:
        return SpirvFormatComponentType::Uint;
    case IrBufferFormat::Format8SInt:
    case IrBufferFormat::Format16SInt:
    case IrBufferFormat::Format8_8SInt:
    case IrBufferFormat::Format32SInt:
    case IrBufferFormat::Format16_16SInt:
    case IrBufferFormat::Format11_11_10SInt:
    case IrBufferFormat::Format10_11_11SInt:
    case IrBufferFormat::Format2_10_10_10SInt:
    case IrBufferFormat::Format10_10_10_2SInt:
    case IrBufferFormat::Format8_8_8_8SInt:
    case IrBufferFormat::Format32_32SInt:
    case IrBufferFormat::Format16_16_16_16SInt:
    case IrBufferFormat::Format32_32_32SInt:
    case IrBufferFormat::Format32_32_32_32SInt:
        return SpirvFormatComponentType::Sint;
    case IrBufferFormat::Format8UNorm:
    case IrBufferFormat::Format16UNorm:
    case IrBufferFormat::Format8_8UNorm:
    case IrBufferFormat::Format16_16UNorm:
    case IrBufferFormat::Format11_11_10UNorm:
    case IrBufferFormat::Format10_11_11UNorm:
    case IrBufferFormat::Format2_10_10_10UNorm:
    case IrBufferFormat::Format10_10_10_2UNorm:
    case IrBufferFormat::Format8_8_8_8UNorm:
    case IrBufferFormat::Format16_16_16_16UNorm:
        return SpirvFormatComponentType::Unorm;
    case IrBufferFormat::Format8SNorm:
    case IrBufferFormat::Format16SNorm:
    case IrBufferFormat::Format8_8SNorm:
    case IrBufferFormat::Format16_16SNorm:
    case IrBufferFormat::Format11_11_10SNorm:
    case IrBufferFormat::Format10_11_11SNorm:
    case IrBufferFormat::Format2_10_10_10SNorm:
    case IrBufferFormat::Format10_10_10_2SNorm:
    case IrBufferFormat::Format8_8_8_8SNorm:
    case IrBufferFormat::Format16_16_16_16SNorm:
        return SpirvFormatComponentType::Snorm;
    case IrBufferFormat::Format8UScaled:
    case IrBufferFormat::Format16UScaled:
    case IrBufferFormat::Format8_8UScaled:
    case IrBufferFormat::Format16_16UScaled:
    case IrBufferFormat::Format11_11_10UScaled:
    case IrBufferFormat::Format10_11_11UScaled:
    case IrBufferFormat::Format2_10_10_10UScaled:
    case IrBufferFormat::Format10_10_10_2UScaled:
    case IrBufferFormat::Format8_8_8_8UScaled:
    case IrBufferFormat::Format16_16_16_16UScaled:
        return SpirvFormatComponentType::Uscaled;
    case IrBufferFormat::Format8SScaled:
    case IrBufferFormat::Format16SScaled:
    case IrBufferFormat::Format8_8SScaled:
    case IrBufferFormat::Format16_16SScaled:
    case IrBufferFormat::Format11_11_10SScaled:
    case IrBufferFormat::Format10_11_11SScaled:
    case IrBufferFormat::Format2_10_10_10SScaled:
    case IrBufferFormat::Format10_10_10_2SScaled:
    case IrBufferFormat::Format8_8_8_8SScaled:
    case IrBufferFormat::Format16_16_16_16SScaled:
        return SpirvFormatComponentType::Sscaled;
    case IrBufferFormat::Format16Float:
    case IrBufferFormat::Format32Float:
    case IrBufferFormat::Format16_16Float:
    case IrBufferFormat::Format11_11_10Float:
    case IrBufferFormat::Format10_11_11Float:
    case IrBufferFormat::Format32_32Float:
    case IrBufferFormat::Format16_16_16_16Float:
    case IrBufferFormat::Format32_32_32Float:
    case IrBufferFormat::Format32_32_32_32Float:
        return SpirvFormatComponentType::Float;
    default:
        throw std::runtime_error("GetFormatComponentType: unsupported buffer format " +
                                 std::to_string(static_cast<std::uint32_t>(format)));
    }
}

SpirvBufferFormatInfo GetFormatInfo(IrBufferFormat format) {
    if (format == IrBufferFormat::Invalid) {
        return {};
    }
    const auto type = GetFormatComponentType(format);
    switch (format) {
    case IrBufferFormat::Format8UNorm:
    case IrBufferFormat::Format8SNorm:
    case IrBufferFormat::Format8UScaled:
    case IrBufferFormat::Format8SScaled:
    case IrBufferFormat::Format8UInt:
    case IrBufferFormat::Format8SInt:
        return {format, type, 1u, 1u, {8u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u}, false};
    case IrBufferFormat::Format16UNorm:
    case IrBufferFormat::Format16SNorm:
    case IrBufferFormat::Format16UScaled:
    case IrBufferFormat::Format16SScaled:
    case IrBufferFormat::Format16UInt:
    case IrBufferFormat::Format16SInt:
    case IrBufferFormat::Format16Float:
        return {format, type, 1u, 2u, {16u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u}, false};
    case IrBufferFormat::Format8_8UNorm:
    case IrBufferFormat::Format8_8SNorm:
    case IrBufferFormat::Format8_8UScaled:
    case IrBufferFormat::Format8_8SScaled:
    case IrBufferFormat::Format8_8UInt:
    case IrBufferFormat::Format8_8SInt:
        return {format, type, 2u, 2u, {8u, 8u, 0u, 0u}, {0u, 8u, 0u, 0u}, false};
    case IrBufferFormat::Format32UInt:
    case IrBufferFormat::Format32SInt:
    case IrBufferFormat::Format32Float:
        return {format, type, 1u, 4u, {32u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u}, false};
    case IrBufferFormat::Format16_16UNorm:
    case IrBufferFormat::Format16_16SNorm:
    case IrBufferFormat::Format16_16UScaled:
    case IrBufferFormat::Format16_16SScaled:
    case IrBufferFormat::Format16_16UInt:
    case IrBufferFormat::Format16_16SInt:
    case IrBufferFormat::Format16_16Float:
        return {format, type, 2u, 4u, {16u, 16u, 0u, 0u}, {0u, 16u, 0u, 0u}, false};
    case IrBufferFormat::Format11_11_10UNorm:
    case IrBufferFormat::Format11_11_10SNorm:
    case IrBufferFormat::Format11_11_10UScaled:
    case IrBufferFormat::Format11_11_10SScaled:
    case IrBufferFormat::Format11_11_10UInt:
    case IrBufferFormat::Format11_11_10SInt:
    case IrBufferFormat::Format11_11_10Float:
        return {format, type, 3u, 4u, {11u, 11u, 10u, 0u}, {0u, 11u, 22u, 0u}, true};
    case IrBufferFormat::Format10_11_11UNorm:
    case IrBufferFormat::Format10_11_11SNorm:
    case IrBufferFormat::Format10_11_11UScaled:
    case IrBufferFormat::Format10_11_11SScaled:
    case IrBufferFormat::Format10_11_11UInt:
    case IrBufferFormat::Format10_11_11SInt:
    case IrBufferFormat::Format10_11_11Float:
        return {format, type, 3u, 4u, {10u, 11u, 11u, 0u}, {0u, 10u, 21u, 0u}, true};
    case IrBufferFormat::Format2_10_10_10UNorm:
    case IrBufferFormat::Format2_10_10_10SNorm:
    case IrBufferFormat::Format2_10_10_10UScaled:
    case IrBufferFormat::Format2_10_10_10SScaled:
    case IrBufferFormat::Format2_10_10_10UInt:
    case IrBufferFormat::Format2_10_10_10SInt:
        return {format, type, 4u, 4u, {2u, 10u, 10u, 10u}, {0u, 2u, 12u, 22u}, true};
    case IrBufferFormat::Format10_10_10_2UNorm:
    case IrBufferFormat::Format10_10_10_2SNorm:
    case IrBufferFormat::Format10_10_10_2UScaled:
    case IrBufferFormat::Format10_10_10_2SScaled:
    case IrBufferFormat::Format10_10_10_2UInt:
    case IrBufferFormat::Format10_10_10_2SInt:
        return {format, type, 4u, 4u, {10u, 10u, 10u, 2u}, {0u, 10u, 20u, 30u}, true};
    case IrBufferFormat::Format8_8_8_8UNorm:
    case IrBufferFormat::Format8_8_8_8SNorm:
    case IrBufferFormat::Format8_8_8_8UScaled:
    case IrBufferFormat::Format8_8_8_8SScaled:
    case IrBufferFormat::Format8_8_8_8UInt:
    case IrBufferFormat::Format8_8_8_8SInt:
        return {format, type, 4u, 4u, {8u, 8u, 8u, 8u}, {0u, 8u, 16u, 24u}, false};
    case IrBufferFormat::Format32_32UInt:
    case IrBufferFormat::Format32_32SInt:
    case IrBufferFormat::Format32_32Float:
        return {format, type, 2u, 8u, {32u, 32u, 0u, 0u}, {0u, 32u, 0u, 0u}, false};
    case IrBufferFormat::Format16_16_16_16UNorm:
    case IrBufferFormat::Format16_16_16_16SNorm:
    case IrBufferFormat::Format16_16_16_16UScaled:
    case IrBufferFormat::Format16_16_16_16SScaled:
    case IrBufferFormat::Format16_16_16_16UInt:
    case IrBufferFormat::Format16_16_16_16SInt:
    case IrBufferFormat::Format16_16_16_16Float:
        return {format, type, 4u, 8u, {16u, 16u, 16u, 16u}, {0u, 16u, 32u, 48u}, false};
    case IrBufferFormat::Format32_32_32UInt:
    case IrBufferFormat::Format32_32_32SInt:
    case IrBufferFormat::Format32_32_32Float:
        return {format, type, 3u, 12u, {32u, 32u, 32u, 0u}, {0u, 32u, 64u, 0u}, false};
    case IrBufferFormat::Format32_32_32_32UInt:
    case IrBufferFormat::Format32_32_32_32SInt:
    case IrBufferFormat::Format32_32_32_32Float:
        return {format, type, 4u, 16u, {32u, 32u, 32u, 32u}, {0u, 32u, 64u, 96u}, false};
    default:
        throw std::runtime_error("GetFormatInfo: unsupported buffer format " +
                                 std::to_string(static_cast<std::uint32_t>(format)));
    }
}

SpirvFormattedSource ResolveFormattedSource(const SpirvBufferFormatInfo& info, std::uint32_t selector) {
    if (selector == 0u) {
        return {SpirvFormattedSourceKind::Zero, 0u};
    }
    if (selector == 1u) {
        return {SpirvFormattedSourceKind::One, 0u};
    }
    if (selector < 4u || selector > 7u) {
        throw std::runtime_error("ResolveFormattedSource: reserved dst_sel " + std::to_string(selector));
    }
    if (info.componentCount == 0u) {
        throw std::runtime_error("ResolveFormattedSource: format has no components");
    }
    return {SpirvFormattedSourceKind::Memory, (selector - 4u) % info.componentCount};
}

std::uint32_t FormattedConstantBits(const SpirvBufferFormatInfo& info, SpirvFormattedSourceKind kind) {
    switch (kind) {
    case SpirvFormattedSourceKind::Zero:
        return 0u;
    case SpirvFormattedSourceKind::One:
        if (info.type == SpirvFormatComponentType::Unknown) {
            throw std::runtime_error("FormattedConstantBits: format type is unknown");
        }
        return info.type == SpirvFormatComponentType::Uint || info.type == SpirvFormatComponentType::Sint ? 1u
                                                                                                          : 0x3f800000u;
    default:
        throw std::runtime_error("FormattedConstantBits: source kind has no constant");
    }
}

std::uint32_t GetFormatComponentByteOffset(const SpirvBufferFormatInfo& info, std::uint32_t component) {
    if (component >= info.componentCount) {
        throw std::runtime_error("GetFormatComponentByteOffset: component " + std::to_string(component) +
                                 " is out of range");
    }
    return info.packedBitfield ? 0u : info.componentBitOffset[component] / 8u;
}

}

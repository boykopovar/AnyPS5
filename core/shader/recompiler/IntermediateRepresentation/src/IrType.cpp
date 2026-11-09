#include "IntermediateRepresentation/IrType.hpp"
#include <array>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

constexpr std::uint32_t FloatTypeMask = static_cast<std::uint32_t>(IrType::F16) | static_cast<std::uint32_t>(IrType::F32) | static_cast<std::uint32_t>(IrType::F64) | static_cast<std::uint32_t>(IrType::Vec2F32) | static_cast<std::uint32_t>(IrType::Vec3F32) | static_cast<std::uint32_t>(IrType::Vec4F32);

constexpr std::uint32_t IntegerTypeMask = static_cast<std::uint32_t>(IrType::Bool) | static_cast<std::uint32_t>(IrType::U8) | static_cast<std::uint32_t>(IrType::U16) | static_cast<std::uint32_t>(IrType::U32) | static_cast<std::uint32_t>(IrType::S32) | static_cast<std::uint32_t>(IrType::U64) | static_cast<std::uint32_t>(IrType::S64) | static_cast<std::uint32_t>(IrType::Vec2U32) | static_cast<std::uint32_t>(IrType::Vec3U32) | static_cast<std::uint32_t>(IrType::Vec4U32);

constexpr std::uint32_t VectorTypeMask = static_cast<std::uint32_t>(IrType::Vec2U32) | static_cast<std::uint32_t>(IrType::Vec3U32) | static_cast<std::uint32_t>(IrType::Vec4U32) | static_cast<std::uint32_t>(IrType::Vec2F32) | static_cast<std::uint32_t>(IrType::Vec3F32) | static_cast<std::uint32_t>(IrType::Vec4F32);

}

bool IsFloatType(IrType type) {
    return (static_cast<std::uint32_t>(type) & FloatTypeMask) != 0u;
}

bool IsIntegerType(IrType type) {
    return (static_cast<std::uint32_t>(type) & IntegerTypeMask) != 0u;
}

bool IsVectorType(IrType type) {
    return (static_cast<std::uint32_t>(type) & VectorTypeMask) != 0u;
}

std::uint32_t ComponentCount(IrType type) {
    switch (type) {
        case IrType::Bool:
        case IrType::U8:
        case IrType::U16:
        case IrType::U32:
        case IrType::S32:
        case IrType::F16:
        case IrType::F32:
        case IrType::U64:
        case IrType::S64:
        case IrType::F64:
            return 1u;
        case IrType::Vec2U32:
        case IrType::Vec2F32:
            return 2u;
        case IrType::Vec3U32:
        case IrType::Vec3F32:
            return 3u;
        case IrType::Vec4U32:
        case IrType::Vec4F32:
            return 4u;
        default:
            throw std::runtime_error("ComponentCount does not accept this type");
    }
}

std::string TypeToString(IrType type) {
    static constexpr std::array<const char*, 26> names = {
        "Bool", "U32", "S32", "F32", "U64", "S64", "F64", "Vec2U32", "Vec3U32", "Vec4U32",
        "Vec2F32", "Vec3F32", "Vec4F32", "Label", "Opaque", "ScalarReg", "VectorReg", "U8",
        "U16", "F16", "SrtResource", "BufferResource", "AddressResource", "ImageResource",
        "SamplerResource", "ImageAddress"
    };
    const auto bits = static_cast<std::uint32_t>(type);
    if (bits == 0u) {
        return "Void";
    }
    std::string result;
    for (std::uint32_t index = 0; index < names.size(); index++) {
        if ((bits & (1u << index)) == 0u) {
            continue;
        }
        if (!result.empty()) {
            result += '|';
        }
        result += names[index];
    }
    return result;
}

IrType operator|(IrType lhs, IrType rhs) {
    return static_cast<IrType>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

IrType operator&(IrType lhs, IrType rhs) {
    return static_cast<IrType>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

bool TypesOverlap(IrType lhs, IrType rhs) {
    return (lhs & rhs) != IrType::Void;
}

bool AreTypesCompatible(IrType lhs, IrType rhs) {
    return lhs == rhs || lhs == IrType::Opaque || rhs == IrType::Opaque || TypesOverlap(lhs, rhs);
}

}

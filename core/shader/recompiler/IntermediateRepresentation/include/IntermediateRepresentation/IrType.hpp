#ifndef CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRTYPE_HPP
#define CORE_SHADER_RECOMPILIER_INTERMEDIATEREPRESENTATION_INCLUDE_INTERMEDIATEREPRESENTATION_IRTYPE_HPP

#include <cstdint>
#include <string>

namespace ShaderRecompiler {

enum class IrType : std::uint32_t {
    Void = 0,
    Bool = 1u << 0u,
    U32 = 1u << 1u,
    S32 = 1u << 2u,
    F32 = 1u << 3u,
    U64 = 1u << 4u,
    S64 = 1u << 5u,
    F64 = 1u << 6u,
    Vec2U32 = 1u << 7u,
    Vec3U32 = 1u << 8u,
    Vec4U32 = 1u << 9u,
    Vec2F32 = 1u << 10u,
    Vec3F32 = 1u << 11u,
    Vec4F32 = 1u << 12u,
    Label = 1u << 13u,
    Opaque = 1u << 14u,
    ScalarReg = 1u << 15u,
    VectorReg = 1u << 16u,
    U8 = 1u << 17u,
    U16 = 1u << 18u,
    F16 = 1u << 19u,
    SrtResource = 1u << 20u,
    BufferResource = 1u << 21u,
    AddressResource = 1u << 22u,
    ImageResource = 1u << 23u,
    SamplerResource = 1u << 24u,
    ImageAddress = 1u << 25u,
    U1 = Bool,
    U32x2 = Vec2U32,
    U32x3 = Vec3U32,
    U32x4 = Vec4U32,
    F32x2 = Vec2F32
};

[[nodiscard]] bool IsFloatType(IrType type);
[[nodiscard]] bool IsIntegerType(IrType type);
[[nodiscard]] bool IsVectorType(IrType type);
[[nodiscard]] std::uint32_t ComponentCount(IrType type);
[[nodiscard]] std::string TypeToString(IrType type);

[[nodiscard]] IrType operator|(IrType lhs, IrType rhs);
[[nodiscard]] IrType operator&(IrType lhs, IrType rhs);
[[nodiscard]] bool TypesOverlap(IrType lhs, IrType rhs);
[[nodiscard]] bool AreTypesCompatible(IrType lhs, IrType rhs);

}

#endif

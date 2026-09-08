#ifndef CORE_LIBS_LIBSCEAGC_SHADER_SHADERS_HPP
#define CORE_LIBS_LIBSCEAGC_SHADER_SHADERS_HPP

#include <cstdint>

namespace ShaderRegs {

constexpr std::uint32_t SPI_PS_INPUT_CNTL_0 = 0x191u;

constexpr std::uint32_t SPI_SHADER_PGM_LO_PS = 0x008u;
constexpr std::uint32_t SPI_SHADER_PGM_LO_GS = 0x088u;
constexpr std::uint32_t SPI_SHADER_PGM_LO_ES = 0x0C8u;
constexpr std::uint32_t SPI_SHADER_PGM_LO_HS = 0x108u;
constexpr std::uint32_t SPI_SHADER_PGM_LO_LS = 0x148u;
constexpr std::uint32_t COMPUTE_PGM_LO = 0x20Cu;

constexpr std::uint32_t VGT_GS_OUT_PRIM_TYPE = 0x29Bu;
constexpr std::uint32_t VGT_SHADER_STAGES_EN = 0x2D5u;
constexpr std::uint32_t VGT_PRIMITIVE_TYPE = 0x242u;

constexpr std::uint32_t GE_CNTL = 0x25Bu;
constexpr std::uint32_t GE_USER_VGPR_EN = 0x262u;

constexpr std::uint32_t SHADER_FILE_HEADER_MAGIC = 0x34333231u;
constexpr std::uint32_t SHADER_VERSION = 0x00000018u;

constexpr std::uint64_t SHADER_BASE_ALIGN_MASK = 0xFFFF0000000000FFull;

constexpr int GRAPHICS5_ERROR_INVALID_SHADER_PROGRAM = static_cast<int>(0x8a6c0005u);

enum class ShaderBinaryType : std::uint8_t {
    Cs = 0,
    Ps = 1,
    Gs = 2,
    Hs = 3,
    GsFront = 4,
    HsFront = 5,
    GsBack = 6,
    HsBack = 7,
    Fs = 8,
};

enum class GsOutputPrimitiveType : std::uint32_t {
    Points = 0,
    Lines = 1,
    Triangles = 2,
    Rectangle2D = 3,
    RectList = 4,
};

enum class PrimitiveType : std::uint32_t {
    None = 0,
    PointList = 1,
    LineList = 2,
    LineStrip = 3,
    TriList = 4,
    TriFan = 5,
    TriStrip = 6,
    RectList = 7,
    Patch = 9,
    LineListAdjacency = 10,
    LineStripAdjacency = 11,
    TriListAdjacency = 12,
    TriStripAdjacency = 13,
    RectListLegacy = 17,
    LineLoop = 18,
    QuadListLegacy = 19,
    QuadStripLegacy = 20,
    Polygon = 21,
};

constexpr std::uint32_t VGT_SHADER_STAGES_GS_BIT = 0x20u;
constexpr std::uint32_t VGT_SHADER_STAGES_NGG_BIT = 0x04u;

}

#endif

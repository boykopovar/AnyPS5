#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAINSTRUCTION_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_RDNAINSTRUCTION_HPP

#include "RdnaDecoder/RdnaOpcode.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace ShaderRecompiler {

class UnsupportedInstructionError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class RdnaInstructionFamily {
    Unknown,
    SOP1,
    SOP2,
    SOPK,
    SOPC,
    SOPP,
    VOP1,
    VOP2,
    VOP3,
    VOP3P,
    VOPC,
    VINTRP,
    SMEM,
    MUBUF,
    MTBUF,
    FLAT,
    DS,
    MIMG,
    EXP
};

enum class RdnaOperandKind {
    None,
    ScalarRegister,
    VectorRegister,
    VccLo,
    VccHi,
    ExecLo,
    ExecHi,
    Scc,
    Null,
    LiteralConstant,
    IntegerInlineConstant,
    FloatInlineConstant,
    Unknown,
    VccZ,
    ExecZ,
    M0,
    PopsExitingWaveId,
    SrcSharedBase,
    SrcSharedLimit,
    SrcPrivateBase,
    SrcPrivateLimit
};

enum class RdnaImageDimension : std::uint32_t {
    Unknown,
    Dim1D,
    Dim1DArray,
    Dim2D,
    Dim3D,
    Dim2DArray,
    Dim2DMsaa,
    Dim2DMsaaArray
};

enum RdnaImageSampleFlag : std::uint32_t {
    RdnaImageSampleFlagLod = 1u << 0u,
    RdnaImageSampleFlagBias = 1u << 1u,
    RdnaImageSampleFlagDerivative = 1u << 2u,
    RdnaImageSampleFlagCompare = 1u << 3u,
    RdnaImageSampleFlagOffset = 1u << 4u,
    RdnaImageSampleFlagLevelZero = 1u << 5u,
    RdnaImageSampleFlagLodClamp = 1u << 6u,
    RdnaImageSampleFlagA16 = 1u << 7u,
    RdnaImageSampleFlagCd = 1u << 8u,
    RdnaImageSampleFlagGatherHorizontal = 1u << 9u,
    RdnaImageSampleFlagAdjust = 1u << 10u,
    RdnaImageSampleFlagG16 = 1u << 11u,
};

inline constexpr std::uint32_t RdnaImageSampleGradientCountShift = 12u;
inline constexpr std::uint32_t RdnaImageSampleGradientCountMask = 3u << RdnaImageSampleGradientCountShift;

inline constexpr std::uint32_t MaxRdnaInstructionRawWords = 5;
inline constexpr std::uint32_t MaxRdnaImageNsaAddressComponents = 12;

struct RdnaOperand {
    RdnaOperandKind kind = RdnaOperandKind::Unknown;
    std::uint32_t value = 0;
    std::int32_t signedVal = 0;
    std::uint32_t reg = 0;
    std::uint32_t sdwaSel = 6;
    std::uint32_t sdwaDstUnused = 2;
    std::uint32_t omod = 0;
    std::uint32_t dppCtrl = 0;
    std::uint32_t dppRowMask = 0xf;
    std::uint32_t dppBankMask = 0xf;
    bool explicitSdwaDst = false;
    bool sdwaSext = false;
    bool dppFetchInactive = false;
    bool dppBoundCtrl = false;
    bool opSel = false;
    bool opSelHi = false;
    bool negate = false;
    bool negateHi = false;
    bool absolute = false;
    bool clamp = false;
    bool dpp = false;
    bool dpp8 = false;
};

struct RdnaInstruction {
    RdnaOpcode op = RdnaOpcode::Unknown;
    std::uint32_t programCounter = 0;
    RdnaOperand destination{};
    RdnaOperand source0{};
    RdnaOperand source1{};
    RdnaOperand source2{};
    RdnaOperand source3{};
    std::int32_t branchOffset = 0;
    std::uint32_t memoryOffset = 0;
    std::uint32_t dataDwordCount = 1;
    bool clampResult = false;
    bool is64Bit = false;
    std::uint32_t exportTarget = 0;
    std::uint32_t exportEnableMask = 0;
    bool exportIsCompressed = false;
    bool exportIsLast = false;
    std::uint32_t imageOpcodeId = 0;
    std::uint32_t imageDmask = 0;
    RdnaImageDimension imageDimension = RdnaImageDimension::Unknown;
    bool imageR128 = false;
    bool imageGlc = false;
    bool imageSlc = false;
    bool imageA16 = false;
    bool imageD16 = false;
    std::uint32_t imageNsaDwordCount = 0;
    std::array<std::uint32_t, MaxRdnaImageNsaAddressComponents> imageNsaVectorRegisters{};
    std::uint32_t wordCount = 1;
    std::array<std::uint32_t, MaxRdnaInstructionRawWords> rawWords{};
    RdnaInstructionFamily family = RdnaInstructionFamily::Unknown;
    std::uint32_t opcodeId = 0;
    RdnaOperand destination2{};
    std::uint32_t sourceCount = 0;
    std::uint32_t secondaryOffset = 0;
    std::uint32_t dataComponents = 0;
    std::uint32_t dataBits = 32;
    std::uint32_t dataFormat = 0;
    std::uint32_t numberFormat = 0;
    std::uint32_t imageSampleFlags = 0;
    std::uint32_t imageAddressComponents = 0;
    std::uint32_t memorySegment = 0;
    bool dataSigned = false;
    bool typed = false;
    bool formatted = false;
    bool gds = false;
    bool glc = false;
    bool dlc = false;
    bool slc = false;
    bool idxen = false;
    bool offen = false;
    std::uint32_t branchTarget = 0;
    bool exportValidMask = false;
    std::string_view unsupportedReason{};
};

}

#endif

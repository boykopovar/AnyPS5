#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_STATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_STATE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include <vector>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include "Recompiler.hpp"

namespace AgcDriver::Graphics {

enum class ShaderPath {
    Vertex,
    Geometry,
    Tessellation,
    TessellationGeometry
};

struct ShaderStages {
    ShaderPath path;
    std::uint32_t registerValue;
    std::uint32_t vertexWaveSize;
    std::uint32_t fragmentWaveSize;
    std::optional<ShaderRecompiler::MeshConfiguration> mesh;
    std::optional<ShaderRecompiler::TessellationConfiguration> tessellation;
};

struct ColorTarget {
    std::uint64_t address;
    VkExtent2D extent;
    VkFormat format;
    std::size_t bytes;
    std::uint8_t componentMapping;
    ColorTileMode tileMode = ColorTileMode::Linear;
    std::uint32_t elementBytes = 4;
    // DCC metadata of a compressed target (CB_COLOR_INFO DCC_ENABLE), or 0 (see DccMetadata.hpp).
    std::uint64_t dccAddress = 0;
    bool dccAlphaOnMsb = false;
    std::uint64_t surfaceAddress = 0;
    VkExtent2D surfaceExtent{};
    std::uint32_t mipCount = 1;
    std::uint32_t mip = 0;
    bool mipTail = false;
    std::array<std::uint32_t, 2> clearWords{};
};

struct DepthTarget {
    std::uint64_t address;
    std::uint64_t stencilAddress;
    VkExtent2D extent;
    VkFormat format;
    float clearDepth;
    std::uint8_t clearStencil;
};

struct State {
    ShaderStages stages;
    std::optional<DepthTarget> depth;
    bool depthTest = false;
    bool depthWrite = false;
    VkCompareOp depthCompare = VK_COMPARE_OP_ALWAYS;
    bool depthBoundsTest = false;
    float minDepthBounds = 0.0f;
    float maxDepthBounds = 1.0f;
    bool depthBias = false;
    float depthBiasConstant = 0.0f;
    float depthBiasSlope = 0.0f;
    float depthBiasClamp = 0.0f;
    bool stencilTest = false;
    VkStencilOpState stencilFront{};
    VkStencilOpState stencilBack{};
    // MRT slot 0; `colors`/`blends` hold every written slot, attachment i being slot i.
    ColorTarget color;
    std::vector<ColorTarget> colors;
    std::vector<VkPipelineColorBlendAttachmentState> blends;
    bool hasColorTarget;
    bool rectList = false;
    VkExtent2D renderExtent;
    VkPrimitiveTopology topology;
    bool primitiveRestart = false;
    VkViewport viewport;
    bool negativeOneToOne;
    bool depthClamp = false;
    VkRect2D scissor;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    VkPipelineColorBlendAttachmentState blend;
    std::array<float, 4> blendConstants;
};

ShaderStages DecodeShaderStages(const QueueState& queue);
State DecodeState(const QueueState& queue);
std::array<std::uint8_t, 8> ExportMappings(const State& state);
ColorTarget DecodeColorBuffer(const Registers& context, std::uint32_t slot);

struct ColorMetadataPass {
    enum class Mode { EliminateFastClear, DccDecompress };
    Mode mode;
    std::vector<ColorTarget> targets;
};
std::optional<ColorMetadataPass> DecodeColorMetadataPass(const QueueState& queue);
// The message DecodeState (or the pixel stage decode after it) would throw for the register rules
// this precheck covers, evaluated without exceptions before the draw is decoded; empty when they
// pass (DecodeState still checks everything). A register a rule needs that is absent is no verdict.
std::string DrawRejection(const QueueState& queue, bool indexed);

// The recording facade of the draw decoders (design_cpu_final M8, step 8a): every register read
// of DecodeShaderStages, DecodeState, DrawRejection, DecodePixelStageInfo and Driver::draw's
// program prepare goes through NoteRegisterRead, which appends the bank and offset to the calling
// thread's log while one is set (null in production; Driver::draw sets one on a miss under
// APS5_VERIFY_DRAW_RECIPE=1 and checks the log against DrawKeyRegisters).
enum class RegisterBank : std::uint8_t { Context, Shader, UserConfig, Count };
struct RegisterRead {
    RegisterBank bank;
    std::uint32_t offset;
};
std::vector<RegisterRead>*& RegisterReadLog();
inline void NoteRegisterRead(RegisterBank bank, std::uint32_t offset) {
    if (auto* log = RegisterReadLog()) log->push_back({bank, offset});
}
inline const char* RegisterBankName(RegisterBank bank) {
    return bank == RegisterBank::Context ? "context" : bank == RegisterBank::Shader ? "shader" : "user-config";
}

// The frozen table of the registers the draw decoders read, as [first, first + count) per bank in
// bank and offset order: the draw key (Driver::draw) hashes every present register of these
// ranges with its offset, so the decoded state, pixel and program inputs are a pure function of
// the key. A range covers whole register groups (the eight CB_COLOR slots, the 32 user words of
// each bank, the interpolator controls) whether or not a draw's slot count or user count reaches
// them: a stricter key, never a wrong one. A register the decoders read that the table lacks would
// be a wrong hit, which the facade's log finds (DrawKeyCovers) on every verified miss.
struct DrawKeyRange {
    RegisterBank bank;
    std::uint32_t first;
    std::uint32_t count;
};
inline constexpr std::array<DrawKeyRange, 45> DrawKeyRegisters{{
    {RegisterBank::Context, 0x000, 1}, {RegisterBank::Context, 0x002, 1}, {RegisterBank::Context, 0x007, 7}, {RegisterBank::Context, 0x010, 6}, {RegisterBank::Context, 0x01a, 4},
    {RegisterBank::Context, 0x080, 4}, {RegisterBank::Context, 0x08c, 4}, {RegisterBank::Context, 0x090, 2}, {RegisterBank::Context, 0x094, 2}, {RegisterBank::Context, 0x0b4, 2}, {RegisterBank::Context, 0x105, 4}, {RegisterBank::Context, 0x10b, 3}, {RegisterBank::Context, 0x10f, 6},
    // SPI_PS_INPUT_CNTL_0..31, SPI_PS_INPUT_ENA/ADDR, SPI_PS_IN_CONTROL, SPI_SHADER_POS/Z/COL_FORMAT,
    // CB_BLEND0..7_CONTROL, GE_MAX_OUTPUT_PER_SUBGROUP.
    {RegisterBank::Context, 0x191, 32}, {RegisterBank::Context, 0x1b3, 2}, {RegisterBank::Context, 0x1b6, 1}, {RegisterBank::Context, 0x1c3, 3}, {RegisterBank::Context, 0x1e0, 8}, {RegisterBank::Context, 0x1ff, 1},
    // DB_DEPTH_CONTROL .. PA_CL_VS_OUT_CNTL, PA_SC_MODE_CNTL_0/1, VGT_GS_MODE, VGT_GS_VERT_ITEMSIZE,
    // PA_SU_VTX_CNTL, the sample masks, PA_SC_CONSERVATIVE_RASTERIZATION_CNTL.
    {RegisterBank::Context, 0x200, 8}, {RegisterBank::Context, 0x292, 2}, {RegisterBank::Context, 0x29b, 1}, {RegisterBank::Context, 0x2ab, 1}, {RegisterBank::Context, 0x2ce, 1}, {RegisterBank::Context, 0x2d5, 2}, {RegisterBank::Context, 0x2db, 2}, {RegisterBank::Context, 0x2de, 6}, {RegisterBank::Context, 0x2f8, 2}, {RegisterBank::Context, 0x30e, 2}, {RegisterBank::Context, 0x313, 1},
    // CB_COLOR0..7_BASE .. DCC_BASE (15 words a slot), CB_COLOR0..7_BASE_EXT, DCC_BASE_EXT, ATTRIB2, ATTRIB3.
    {RegisterBank::Context, 0x318, 0x78}, {RegisterBank::Context, 0x390, 8}, {RegisterBank::Context, 0x3a8, 0x18},
    // The pixel program address, RSRC2 and user words; the geometry-back user pointer and program
    // address; the vertex/geometry-front RSRC1/RSRC2 and user words; the vertex program address;
    // the hull user pointer, program address, RSRC2 and user words; the local program address.
    {RegisterBank::Shader, 0x008, 0x24}, {RegisterBank::Shader, 0x082, 2}, {RegisterBank::Shader, 0x088, 2}, {RegisterBank::Shader, 0x08a, 0x22}, {RegisterBank::Shader, 0x0c8, 2}, {RegisterBank::Shader, 0x102, 2}, {RegisterBank::Shader, 0x108, 2}, {RegisterBank::Shader, 0x10b, 0x21}, {RegisterBank::Shader, 0x148, 2},
    // GE_PRIM_TYPE, GE_MULTI_PRIM_IB_RESET_EN, the geometry subgroup sizes.
    {RegisterBank::UserConfig, 0x242, 1}, {RegisterBank::UserConfig, 0x24b, 1}, {RegisterBank::UserConfig, 0x25b, 1},
}};
// Whether DrawKeyRegisters holds the read.
bool DrawKeyCovers(RegisterRead read);

}

#endif

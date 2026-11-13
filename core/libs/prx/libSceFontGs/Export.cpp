#include "prx/libSceFontGs/include/InventoryExports.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include <array>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int FontGsFatal = static_cast<int>(0x80460001);
constexpr int FontGsInvalidParameter = static_cast<int>(0x80460002);
constexpr int FontGsInvalidSequence = static_cast<int>(0x8046009a);
constexpr int FontGsSequenceLocked = static_cast<int>(0x804600b8);
constexpr std::uint8_t FontGnmSurfaceTag = 0x48;
constexpr std::uint16_t FontGnmSequenceTag = 0xf80;
constexpr float FontGnmDrawupLimit = 16834.0f;
constexpr std::size_t FontGnmRecordStateOffset = 0x10;

struct FontGnmSurface {
    void* head;
    std::uint8_t reserved8[5];
    std::uint8_t tag;
    std::uint8_t reserved0e;
    std::uint8_t mode;
    std::uint8_t reserved10[0xc];
    std::uint32_t busy;
    std::uint8_t reserved20[8];
    const void* fillObject;
    std::uint8_t reserved30[8];
    std::uint32_t initialized;
};

static_assert(offsetof(FontGnmSurface, tag) == 0x0d);
static_assert(offsetof(FontGnmSurface, mode) == 0x0f);
static_assert(offsetof(FontGnmSurface, busy) == 0x1c);
static_assert(offsetof(FontGnmSurface, fillObject) == 0x28);
static_assert(offsetof(FontGnmSurface, initialized) == 0x38);

struct FontGnmCommands {
    std::uint8_t reserved[0x28];
    const void* first;
    std::uint32_t firstSize;
    std::uint32_t secondCount;
    const void* second;
};

static_assert(offsetof(FontGnmCommands, first) == 0x28);
static_assert(offsetof(FontGnmCommands, firstSize) == 0x30);
static_assert(offsetof(FontGnmCommands, secondCount) == 0x34);
static_assert(offsetof(FontGnmCommands, second) == 0x38);

struct FontGnmSequence {
    std::uint16_t tag;
    std::uint8_t reserved2[0x19];
    std::uint8_t lock;
    std::uint8_t reserved1c[0xc];
    FontGnmCommands* commands;
};

static_assert(offsetof(FontGnmSequence, lock) == 0x1b);
static_assert(offsetof(FontGnmSequence, commands) == 0x28);

constexpr int FontGsSurfaceConfigInvalid = static_cast<int>(0x8046005c);
constexpr std::uint8_t FontGnmSurfaceConfigTag = 0x44;

enum GnmArrayModeValue : std::uint32_t {
    GnmArrayModeLinearGeneral = 0,
    GnmArrayModeLinearAligned = 1,
    GnmArrayMode1dTiledThin1 = 2,
    GnmArrayMode1dTiledThick = 3,
    GnmArrayMode2dTiledThin1 = 4,
    GnmArrayModePrtTiledThin1 = 5,
    GnmArrayModePrt2dTiledThin1 = 6,
    GnmArrayMode2dTiledThick = 7,
    GnmArrayMode2dTiledXThick = 8,
    GnmArrayModePrtTiledThick = 9,
    GnmArrayModePrt2dTiledThick = 10,
    GnmArrayModePrt3dTiledThin1 = 11,
    GnmArrayMode3dTiledThin1 = 12,
    GnmArrayMode3dTiledThick = 13,
    GnmArrayMode3dTiledXThick = 14,
    GnmArrayModePrt3dTiledThick = 15,
};

enum GnmMicroTileModeValue : std::uint32_t {
    GnmMicroTileModeDisplay = 0,
    GnmMicroTileModeThin = 1,
    GnmMicroTileModeDepth = 2,
    GnmMicroTileModeRotated = 3,
    GnmMicroTileModeThick = 4,
};

enum GnmPipeConfigValue : std::uint32_t {
    GnmPipeConfigP2 = 0,
    GnmPipeConfigP8_32x32_8x16 = 0xa,
    GnmPipeConfigP8_32x32_16x16 = 0xc,
    GnmPipeConfigNeoP16 = 0x12,
};

enum GnmTileSplitValue : std::uint32_t {
    GnmTileSplit64B = 0,
    GnmTileSplit128B = 1,
    GnmTileSplit256B = 2,
    GnmTileSplit512B = 3,
    GnmTileSplit1KB = 4,
};

enum GnmSampleSplitValue : std::uint32_t {
    GnmSampleSplit1 = 0,
    GnmSampleSplit2 = 1,
};

constexpr std::uint32_t GnmTileModeWord(std::uint32_t arrayMode, std::uint32_t pipeConfig, std::uint32_t tileSplit, std::uint32_t microMode,
                                        std::uint32_t sampleSplit, std::uint32_t altPipeConfig) {
    return arrayMode << 2 | pipeConfig << 6 | tileSplit << 11 | microMode << 22 | sampleSplit << 25 | altPipeConfig << 27;
}

constexpr std::uint32_t GnmTile(std::uint32_t arrayMode, std::uint32_t pipeConfig, std::uint32_t tileSplit, std::uint32_t microMode,
                                std::uint32_t sampleSplit) {
    return GnmTileModeWord(arrayMode, pipeConfig, tileSplit, microMode, sampleSplit, GnmPipeConfigNeoP16);
}

constexpr std::uint32_t GnmTileModes[32] = {
    GnmTile(GnmArrayMode2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeDepth, GnmSampleSplit1),
    GnmTile(GnmArrayMode2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit128B, GnmMicroTileModeDepth, GnmSampleSplit1),
    GnmTile(GnmArrayMode2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit256B, GnmMicroTileModeDepth, GnmSampleSplit1),
    GnmTile(GnmArrayMode2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit512B, GnmMicroTileModeDepth, GnmSampleSplit1),
    GnmTile(GnmArrayMode2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit1KB, GnmMicroTileModeDepth, GnmSampleSplit1),
    GnmTile(GnmArrayMode1dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeDepth, GnmSampleSplit1),
    GnmTile(GnmArrayModePrt2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit256B, GnmMicroTileModeDepth, GnmSampleSplit1),
    GnmTile(GnmArrayModePrt2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit1KB, GnmMicroTileModeDepth, GnmSampleSplit1),
    GnmTile(GnmArrayModeLinearAligned, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeDisplay, GnmSampleSplit1),
    GnmTile(GnmArrayMode1dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeDisplay, GnmSampleSplit1),
    GnmTile(GnmArrayMode2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeDisplay, GnmSampleSplit2),
    GnmTile(GnmArrayModePrtTiledThin1, GnmPipeConfigP8_32x32_8x16, GnmTileSplit64B, GnmMicroTileModeDisplay, GnmSampleSplit2),
    GnmTile(GnmArrayModePrt2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeDisplay, GnmSampleSplit2),
    GnmTile(GnmArrayMode1dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeThin, GnmSampleSplit1),
    GnmTile(GnmArrayMode2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeThin, GnmSampleSplit2),
    GnmTile(GnmArrayMode3dTiledThin1, GnmPipeConfigP8_32x32_8x16, GnmTileSplit64B, GnmMicroTileModeThin, GnmSampleSplit2),
    GnmTile(GnmArrayModePrtTiledThin1, GnmPipeConfigP8_32x32_8x16, GnmTileSplit64B, GnmMicroTileModeThin, GnmSampleSplit2),
    GnmTile(GnmArrayModePrt2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeThin, GnmSampleSplit2),
    GnmTile(GnmArrayModePrt3dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeThin, GnmSampleSplit2),
    GnmTile(GnmArrayMode1dTiledThick, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeThick, GnmSampleSplit1),
    GnmTile(GnmArrayMode2dTiledThick, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeThick, GnmSampleSplit1),
    GnmTile(GnmArrayMode3dTiledThick, GnmPipeConfigP8_32x32_8x16, GnmTileSplit64B, GnmMicroTileModeThick, GnmSampleSplit1),
    GnmTile(GnmArrayModePrtTiledThick, GnmPipeConfigP8_32x32_8x16, GnmTileSplit64B, GnmMicroTileModeThick, GnmSampleSplit1),
    GnmTile(GnmArrayModePrt2dTiledThick, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeThick, GnmSampleSplit1),
    GnmTile(GnmArrayModePrt3dTiledThick, GnmPipeConfigP8_32x32_8x16, GnmTileSplit64B, GnmMicroTileModeThick, GnmSampleSplit1),
    GnmTile(GnmArrayMode2dTiledXThick, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeThick, GnmSampleSplit1),
    GnmTile(GnmArrayMode3dTiledXThick, GnmPipeConfigP8_32x32_8x16, GnmTileSplit64B, GnmMicroTileModeThick, GnmSampleSplit1),
    GnmTile(GnmArrayMode1dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeRotated, GnmSampleSplit1),
    GnmTile(GnmArrayMode2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeRotated, GnmSampleSplit2),
    GnmTile(GnmArrayModePrtTiledThin1, GnmPipeConfigP8_32x32_8x16, GnmTileSplit64B, GnmMicroTileModeRotated, GnmSampleSplit2),
    GnmTile(GnmArrayModePrt2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit64B, GnmMicroTileModeRotated, GnmSampleSplit2),
    GnmTileModeWord(GnmArrayModeLinearGeneral, GnmPipeConfigP2, GnmTileSplit64B, GnmMicroTileModeDisplay, GnmSampleSplit1, GnmPipeConfigP2)};

enum GnmMacroTileFieldValue : std::uint32_t {
    GnmBankWidth1 = 0,
    GnmBankHeight1 = 0,
    GnmBankHeight2 = 1,
    GnmBankHeight4 = 2,
    GnmBankHeight8 = 3,
    GnmMacroAspect1 = 0,
    GnmMacroAspect2 = 1,
    GnmMacroAspect4 = 2,
    GnmBanks2 = 0,
    GnmBanks4 = 1,
    GnmBanks8 = 2,
    GnmBanks16 = 3,
};

constexpr std::uint32_t GnmMacroTileWord(std::uint32_t bankWidth, std::uint32_t bankHeight, std::uint32_t aspect, std::uint32_t banks,
                                     std::uint32_t altBankHeight, std::uint32_t altAspect, std::uint32_t altBanks) {
    return bankWidth | bankHeight << 2 | aspect << 4 | banks << 6 | altBankHeight << 8 | altAspect << 10 | altBanks << 12;
}

constexpr std::uint32_t GnmMacroTileModes[16] = {
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight4, GnmMacroAspect4, GnmBanks16, GnmBankHeight4, GnmMacroAspect2, GnmBanks8),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight2, GnmMacroAspect2, GnmBanks16, GnmBankHeight4, GnmMacroAspect2, GnmBanks8),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight1, GnmMacroAspect2, GnmBanks16, GnmBankHeight2, GnmMacroAspect1, GnmBanks8),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight1, GnmMacroAspect2, GnmBanks16, GnmBankHeight2, GnmMacroAspect1, GnmBanks8),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight1, GnmMacroAspect1, GnmBanks8, GnmBankHeight1, GnmMacroAspect1, GnmBanks8),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight1, GnmMacroAspect1, GnmBanks4, GnmBankHeight1, GnmMacroAspect1, GnmBanks8),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight1, GnmMacroAspect1, GnmBanks2, GnmBankHeight1, GnmMacroAspect1, GnmBanks4),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight1, GnmMacroAspect1, GnmBanks2, GnmBankHeight1, GnmMacroAspect1, GnmBanks2),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight8, GnmMacroAspect4, GnmBanks16, GnmBankHeight4, GnmMacroAspect2, GnmBanks16),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight4, GnmMacroAspect4, GnmBanks16, GnmBankHeight4, GnmMacroAspect2, GnmBanks8),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight2, GnmMacroAspect2, GnmBanks16, GnmBankHeight2, GnmMacroAspect1, GnmBanks8),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight1, GnmMacroAspect2, GnmBanks16, GnmBankHeight1, GnmMacroAspect1, GnmBanks8),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight1, GnmMacroAspect1, GnmBanks8, GnmBankHeight1, GnmMacroAspect1, GnmBanks4),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight1, GnmMacroAspect1, GnmBanks4, GnmBankHeight1, GnmMacroAspect1, GnmBanks4),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight1, GnmMacroAspect1, GnmBanks2, GnmBankHeight1, GnmMacroAspect1, GnmBanks2),
    GnmMacroTileWord(GnmBankWidth1, GnmBankHeight1, GnmMacroAspect1, GnmBanks2, GnmBankHeight1, GnmMacroAspect1, GnmBanks2)};

enum GnmChannelTypeValue : std::uint32_t {
    GnmChannelTypeUNorm = 0,
    GnmChannelTypeSNorm = 1,
    GnmChannelTypeUInt = 4,
    GnmChannelTypeSInt = 5,
    GnmChannelTypeFloat = 7,
    GnmChannelTypeSrgb = 9,
};

constexpr std::uint32_t GnmRenderTargetTypeSrgb = 6;
constexpr std::uint32_t GnmMicroTileTexels = 64;

enum GnmSurfaceFormatValue : std::uint32_t {
    GnmFormatInvalid = 0x00,
    GnmFormat8 = 0x01,
    GnmFormat16 = 0x02,
    GnmFormat8_8 = 0x03,
    GnmFormat32 = 0x04,
    GnmFormat16_16 = 0x05,
    GnmFormat10_11_11 = 0x06,
    GnmFormat11_11_10 = 0x07,
    GnmFormat10_10_10_2 = 0x08,
    GnmFormat2_10_10_10 = 0x09,
    GnmFormat8_8_8_8 = 0x0a,
    GnmFormat32_32 = 0x0b,
    GnmFormat16_16_16_16 = 0x0c,
    GnmFormat32_32_32 = 0x0d,
    GnmFormat32_32_32_32 = 0x0e,
    GnmFormat5_6_5 = 0x10,
    GnmFormat1_5_5_5 = 0x11,
    GnmFormat5_5_5_1 = 0x12,
    GnmFormat4_4_4_4 = 0x13,
    GnmFormat8_24 = 0x14,
    GnmFormat24_8 = 0x15,
    GnmFormatX24_8_32 = 0x16,
    GnmFormatGB_GR = 0x20,
    GnmFormatBG_RG = 0x21,
    GnmFormat5_9_9_9 = 0x22,
    GnmFormatBc1 = 0x23,
    GnmFormatBc2 = 0x24,
    GnmFormatBc3 = 0x25,
    GnmFormatBc4 = 0x26,
    GnmFormatBc5 = 0x27,
    GnmFormatBc6 = 0x28,
    GnmFormatBc7 = 0x29,
    GnmFormatFmask8_S2_F1 = 0x2c,
    GnmFormatFmask8_S4_F1 = 0x2d,
    GnmFormatFmask8_S8_F1 = 0x2e,
    GnmFormatFmask8_S2_F2 = 0x2f,
    GnmFormatFmask8_S4_F2 = 0x30,
    GnmFormatFmask8_S4_F4 = 0x31,
    GnmFormatFmask16_S16_F1 = 0x32,
    GnmFormatFmask16_S8_F2 = 0x33,
    GnmFormatFmask32_S16_F2 = 0x34,
    GnmFormatFmask32_S8_F4 = 0x35,
    GnmFormatFmask32_S8_F8 = 0x36,
    GnmFormatFmask64_S16_F4 = 0x37,
    GnmFormatFmask64_S16_F8 = 0x38,
    GnmFormat4_4 = 0x39,
    GnmFormat6_5_5 = 0x3a,
    GnmFormat1 = 0x3b,
    GnmFormat1Reversed = 0x3c,
};

constexpr std::uint32_t GnmFormatCount = GnmFormat1Reversed + 1;
constexpr std::uint32_t GnmFormatUndefined = 0xffffffff;
constexpr std::uint32_t GnmBlockTexels = 16;
constexpr std::uint32_t GnmFmaskComponents = 2;

struct GnmFormatShape {
    std::uint32_t bits;
    std::uint32_t components;
};

constexpr GnmFormatShape GnmChannels(std::uint32_t x = 0, std::uint32_t y = 0, std::uint32_t z = 0, std::uint32_t w = 0) {
    return {x + y + z + w, static_cast<std::uint32_t>((x != 0) + (y != 0) + (z != 0) + (w != 0))};
}

constexpr GnmFormatShape GnmPadded(std::uint32_t padding, GnmFormatShape shape) { return {shape.bits + padding, shape.components}; }

constexpr GnmFormatShape GnmBlock(std::uint32_t blockBits, std::uint32_t components) { return {blockBits / GnmBlockTexels, components}; }

constexpr GnmFormatShape GnmShape(std::uint32_t format) {
    switch (format) {
    case GnmFormatInvalid: return GnmChannels();
    case GnmFormat8: return GnmChannels(8);
    case GnmFormat16: return GnmChannels(16);
    case GnmFormat8_8: return GnmChannels(8, 8);
    case GnmFormat32: return GnmChannels(32);
    case GnmFormat16_16: return GnmChannels(16, 16);
    case GnmFormat10_11_11: return GnmChannels(10, 11, 11);
    case GnmFormat11_11_10: return GnmChannels(11, 11, 10);
    case GnmFormat10_10_10_2: return GnmChannels(10, 10, 10, 2);
    case GnmFormat2_10_10_10: return GnmChannels(2, 10, 10, 10);
    case GnmFormat8_8_8_8: return GnmChannels(8, 8, 8, 8);
    case GnmFormat32_32: return GnmChannels(32, 32);
    case GnmFormat16_16_16_16: return GnmChannels(16, 16, 16, 16);
    case GnmFormat32_32_32: return GnmChannels(32, 32, 32);
    case GnmFormat32_32_32_32: return GnmChannels(32, 32, 32, 32);
    case GnmFormat5_6_5: return GnmChannels(5, 6, 5);
    case GnmFormat1_5_5_5: return GnmChannels(1, 5, 5, 5);
    case GnmFormat5_5_5_1: return GnmChannels(5, 5, 5, 1);
    case GnmFormat4_4_4_4: return GnmChannels(4, 4, 4, 4);
    case GnmFormat8_24: return GnmChannels(8, 24);
    case GnmFormat24_8: return GnmChannels(24, 8);
    case GnmFormatX24_8_32: return GnmPadded(24, GnmChannels(8, 32));
    case GnmFormatGB_GR:
    case GnmFormatBG_RG: return {GnmChannels(8, 8, 8, 8).bits / 2, 3};
    case GnmFormat5_9_9_9: return GnmPadded(5, GnmChannels(9, 9, 9));
    case GnmFormatBc1: return GnmBlock(64, 4);
    case GnmFormatBc2:
    case GnmFormatBc3:
    case GnmFormatBc7: return GnmBlock(128, 4);
    case GnmFormatBc4: return GnmBlock(64, 1);
    case GnmFormatBc5: return GnmBlock(128, 2);
    case GnmFormatBc6: return GnmBlock(128, 3);
    case GnmFormatFmask8_S2_F1:
    case GnmFormatFmask8_S4_F1:
    case GnmFormatFmask8_S8_F1:
    case GnmFormatFmask8_S2_F2:
    case GnmFormatFmask8_S4_F2:
    case GnmFormatFmask8_S4_F4: return {8, GnmFmaskComponents};
    case GnmFormatFmask16_S16_F1:
    case GnmFormatFmask16_S8_F2: return {16, GnmFmaskComponents};
    case GnmFormatFmask32_S16_F2:
    case GnmFormatFmask32_S8_F4:
    case GnmFormatFmask32_S8_F8: return {32, GnmFmaskComponents};
    case GnmFormatFmask64_S16_F4:
    case GnmFormatFmask64_S16_F8: return {64, GnmFmaskComponents};
    case GnmFormat4_4: return GnmChannels(4, 4);
    case GnmFormat6_5_5: return GnmChannels(6, 5, 5);
    case GnmFormat1:
    case GnmFormat1Reversed: return GnmChannels(1);
    default: return {GnmFormatUndefined, GnmFormatUndefined};
    }
}

template <std::uint32_t GnmFormatShape::*Field>
constexpr std::array<std::uint32_t, GnmFormatCount> GnmFormatTable() {
    std::array<std::uint32_t, GnmFormatCount> table{};
    for (std::uint32_t format = 0; format < GnmFormatCount; ++format) table[format] = GnmShape(format).*Field;
    return table;
}

constexpr std::array<std::uint32_t, GnmFormatCount> GnmComponentCounts = GnmFormatTable<&GnmFormatShape::components>();
constexpr std::array<std::uint32_t, GnmFormatCount> GnmElementBits = GnmFormatTable<&GnmFormatShape::bits>();

static_assert(GnmElementBits[GnmFormat8_8_8_8] == 32 && GnmComponentCounts[GnmFormat8_8_8_8] == 4);
static_assert(GnmElementBits[GnmFormat32_32_32] == 96 && GnmComponentCounts[GnmFormat32_32_32] == 3);
static_assert(GnmElementBits[GnmFormat5_6_5] == 16 && GnmComponentCounts[GnmFormat5_6_5] == 3);
static_assert(GnmElementBits[GnmFormatBc1] == 4 && GnmElementBits[GnmFormatBc7] == 8);
static_assert(GnmTileModeWord(GnmArrayMode2dTiledThin1, GnmPipeConfigP8_32x32_16x16, GnmTileSplit1KB, GnmMicroTileModeDepth, GnmSampleSplit2,
                              GnmPipeConfigNeoP16) == (4u << 2 | 0xcu << 6 | 4u << 11 | 2u << 22 | 1u << 25 | 0x12u << 27));

struct GnmSurfaceLayout {
    std::uint32_t pitch;
    std::uint32_t height;
    std::uint32_t tileMode;
};

std::uint32_t Read32(const std::uint8_t* bytes, std::size_t offset) {
    std::uint32_t value;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

std::uint64_t Read64(const std::uint8_t* bytes, std::size_t offset) {
    std::uint64_t value;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

template <typename T>
void Write(std::uint8_t* bytes, std::size_t offset, T value) {
    std::memcpy(bytes + offset, &value, sizeof(value));
}

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment) {
    return (value + alignment - 1) & (0u - alignment);
}

constexpr std::uint32_t GnmArrayMode(std::uint32_t entry) { return (entry >> 2) & 0xf; }

constexpr std::uint32_t GnmMicroMode(std::uint32_t entry) { return (entry >> 22) & 7; }

constexpr std::uint32_t GnmThicknessOf(std::uint32_t arrayMode) {
    switch (arrayMode) {
    case GnmArrayMode1dTiledThick:
    case GnmArrayMode2dTiledThick:
    case GnmArrayModePrtTiledThick:
    case GnmArrayModePrt2dTiledThick:
    case GnmArrayMode3dTiledThick:
    case GnmArrayModePrt3dTiledThick: return 4;
    case GnmArrayMode2dTiledXThick:
    case GnmArrayMode3dTiledXThick: return 8;
    default: return 1;
    }
}

constexpr std::uint32_t GnmThinOrThickTileMode(std::uint32_t arrayMode) {
    std::uint32_t tileMode = 0;
    while (tileMode < 0x1f) {
        const std::uint32_t micro = GnmMicroMode(GnmTileModes[tileMode]);
        if ((micro == GnmMicroTileModeThin || micro == GnmMicroTileModeThick) && GnmArrayMode(GnmTileModes[tileMode]) == arrayMode) break;
        ++tileMode;
    }
    return tileMode;
}

static_assert([] {
    for (std::uint32_t arrayMode = 2; arrayMode < 0xe; ++arrayMode) {
        if (((0xe3fu >> (arrayMode - 2)) & 1) != 0 && GnmThinOrThickTileMode(arrayMode) == 0x1f) return false;
    }
    return true;
}());

constexpr std::uint32_t GnmRenderTargetType(std::uint32_t channelType) {
    switch (channelType) {
    case GnmChannelTypeSNorm:
    case GnmChannelTypeUInt:
    case GnmChannelTypeSInt:
    case GnmChannelTypeFloat: return channelType;
    case GnmChannelTypeSrgb: return GnmRenderTargetTypeSrgb;
    default: return GnmChannelTypeUNorm;
    }
}

std::uint32_t GnmDataFormat(std::uint32_t surface, std::uint32_t type, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t w) {
    return (surface & 0xff) | (type & 0xf) << 8 | (x & 7) << 12 | (y & 7) << 15 | (z & 7) << 18 | (w & 7) << 21;
}

std::uint32_t GnmChannel(std::uint32_t format, std::uint32_t index) { return (format >> (12 + 3 * index)) & 7; }

bool GnmSwizzle(std::uint32_t format, std::uint32_t* index) {
    const std::uint32_t surface = format & 0xff;
    if (surface > 0x3c) return false;
    const std::uint32_t x = GnmChannel(format, 0);
    const std::uint32_t y = GnmChannel(format, 1);
    const std::uint32_t z = GnmChannel(format, 2);
    const std::uint32_t w = GnmChannel(format, 3);
    int result = -1;
    switch (GnmComponentCounts[surface]) {
    case 1:
        result = x == 4 ? 0 : y == 4 ? 1 : z == 4 ? 2 : w == 4 ? 3 : -1;
        break;
    case 2:
        result = x == 4 && y == 5 ? 0 : x == 4 && w == 5 ? 1 : y == 4 && x == 5 ? 2 : w == 4 && x == 5 ? 3 : -1;
        break;
    case 3:
        result = x == 4 && y == 5 && z == 6   ? 0
                 : x == 4 && y == 5 && w == 6 ? 1
                 : z == 4 && y == 5 && x == 6 ? 2
                 : w == 4 && y == 5 && x == 6 ? 3
                                              : -1;
        break;
    case 4:
        result = x == 4 && y == 5 && z == 6 && w == 7   ? 0
                 : z == 4 && y == 5 && x == 6 && w == 7 ? 1
                 : w == 4 && z == 5 && y == 6 && x == 7 ? 2
                 : w == 4 && x == 5 && y == 6 && z == 7 ? 3
                                                        : -1;
        break;
    default:
        return false;
    }
    if (result < 0) return false;
    if (index != nullptr) *index = static_cast<std::uint32_t>(result);
    return true;
}

bool GnmTextureFormatSupported(std::uint32_t format) {
    const std::uint32_t surface = format & 0xff;
    if (surface - 1 > 0x15) return false;
    if (surface == 8 || surface == 9) {
        if ((format & 0xf00) == 0x900) return false;
    } else if (surface == 0xd || surface == 0xf) {
        return false;
    }
    const std::uint32_t type = (format >> 8) & 0xf;
    if (type > 9 || ((0x2b3u >> type) & 1) == 0) return false;
    return GnmSwizzle(format, nullptr);
}

std::uint32_t GnmDegradeArrayMode(std::uint32_t arrayMode, std::uint32_t bits) {
    const std::uint32_t index = arrayMode - 3;
    if (GnmThicknessOf(arrayMode) == 1) return arrayMode;
    const std::uint32_t size = bits * GnmMicroTileTexels * GnmThicknessOf(arrayMode);
    if (size < 0x2001) return arrayMode;
    switch (index) {
    case 0: return 3;
    case 4: return 4;
    case 5: return size < 0x4010 ? 7 : 4;
    case 6: return 5;
    case 7: return 6;
    case 10: return 0xc;
    case 11: return size < 0x4010 ? 0xd : 0xc;
    case 12: return 0xb;
    default: return arrayMode;
    }
}

bool GnmTileModeForArrayMode(std::uint32_t tileMode, std::uint32_t arrayMode, std::uint32_t& result) {
    if (tileMode > 0x1f) return false;
    const std::uint32_t entry = GnmTileModes[tileMode];
    if (GnmArrayMode(entry) == arrayMode) {
        result = tileMode;
        return true;
    }
    switch (GnmMicroMode(entry)) {
    case 0:
        if (arrayMode != 2) return false;
        result = 9;
        return true;
    case 1:
    case 4:
        if (arrayMode - 2 >= 0xc || ((0xe3fu >> (arrayMode - 2)) & 1) == 0) return false;
        result = GnmThinOrThickTileMode(arrayMode);
        return true;
    case 2:
        result = 5;
        return true;
    default:
        return false;
    }
}

struct GnmMacroTile {
    bool found;
    std::uint32_t index;
    std::uint32_t residual;
};

GnmMacroTile GnmFindMacroTile(std::uint32_t tileMode, std::uint32_t bits, std::uint32_t fragments) {
    const auto fragmentBits = static_cast<std::uint32_t>(std::popcount(fragments));
    if (fragmentBits != 1) return {false, 0, fragmentBits};
    if (fragments > 8 || tileMode > 0x1f) return {false, 0, 1};
    const std::uint32_t entry = GnmTileModes[tileMode];
    const std::uint32_t arrayMode = GnmArrayMode(entry);
    if (((0xfff0u >> arrayMode) & 1) == 0 || bits - 1 > 0x7f) return {false, 0, arrayMode};
    const std::uint32_t tileBytes = ((bits * GnmThicknessOf(arrayMode)) << 6) >> 3;
    const std::uint32_t scaled = tileBytes << ((entry >> 25) & 3);
    const std::uint32_t split = tileMode >= 8 ? (scaled >= 0x101 ? scaled : 0x100) : 0x40u << ((entry >> 11) & 7);
    const std::uint32_t limit = split < 0x400 ? split : 0x400;
    const std::uint32_t fragmentBytes = tileBytes * fragments;
    const std::uint32_t bytes = fragmentBytes >= limit ? limit : fragmentBytes;
    const auto byteBits = static_cast<std::uint32_t>(std::popcount(bytes));
    if (byteBits != 1) return {false, 0, byteBits};
    if (bytes < 0x40) return {false, 0, 1};
    const std::uint32_t index = static_cast<std::uint32_t>(std::bit_width((bytes >> 6) | 1)) - 1 + (((0x8e60u >> arrayMode) & 1) != 0 ? 8 : 0);
    return {true, index, 0};
}

bool GnmMacroTileIndex(std::uint32_t tileMode, std::uint32_t bits, std::uint32_t& index) {
    const GnmMacroTile tile = GnmFindMacroTile(tileMode, bits, 1);
    if (tile.found) index = tile.index;
    return tile.found;
}

bool GnmComputeMacroLayout(std::uint32_t tileMode, std::uint32_t arrayMode, std::uint32_t pipes, bool neo, std::uint32_t width,
                           std::uint32_t height, std::uint32_t bits, GnmSurfaceLayout& layout) {
    std::uint32_t macroIndex = 0;
    if (!GnmMacroTileIndex(tileMode, bits, macroIndex)) return false;
    const std::uint32_t macro = GnmMacroTileModes[macroIndex];
    const std::uint32_t bankWidth = 1u << (macro & 3);
    const std::uint32_t bankHeight = 1u << (neo ? (macro >> 8) & 3 : (macro >> 2) & 3);
    const std::uint32_t aspect = 1u << (neo ? (macro >> 10) & 3 : (macro >> 4) & 3);
    const std::uint32_t banks = 2u << (neo ? (macro >> 12) & 3 : (macro >> 6) & 3);
    const std::uint32_t entry = GnmTileModes[tileMode];
    const std::uint32_t tileBytes = (GnmThicknessOf(arrayMode) * bits * 8) & 0x1ffffff8;
    std::uint32_t split = 0;
    if (GnmMicroMode(entry) == 2) {
        split = 0x40u << ((entry >> 11) & 7);
    } else {
        split = tileBytes << ((entry >> 25) & 3);
        if (split < 0x101) split = 0x100;
    }
    if (split >= 0x400) split = 0x400;
    if (banks < aspect || banks * pipes == 0) return false;
    const std::uint32_t splitBytes = tileBytes >= split ? split : tileBytes;
    std::uint32_t bankTiles = 0x100 / (splitBytes * bankWidth);
    if (bankTiles == 0) bankTiles = 1;
    if (!std::has_single_bit(bankTiles)) return false;
    std::uint32_t pipeTiles = 0x100 / (splitBytes * pipes * bankWidth);
    if (pipeTiles == 0) pipeTiles = 1;
    if (!std::has_single_bit(pipeTiles)) return false;
    const std::uint32_t macroAspect = AlignUp(aspect, pipeTiles);
    const std::uint32_t macroBankHeight = AlignUp(bankHeight, bankTiles);
    if (splitBytes * bankWidth * macroBankHeight >= 0x401) NotImplemented_nid_no_patch("sceFontGraphicsGnmSurfaceInit");
    std::uint32_t pitchAlign = macroAspect * pipes * bankWidth * 8;
    const std::uint32_t heightAlign = banks * macroBankHeight * 8 / macroAspect;
    if (((0x8e60u >> arrayMode) & 1) != 0) {
        const std::uint32_t sliceBits = heightAlign * bits * pitchAlign;
        if (sliceBits <= 0x7ffff) {
            if (0x10000 % (sliceBits >> 3) != 0) return false;
            pitchAlign *= 0x10000 / (sliceBits >> 3);
        }
    }
    if (macroBankHeight * bankWidth * splitBytes < 0x100 || macroAspect * pipes * bankWidth * splitBytes <= 0xff) return false;
    if (!std::has_single_bit(pitchAlign) || !std::has_single_bit(heightAlign)) return false;
    layout.pitch = AlignUp(width, pitchAlign);
    layout.height = AlignUp(height, heightAlign);
    return true;
}

bool GnmComputeSurface(std::uint32_t tileMode, bool neo, std::uint32_t width, std::uint32_t height, std::uint32_t bits,
                       GnmSurfaceLayout& layout) {
    if (bits - 1 > 0x7f || !std::has_single_bit(bits) || tileMode > 0x1f) return false;
    const std::uint32_t entry = GnmTileModes[tileMode];
    const std::uint32_t pipeConfig = neo ? entry >> 27 : (entry >> 6) & 0x1f;
    const std::uint32_t pipes = pipeConfig == 0xa || pipeConfig == 0xc ? 8 : pipeConfig == 0x12 ? 16 : 0;
    const std::uint32_t original = GnmArrayMode(entry);
    if (GnmThicknessOf(original) == 8 && bits >= 0x41) return false;
    const std::uint32_t arrayMode = GnmDegradeArrayMode(original, bits);
    std::uint32_t macroMode = tileMode;
    GnmTileModeForArrayMode(tileMode, arrayMode, macroMode);
    layout.pitch = width;
    layout.height = height;
    if (arrayMode == 1) {
        const std::uint32_t bytes = (bits + 7) >> 3;
        const std::uint32_t elements = (0x40 / bytes) & 0xff;
        const std::uint32_t pitchAlign = elements >= 9 ? elements : 8;
        const std::uint32_t slice = (0x100 / bytes) & 0xffff;
        const std::uint32_t sliceAlign = slice >= 0x41 ? slice : 0x40;
        layout.pitch = AlignUp(width, pitchAlign);
        while (static_cast<std::uint64_t>(layout.pitch) * height % sliceAlign != 0) layout.pitch += pitchAlign;
    } else if (arrayMode == 2 || arrayMode == 3) {
        const std::uint32_t thickness = GnmThicknessOf(arrayMode);
        layout.pitch = AlignUp(width, 8);
        layout.height = AlignUp(height, 8);
        while ((((layout.pitch * layout.height * bits + 7) >> 3) * thickness & 0xff) != 0) layout.pitch += 8;
    } else if (arrayMode >= 4) {
        if (macroMode > 0x1f || !GnmComputeMacroLayout(macroMode, arrayMode, pipes, neo, width, height, bits, layout)) return false;
    }
    return GnmTileModeForArrayMode(tileMode, arrayMode, layout.tileMode);
}

std::uint32_t GnmTextureFormatWord(std::uint32_t format) {
    const std::uint32_t numeric = GnmRenderTargetType((format >> 8) & 0xf);
    const std::uint32_t paired = numeric & ~1u;
    std::uint32_t swizzle = 0;
    GnmSwizzle(format, &swizzle);
    std::uint32_t word = numeric == 0 || numeric == 6 ? 0 : static_cast<std::uint32_t>(numeric != 1) << 18;
    word |= static_cast<std::uint32_t>(paired == 4) << 16;
    word |= static_cast<std::uint32_t>(numeric < 7 && ((0x43u >> numeric) & 1) != 0 && paired != 4) << 15;
    return word | (swizzle & 3) << 11 | (numeric & 7) << 8 | (format << 2 & 0x7c);
}

bool GnmInitTexture(std::uint8_t* output, std::uint32_t width, std::uint32_t height, std::uint32_t format, std::uint32_t tileMode,
                    bool neo) {
    if (!GnmTextureFormatSupported(format) || width - 1 > 0x3fff || height - 1 > 0x3fff) return false;
    std::uint32_t texture[16] = {};
    if (neo) texture[6] = 8;
    GnmSurfaceLayout layout{};
    const bool computed = GnmComputeSurface(tileMode, neo, width, height, GnmElementBits[format & 0xff], layout);
    if (computed) {
        const std::uint32_t pitchTiles = (layout.pitch >> 3) - 1;
        const std::uint32_t mode = layout.tileMode & 0x1f;
        texture[1] = (pitchTiles & 0x7ff) | (pitchTiles << 20 & 0x7ff00000);
        texture[2] = ((layout.pitch * layout.height >> 6) - 1) & 0x3fffff;
        texture[4] = GnmTextureFormatWord(format) | static_cast<std::uint32_t>(neo) << 31;
        texture[5] = mode << 5 | mode;
        texture[15] = height << 16 | (width & 0xffff);
    }
    std::memcpy(output, texture, sizeof(texture));
    return computed;
}

std::uint32_t GnmFormatBits(std::uint32_t surface) {
    if (surface > 0x3c) return 0;
    const std::uint32_t scale = ((surface - 0x23) & 0xff) < 7 ? 16 : ((surface - 0x3b) & 0xff) <= 1 ? 8 : 1;
    return GnmElementBits[surface] * scale;
}

constexpr std::uint32_t GnmChannelSelects[4][4][4] = {
    {{4, 0, 0, 1}, {0, 4, 0, 1}, {0, 0, 4, 1}, {0, 0, 0, 4}},
    {{4, 5, 0, 1}, {4, 0, 0, 5}, {5, 4, 0, 1}, {5, 0, 0, 4}},
    {{4, 5, 6, 1}, {4, 5, 0, 6}, {6, 5, 4, 1}, {6, 5, 0, 4}},
    {{4, 5, 6, 7}, {6, 5, 4, 7}, {7, 6, 5, 4}, {5, 6, 7, 4}}};

std::uint32_t GnmTextureDataFormat(const std::uint8_t* texture) {
    const std::uint32_t word = Read32(texture, 0x10);
    const std::uint32_t surface = (word >> 2) & 0x1f;
    const std::uint32_t numeric = (word >> 8) & 7;
    const std::uint32_t count = GnmComponentCounts[surface];
    const std::uint32_t* const selects = count - 1 < 4 ? GnmChannelSelects[count - 1][(word >> 11) & 3] : nullptr;
    const std::uint32_t channels = selects != nullptr ? GnmDataFormat(0, 0, selects[0], selects[1], selects[2], selects[3]) : 1u << 21;
    return surface | (numeric == 6 ? 0x900 : numeric << 8) | channels;
}

std::uint32_t GnmBankMask(std::uint32_t tileMode, std::uint32_t surface, std::uint32_t fragments, bool neo, bool incrementResidual) {
    const GnmMacroTile tile = GnmFindMacroTile(tileMode, GnmFormatBits(surface), fragments);
    const std::uint32_t field = neo ? (GnmMacroTileModes[tile.index] >> 12) & 3 : (GnmMacroTileModes[tile.index] >> 6) & 3;
    const std::uint32_t bits = tile.found ? field + 1 : tile.residual + (incrementResidual ? 1 : 0);
    return ~(0xffffffffu << (bits & 0x1f)) << (neo ? 4 : 3);
}

std::uint32_t GnmTextureBankMask(const std::uint8_t* texture) {
    const std::uint32_t tiling = Read32(texture, 0x14);
    return GnmBankMask(tiling & 0x1f, GnmTextureDataFormat(texture) & 0xff, 1u << ((tiling >> 15) & 3),
                       (Read32(texture, 0x10) & 0x80000000) != 0, true);
}

bool GnmTextureHasBanks(const std::uint8_t* texture) {
    return (GnmTextureDataFormat(texture) & 0xff) != 0 && ((0x7f7dcdfu >> (Read32(texture, 0x14) & 0x1f)) & 1) != 0;
}

void GnmSetTextureBase(std::uint8_t* texture, std::uint32_t address) {
    if (GnmTextureHasBanks(texture)) {
        const std::uint32_t mask = GnmTextureBankMask(texture);
        address = (address & ~mask) | (Read32(texture, 0) & mask);
    }
    Write(texture, 0, address);
}

void GnmSetTextureMetadata(std::uint8_t* texture, std::uint32_t address) {
    const std::uint32_t tiling = Read32(texture, 0x14);
    if ((tiling & 0x7000) != 0 && ((0x7f7dcdfu >> ((tiling >> 5) & 0x1f)) & 1) != 0) {
        const std::uint32_t mask = GnmTextureBankMask(texture);
        address = (address & ~mask) | (Read32(texture, 0x24) & mask);
    }
    Write(texture, 0x24, address);
}

std::uint32_t GnmTextureBase(const std::uint8_t* texture) {
    const std::uint32_t address = Read32(texture, 0);
    return GnmTextureHasBanks(texture) ? address & ~GnmTextureBankMask(texture) : address;
}

std::uint32_t GnmResourceFragments(std::uint32_t word3) { return word3 >= 0xe0000000 ? 1u << ((word3 >> 16) & 0xf) : 1; }

bool GnmResourceHasBanks(const std::uint32_t* resource) {
    return (resource[1] & 0x3f00000) != 0 && ((0x7f7dcdfu >> ((resource[3] >> 20) & 0x1f)) & 1) != 0;
}

std::uint32_t GnmResourceBankMask(const std::uint32_t* resource, bool neo, bool incrementResidual) {
    return GnmBankMask((resource[3] >> 20) & 0x1f, (resource[1] >> 20) & 0x3f, GnmResourceFragments(resource[3]), neo, incrementResidual);
}

void GnmSetResourceBase(std::uint32_t* resource, std::uint32_t address) {
    if (GnmResourceHasBanks(resource)) {
        const std::uint32_t mask = GnmResourceBankMask(resource, (resource[6] & 0x1000000) != 0, true);
        address = (address & ~mask) | (resource[0] & mask);
    }
    resource[0] = address;
    resource[1] &= 0xffffffc0;
}

std::uint32_t GnmResourceType(std::uint32_t depth, std::uint32_t tiling, std::uint32_t depthWord, std::uint32_t heightBits) {
    const bool layered = depthWord >= 0x2000;
    const bool single = (depth & 0x7ff) == 0;
    if ((tiling & 0x18000) != 0) return single && !layered ? 0xe0000000 : 0xf0000000;
    const bool flat = !layered && single;
    if (heightBits == 0) return flat ? 0x80000000 : 0xc0000000;
    return flat ? 0x90000000 : 0xd0000000;
}

void GnmTextureToResource(std::uint8_t* output, const std::uint8_t* texture, bool cube) {
    std::uint32_t resource[8] = {};
    const std::uint32_t extent = Read32(texture, 0x3c);
    const std::uint32_t widthBits = (extent - 1) & 0x3fff;
    const std::uint32_t heightBits = (((extent >> 2) & 0xfffc000) + 0xfffc000) & 0xfffc000;
    const std::uint32_t pitch = ((Read32(texture, 0x04) << 16) & 0x7ff0000) + 0xe000;
    const std::uint32_t format = GnmTextureDataFormat(texture);
    resource[1] = (format & 0x3f) << 20 | ((format << 18) & 0x3c000000);
    const std::uint32_t selects = GnmChannel(format, 0) | GnmChannel(format, 1) << 3 | GnmChannel(format, 2) << 6 | GnmChannel(format, 3) << 9;
    const std::uint32_t tiling = Read32(texture, 0x14);
    const std::uint32_t word3 = (((0x94000000 | selects) & 0xfe0fffff) | (tiling & 0x1f) << 20) & 0xfffffff;
    const std::uint32_t depth = Read32(texture, 0x0c);
    const std::uint32_t slices = (depth >> 13) & 0x7ff;
    const std::uint32_t depthWord = depth & 0xffe7ff;
    resource[2] = heightBits + widthBits + 0x70000000;
    resource[4] = pitch + slices;
    resource[5] = depthWord;
    if (cube && depthWord >= 0x2000) {
        resource[4] = pitch | ((((slices * 0xaaab + 0xaaab) >> 18) - 1) & 0x1fff);
        resource[3] = word3 | 0xb0000000;
    } else {
        resource[3] = word3 | GnmResourceType(depth, tiling, depthWord, heightBits);
    }
    if (const std::uint32_t fragments = (tiling >> 15) & 3; fragments != 0) resource[3] = (resource[3] & 0xfff0ffff) | fragments << 16;
    const std::uint32_t formatWord = Read32(texture, 0x10);
    const std::uint32_t flags = Read32(texture, 0x18);
    std::uint32_t extra = 0;
    if ((formatWord & 0x10000000) != 0 && (flags & 0x260) == 0x200 && Read32(texture, 0x34) != 0) {
        extra = (~(formatWord << 10) & 0x400000) + ((flags << 16) & 0x800000) + 0x200000;
        resource[7] = Read32(texture, 0x34);
    }
    resource[6] = ((formatWord >> 7) & 0x1000000) | extra;
    GnmSetResourceBase(resource, GnmTextureBase(texture));
    std::uint32_t banks = 0;
    if ((formatWord & 0x80000000) != 0 && GnmTextureHasBanks(texture)) {
        const std::uint32_t mask = GnmBankMask(tiling & 0x1f, GnmTextureDataFormat(texture) & 0xff, 1u << ((tiling >> 15) & 3), true, false);
        banks = mask & Read32(texture, 0) & 0xff0;
    }
    if ((resource[6] & 0x1000000) != 0 && GnmResourceHasBanks(resource)) {
        const std::uint32_t mask = GnmResourceBankMask(resource, true, false);
        resource[0] = (resource[0] & ~mask) | (mask & banks);
    }
    std::memcpy(output, resource, sizeof(resource));
}

std::uint32_t GnmResourceDataFormat(const std::uint8_t* resource) {
    const std::uint32_t word1 = Read32(resource, 4);
    return ((word1 >> 20) & 0x3f) | ((word1 >> 18) & 0xf00) | ((Read32(resource, 0x0c) << 12) & 0xfff000);
}

int APS5_VABI GnmBackendStructureTexture(void* object, void* texture, std::uint64_t base) {
    if (object == nullptr) return 0;
    auto* const output = static_cast<std::uint8_t*>(object);
    auto* const source = static_cast<std::uint8_t*>(texture);
    GnmSetTextureBase(source, static_cast<std::uint32_t>(base >> 8));
    Write<std::uint32_t>(source, 0x1c, 0);
    GnmSetTextureMetadata(source, 0);
    std::uint8_t* const resource = output + 0x40;
    GnmTextureToResource(resource, source, false);
    std::uint32_t format = GnmResourceDataFormat(resource);
    const std::uint32_t surface = format & 0xff;
    if (surface == 1 || surface == 2 || surface == 4) {
        const std::uint32_t index = GnmChannel(format, 3) == 4   ? 3
                                    : GnmChannel(format, 0) == 4 ? 0
                                    : GnmChannel(format, 1) == 4 ? 1
                                    : GnmChannel(format, 2) == 4 ? 2
                                                                 : 3;
        format = GnmDataFormat(surface, (format >> 8) & 0xf, 1, 1, 1, GnmChannel(format, index));
    } else if (surface == 3) {
        format = GnmDataFormat(3, 0, 4, 4, 4, 5);
    }
    Write(resource, 4, (Read32(resource, 4) & 0xc00fffff) | (format & 0x3f) << 20 | ((format << 18) & 0x3c000000));
    Write(resource, 0x0c, (Read32(resource, 0x0c) & 0xfffff000) | GnmChannel(format, 0) | GnmChannel(format, 1) << 3 |
                             GnmChannel(format, 2) << 6 | GnmChannel(format, 3) << 9);
    return 0x20;
}

int GnmSurfaceInit(void* surface, const void* config) {
    if (surface == nullptr) return FontGsInvalidParameter;
    auto* const out = static_cast<std::uint8_t*>(surface);
    const auto* const in = static_cast<const std::uint8_t*>(config);
    if (in[0x0d] == FontGnmSurfaceConfigTag) std::memset(out, 0, 0x100);
    else Write<std::uint16_t>(out, 0x0e, 0);
    if ((in[0x0d] | 4) != FontGnmSurfaceConfigTag) return FontGsInvalidParameter;
    if (Read32(in, 0x24) != 1 || Read32(in, 0x18) != 0 || Read32(in, 0x1c) != 0) return FontGsSurfaceConfigInvalid;
    const std::uint64_t base = Read64(in, 0);
    std::uint32_t format = GnmDataFormat(Read32(in, 0x40), Read32(in, 0x44), Read32(in, 0x48), Read32(in, 0x4c), Read32(in, 0x50),
                                         Read32(in, 0x54));
    const std::uint32_t stride = Read32(in, 0x08);
    const std::uint32_t width = Read32(in, 0x10);
    const std::uint32_t height = Read32(in, 0x14);
    const std::uint8_t flags = in[0x0c];
    const std::uint32_t tileMode = Read32(in, 0x20);
    if (static_cast<std::int32_t>(stride) < 0 || static_cast<std::int32_t>(width) < 0 || static_cast<std::int32_t>(height) < 0)
        return FontGsSurfaceConfigInvalid;
    const std::uint32_t type = (format >> 8) & 0xf;
    std::uint8_t tag = 0;
    switch (format & 0xff) {
    case 1:
        if ((format & 0xf00) != 0 || GnmChannel(format, 0) != 0 || GnmChannel(format, 1) != 0 || GnmChannel(format, 2) != 0 ||
            GnmChannel(format, 3) != 4)
            return FontGsSurfaceConfigInvalid;
        tag = 0x10;
        break;
    case 3:
        if ((type != 9 && type != 0) || GnmChannel(format, 0) != 4 || GnmChannel(format, 3) != 5) return FontGsSurfaceConfigInvalid;
        format = GnmDataFormat(3, 0, 5, 0, 0, 4);
        tag = 0x20;
        break;
    case 9:
        tag = 0x4a;
        break;
    case 10:
        if ((type != 9 && type != 0) || GnmChannel(format, 0) < 2 || GnmChannel(format, 1) < 2 || GnmChannel(format, 2) < 2 ||
            GnmChannel(format, 3) != 7)
            return FontGsSurfaceConfigInvalid;
        tag = 0x40;
        break;
    case 12:
        tag = 0x4e;
        break;
    case 14:
        tag = 0x4f;
        break;
    default:
        return FontGsSurfaceConfigInvalid;
    }
    const bool neo = sceKernelIsNeoMode() != 0;
    if (!GnmInitTexture(out + 0x40, width, height, format, tileMode, neo)) return FontGsSurfaceConfigInvalid;
    GnmSetTextureBase(out + 0x40, static_cast<std::uint32_t>(base >> 8));
    Write(out, 0x5c, static_cast<std::uint32_t>(Read64(in, 0x58) >> 8));
    Write(out, 0x64, static_cast<std::uint32_t>(Read64(in, 0x78) >> 8));
    Write(out, 0x6c, Read64(in, 0x68));
    Write(out, 0x00, base);
    Write(out, 0x08, stride);
    out[0x0c] = flags;
    out[0x0d] = tag;
    Write<std::uint16_t>(out, 0x0e, 0x47e0);
    Write(out, 0x10, width);
    Write(out, 0x14, height);
    Write<std::uint64_t>(out, 0x18, 0);
    Write(out, 0x20, width);
    Write(out, 0x24, height);
    Write(out, 0x28, Read32(in, 0x34));
    std::uint16_t extent;
    std::memcpy(&extent, in + 0x30, sizeof(extent));
    Write(out, 0x2c, extent);
    Write<std::uint16_t>(out, 0x2e, 1);
    Write<std::uint32_t>(out, 0x30, 0);
    Write<std::uint16_t>(out, 0x34, 0x100);
    Write<std::uint32_t>(out, 0x36, 0x44);
    Write<std::uint16_t>(out, 0x3a, 0);
    Write(out, 0x3c, static_cast<std::uint16_t>(width));
    Write(out, 0x3e, static_cast<std::uint16_t>(height));
    return 0;
}

const FontGnmSurface* ReadGnmSurface(const void* surface) {
    const auto* const gnm = static_cast<const FontGnmSurface*>(surface);
    if (gnm->tag != FontGnmSurfaceTag || gnm->head == nullptr || gnm->initialized != 1 || gnm->busy != 0) return nullptr;
    return gnm;
}

void MarkGnmRecord(void* output) {
    *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(output) + FontGnmRecordStateOffset) = 1;
}

const FontGnmCommands* CheckSequence(const FontGraphicsCanvasSequence* sequence, bool outputs, int& error) {
    const auto* const seq = reinterpret_cast<const FontGnmSequence*>(sequence);
    if (seq == nullptr || seq->tag != FontGnmSequenceTag) error = FontGsInvalidSequence;
    else if (!outputs) error = FontGsInvalidParameter;
    else if ((seq->lock & 1) != 0) error = FontGsSequenceLocked;
    else if (seq->commands == nullptr) error = FontGsFatal;
    else return seq->commands;
    sceFontGraphicsReportError(error);
    return nullptr;
}

template <std::uint32_t Opcode>
void APS5_VABI GnmBackendUnported() {
    char name[48];
    std::snprintf(name, sizeof(name), "sceFontSelectGraphicsGnm_backend_%08x", Opcode);
    NotImplemented_nid_no_patch(name);
}

void APS5_VABI GnmBackendTerminate(void*) {}

template <std::uint32_t Opcode>
FontGraphicsBackendEntry GnmUnportedEntry() {
    return {Opcode, 0, reinterpret_cast<std::uint64_t>(&GnmBackendUnported<Opcode>)};
}

struct GnmBackend {
    FontGraphicsServiceParam header;
    FontGraphicsBackendEntry entries[21];
    FontGraphicsBackendList list;
};

static_assert(offsetof(GnmBackend, entries) == 0x20);
static_assert(offsetof(GnmBackend, list) == 0x170);

void* g_gnmInstance = nullptr;

int APS5_VABI GnmBackendEnumerate(void* dispatch, FontGraphicsBackendSink sink);

const GnmBackend g_gnmBackend = {
    {0x0e08, 4, 0x200, &g_gnmInstance, 0x20000, 0, GnmBackendEnumerate},
    {GnmUnportedEntry<0x47000001>(),
     {0x47000002, 0, reinterpret_cast<std::uint64_t>(&GnmBackendTerminate)},
     GnmUnportedEntry<0x4700002d>(),
     GnmUnportedEntry<0x4700002e>(),
     GnmUnportedEntry<0x470000c0>(),
     GnmUnportedEntry<0x470000cb>(),
     GnmUnportedEntry<0x470000ce>(),
     GnmUnportedEntry<0x470000d1>(),
     GnmUnportedEntry<0x470000d2>(),
     GnmUnportedEntry<0x470000d4>(),
     GnmUnportedEntry<0x470000e0>(),
     GnmUnportedEntry<0x470000e1>(),
     GnmUnportedEntry<0x470000ec>(),
     GnmUnportedEntry<0x470000d0>(),
     GnmUnportedEntry<0x470000e2>(),
     GnmUnportedEntry<0x470000e3>(),
     GnmUnportedEntry<0x470000f0>(),
     {0x47000003, 0, 0x1000000040},
     {0x47000004, 0, 0x4000000020},
     GnmUnportedEntry<0x4700000b>(),
     {0x47000005, 0, reinterpret_cast<std::uint64_t>(&GnmBackendStructureTexture)}},
    {0, 21, g_gnmBackend.entries}};

int APS5_VABI GnmBackendEnumerate(void* dispatch, FontGraphicsBackendSink sink) {
    if (dispatch == nullptr) return 0;
    sink(dispatch, &g_gnmBackend.list);
    return 0x400;
}

}

extern "C" {

void* APS5_VABI sceFontSelectGraphicsGnm(std::uint32_t index) {
    return index == 0 ? const_cast<GnmBackend*>(&g_gnmBackend) : nullptr;
}

int APS5_VABI sceFontGraphicsGnmSurfaceInit(void* surface, const void* config) {
    return GnmSurfaceInit(surface, config);
}

int APS5_VABI sceFontGraphicsGnmDrawupFillTextureImage(FontGraphicsFrame* graphicsObject, const void* surface, void** output,
                                                     float x, float y, float width, float height) {
    if (output == nullptr) return FontGsInvalidParameter;
    void* zero = nullptr;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(output, &zero, sizeof(zero));
    errno = savedErrno;
    if (!copied) return FontGsInvalidParameter;
    if (graphicsObject == nullptr || surface == nullptr) return FontGsInvalidParameter;
    const auto* const gnm = ReadGnmSurface(surface);
    if (gnm == nullptr || !(x >= 0.0f)) return FontGsInvalidParameter;
    const float right = x + width;
    if (!(right >= x) || !(right <= FontGnmDrawupLimit) || !(y >= 0.0f)) return FontGsInvalidParameter;
    const float bottom = y + height;
    if (!(bottom >= y) || !(bottom <= FontGnmDrawupLimit)) return FontGsInvalidParameter;
    void* record = nullptr;
    const int error = sceFontGraphicsDrawupFillTextureImageObject(graphicsObject, gnm->fillObject, gnm->mode, &record, x, y, width, height);
    if (error != 0) return error;
    MarkGnmRecord(record);
    const int savedErrno2 = errno;
    const bool copied2 = GuestCopyOut_nid_no_patch(output, &record, sizeof(record));
    errno = savedErrno2;
    if (!copied2) return FontGsInvalidParameter;
    return 0;
}

int APS5_VABI sceFontGraphicsGnmDrawupFillTexturePattern(FontGraphicsFrame* graphicsObject, const void* surface, std::uint32_t format,
                                                       std::uint32_t select, std::uint32_t dimension1, std::uint32_t option1,
                                                       std::uint32_t option2, std::uint32_t dimension2, void** output) {
    if (output == nullptr) return FontGsInvalidParameter;
    *output = nullptr;
    if (graphicsObject == nullptr || surface == nullptr) return FontGsInvalidParameter;
    const auto* const gnm = ReadGnmSurface(surface);
    if (gnm == nullptr) return FontGsInvalidParameter;
    const int error = sceFontGraphicsDrawupFillTexturePatternObject(graphicsObject, gnm->fillObject, gnm->mode, format, select,
                                                                  dimension1, option1, option2, dimension2, output);
    if (error != 0) return error;
    MarkGnmRecord(*output);
    return 0;
}

int APS5_VABI sceFontGraphicsSequenceGetGnmCommandsForChain(FontGraphicsCanvasSequence* sequence, void** commands,
                                                          std::uint32_t* numCommands, void** commands2, std::uint32_t* numCommands2) {
    if (commands != nullptr) *commands = nullptr;
    if (numCommands != nullptr) *numCommands = 0;
    if (commands2 != nullptr) *commands2 = nullptr;
    if (numCommands2 != nullptr) *numCommands2 = 0;
    int error = 0;
    const bool outputs = commands != nullptr && numCommands != nullptr && commands2 != nullptr && numCommands2 != nullptr;
    const auto* const gnm = CheckSequence(sequence, outputs, error);
    if (gnm == nullptr) return error;
    *commands = const_cast<void*>(gnm->first);
    *numCommands = gnm->firstSize >> 2;
    *commands2 = const_cast<void*>(gnm->second);
    *numCommands2 = gnm->secondCount;
    return 0;
}

int APS5_VABI sceFontGraphicsSequenceGetGnmCommandsForSubmit(FontGraphicsCanvasSequence* sequence, void** commands,
                                                           std::uint32_t* numCommands) {
    if (commands != nullptr) *commands = nullptr;
    if (numCommands != nullptr) *numCommands = 0;
    int error = 0;
    const auto* const gnm = CheckSequence(sequence, commands != nullptr && numCommands != nullptr, error);
    if (gnm == nullptr) return error;
    *commands = const_cast<void*>(gnm->first);
    *numCommands = gnm->firstSize;
    return 0;
}

}

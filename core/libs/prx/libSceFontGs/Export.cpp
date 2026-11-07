#include "prx/libSceFontGs/include/InventoryExports.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceGnmDriver/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include "mbedtls/sha256.h"
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
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

constexpr std::uint32_t GnmSel0 = 0;
constexpr std::uint32_t GnmSel1 = 1;
constexpr std::uint32_t GnmSelX = 4;
constexpr std::uint32_t GnmSwapAlt = 1;
constexpr std::uint32_t GnmSwapStdRev = 2;
constexpr std::uint32_t GnmChannelA = 3;

constexpr std::uint32_t GnmSwapChannel(std::uint32_t count, std::uint32_t swap, std::uint32_t component) {
    if (count == 1) return swap;
    const std::uint32_t order = (swap & GnmSwapStdRev) != 0 ? count - 1 - component : component;
    if ((swap & GnmSwapAlt) == 0) return order;
    if (count == 4) return order == 0 ? 2 : order == 2 ? 0 : order;
    return order == count - 1 ? GnmChannelA : order;
}

using GnmSelects = std::array<std::array<std::array<std::uint32_t, 4>, 4>, 4>;

constexpr GnmSelects GnmSwapSelects() {
    GnmSelects selects{};
    for (std::uint32_t count = 1; count <= 4; ++count)
        for (std::uint32_t swap = 0; swap < 4; ++swap) {
            auto& channels = selects[count - 1][swap];
            channels = {GnmSel0, GnmSel0, GnmSel0, GnmSel1};
            for (std::uint32_t component = 0; component < count; ++component) channels[GnmSwapChannel(count, swap, component)] = GnmSelX + component;
        }
    return selects;
}

constexpr GnmSelects GnmChannelSelects = GnmSwapSelects();

std::uint32_t GnmTextureDataFormat(const std::uint8_t* texture) {
    const std::uint32_t word = Read32(texture, 0x10);
    const std::uint32_t surface = (word >> 2) & 0x1f;
    const std::uint32_t numeric = (word >> 8) & 7;
    const std::uint32_t count = GnmComponentCounts[surface];
    const std::uint32_t* const selects = count - 1 < 4 ? GnmChannelSelects[count - 1][(word >> 11) & 3].data() : nullptr;
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

int GnmSetVgtControl(std::uint32_t* cmd, std::uint32_t size, std::uint32_t primGroupSizeMinusOne, std::uint32_t partialVsWaveMode,
                     std::uint32_t switchOnEop) {
    if (cmd == nullptr || size != 3 || primGroupSizeMinusOne > 0xff || partialVsWaveMode > 1 || switchOnEop > 1) return -1;
    cmd[0] = 0xc0016900;
    cmd[1] = 0x2aa;
    cmd[2] = (partialVsWaveMode & 1) << 16 | primGroupSizeMinusOne;
    return 0;
}

int GnmResetVgtControl(std::uint32_t* cmd, std::uint32_t size) {
    if (cmd == nullptr || size != 3) return -1;
    cmd[0] = 0xc0016900;
    cmd[1] = 0x2aa;
    cmd[2] = 0xff;
    return 0;
}

constexpr std::uint32_t GnmShRegBase = 0x2c00;
constexpr std::uint32_t GnmComputeUserData0 = 0x2e40;
constexpr std::uint32_t GnmSpiShaderUserDataPs0 = 0x2c0c;
constexpr std::uint32_t GnmSpiShaderUserDataVs0 = 0x2c4c;
constexpr std::uint32_t GnmSpiShaderUserDataGs0 = 0x2c8c;
constexpr std::uint32_t GnmSpiShaderUserDataEs0 = 0x2ccc;
constexpr std::uint32_t GnmSpiShaderUserDataHs0 = 0x2d0c;
constexpr std::uint32_t GnmSpiShaderUserDataLs0 = 0x2d4c;

int GnmDrawIndirect(std::uint32_t* cmd, std::uint32_t size, std::uint32_t dataOffset, std::uint32_t stage, std::uint32_t vertexSgpr,
                    std::uint32_t instanceSgpr, std::uint32_t flags) {
    constexpr std::uint32_t UserDataBase[7] = {0, 0, GnmSpiShaderUserDataVs0 - GnmShRegBase, 0, GnmSpiShaderUserDataEs0 - GnmShRegBase, 0,
                                               GnmSpiShaderUserDataLs0 - GnmShRegBase};
    if (cmd == nullptr || size != 9 || vertexSgpr > 0xf || instanceSgpr > 0xf || stage > 6 || ((0x2bu >> stage) & 1) != 0) return -1;
    const std::uint32_t base = UserDataBase[stage];
    cmd[0] = (flags & 1) | 0xc0032400;
    cmd[1] = dataOffset;
    cmd[2] = vertexSgpr != 0 ? (base + vertexSgpr) & 0xffff : 0;
    cmd[3] = instanceSgpr != 0 ? (base + instanceSgpr) & 0xffff : 0;
    cmd[4] = 2;
    cmd[5] = 0xc0021000;
    cmd[6] = 0;
    return 0;
}
constexpr std::uint32_t GnmShaderCount = 9;

template <typename T>
T Load(const void* base, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, sizeof(value));
    return value;
}

template <typename T>
void Store(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

std::uint8_t* LoadPtr(const void* base, std::size_t offset) {
    return reinterpret_cast<std::uint8_t*>(Load<std::uintptr_t>(base, offset));
}

void StorePtr(void* base, std::size_t offset, const void* pointer) {
    Store(base, offset, reinterpret_cast<std::uintptr_t>(pointer));
}

void GnmStoreFloats(std::uint8_t* at, std::initializer_list<float> values) {
    std::size_t offset = 0;
    for (const float value : values) {
        Store(at, offset, value);
        offset += 4;
    }
}

void GnmStoreWords(std::uint8_t* at, std::initializer_list<std::uint32_t> values) {
    std::size_t offset = 0;
    for (const std::uint32_t value : values) {
        Store(at, offset, value);
        offset += 4;
    }
}

void GnmStoreOnes(std::uint8_t* at, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) Store(at, 4 * i, 1.0f);
}

std::uint8_t* AlignPtr(std::uint8_t* pointer, std::uintptr_t alignment) {
    return reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(pointer) + alignment - 1) & ~(alignment - 1));
}

struct GnmShaderBlob {
    std::uint32_t kind;
    std::uint32_t offset;
    std::uint32_t size;
};

constexpr std::uint64_t GnmShaderBytesBegin = 0x341c0;
constexpr std::uint64_t GnmShaderBytesEnd = 0x3e7a4;
constexpr std::uint64_t GnmShaderTableBegin = 0x3e7a8;
constexpr std::uint64_t GnmShaderTableEntry = 0x18;
constexpr std::uint64_t GnmShaderTableEnd = GnmShaderTableBegin + GnmShaderCount * GnmShaderTableEntry;
constexpr std::string_view GnmShaderBytesSha256 = "effb7f1f19b0595a6699220a783f3300a63010d0d395c14de358c87edfa6a097";
constexpr std::string_view GnmShaderTableSha256 = "39ac1e2ea16682de9b3ddc81eb2d19027ee8a1233c91ceac7abcebe02b8c31fb";
constexpr const char* GnmModuleName = "libSceFontGs.sprx";

struct GnmModuleShaders {
    std::vector<std::uint8_t> bytes;
    std::array<GnmShaderBlob, GnmShaderCount> blobs;
};

std::filesystem::path GnmExecutableDirectory() {
#ifdef _WIN32
    wchar_t module[MAX_PATH];
    const auto length = GetModuleFileNameW(nullptr, module, MAX_PATH);
    if (length == 0 || length == MAX_PATH) throw std::runtime_error("libSceFontGs: cannot locate the executable");
    return std::filesystem::path(module).parent_path();
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    std::error_code error;
    std::filesystem::path path;
    if (_NSGetExecutablePath(executable.data(), &size) == 0) path = std::filesystem::canonical(executable.data(), error);
    if (path.empty()) throw std::runtime_error("libSceFontGs: cannot locate the executable");
    return path.parent_path();
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}

std::filesystem::path GnmModuleDirectory() {
#ifdef _WIN32
    const wchar_t* configured = _wgetenv(L"ANYPS5_SYSTEM_MODULES");
#else
    const char* configured = std::getenv("ANYPS5_SYSTEM_MODULES");
#endif
    if (!configured || configured[0] == 0) return GnmExecutableDirectory() / "anyps5-modules";
    const std::filesystem::path directory(configured);
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) throw std::runtime_error("libSceFontGs: ANYPS5_SYSTEM_MODULES is not a directory: " + directory.string());
    return directory;
}

std::string GnmSha256(const std::vector<std::uint8_t>& bytes) {
    std::uint8_t digest[32];
    if (mbedtls_sha256(bytes.data(), bytes.size(), digest, 0) != 0) throw std::runtime_error("libSceFontGs: SHA-256 failed");
    constexpr char Digits[] = "0123456789abcdef";
    std::string text;
    for (const std::uint8_t byte : digest) {
        text += Digits[byte >> 4];
        text += Digits[byte & 0xf];
    }
    return text;
}

[[noreturn]] void GnmUnsupportedModule(const std::filesystem::path& path) {
    throw std::runtime_error("libSceFontGs: " + path.string() + " is not a supported build of " + GnmModuleName +
                             ": its GNM shaders do not match the PS4 firmware 10.71 module");
}

std::vector<std::uint8_t> GnmModuleRange(const std::vector<std::uint8_t>& file, const std::filesystem::path& path, std::uint64_t begin,
                                         std::uint64_t end, std::string_view digest) {
    const auto offset = Load<std::uint64_t>(file.data(), 0x20);
    const auto entry = Load<std::uint16_t>(file.data(), 0x36);
    const auto count = Load<std::uint16_t>(file.data(), 0x38);
    if (entry < 0x38 || offset > file.size()) GnmUnsupportedModule(path);
    for (std::uint64_t i = 0; i < count && (i + 1) * entry <= file.size() - offset; ++i) {
        const std::uint8_t* const header = file.data() + offset + i * entry;
        const auto data = Load<std::uint64_t>(header, 8);
        const auto address = Load<std::uint64_t>(header, 0x10);
        const auto size = Load<std::uint64_t>(header, 0x20);
        if (Load<std::uint32_t>(header, 0) != 1 || begin < address || end - address > size || data > file.size() || end - address > file.size() - data) continue;
        const std::vector<std::uint8_t> bytes(file.begin() + static_cast<std::ptrdiff_t>(data + (begin - address)),
                                              file.begin() + static_cast<std::ptrdiff_t>(data + (end - address)));
        if (GnmSha256(bytes) != digest) GnmUnsupportedModule(path);
        return bytes;
    }
    GnmUnsupportedModule(path);
}

GnmModuleShaders GnmLoadShaders() {
    const std::filesystem::path directory = GnmModuleDirectory();
    const std::filesystem::path path = directory / GnmModuleName;
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("libSceFontGs: " + path.string() + " not found; the GNM font backend uploads the shaders of the console's own " +
                                 GnmModuleName + ": copy it from your console, decrypted to an ELF, into " + directory.string() +
                                 " or set ANYPS5_SYSTEM_MODULES to its directory");
    const std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    constexpr std::uint8_t Self[4] = {0x4f, 0x15, 0x3d, 0x1d};
    constexpr std::uint8_t Elf[4] = {0x7f, 'E', 'L', 'F'};
    if (file.size() >= 4 && std::memcmp(file.data(), Self, 4) == 0)
        throw std::runtime_error("libSceFontGs: " + path.string() + " is an encrypted SELF; decrypt it to an ELF on your own console and use that file");
    if (file.size() < 0x40 || std::memcmp(file.data(), Elf, 4) != 0 || file[4] != 2 || file[5] != 1)
        throw std::runtime_error("libSceFontGs: " + path.string() + " is not a decrypted 64-bit ELF of " + GnmModuleName);
    GnmModuleShaders shaders{GnmModuleRange(file, path, GnmShaderBytesBegin, GnmShaderBytesEnd, GnmShaderBytesSha256), {}};
    const std::vector<std::uint8_t> table = GnmModuleRange(file, path, GnmShaderTableBegin, GnmShaderTableEnd, GnmShaderTableSha256);
    for (std::uint32_t i = 0; i < GnmShaderCount; ++i) {
        const auto kind = Load<std::uint64_t>(table.data(), i * GnmShaderTableEntry);
        const auto address = Load<std::uint64_t>(table.data(), i * GnmShaderTableEntry + 8);
        const auto size = Load<std::uint64_t>(table.data(), i * GnmShaderTableEntry + 0x10);
        if ((kind != 1 && kind != 2) || address < GnmShaderBytesBegin || address > GnmShaderBytesEnd || size > GnmShaderBytesEnd - address)
            GnmUnsupportedModule(path);
        shaders.blobs[i] = {static_cast<std::uint32_t>(kind), static_cast<std::uint32_t>(address - GnmShaderBytesBegin), static_cast<std::uint32_t>(size)};
    }
    return shaders;
}

const GnmModuleShaders& GnmLoadedShaders() {
    static const GnmModuleShaders shaders = GnmLoadShaders();
    return shaders;
}

constexpr std::uint32_t GnmShaderObjectOffsets[GnmShaderCount] = {0x000, 0x050, 0x0b8, 0x120, 0x188, 0x220, 0x288, 0x320, 0x3b8};

struct GnmShaderResource {
    std::uint32_t member;
    std::uint8_t type;
    std::uint8_t slot;
    std::uint32_t mask;
    std::uint32_t bit;
};

struct GnmShaderClass {
    std::uint32_t slots;
    std::uint32_t limitOffset;
    std::uint32_t limit;
    std::uint32_t limit2Offset;
    std::uint32_t limit2;
    std::uint32_t maskBase;
    std::uint32_t resourceCount;
    GnmShaderResource resources[5];
};

constexpr GnmShaderClass GnmShaderClasses[GnmShaderCount] = {
    {0x28, 0, 0, 0, 0, 0, 0, {}},
    {0x28, 0x4c, 0x08, 0, 0, 0x58, 1, {{0x50, 2, 0, 0, 1}}},
    {0x28, 0x4c, 0x20, 0, 0, 0x58, 1, {{0x50, 2, 0, 0, 1}}},
    {0x28, 0x4c, 0x50, 0, 0, 0x58, 1, {{0x50, 2, 0, 0, 1}}},
    {0x28, 0, 0, 0, 0, 0, 0, {}},
    {0x3c, 0x4c, 0x10, 0, 0, 0x58, 1, {{0x50, 2, 0, 0, 1}}},
    {0x3c, 0x4c, 0x10, 0, 0, 0x88, 3, {{0x50, 2, 0, 0, 1}, {0x68, 0, 0, 8, 1}, {0x80, 1, 0, 4, 1}}},
    {0x3c, 0x4c, 0x10, 0, 0, 0x88, 3, {{0x50, 2, 0, 0, 1}, {0x68, 0, 0, 8, 1}, {0x80, 1, 0, 4, 1}}},
    {0x3c, 0x4c, 0x40, 0x64, 0x40, 0xa0, 5,
     {{0x50, 2, 0, 0, 1}, {0x68, 2, 1, 0, 2}, {0x80, 0, 0, 8, 1}, {0x80, 0, 0, 0xc, 1}, {0x98, 0, 1, 8, 2}}}};

constexpr std::uint32_t GnmFetchShaderIndex = 4;
constexpr std::uint32_t GnmFetchRemap[1] = {0};

alignas(16) std::uint8_t g_gnmShaders[0x4d0] = {};

std::uint8_t* GnmShaderObject(std::uint32_t index) { return g_gnmShaders + GnmShaderObjectOffsets[index]; }

void GnmConstructShaderObjects() {
    Store<std::uint64_t>(GnmShaderObject(8), 0x60, 1);
    Store<std::uint64_t>(GnmShaderObject(8), 0x90, 1);
}

const std::uint8_t* GnmFindUsageSlot(const std::uint8_t* slots, std::uint32_t count, std::uint8_t type) {
    for (std::uint32_t i = 0; i < count; ++i)
        if (slots[4 * i] == type) return slots + 4 * i;
    return nullptr;
}

void GnmParseShader(std::uint32_t index, std::uint8_t* header) {
    std::uint8_t* const object = GnmShaderObject(index);
    const GnmShaderClass& info = GnmShaderClasses[index];
    StorePtr(object, 0x20, header);
    const std::uint8_t* const slots = header + info.slots;
    const std::uint32_t count = Load<std::uint32_t>(header, 0) >> 24;
    const std::uint8_t* const constants = GnmFindUsageSlot(slots, count, 0xa);
    StorePtr(object, 0x30, constants);
    Store<std::uint32_t>(object, 0x28, constants != nullptr ? constants[3] + 1u : 0u);
    std::int32_t top = 0;
    std::int32_t end = 0;
    for (std::uint32_t i = count; i > 0; --i) {
        const std::int32_t start = slots[4 * (i - 1) + 2];
        if (top > start) continue;
        const std::int32_t last = start + (slots[4 * (i - 1) + 3] & 1) * 4 + 4;
        if (last >= end) {
            end = last;
            top = start;
        }
    }
    Store<std::int32_t>(object, 0x2c, count != 0 && top >= 0x10 ? end * 4 - 0x40 : 0);
    StorePtr(object, 0x38, GnmFindUsageSlot(slots, count, 0x1b));
    for (std::uint32_t r = 0; r < info.resourceCount; ++r) {
        const GnmShaderResource& resource = info.resources[r];
        const std::uint8_t* found = nullptr;
        for (std::uint32_t i = 0; i < count && found == nullptr; ++i)
            if (slots[4 * i] == resource.type && slots[4 * i + 1] == resource.slot) found = slots + 4 * i;
        StorePtr(object, resource.member, found);
        if (found != nullptr) {
            const std::size_t mask = info.maskBase + resource.mask;
            Store(object, mask, Load<std::uint32_t>(object, mask) | resource.bit);
        }
    }
    if (info.limitOffset != 0) Store(object, info.limitOffset, info.limit);
    if (info.limit2Offset != 0) Store(object, info.limit2Offset, info.limit2);
}
struct GnmFetchState {
    std::uint16_t size;
    std::uint16_t reserved2;
    std::uint8_t firstSgpr;
    std::uint8_t loadsPerGroup;
    std::uint16_t remapCount;
    std::uint32_t rsrc;
    std::uint32_t reserved0c;
    const std::uint32_t* remap;
    std::uint8_t inputCount;
    std::uint8_t vertexOffset;
    std::uint8_t instanceOffset;
    std::uint8_t reserved1b;
    std::uint32_t slotCount;
    const std::uint8_t* inputs;
    const std::uint8_t* slots;
    std::uint32_t trailer;
    std::uint32_t reserved34;
    const std::uint8_t* trailerSource;
    std::uint8_t* address;
};

static_assert(offsetof(GnmFetchState, remap) == 0x10);
static_assert(offsetof(GnmFetchState, inputCount) == 0x18);
static_assert(offsetof(GnmFetchState, inputs) == 0x20);
static_assert(offsetof(GnmFetchState, trailer) == 0x30);
static_assert(offsetof(GnmFetchState, address) == 0x40);

void GnmBuildFetchState(GnmFetchState& state, const std::uint8_t* registers, std::uint32_t inputs, const std::uint32_t* remap,
                        std::uint32_t remapCount, std::uint32_t vertexOffset, std::uint32_t instanceOffset) {
    const std::uint32_t rsrc1 = Load<std::uint32_t>(registers, 8);
    const std::uint32_t rsrc2 = Load<std::uint32_t>(registers, 0xc);
    const std::uint32_t enable = (rsrc2 >> 7) & 1;
    const std::uint32_t used = ((rsrc2 >> 1) & 0x1f) + (rsrc2 & 1) + ((rsrc2 >> 12) & 1) + ((rsrc2 >> 8) & 1) + ((rsrc2 >> 9) & 1) +
                               ((rsrc2 >> 10) & 1) + ((rsrc2 >> 11) & 1) + enable;
    const std::uint32_t first = ((((rsrc2 >> 12) & 1) | enable) + used + 3) & ~3u;
    const std::uint32_t sgprBlocks = rsrc1 >> 6;
    const std::uint32_t free = ((sgprBlocks & 0xf) << 3) - first + 8;
    const std::uint32_t exhausted = free == 0 ? 1 : 0;
    const std::uint32_t perGroup = (free >> 2) + exhausted * 2;
    const std::uint32_t groups = (inputs + perGroup - 1) / perGroup;
    const std::uint32_t loads = inputs + (vertexOffset != 0 ? 1 : 0) + (instanceOffset != 0 ? 1 : 0) + groups;
    state.size = static_cast<std::uint16_t>((loads << 2) + inputs * 8 + 0xc);
    state.reserved2 = 0;
    state.firstSgpr = static_cast<std::uint8_t>(first);
    state.loadsPerGroup = static_cast<std::uint8_t>(perGroup);
    state.remap = remap;
    state.remapCount = static_cast<std::uint16_t>(remap != nullptr ? remapCount : 0);
    state.vertexOffset = static_cast<std::uint8_t>(vertexOffset);
    state.instanceOffset = static_cast<std::uint8_t>(instanceOffset);
    state.reserved1b = 0;
    state.reserved34 = 0;
    state.reserved0c = 0;
    state.rsrc = (remap != nullptr ? 0x3000000u : rsrc1 & 0x3000000u) | ((sgprBlocks + exhausted) & 0xf) << 6;
}

void GnmGenerateFetchShader(std::uint32_t* out, const GnmFetchState& state) {
    const std::uint32_t formats[4] = {0, 3, (state.reserved2 & 1u) + 1, 2};
    if (state.slotCount == 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::uint32_t base = 0xffffffff;
    for (std::uint32_t i = 0; i < state.slotCount; ++i)
        if (state.slots[4 * i] == 0x17) base = state.slots[4 * i + 2];
    if (state.vertexOffset == 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::uint32_t at = 0;
    out[at++] = state.vertexOffset | 0x4a000000;
    if (state.instanceOffset != 0) out[at++] = state.instanceOffset | 0x4a060600;
    const std::uint32_t inputs = state.inputCount;
    const std::uint32_t sbase = (base & 0x7e) << 8;
    std::uint32_t next = 0;
    while (next < inputs) {
        const std::uint32_t start = next;
        if (state.loadsPerGroup == 0 || state.trailerSource != nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
        for (std::uint32_t load = 0; load < state.loadsPerGroup && next < inputs; ++load, ++next) {
            const std::uint32_t offset = (state.inputs[4 * next] << 2) & 0xff;
            out[at++] = ((state.firstSgpr + load * 4) & 0x7f) << 15 | offset | sbase | 0xc0800100;
        }
        out[at++] = 0xbf8c007f;
        std::uint32_t resource = (state.firstSgpr >> 2) << 16;
        for (std::uint32_t i = start; i < next; ++i) {
            const std::uint32_t format = state.remap != nullptr && i < state.remapCount ? state.remap[i] : 0;
            const std::uint32_t count = (state.inputs[4 * i + 2] - 1u) & 0x7f;
            out[at++] = count << 18 | 0xe0002000;
            out[at++] = (resource & 0x1f0000) + (state.inputs[4 * i + 1] << 8 | (formats[format] & 0xff)) - 0x80000000;
            resource += 0x10000;
        }
    }
    out[at] = 0xbf8c0000;
    out[at + 1] = 0xbe802000;
    out[at + 2] = state.trailer != 0 ? state.trailer : inputs;
}

void GnmPrepareShaderObject(std::uint32_t index, std::uint8_t* header, GnmFetchState& state) {
    std::uint8_t* const object = GnmShaderObject(index);
    if (index != GnmFetchShaderIndex) {
        if (GnmShaderClasses[index].slots == 0x28) state.address = nullptr;
        GnmParseShader(index, header);
        return;
    }
    Store<std::uint64_t>(object, 0x40, 0);
    Store<std::uint64_t>(object, 0x60, 0);
    Store<std::uint32_t>(object, 0x68, 0);
    Store<std::uint64_t>(object, 0x70, 0);
    Store<std::uint32_t>(object, 0x78, 0);
    GnmParseShader(index, header);
    Store<std::uint64_t>(object, 0x48, 0x100000000);
    StorePtr(object, 0x60, GnmFetchRemap);
    Store<std::uint32_t>(object, 0x68, 1);
    Store<std::uint64_t>(object, 0x70, 0);
    Store<std::uint32_t>(object, 0x78, 0);
    std::uint8_t* const shader = LoadPtr(object, 0x20);
    if (shader == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    const std::uint8_t layout = shader[0x27];
    GnmBuildFetchState(state, shader + 8, shader[0x24], reinterpret_cast<const std::uint32_t*>(LoadPtr(object, 0x60)),
                       Load<std::uint32_t>(object, 0x68), layout & 0xf, layout >> 4);
    if (state.size == 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    const std::uint32_t count = Load<std::uint32_t>(shader, 0) >> 24;
    const std::uint8_t* const slots = shader + 0x28;
    state.inputs = slots + count * 4;
    state.inputCount = shader[0x24];
    state.slots = slots;
    state.slotCount = count;
    state.trailerSource = LoadPtr(object, 0x70);
    state.trailer = Load<std::uint32_t>(object, 0x78);
    Store(shader, 0x10, (Load<std::uint32_t>(shader, 0x10) & 0xfcfffc3f) | state.rsrc);
    StorePtr(object, 0x50, GnmFindUsageSlot(slots, count, 0x12));
    StorePtr(object, 0x58, GnmFindUsageSlot(slots, count, 0x17));
    state.address = object + 0x40;
}

int GnmUploadShader(std::uint32_t index, std::uint8_t* headers, std::uint32_t headerRoom, std::uint32_t* headerUsed, std::uint8_t* code,
                    std::uint32_t codeRoom, std::uint32_t* codeUsed, GnmFetchState& state) {
    std::uint8_t* const object = GnmShaderObject(index);
    const std::uint8_t* const blob = LoadPtr(object, 0x10);
    const std::uint8_t* const header = blob + 0x34;
    const std::uint8_t* const program = blob + 0x34 + (Load<std::uint32_t>(blob, 0x3c) & ~3u);
    const std::uint32_t programSize = (Load<std::uint32_t>(blob, 0x34) & 0x7fffff) + Load<std::uint16_t>(blob, 0x38) * 16u;
    std::uint8_t* const headerAt = AlignPtr(headers, 8);
    std::uint8_t* const codeAt = AlignPtr(code, 0x100);
    const std::uint32_t kind = Load<std::uint32_t>(object, 0x18);
    const std::uint32_t entries = Load<std::uint32_t>(header, 0) >> 24;
    std::uint32_t headerSize = 0;
    if (kind == 2) headerSize = header[0x38] * 2u + entries * 4 + 0x3f;
    else if (kind == 1) headerSize = header[0x24] * 4u + entries * 4 + header[0x25] * 2u + 0x2b;
    else NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    headerSize &= ~3u;
    const bool headerFull = headers != nullptr && static_cast<std::int64_t>(headerAt + headerSize - headers) > static_cast<std::int64_t>(headerRoom);
    if (headerSize == 0 || programSize == 0 || code == nullptr || headerFull || headers == nullptr ||
        static_cast<std::int64_t>(codeAt + programSize - code) > static_cast<std::int64_t>(codeRoom))
        NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::memcpy(headerAt, header, headerSize);
    std::memcpy(codeAt, program, programSize);
    if (kind - 1 <= 1) {
        Store(headerAt, 8, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(codeAt) >> 8));
        Store(headerAt, 0xc, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(codeAt) >> 40));
    }
    std::memcpy(codeAt, program, programSize);
    GnmPrepareShaderObject(index, headerAt, state);
    std::uint32_t gpuSize = programSize;
    if (state.address != nullptr) {
        gpuSize = programSize + state.size;
        auto* const fetch = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(codeAt) + programSize + 3) & ~std::uintptr_t{3});
        if (static_cast<std::int64_t>(fetch + state.size - code) > static_cast<std::int64_t>(codeRoom))
            NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
        GnmGenerateFetchShader(reinterpret_cast<std::uint32_t*>(fetch), state);
        StorePtr(state.address, 0, fetch);
    }
    *headerUsed = headerSize + static_cast<std::uint32_t>(headerAt - headers);
    *codeUsed = gpuSize + static_cast<std::uint32_t>(codeAt - code);
    return 1;
}
std::uint32_t GnmSwap32(std::uint32_t value) {
    return value >> 24 | (value >> 8 & 0xff00) | (value << 8 & 0xff0000) | value << 24;
}

constexpr float GnmShaderKeyDivisor = 100.0f;
constexpr float GnmShaderKeyStep = 1.1f;

void GnmDecodeShader(std::uint8_t* destination, const std::uint8_t* source, std::uint32_t size) {
    float key = static_cast<float>(static_cast<std::int32_t>(size)) / GnmShaderKeyDivisor;
    const float step = GnmShaderKeyStep;
    for (std::uint32_t at = 0; at + 4 <= size; at += 4) {
        const std::uint32_t word = (static_cast<std::uint32_t>(source[at]) << 24 | static_cast<std::uint32_t>(source[at + 2]) << 16 |
                                    static_cast<std::uint32_t>(source[at + 1]) << 8 | source[at + 3]) ^
                                   std::bit_cast<std::uint32_t>(key);
        Store(destination, at, GnmSwap32(word));
        key += step;
    }
}

struct GnmInitLayout {
    std::uint8_t* cursor;
    std::uint8_t* gpu;
    std::uint8_t* code;
    std::uint8_t* tail;
};

bool GnmUploadShaders(GnmInitLayout& layout) {
    const GnmModuleShaders& shaders = GnmLoadedShaders();
    const std::uint8_t* const blobs = shaders.bytes.data();
    for (std::uint32_t i = 0; i < GnmShaderCount; ++i) {
        std::uint8_t* const object = GnmShaderObject(i);
        const GnmShaderBlob& blob = shaders.blobs[i];
        std::uint8_t* const destination = AlignPtr(layout.cursor, 8);
        std::uint8_t* const next = destination != nullptr ? destination + blob.size : layout.cursor;
        if (next >= layout.gpu) return false;
        layout.cursor = next;
        if (static_cast<std::int32_t>(blob.size) >= 4) GnmDecodeShader(destination, blobs + blob.offset, blob.size);
        Store(object, 0x18, blob.kind);
        StorePtr(object, 0x10, destination);
        Store<std::uint64_t>(object, 0x08, blob.size);
    }
    std::uint32_t headerRoom = static_cast<std::uint32_t>(layout.gpu - layout.cursor);
    std::uint32_t codeRoom = 0xc000;
    int status = 0;
    GnmFetchState fetch{};
    for (std::uint32_t i = 0; i < GnmShaderCount; ++i) {
        std::uint32_t headerUsed = 0;
        std::uint32_t codeUsed = 0;
        if (GnmUploadShader(i, layout.cursor, headerRoom, &headerUsed, layout.code, codeRoom, &codeUsed, fetch) < status) status = -1;
        if (layout.cursor != nullptr) layout.cursor += headerUsed;
        if (layout.code != nullptr) layout.code += codeUsed;
        headerRoom -= headerRoom < headerUsed ? 0 : headerUsed;
        codeRoom -= codeRoom < codeUsed ? 0 : codeUsed;
    }
    return status >= 0;
}
constexpr std::size_t GnmCbBegin = 0x00;
constexpr std::size_t GnmCbEnd = 0x08;
constexpr std::size_t GnmCbCursor = 0x10;
constexpr std::size_t GnmCbFlags = 0x28;

void GnmDefaultCallback() {}

void GnmCbInit(std::uint8_t* cb, std::uint8_t* begin, std::uint32_t bytes) {
    const std::uint32_t dwords = bytes >> 2;
    StorePtr(cb, GnmCbCursor, begin);
    StorePtr(cb, GnmCbBegin, begin);
    Store(cb, 0x38, dwords);
    StorePtr(cb, GnmCbEnd, begin + static_cast<std::uintptr_t>(dwords) * 4);
    StorePtr(cb, 0x18, reinterpret_cast<const void*>(&GnmDefaultCallback));
    Store<std::uint64_t>(cb, 0x20, 0);
    cb[GnmCbFlags] &= 0xfc;
    Store<std::uint64_t>(cb, 0x30, 0);
    Store<std::uint32_t>(cb, 0x3c, 0);
}

std::uint8_t* GnmCbCur(const std::uint8_t* cb) { return LoadPtr(cb, GnmCbCursor); }

std::uint8_t* GnmEmit(std::uint8_t* cb, std::initializer_list<std::uint32_t> words) {
    std::uint8_t* const at = GnmCbCur(cb);
    std::size_t offset = 0;
    for (const std::uint32_t word : words) {
        Store(at, offset, word);
        offset += 4;
    }
    StorePtr(cb, GnmCbCursor, at + offset);
    return at;
}

void GnmContextReg(std::uint8_t* cb, std::uint32_t reg, std::uint32_t value) { GnmEmit(cb, {0xc0016900, reg, value}); }

void GnmNop(std::uint8_t* cb, std::uint32_t dwords) {
    if (dwords == 0) return;
    if (dwords == 1) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::uint8_t* const at = GnmCbCur(cb);
    Store(at, 0, ((dwords << 16) + 0x3ffe0000u) | 0xc0001000u);
    Store<std::uint32_t>(at, 4, 0);
    StorePtr(cb, GnmCbCursor, at + static_cast<std::uintptr_t>(dwords) * 4);
}

std::uint8_t* GnmMarker(std::uint8_t* cb, std::uint32_t dwords) {
    if (dwords != 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::uint8_t* const at = GnmCbCur(cb);
    Store<std::uint64_t>(at, 0, 0xc0005900);
    StorePtr(cb, GnmCbCursor, at + 8);
    return at + 8;
}

void GnmIndirectBuffer(std::uint8_t* cb, const void* address, std::uint32_t dwords) {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    GnmEmit(cb, {0xc0023f00, static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32) & 0xffff,
                 (dwords & 0xfffff) | 0x1900000});
}

constexpr std::uint32_t GnmShBases[8] = {GnmComputeUserData0,     GnmSpiShaderUserDataPs0, GnmSpiShaderUserDataVs0, GnmSpiShaderUserDataGs0,
                                         GnmSpiShaderUserDataEs0, GnmSpiShaderUserDataHs0, GnmSpiShaderUserDataLs0, 0};

void GnmShRegs(std::uint8_t* cb, std::uint32_t stage, std::uint32_t reg, const std::uint32_t* values, std::uint32_t count) {
    std::uint8_t* const at = GnmCbCur(cb);
    Store<std::uint64_t>(at, 0, 0x6875000dc0001000);
    Store(at, 8, count << 16 | (stage == 0 ? 2u : 0u) | 0xc0007600);
    Store(at, 0xc, reg + GnmShBases[stage] - GnmShRegBase);
    if (count <= 0x10) {
        if (count == 0) Store<std::uint64_t>(at, 8, 0xc0001000);
        for (std::uint32_t i = 0; i < count; ++i) Store(at, 0x10 + 4 * i, values[i]);
    }
    StorePtr(cb, GnmCbCursor, at + 8 + static_cast<std::uintptr_t>(count + 2) * 4);
}

void GnmFlushGarlicAt(std::uint8_t* at) {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    sceGnmFlushGarlic();
    at[-1] |= 0x80;
}

std::uint8_t* GnmReservedAt(const std::uint8_t* command) { return LoadPtr(command, 8) + Load<std::uint32_t>(command, 0) * 4u; }

std::uint32_t GnmReserve(std::uint8_t* command, std::uint32_t dwords) {
    std::uint8_t* const begin = LoadPtr(command, 8);
    std::uint8_t* const cursor = LoadPtr(command, 0x18);
    std::uint32_t skip = Load<std::uint32_t>(command, 0);
    std::uint8_t* const at = begin + skip * 4u;
    std::uint8_t* const from = cursor > at ? cursor : at;
    skip += (cursor > at ? static_cast<std::uint32_t>((cursor - at) >> 2) : 0) + dwords;
    Store(command, 0, skip);
    std::uint8_t* const to = begin + skip * 4u;
    if (cursor > to) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    return to >= from && to <= LoadPtr(command, 0x10) ? static_cast<std::uint32_t>(to - from) : 0;
}

void GnmResetAux(std::uint8_t* aux) {
    std::memset(aux + 0x20, 0, 0x10);
    GnmCbInit(aux + 0x30, nullptr, 0);
    Store<std::uint64_t>(aux, 0x70, 0);
}

std::uint8_t* GnmFlush(std::uint8_t* command) {
    const std::uint32_t skip = Load<std::uint32_t>(command, 0);
    std::uint8_t* cursor = LoadPtr(command, 0x18);
    std::uint8_t* const at = LoadPtr(command, 8) + skip * 4u;
    if (cursor > at) {
        Store(command, 0, skip + static_cast<std::uint32_t>((cursor - at) >> 2));
    } else {
        if (cursor < at) StorePtr(command, 0x18, cursor + ((at - cursor) & 0x3fffffffc));
        cursor = at;
    }
    std::uint8_t* const aux = LoadPtr(command, 0x48);
    if (aux != nullptr) {
        GnmResetAux(aux);
        StorePtr(command, 8, LoadPtr(command, 0x18));
    }
    Store<std::uint32_t>(command, 0, 0);
    return cursor;
}

void GnmLinkPending(std::uint8_t* aux, std::uint8_t* begin, std::uint8_t* end) {
    std::uint8_t* const pending = LoadPtr(aux, 0x70);
    if (pending != nullptr) {
        GnmIndirectBuffer(aux + 0x30, pending, static_cast<std::uint32_t>((end - pending) >> 2));
        return;
    }
    StorePtr(aux, 0x20, begin);
    StorePtr(aux, 0x28, end);
}

std::uint8_t* GnmSyncReserved(std::uint8_t* command) {
    const std::uint32_t skip = Load<std::uint32_t>(command, 0);
    std::uint8_t* const cursor = LoadPtr(command, 0x18);
    std::uint8_t* at = LoadPtr(command, 8) + static_cast<std::uintptr_t>(skip) * 4;
    if (cursor > at) {
        Store(command, 0, skip + static_cast<std::uint32_t>((cursor - at) >> 2));
        at = cursor;
    }
    return at;
}

void GnmRestartAux(std::uint8_t* command, bool rebase) {
    std::uint8_t* const aux = LoadPtr(command, 0x48);
    if (aux == nullptr) return;
    GnmResetAux(aux);
    if (rebase) StorePtr(command, 8, LoadPtr(command, 0x18));
}

std::uint32_t GnmBeginCommands(std::uint8_t* buffer, std::uint8_t* command) {
    Store<std::uint64_t>(command, 0, 0);
    GnmCbInit(command + 8, buffer, 0x1320);
    std::uint8_t* const cursor = LoadPtr(command, 0x18);
    std::uint8_t* const begin = LoadPtr(command, 8);
    Store<std::uint32_t>(command, 4, 0);
    const std::uint32_t reserved = cursor > begin ? static_cast<std::uint32_t>((cursor - begin) >> 2) + 4 : 4;
    Store(command, 0, reserved);
    std::uint8_t* const at = begin + reserved * 4u;
    if (cursor > at) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::uint8_t tail[0x40];
    GnmCbInit(tail, cursor, (Load<std::uint32_t>(command, 0x10) - static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(cursor))) & ~3u);
    const std::uint32_t total = Load<std::uint32_t>(command, 0) + static_cast<std::uint32_t>((LoadPtr(tail, 0) - at + 0x10) >> 2);
    Store(command, 0, total);
    std::uint8_t* const end = LoadPtr(command, 8) + total * 4u;
    if (LoadPtr(command, 0x18) > end) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    const std::uint32_t used = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(end)) -
                               static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(cursor > begin ? cursor : begin));
    return used < 0x1321 ? used : 0;
}

std::uint32_t GnmPrologue(std::uint8_t* header, std::uint8_t* buffer, std::uint8_t* cb) {
    GnmCbInit(cb, buffer, 0x1320);
    std::uint8_t* const begin = LoadPtr(cb, GnmCbBegin);
    StorePtr(cb, GnmCbCursor, begin);
    StorePtr(cb, GnmCbEnd, begin + Load<std::uint32_t>(cb, 0x38) * 4u);
    std::uint8_t* const first = GnmCbCur(cb);
    GnmMarker(cb, 0);
    std::uint8_t* const second = GnmCbCur(cb);
    const std::uint32_t firstSize = static_cast<std::uint32_t>(second - begin);
    std::uint8_t* const flushed = GnmMarker(cb, 0);
    const std::uint32_t secondSize = static_cast<std::uint32_t>(GnmCbCur(cb) - second);
    GnmFlushGarlicAt(flushed);
    std::uint8_t* const loop = GnmCbCur(cb);
    std::uint8_t local[0x40];
    GnmCbInit(local, loop, (Load<std::uint32_t>(cb, GnmCbEnd) - static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(loop))) & ~3u);
    GnmIndirectBuffer(local, loop, 4);
    const auto loopDwords = static_cast<std::uint32_t>((GnmCbCur(local) - loop) >> 2);
    GnmNop(cb, loopDwords);
    StorePtr(header, 0x08, first);
    Store(header, 0x18, firstSize);
    StorePtr(header, 0x10, second);
    Store(header, 0x1c, secondSize);
    Store(header, 0x2c, loopDwords);
    StorePtr(header, 0x30, loop);
    StorePtr(LoadPtr(cb, 0x40), 0, header);
    const std::uint32_t used = static_cast<std::uint32_t>(GnmCbCur(cb) - begin);
    return used >= 0x1321 ? 0 : used;
}

std::uint32_t GnmReserveLoop(std::uint8_t* command) {
    std::uint8_t* const aux = LoadPtr(command, 0x48);
    if (aux == nullptr || LoadPtr(aux, 0) == nullptr) return 0;
    std::uint8_t* const header = LoadPtr(aux, 0);
    std::uint8_t* const begin = LoadPtr(command, 8);
    std::uint8_t* const cursor = LoadPtr(command, 0x18);
    const std::uint32_t skip = Load<std::uint32_t>(command, 0);
    std::uint8_t* const at = begin + skip * 4u;
    std::uint8_t* const from = cursor > at ? cursor : at;
    const std::uint32_t first = cursor > at ? static_cast<std::uint32_t>((cursor - at) >> 2) : 0;
    std::uint8_t* const mid = begin + (skip + first + 2) * 4u;
    const std::uint32_t second = cursor > mid ? static_cast<std::uint32_t>((cursor - mid) >> 2) : 0;
    const std::uint32_t total = second + first + skip + 2 + Load<std::uint32_t>(header, 0x2c);
    Store(command, 0, total);
    std::uint8_t* const to = begin + total * 4u;
    if (cursor > to) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    if (to < from) return 0;
    return to <= LoadPtr(command, 0x10) ? static_cast<std::uint32_t>(to - from) : 0;
}

std::uint32_t GnmEmitLoop(std::uint8_t* cb) {
    std::uint8_t* const aux = LoadPtr(cb, 0x40);
    if (aux == nullptr || LoadPtr(aux, 0) == nullptr) return 0;
    std::uint8_t* const header = LoadPtr(aux, 0);
    std::uint8_t* const start = GnmCbCur(cb);
    GnmEmit(cb, {0xc0004200, 0});
    std::uint8_t* const nop = GnmCbCur(cb);
    GnmNop(cb, Load<std::uint32_t>(header, 0x2c));
    std::uint8_t* const end = GnmCbCur(cb);
    if (end < start || end > LoadPtr(cb, GnmCbEnd)) return 0;
    const auto bytes = static_cast<std::uint32_t>(end - start);
    StorePtr(header, 0x20, start);
    Store(header, 0x28, bytes >> 2);
    StorePtr(header, 0x38, nop);
    return bytes;
}

std::uint32_t GnmCallLoop(std::uint8_t* cb) {
    std::uint8_t* const header = LoadPtr(LoadPtr(cb, 0x40), 0);
    std::uint8_t* const start = GnmCbCur(cb);
    GnmIndirectBuffer(cb, LoadPtr(header, 0x20), Load<std::uint32_t>(header, 0x28));
    std::uint8_t* const end = GnmCbCur(cb);
    if (end < start || end > LoadPtr(cb, GnmCbEnd)) return 0;
    std::uint8_t* const aux = LoadPtr(cb, 0x40);
    GnmLinkPending(aux, LoadPtr(cb, GnmCbBegin), end);
    Store<std::uint64_t>(aux, 0x70, 0);
    GnmCbInit(aux + 0x30, nullptr, 0);
    return static_cast<std::uint32_t>(end - start);
}
bool GnmIsNeo() { return sceKernelIsNeoMode() != 0; }

constexpr std::uint32_t GnmSpiShaderPgmRsrc3Ps = 0x2c07;
constexpr std::uint32_t GnmSpiShaderPgmRsrc3Vs = 0x2c46;
constexpr std::uint32_t GnmSpiShaderLateAllocVs = 0x2c47;
constexpr std::uint32_t GnmSpiShaderPgmRsrc3Gs = 0x2c87;
constexpr std::uint32_t GnmSpiShaderPgmRsrc3Es = 0x2cc7;
constexpr std::uint32_t GnmSpiShaderPgmRsrc3Hs = 0x2d07;
constexpr std::uint32_t GnmSpiShaderPgmRsrc3Ls = 0x2d47;
constexpr std::uint32_t GnmSpiShaderPgmRsrc3CuEnShift = 0;
constexpr std::uint32_t GnmSpiShaderPgmRsrc3WaveLimitShift = 16;

std::uint32_t GnmFloatBits(float value) { return std::bit_cast<std::uint32_t>(value); }

void GnmUconfigReg(std::uint8_t* cb, std::uint32_t reg, std::uint32_t value) { GnmEmit(cb, {0xc0017900, reg, value}); }

void GnmVgtControl(std::uint8_t* cb, std::uint32_t primGroupSize, std::uint32_t partialVsWave, std::uint32_t switchOnEop) {
    if (GnmSetVgtControl(reinterpret_cast<std::uint32_t*>(GnmCbCur(cb)), 3, primGroupSize, partialVsWave, switchOnEop) == 0)
        StorePtr(cb, GnmCbCursor, GnmCbCur(cb) + 0xc);
}

void GnmResetVgt(std::uint8_t* cb) {
    if (GnmResetVgtControl(reinterpret_cast<std::uint32_t*>(GnmCbCur(cb)), 3) == 0) StorePtr(cb, GnmCbCursor, GnmCbCur(cb) + 0xc);
}

void GnmDrawIndirectAt(std::uint8_t* cb, std::uint32_t offset, std::uint32_t stage, std::uint32_t vertexSgpr, std::uint32_t instanceSgpr) {
    if (GnmDrawIndirect(reinterpret_cast<std::uint32_t*>(GnmCbCur(cb)), 9, offset, stage, vertexSgpr, instanceSgpr, Load<std::uint32_t>(cb, GnmCbFlags) & 1) == 0)
        StorePtr(cb, GnmCbCursor, GnmCbCur(cb) + 0x24);
}

std::uint32_t GnmEmitted(const std::uint8_t* cb, const std::uint8_t* start) {
    std::uint8_t* const end = GnmCbCur(cb);
    return end >= start && end <= LoadPtr(cb, GnmCbEnd) ? static_cast<std::uint32_t>(end - start) : 0;
}

std::uint32_t GnmReserveCounted(std::uint8_t* command, std::uint8_t* start, std::uint32_t skip) {
    Store(command, 0, skip);
    std::uint8_t* const at = LoadPtr(command, 8) + skip * 4u;
    if (LoadPtr(command, 0x18) > at) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    return at >= start && at <= LoadPtr(command, 0x10) ? static_cast<std::uint32_t>(at - start) : 0;
}

std::uint8_t GnmHeaderFlags(const std::uint8_t* command) { return LoadPtr(LoadPtr(command, 0x48), 0)[4]; }

std::uint8_t* GnmReserveStart(std::uint8_t* command) {
    std::uint8_t* const at = LoadPtr(command, 8) + Load<std::uint32_t>(command, 0) * 4u;
    if (LoadPtr(command, 0x18) > at) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    return at;
}

std::uint32_t GnmReserveDefaults(std::uint8_t* command) {
    const bool neo = GnmIsNeo();
    std::uint8_t* const start = GnmReserveStart(command);
    std::uint32_t skip = Load<std::uint32_t>(command, 0) + 0x15;
    if ((GnmHeaderFlags(command) & 1) != 0) skip += 3;
    skip += neo ? 3 + 0x1f : 0x15;
    skip += 3 + 6 + 0xf + 6 + (neo ? 0x2b : 0x25) + 3 + 3 + 3 + 6;
    return GnmReserveCounted(command, start, skip);
}

std::uint32_t GnmDepthTest(std::uint32_t value, std::uint32_t write, std::uint32_t function) {
    return (value & 0xfffff807u) | (function & 7) << 8 | ((write << 5) & 0xff) | (((write ^ 2) | (function ^ 2)) != 0 ? 8u : 0u);
}

std::uint32_t GnmDepthControlValue() {
    std::uint32_t value = 0x240;
    value = (value & ~3u) | (0 & 3u);
    value = GnmDepthTest(value, 2, 2);
    value = (value & 0xfff7ffffu) | (1u & 1) << 19;
    value &= 0xffffe7ffu;
    value &= 0xfffeffffu;
    value = (value & 0xffefffffu) | ((1u ^ 1) & 0xff) << 20;
    return value;
}

std::uint32_t GnmEmitDefaults(std::uint8_t* cb) {
    const bool neo = GnmIsNeo();
    std::uint8_t* const start = GnmCbCur(cb);
    constexpr std::uint32_t WaveLimit = 0x17;
    constexpr std::uint32_t AllCus = 0x1ff;
    constexpr std::uint32_t CusButOne = 0x1fd;
    const auto rsrc3 = [](std::uint32_t cus) { return WaveLimit << GnmSpiShaderPgmRsrc3WaveLimitShift | cus << GnmSpiShaderPgmRsrc3CuEnShift; };
    const std::uint32_t sh[7][2] = {{GnmSpiShaderPgmRsrc3Ps, rsrc3(AllCus)},    {GnmSpiShaderPgmRsrc3Vs, rsrc3(CusButOne)},
                                    {GnmSpiShaderPgmRsrc3Gs, rsrc3(AllCus)},    {GnmSpiShaderPgmRsrc3Es, rsrc3(CusButOne)},
                                    {GnmSpiShaderPgmRsrc3Hs, WaveLimit},        {GnmSpiShaderPgmRsrc3Ls, rsrc3(CusButOne)},
                                    {GnmSpiShaderLateAllocVs, 0x1c}};
    for (const auto& [reg, value] : sh) GnmEmit(cb, {0xc0017600, reg - GnmShRegBase, value});
    const std::uint8_t flags = LoadPtr(LoadPtr(cb, 0x40), 0)[4];
    if ((flags & 1) != 0) GnmContextReg(cb, 0x1b5, (flags & 4) >> 2);
    if (neo) {
        GnmVgtControl(cb, 0x17, 0, 1);
        GnmContextReg(cb, 0x2d4, 0);
    } else {
        GnmVgtControl(cb, 0x17, 0, 0);
    }
    GnmContextReg(cb, 0x2ad, 0);
    GnmContextReg(cb, 0x2d5, 0);
    GnmContextReg(cb, 0x290, 0);
    GnmContextReg(cb, 0x2e5, 0);
    if (neo) {
        GnmUconfigReg(cb, 0x248, 0);
        GnmContextReg(cb, 0x2a1, 1);
        GnmContextReg(cb, 0x2a5, 0);
        GnmContextReg(cb, 0x20d, 0);
        GnmEmit(cb, {0xc0026900, 0xeb, 0xff00ff00, 0xff00});
    } else {
        GnmContextReg(cb, 0x2a1, 1);
        GnmContextReg(cb, 0x2a5, 0);
    }
    GnmContextReg(cb, 0x80, 0);
    GnmContextReg(cb, 0x206, 0x43f & 0xf3f);
    GnmContextReg(cb, 0x292, 0);
    GnmContextReg(cb, 0x205, GnmDepthControlValue() & 0x393fff);
    GnmContextReg(cb, 0x281, 0x20u << 16 | 1);
    const std::uint32_t scale = GnmFloatBits(1.0f);
    GnmEmit(cb, {0xc0046900, 0x2fa, scale, scale, scale, scale});
    cb[GnmCbFlags] &= 0xfe;
    GnmEmit(cb, {0xc0012000, 0, 0});
    GnmContextReg(cb, 0x204, 0);
    GnmContextReg(cb, 0x83, 0xffff);
    GnmContextReg(cb, 0x2f8, 0);
    GnmEmit(cb, {0xc0106900, 0x2fe, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    GnmEmit(cb, {0xc0026900, 0x2f5, 0, 0});
    GnmContextReg(cb, 0x30e, 0xffffffff);
    GnmContextReg(cb, 0x30f, 0xffffffff);
    GnmContextReg(cb, 0x293, 0x6020000);
    if (neo) {
        GnmUconfigReg(cb, 0x382, 0x40000040);
        GnmContextReg(cb, 0x8e, 0xf);
        GnmContextReg(cb, 0x6, 0);
    } else {
        GnmContextReg(cb, 0x8e, 0xf);
    }
    GnmContextReg(cb, 0x202, (1u << 4 & 0x70) | 0xccu << 16);
    GnmContextReg(cb, 0x2dc, 0xaa00);
    GnmContextReg(cb, 0x000, 0);
    GnmContextReg(cb, 0x201, 0);
    GnmContextReg(cb, 0x200, 0);
    return GnmEmitted(cb, start);
}

std::uint32_t GnmReserveBlend(std::uint8_t* command) {
    const bool neo = GnmIsNeo();
    std::uint8_t* const start = GnmReserveStart(command);
    std::uint32_t skip = Load<std::uint32_t>(command, 0);
    const std::uint8_t flags = GnmHeaderFlags(command);
    if ((flags & 1) != 0 && (((flags >> 1) ^ (flags >> 2)) & 1) != 0) skip += 3;
    skip += 8;
    if (neo) skip += 6;
    skip += 3 + 0xc + 0xc + 6;
    return GnmReserveCounted(command, start, skip);
}

std::uint32_t GnmBlendControlValue() {
    std::uint32_t value = 0;
    value = (value & 0xbfffffffu) | 1u << 30;
    value = (value & 0xffffe000u) | (((0u << 5) & 0xff) | (4u & 0x1f)) | (5u & 0x1f) << 8;
    value = (value & 0xdfffffffu) | 1u << 29;
    value = (value & 0xe000ffffu) | (1u & 0x1f) << 16 | (0u & 7) << 21 | (0u & 0x1f) << 24;
    return value;
}

std::uint32_t GnmEmitBlend(std::uint8_t* cb) {
    const bool neo = GnmIsNeo();
    std::uint8_t* const start = GnmCbCur(cb);
    const std::uint8_t flags = LoadPtr(LoadPtr(cb, 0x40), 0)[4];
    if ((flags & 1) != 0 && ((flags >> 2) & 1) != ((flags >> 1) & 1)) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    GnmEmit(cb, {0xc0002f00, 1});
    GnmEmit(cb, {0xc0016800, 0x8102, 0});
    GnmResetVgt(cb);
    if (neo) {
        GnmContextReg(cb, 0x2a6, 0);
        GnmUconfigReg(cb, 0x248, 0);
    }
    GnmContextReg(cb, 0x2a1, 0);
    GnmContextReg(cb, 0x205, 0x240 & 0x393fff);
    GnmContextReg(cb, 0x281, 0x10u << 16 | 4);
    GnmContextReg(cb, 0x280, 8u << 16 | 8);
    GnmContextReg(cb, 0x282, 0x10);
    GnmContextReg(cb, 0x204, 0);
    GnmContextReg(cb, 0x2f9, (5u & 7) << 3 | (2u & 3) << 1 | (1u & 1));
    const std::uint32_t scale = GnmFloatBits(1.0f);
    GnmEmit(cb, {0xc0046900, 0x105, scale, scale, scale, scale});
    GnmContextReg(cb, 0x1e0, GnmBlendControlValue() & 0xffff1fff);
    GnmContextReg(cb, 0x202, (1u << 4 & 0x70) | 0xccu << 16);
    return GnmEmitted(cb, start);
}
constexpr std::uint32_t GnmTextureInitFailed = 0x8eee00ff;
constexpr std::uint32_t GnmSurfaceFailed = 0x80d30000;

void GnmBufferDescriptor(std::uint8_t* out, const void* address, std::uint32_t bytes) {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    Store(out, 0, static_cast<std::uint32_t>(value));
    Store(out, 4, (static_cast<std::uint32_t>(value >> 32) & 0xfff) | 0x100000);
    Store(out, 0xc, 0x20077facu);
    Store(out, 8, (bytes + 0xf) >> 4);
}

void GnmTypedBufferDescriptor(std::uint8_t* out, const void* address, std::uint32_t format, std::uint32_t count) {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    const std::uint32_t surface = format & 0xff;
    Store(out, 0, static_cast<std::uint32_t>(value));
    const std::uint32_t stride = surface <= 0x3c ? (GnmElementBits[surface] << 13) & 0x3fff0000 : 0;
    Store(out, 4, (static_cast<std::uint32_t>(value >> 32) & 0xfff) | stride);
    std::uint32_t numeric = 0;
    if (surface - 1 <= 0xd && (format & 0x800) == 0) {
        const std::uint32_t shifted = (surface & 0xf) << 15;
        numeric = (format & 0xf00) == 0x700 || (format & 0xfe) != 6 ? shifted : 0;
    }
    Store(out, 0xc, ((format >> 12) & 0xfff) | ((format << 4) & 0x7000) | numeric | 0x20000000);
    Store(out, 8, count);
}

void GnmSetTextureFormat(std::uint8_t* texture, std::uint32_t format) {
    Store(texture, 4, (format & 0x3f) << 20 | ((format << 18) & 0x3c000000) | (Load<std::uint32_t>(texture, 4) & 0xc00fffff));
    Store(texture, 0xc, (Load<std::uint32_t>(texture, 0xc) & 0xfffff000) | GnmChannel(format, 0) | GnmChannel(format, 1) << 3 |
                            GnmChannel(format, 2) << 6 | GnmChannel(format, 3) << 9);
}

std::uint32_t GnmSelectTileMode(std::uint8_t* out, std::uint32_t tileMode, std::uint32_t arrayMode) {
    if (out == nullptr || tileMode > 0x1f) return GnmSurfaceFailed;
    if (GnmArrayMode(GnmTileModes[tileMode]) != arrayMode) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    Store(out, 0, tileMode);
    return 0;
}

std::uint32_t GnmComputeSurfaceInfo(std::uint8_t* out, const std::uint8_t* in) {
    if (out == nullptr || in == nullptr) return GnmSurfaceFailed;
    if ((in[0x24] & 0x20) == 0 && Load<std::uint32_t>(in, 0x10) != 1) return GnmSurfaceFailed;
    const std::uint32_t pitchIn = Load<std::uint32_t>(in, 0x18);
    if ((Load<std::uint32_t>(in, 0x1c) != 0 || pitchIn != 0) && pitchIn < Load<std::uint32_t>(in, 8)) return GnmSurfaceFailed;
    const std::uint32_t bits = Load<std::uint32_t>(in, 0x28);
    if (bits - 1 > 0x7f || std::popcount(bits) != 1) return GnmSurfaceFailed;
    std::uint8_t local[0x30];
    std::memcpy(local, in, sizeof(local));
    const std::uint32_t tileMode = Load<std::uint32_t>(local, 0);
    if (tileMode > 0x1f || Load<std::uint32_t>(local, 0x14) >= 2 || (local[0x24] & 2) != 0)
        NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    const std::uint32_t entry = GnmTileModes[tileMode];
    const std::uint32_t pipes = Load<std::uint32_t>(local, 4) == 1 ? entry >> 27 : (entry >> 6) & 0x1f;
    if ((pipes != 0xa && pipes != 0xc) || in[0x2c] != 0 || Load<std::uint32_t>(local, 0x1c) != 0 || (in[0x25] & 0x10) != 0)
        NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    Store(out, 0x2c, bits & 0xffff);
    const std::uint32_t width = Load<std::uint32_t>(local, 8) != 0 ? Load<std::uint32_t>(local, 8) : 1;
    const std::uint32_t height = Load<std::uint32_t>(local, 0xc) != 0 ? Load<std::uint32_t>(local, 0xc) : 1;
    const std::uint32_t arrayMode = GnmArrayMode(entry);
    if (arrayMode != 1 || Load<std::uint32_t>(local, 4) == 1 || Load<std::uint32_t>(local, 0x14) != 1 || (local[0x24] & 0x10) != 0)
        NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    GnmSelectTileMode(local, tileMode, arrayMode);
    const std::uint32_t bytes = (bits + 7) >> 3;
    const std::uint32_t elements = (0x40 / bytes) & 0xff;
    const std::uint32_t pitchAlign = elements >= 9 ? elements : 8;
    Store<std::uint32_t>(out, 0x1c, 0x100);
    Store(out, 0x20, pitchAlign);
    Store<std::uint32_t>(out, 0x24, 1);
    Store<std::uint32_t>(out, 0x28, 1);
    if (std::popcount(pitchAlign) != 1) return GnmSurfaceFailed + 1;
    const std::uint32_t depth = Load<std::uint32_t>(local, 0x10);
    const std::uint32_t pitch = AlignUp(pitchIn == 0 ? width : pitchIn, pitchAlign);
    const std::uint32_t slices = (0x100 / bytes) & 0xffff;
    const std::uint32_t sliceAlign = slices >= 0x41 ? slices : 0x40;
    const std::uint64_t area = static_cast<std::uint64_t>(pitch) * height;
    if (area % sliceAlign != 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::uint32_t heightAlign = 0;
    std::uint32_t multiple = pitch;
    std::uint32_t remainder = 0;
    do {
        remainder = multiple % sliceAlign;
        ++heightAlign;
        multiple += pitch;
    } while (remainder != 0);
    const std::uint64_t slice = (area * Load<std::uint32_t>(in, 0x28) + 7) >> 3;
    Store(out, 0, pitch);
    Store(out, 4, height);
    Store(out, 8, depth);
    Store<std::uint64_t>(out, 0x10, depth * slice);
    Store(out, 0x18, arrayMode);
    Store(out, 0x20, pitchAlign);
    Store(out, 0x24, heightAlign);
    if (GnmSelectTileMode(out + 0x34, Load<std::uint32_t>(in, 0), arrayMode) != 0) return GnmSurfaceFailed;
    if (Load<std::uint32_t>(in, 0x1c) != 0 || pitchIn == 0) return 0;
    return pitchIn == pitch ? 0 : GnmSurfaceFailed;
}

std::uint32_t GnmFormatScale(std::uint32_t surface) {
    if (((surface - 0x23) & 0xff) < 7) return 16;
    if (((surface - 0x3b) & 0xff) <= 1) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    return 1;
}

bool GnmIsBlockFormat(std::uint32_t surface) { return ((surface - 0x23) & 0xff) < 7 || ((surface + 0xc5) & 0xff) < 2; }

std::uint32_t GnmTextureSize(std::uint64_t* size, std::uint32_t* align, const std::uint8_t* texture) {
    if (size == nullptr || align == nullptr || texture == nullptr) return GnmSurfaceFailed;
    const std::uint32_t word3 = Load<std::uint32_t>(texture, 0xc);
    const std::uint32_t word4 = Load<std::uint32_t>(texture, 0x10);
    const std::uint32_t type = word3 >> 28;
    std::uint32_t slices = (word4 & 0x1fff) + 1;
    if ((word3 & 0xf0000000) == 0xa0000000 || type == 0xa) slices = 1;
    const std::uint32_t faces = type == 0xb ? slices * 6 : slices;
    if ((word3 & 0x2000000) != 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    if (type <= 0xd && ((0x2a00u >> type) & 1) != 0 && (word3 & 0x1f00000) == 0xd00000)
        NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    *size = 0;
    *align = 1;
    const std::uint32_t tileMode = (word3 >> 20) & 0x1f;
    if (type == 0xa || GnmMicroMode(GnmTileModes[tileMode]) == 2) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    const std::uint32_t flags = (word3 >> 13 & 0x1000) | (type == 0xb ? 0x10u : 0u);
    const std::uint32_t word6 = Load<std::uint32_t>(texture, 0x18);
    if ((word6 & 0x1000000) != 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::uint8_t in[0x30] = {};
    Store(in, 0, tileMode);
    Store(in, 4, (word6 >> 24) & 1);
    const std::uint32_t surface = (Load<std::uint32_t>(texture, 4) >> 20) & 0x3f;
    std::uint32_t bits = 0;
    if (surface <= 0x3c) bits = GnmFormatScale(surface) * GnmElementBits[surface];
    Store(in, 0x28, bits / GnmFormatScale(surface));
    in[0x2c] = GnmIsBlockFormat(surface) ? 1 : 0;
    in[0x2d] = 0;
    const std::uint32_t word2 = Load<std::uint32_t>(texture, 8);
    Store(in, 8, (word2 & 0x3fff) + 1);
    Store(in, 0xc, ((word2 >> 14) & 0x3fff) + 1);
    Store(in, 0x10, (flags & 0x20) != 0 ? (word4 & 0x1fff) + 1 : 1u);
    Store(in, 0x14, word3 >= 0xe0000000 ? 1u << ((word3 >> 16) & 0xf) : 1u);
    Store(in, 0x18, ((word4 >> 13) & 0x3fff) + 1);
    Store<std::uint32_t>(in, 0x1c, 0);
    Store(in, 0x24, flags);
    Store<std::uint32_t>(in, 0x20, 0);
    std::uint8_t out[0x38] = {};
    if (GnmComputeSurfaceInfo(out, in) == 0 && GnmArrayMode(GnmTileModes[Load<std::uint32_t>(in, 0)]) != Load<std::uint32_t>(out, 0x18))
        NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    const std::uint32_t levels = word3 >= 0xe0000000 ? 0 : (word3 >> 16) & 0xf;
    std::uint8_t level[0x30];
    std::memcpy(level, in, sizeof(level));
    for (std::uint32_t mip = 0;; ++mip) {
        const std::uint32_t w = Load<std::uint32_t>(in, 8) >> mip;
        const std::uint32_t h = Load<std::uint32_t>(in, 0xc) >> mip;
        const std::uint32_t d = Load<std::uint32_t>(in, 0x10) >> mip;
        Store(level, 8, w != 0 ? w : 1u);
        Store(level, 0xc, h != 0 ? h : 1u);
        Store(level, 0x10, d != 0 ? d : 1u);
        Store(level, 0x18, ((word4 >> 13) & 0x3fff) + 1);
        Store(level, 0x1c, mip);
        if (Load<std::uint32_t>(level, 0x10) != 1 || mip != 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
        std::memset(out, 0, sizeof(out));
        if (const std::uint32_t error = GnmComputeSurfaceInfo(out, level); error != 0) return error;
        *size += Load<std::uint64_t>(out, 0x10) * faces;
        *align = std::max(*align, Load<std::uint32_t>(out, 0x1c));
        if (((Load<std::uint32_t>(level, 8) ^ 1) | (Load<std::uint32_t>(level, 0xc) ^ 1) | (Load<std::uint32_t>(level, 0x10) ^ 1)) == 0) return 0;
        if (levels == mip) return 0;
    }
}

std::uint32_t GnmInitTextureSpec(std::uint8_t* texture, const std::uint8_t* spec) {
    const std::uint32_t mips = Load<std::uint32_t>(spec, 0x14);
    if (mips == 0) return GnmTextureInitFailed;
    const std::uint32_t largest = std::max({Load<std::uint32_t>(spec, 4), Load<std::uint32_t>(spec, 8), Load<std::uint32_t>(spec, 0xc)}) | 1;
    if (mips > static_cast<std::uint32_t>(std::bit_width(largest))) return GnmTextureInitFailed;
    const std::uint32_t format = Load<std::uint32_t>(spec, 0x1c);
    if ((format & 0xfe) == 6) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    const std::uint32_t type = Load<std::uint32_t>(spec, 0);
    std::memset(texture, 0, 0x20);
    const std::uint32_t multiMip = mips >= 2 ? 1 : 0;
    const std::uint32_t word3 = (multiMip << 25) + 0x4000000;
    Store(texture, 0xc, word3);
    if (type - 8 > 7) return GnmTextureInitFailed;
    if (type != 9) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    if (Load<std::uint32_t>(spec, 0xc) != 1 || Load<std::uint32_t>(spec, 0x28) != 0) return GnmTextureInitFailed;
    Store(texture, 0xc, (Load<std::uint32_t>(spec, 0x18) != 1 ? 1u << 30 : 0u) + word3 - 0x70000000);
    std::uint8_t in[0x30] = {};
    const std::uint32_t gpuMode = Load<std::uint32_t>(spec, 0x24);
    Store(in, 4, gpuMode);
    Store(in, 8, Load<std::uint32_t>(spec, 4));
    Store(in, 0xc, Load<std::uint32_t>(spec, 8));
    Store(in, 0x10, Load<std::uint32_t>(spec, 0xc));
    const std::uint32_t surface = spec[0x1c];
    if (surface >= 0x3d) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    const std::uint32_t flags = multiMip << 12 | (type == 0xb ? 1u : 0u) << 4 | (type == 0xa ? 1u : 0u) << 5 | (gpuMode == 1 ? 1u : 0u) << 13;
    Store(in, 0x28, GnmFormatScale(surface) * GnmElementBits[surface] / GnmFormatScale(surface));
    in[0x2c] = GnmIsBlockFormat(surface) ? 1 : 0;
    Store<std::uint32_t>(in, 0x14, 1);
    Store(in, 0x24, flags);
    Store(in, 0, Load<std::uint32_t>(spec, 0x20));
    Store(in, 0x18, Load<std::uint32_t>(spec, 0x10));
    std::uint8_t out[0x38] = {};
    if (GnmComputeSurfaceInfo(out, in) != 0) return GnmTextureInitFailed;
    std::uint32_t word2 = (Load<std::uint32_t>(texture, 8) & 0xffffc000) | ((Load<std::uint32_t>(spec, 4) - 1) & 0x3fff);
    word2 = (word2 & 0xf0003fff) | (((Load<std::uint32_t>(spec, 8) << 14) + 0xfffc000) & 0xfffc000);
    Store(texture, 8, word2);
    Store(texture, 0x10, (Load<std::uint32_t>(texture, 0x10) & 0xf8001fff) | (((Load<std::uint32_t>(out, 0) << 13) + 0x7ffe000) & 0x7ffe000));
    GnmSetTextureFormat(texture, format);
    const std::uint32_t depth = type == 0xa ? Load<std::uint32_t>(spec, 0xc) : Load<std::uint32_t>(spec, 0x18);
    Store(texture, 0x10, (Load<std::uint32_t>(texture, 0x10) & 0xffffe000) | ((depth - 1) & 0x1fff));
    const std::uint32_t arrays = type == 0xb ? Load<std::uint32_t>(spec, 0x18) * 6 : Load<std::uint32_t>(spec, 0x18);
    Store(texture, 0x14, (Load<std::uint32_t>(texture, 0x14) & 0xfc000000) | (((arrays << 13) + 0x3ffe000) & 0x3ffe000));
    std::uint32_t current = Load<std::uint32_t>(texture, 0xc);
    if (current <= 0xdfffffff) current = (current & 0xfff00fff) | (((mips << 16) + 0xf0000) & 0xf0000);
    Store(texture, 4, Load<std::uint32_t>(texture, 4) & 0xfff000ff);
    if (static_cast<std::int32_t>(Load<std::uint32_t>(spec, 0x28)) > 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    Store(texture, 0xc, ((Load<std::uint32_t>(out, 0x34) & 0x1f) << 20) | (current & 0xfe0fffff));
    Store(texture, 0x18, (Load<std::uint32_t>(texture, 0x18) & 0xfeffffff) | (gpuMode == 1 ? 1u : 0u) << 24);
    texture[0xb] |= 0x70;
    std::uint64_t size = 0;
    std::uint32_t align = 0;
    return GnmTextureSize(&size, &align, texture) == 0 ? 0 : GnmTextureInitFailed;
}

std::uint64_t GnmTextureSizeAlign(const std::uint8_t* texture) {
    std::uint64_t size = 0;
    std::uint32_t align = 0;
    if (GnmTextureSize(&size, &align, texture) != 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    return static_cast<std::uint64_t>(align) << 32 | (size >> 32 == 0 ? size : 0);
}
constexpr std::size_t GnmStagePs = 0x6f0;
constexpr std::size_t GnmStateConstants = 0xcc8;

void GnmBindVsStage(std::uint8_t* state, const std::uint8_t* object, std::size_t maskOffset) {
    const std::uint8_t* const header = LoadPtr(object, 0x20);
    StorePtr(state, 0x6e8, header);
    std::memcpy(state, object + maskOffset, 0x10);
    Store(state, 0x530, Load<std::uint64_t>(object, 0x38));
    Store<std::uint64_t>(state, 0x540, Load<std::uint32_t>(object, 0x2c));
    Store<std::uint64_t>(state, 0x538, 0);
    Store<std::uint64_t>(state, 0x558, 0);
    Store<std::uint64_t>(state, 0x548, 0);
    Store<std::uint32_t>(state, 0x550, 0);
    Store(state, 0x560, Load<std::uint64_t>(object, 0x30));
    Store(state, 0x568, Load<std::uint32_t>(object, 0x28));
    Store<std::uint32_t>(state, 0x56c, 0);
    std::memset(state + 0x5b0, 0, 0x28);
    std::uint32_t inputs = 0;
    if (header != nullptr && header[0x25] != 0) {
        inputs = header[0x25];
        StorePtr(state, 0x6d8, header + (header[0x24] + header[3]) * 4u + 0x28);
    } else {
        Store<std::uint64_t>(state, 0x6d8, 0);
    }
    Store(state, 0x6e0, inputs);
    const std::uint32_t count = Load<std::uint32_t>(object, 0x4c);
    Store(state, 0x5b0, Load<std::uint64_t>(object, 0x50));
    Store(state, 0x5b8, Load<std::uint64_t>(object, 0x40));
    Store(state, 0x5c0, Load<std::uint64_t>(object, 0x58));
    StorePtr(state, 0x5c8, count < 0x11 ? state + 0x5d8 : nullptr);
    Store(state, 0x5d0, static_cast<std::uint16_t>(count));
    Store<std::uint16_t>(state, 0x5d2, 0);
    Store(state, 0x5d4, static_cast<std::uint16_t>(count == 0 ? 0 : ~(0xffffffffu << (count & 0x1f))));
    Store<std::uint16_t>(state, 0x5d6, 0);
}

void GnmBindPsResource(std::uint8_t* stage, const std::uint8_t* object, std::size_t slotOffset, std::size_t usageOffset,
                       std::size_t valueOffset, std::uint32_t mask, std::uint32_t& used) {
    const std::uint32_t slot = Load<std::uint32_t>(object, slotOffset);
    if (slot > 0x13 || ((mask >> slot) & 1) == 0) return;
    const std::uint8_t* const usage = LoadPtr(object, usageOffset);
    StorePtr(stage, slot * 40 + 0x10, usage);
    Store(stage, slot * 40 + 0x20, Load<std::uint32_t>(object, valueOffset));
    if (usage != nullptr && usage[2] >= 0x10) {
        used |= 1u << slot;
        Store(stage, 0x548, used);
    }
}

void GnmBindPsBuffer(std::uint8_t* stage, const std::uint8_t* object, std::size_t slotOffset, std::size_t usageOffset, const void* buffer,
                     bool inverted) {
    const std::uint32_t slot = Load<std::uint32_t>(object, slotOffset);
    if (slot > 0xf) return;
    const std::uint32_t bit = 1u << slot;
    const std::uint32_t enabled = Load<std::uint32_t>(stage, 8) & bit;
    const std::uint32_t chosen = inverted ? enabled & ~Load<std::uint32_t>(stage, 0xc) : enabled & Load<std::uint32_t>(stage, 0xc);
    if (chosen == 0) return;
    const std::uint8_t* const usage = LoadPtr(object, usageOffset);
    StorePtr(stage, slot * 16 + 0x330, usage);
    StorePtr(stage, slot * 16 + 0x338, buffer);
    if (usage != nullptr && usage[2] >= 0x10) Store(stage, 0x550, Load<std::uint32_t>(stage, 0x550) | bit);
}

void GnmBindPsStage(std::uint8_t* state, std::uint8_t* header) {
    const std::uint8_t* const object = GnmShaderObject(8);
    std::uint8_t* const stage = state + GnmStagePs;
    StorePtr(state, 0xd60, object);
    Store(stage, 0x5b0, Load<std::uint64_t>(object, 0x20));
    std::memcpy(stage, object + 0xa0, 0x10);
    Store(stage, 0x530, Load<std::uint64_t>(object, 0x38));
    Store<std::uint64_t>(stage, 0x540, Load<std::uint32_t>(object, 0x2c));
    Store<std::uint64_t>(stage, 0x538, 0);
    Store<std::uint64_t>(stage, 0x558, 0);
    Store<std::uint64_t>(stage, 0x548, 0);
    Store<std::uint32_t>(stage, 0x550, 0);
    Store(stage, 0x560, Load<std::uint64_t>(object, 0x30));
    Store(stage, 0x568, Load<std::uint32_t>(object, 0x28));
    Store<std::uint32_t>(stage, 0x56c, 0);
    const std::uint32_t mask = Load<std::uint32_t>(stage, 0);
    std::uint32_t used = 0;
    GnmBindPsResource(stage, object, 0x48, 0x50, 0x4c, mask, used);
    GnmBindPsResource(stage, object, 0x60, 0x68, 0x64, mask, used);
    if ((mask & 1) != 0) {
        const std::uint32_t high = Load<std::uint32_t>(header, 0x1ac);
        const std::uint64_t base = Load<std::uint32_t>(header, 0x1a8) | static_cast<std::uint64_t>(high & 0xfff) << 32;
        const std::uint32_t stride = (high >> 16) & 0x3fff;
        GnmBufferDescriptor(stage + 0x24, reinterpret_cast<const void*>(base), (stride != 0 ? stride : 1) * Load<std::uint32_t>(header, 0x1b0));
    }
    if (Load<std::uint32_t>(state, 0xd68) == 0x30) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    if ((stage[0] & 2) != 0) {
        const std::uint8_t* const buffer = LoadPtr(header, 0x288);
        if ((reinterpret_cast<std::uintptr_t>(buffer) & 3) == 0) GnmBufferDescriptor(stage + 0x4c, buffer, 0x40);
    }
    GnmBindPsBuffer(stage, object, 0x78, 0x80, header + 0x1b8, false);
    GnmBindPsBuffer(stage, object, 0x90, 0x98, header + 0x2a0, true);
}

template <typename Emit>
void GnmUserDataRuns(std::uint8_t* stage, Emit emit) {
    std::uint32_t pending = Load<std::uint32_t>(stage, 0x56c);
    if (pending == 0) return;
    const std::uint32_t count = Load<std::uint32_t>(stage, 0x568);
    std::uint32_t run = 0;
    std::uint32_t first = 0;
    for (std::uint32_t bit = 0; count != 0 && bit < count; ++bit, pending >>= 1) {
        std::uint32_t next = 0;
        if ((pending & 1) != 0) {
            if (run == 0) first = bit;
            next = ++run;
            if (pending != 1) continue;
        }
        if (run != 0) emit(first, run);
        if (pending < 2) break;
        run = next;
    }
    Store<std::uint32_t>(stage, 0x56c, 0);
}

void GnmEmitUserData(std::uint8_t* cb, std::uint8_t* stage, std::uint32_t shaderStage) {
    GnmUserDataRuns(stage, [&](std::uint32_t first, std::uint32_t run) {
        GnmShRegs(cb, shaderStage, LoadPtr(stage, 0x560)[2] + first, reinterpret_cast<const std::uint32_t*>(stage + 0x570 + first * 4), run);
    });
}

void GnmCountUserData(std::uint8_t* command, std::uint8_t* stage) {
    if (LoadPtr(stage, 0x560) == nullptr) return;
    GnmUserDataRuns(stage, [&](std::uint32_t, std::uint32_t run) { Store(command, 0, Load<std::uint32_t>(command, 0) + run + 4); });
}

void GnmOpenChain(std::uint8_t* cb) {
    std::uint8_t* const aux = LoadPtr(cb, 0x40);
    if (aux == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    const std::uint32_t dwords = Load<std::uint32_t>(LoadPtr(aux, 0), 0x2c);
    std::uint8_t* const start = GnmCbCur(cb);
    GnmNop(cb, dwords);
    std::uint8_t* const end = GnmCbCur(cb);
    if (end < start || end > LoadPtr(cb, GnmCbEnd)) return;
    GnmLinkPending(aux, LoadPtr(cb, GnmCbBegin), end);
    StorePtr(aux, 0x70, end);
    GnmCbInit(aux + 0x30, start, dwords * 4);
}

void GnmUploadPsConstants(std::uint8_t* cb) {
    std::uint8_t* const aux = LoadPtr(cb, 0x40);
    if (aux == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::uint8_t* const state = LoadPtr(aux, 8);
    std::uint8_t* const constants = LoadPtr(state, GnmStateConstants);
    std::uint8_t* const stage = state != nullptr ? state + GnmStagePs : state;
    const std::uint8_t* const source = LoadPtr(state, 0xe00);
    if (LoadPtr(state, 0xd78) != nullptr || source == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::memcpy(constants, source, 0x10);
    if (LoadPtr(stage, 0x560) != nullptr) {
        if (Load<std::uint32_t>(stage, 0x568) < 4) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
        Store(stage, 0x578, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(constants)));
        Store(stage, 0x57c, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(constants) >> 32));
        Store(stage, 0x56c, Load<std::uint32_t>(stage, 0x56c) | 0xc);
        GnmEmitUserData(cb, stage, 1);
    }
    StorePtr(state, GnmStateConstants, constants + 0x10);
}

struct GnmChain {
    std::uint8_t command[0x50];
    std::uint8_t aux[0xa0];
};

void GnmCloseChain(std::uint8_t* command, std::uint8_t* cb, std::uint8_t* before) {
    std::uint8_t* const header = LoadPtr(LoadPtr(command, 0x48), 0);
    GnmIndirectBuffer(cb, LoadPtr(header, 0x20), Load<std::uint32_t>(header, 0x28));
    std::uint8_t* const cursor = LoadPtr(command, 0x18);
    if (cursor < before || cursor > LoadPtr(command, 0x10)) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::uint8_t* const aux = LoadPtr(command, 0x48);
    std::uint8_t* const patch = LoadPtr(aux, 0x70);
    if (patch == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    GnmIndirectBuffer(aux + 0x30, patch, static_cast<std::uint32_t>((cursor - patch) >> 2));
    Store<std::uint64_t>(aux, 0x70, 0);
    GnmCbInit(aux + 0x30, nullptr, 0);
}

void GnmCommitChain(std::uint8_t* outer, GnmChain& chain) {
    std::uint8_t* const end = GnmSyncReserved(chain.command);
    std::uint8_t* const cursor = LoadPtr(outer, 0x18);
    if (end < cursor || LoadPtr(outer, 0x10) < end) return;
    if (end != cursor) StorePtr(outer, 0x18, cursor + ((end - cursor) >> 2) * 4);
    StorePtr(LoadPtr(outer, 0x48), 0x70, end);
}

std::uint8_t* GnmStartChain(std::uint8_t* outer, std::uint8_t* state, GnmChain& chain) {
    if (Load<std::uint32_t>(outer, 0) * 4u + LoadPtr(outer, 8) < LoadPtr(outer, 0x18)) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    GnmOpenChain(outer + 8);
    std::uint8_t* const cursor = LoadPtr(outer, 0x18);
    const std::uint32_t room = Load<std::uint32_t>(outer, 0x10) - static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(cursor));
    std::memset(&chain, 0, sizeof(chain));
    StorePtr(state, 0xcb0, chain.command);
    StorePtr(chain.aux, 0, LoadPtr(outer, 0x50));
    StorePtr(chain.aux, 0x18, cursor);
    GnmCbInit(chain.aux + 0x30, nullptr, 0);
    StorePtr(chain.aux, 8, state);
    Store<std::uint64_t>(chain.command, 0, 0);
    GnmCbInit(chain.command + 8, cursor, room);
    StorePtr(chain.command, 0x48, chain.aux);
    std::uint8_t* const start = GnmReservedAt(chain.command);
    if (LoadPtr(chain.command, 0x18) > start) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    GnmOpenChain(chain.command + 8);
    if (LoadPtr(chain.command, 0x10) >= LoadPtr(chain.command, 0x18)) StorePtr(chain.aux, 0x70, LoadPtr(chain.command, 0x18));
    return start;
}

void GnmFinishRecord(std::uint8_t* record, std::uint8_t* constants, std::uint8_t* start, std::uint32_t used, std::uint32_t dwords,
                     std::uint8_t* data, std::size_t dataOffset) {
    if (Load<std::uint64_t>(record, 0x30) != 0) return;
    if (data != nullptr) StorePtr(record, dataOffset, data);
    StorePtr(record, 0x28, constants);
    StorePtr(record, 0x30, start);
    Store(record, 0x20, static_cast<std::uint16_t>(used << 2));
    Store(record, 0x22, static_cast<std::uint16_t>(dwords << 2));
}

void GnmRecordVsDraw(std::uint8_t* outer, std::uint8_t* state, std::uint8_t* record) {
    std::uint8_t* const data = LoadPtr(record, 8);
    if (data == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    std::uint8_t* const constants = LoadPtr(state, GnmStateConstants);
    GnmChain chain;
    std::uint8_t* const start = GnmStartChain(outer, state, chain);
    StorePtr(state, 0xd90, data);
    if (LoadPtr(state, 0x560) != nullptr) {
        if ((reinterpret_cast<std::uintptr_t>(state + 0xd90) & 3) == 0 && Load<std::uint32_t>(state, 0x568) >= 6) {
            StorePtr(state, 0x580, data);
            state[0x56c] |= 0x30;
        }
        GnmEmitUserData(chain.command + 8, state, 2);
    }
    std::uint8_t* const before = GnmSyncReserved(chain.command);
    const auto dwords = static_cast<std::uint32_t>((before - start) >> 2);
    GnmCloseChain(chain.command, chain.command + 8, before);
    GnmCommitChain(outer, chain);
    const std::uint32_t used = (Load<std::uint32_t>(state, GnmStateConstants) - static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(constants))) >> 2;
    GnmFinishRecord(record, constants, start, used, dwords, data, 0x10);
    GnmSyncReserved(outer);
}

void GnmRecordPsDraw(std::uint8_t* outer, std::uint8_t* state, std::uint8_t* record) {
    std::uint8_t* const constants = LoadPtr(state, GnmStateConstants);
    GnmChain chain;
    std::uint8_t* const start = GnmStartChain(outer, state, chain);
    GnmUploadPsConstants(chain.command + 8);
    std::uint8_t* const before = GnmSyncReserved(chain.command);
    const auto dwords = static_cast<std::uint32_t>((before - start) >> 2);
    GnmCloseChain(chain.command, chain.command + 8, before);
    GnmCommitChain(outer, chain);
    const std::uint32_t used = (Load<std::uint32_t>(state, GnmStateConstants) - static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(constants))) >> 2;
    if (record == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_47000001");
    if (Load<std::uint64_t>(record, 0x30) == 0) {
        StorePtr(record, 8, start);
        StorePtr(record, 0x28, constants);
        StorePtr(record, 0x30, start);
        Store(record, 0x20, static_cast<std::uint16_t>(used << 2));
        Store(record, 0x22, static_cast<std::uint16_t>(dwords << 2));
    }
    GnmSyncReserved(outer);
}

std::uint32_t GnmPrimitiveControl(std::uint32_t culling, std::uint32_t mode, std::uint32_t enable) {
    std::uint32_t value = 0;
    value = (value & 0xbfffffffu) | 1u << 30;
    value = (value & 0xdfffffffu) | 1u << 29;
    value = (value & 0xffffe000u) | (((0u << 5) & 0xff) | 0u) | (1u & 0x1f) << 8;
    return (value & 0xe000ffffu) | (culling & 0x1f) << 16 | (mode & 7) << 21 | (enable & 0x1f) << 24;
}

std::uint32_t GnmReserveDraws(std::uint8_t* command) {
    std::uint8_t* const start = GnmReserveStart(command);
    return GnmReserveCounted(command, start, Load<std::uint32_t>(command, 0) + 3 + 0x15 + 0x69 + 0x6a);
}

void GnmPrimitiveType(std::uint8_t* cb, std::uint32_t type) {
    GnmEmit(cb, {0xc0017900, (GnmIsNeo() ? 1u : 0u) << 28 | 0x242, type & 0x3f});
}

std::uint32_t GnmEmitDraws(std::uint8_t* cb) {
    std::uint8_t* const start = GnmCbCur(cb);
    std::uint8_t* const aux = LoadPtr(cb, 0x40);
    std::uint8_t* const header = LoadPtr(aux, 0);
    const std::uint8_t layout = LoadPtr(LoadPtr(aux, 8), 0x6e8)[0x27];
    const std::uint32_t vertex = layout & 0xf;
    const std::uint32_t instance = layout >> 4;
    GnmPrimitiveType(cb, 5);
    GnmContextReg(cb, 0x1e0, GnmPrimitiveControl(1, 1, 1) & 0xffff1fff);
    GnmDrawIndirectAt(cb, 0, 2, vertex, instance);
    GnmPrimitiveType(cb, 3);
    const std::uint32_t first = GnmFloatBits(1.0f / 8);
    GnmEmit(cb, {0xc0046900, 0x105, first, first, first, first});
    GnmContextReg(cb, 0x1e0, GnmPrimitiveControl(0x13, 4, 1) & 0xffff1fff);
    for (std::uint32_t i = 1; i <= 8; ++i) {
        GnmContextReg(cb, 0x282, i);
        GnmDrawIndirectAt(cb, 0x10, 2, vertex, instance);
    }
    const std::uint32_t second = GnmFloatBits(1.0f / 16);
    GnmEmit(cb, {0xc0046900, 0x105, second, second, second, second});
    GnmContextReg(cb, 0x1e0, GnmPrimitiveControl(0x13, 0, 1) & 0xffff1fff);
    for (std::uint32_t i = 1; i <= 8; ++i) {
        GnmContextReg(cb, 0x282, i);
        GnmDrawIndirectAt(cb, 0x10, 2, vertex, instance);
    }
    GnmPrimitiveType(cb, 6);
    const auto base = reinterpret_cast<std::uintptr_t>(LoadPtr(header, 0x158));
    GnmEmit(cb, {0xc0021100, 1, static_cast<std::uint32_t>(base) & ~7u, static_cast<std::uint32_t>(base >> 32) & 0xff});
    return GnmEmitted(cb, start);
}
struct GnmInitFrame {
    std::uint8_t command[0x50];
    std::uint8_t aux[0xa0];
    std::uint8_t state[0xe90];
};

static_assert(sizeof(GnmInitFrame) == 0xf80);

void GnmInitState(std::uint8_t* state, std::uint8_t* command, std::uint8_t* constants, std::uint8_t* header) {
    StorePtr(state, 0xca8, state + 0x6d8);
    Store<std::uint64_t>(state, 0xcd8, 0x4487000044f00000);
    GnmStoreOnes(state + 0xe18, 8);
    Store<std::uint16_t>(state, 0xe38, 8);
    Store<std::uint64_t>(state, 0xe40, 0x3f8000003f800000);
    GnmStoreOnes(state + 0xe50, 4);
    Store<std::uint64_t>(state, 0xe78, 0x3f8000003f800000);
    Store<std::uint32_t>(state, 0xe80, 0x3f800000);
    StorePtr(state, 0xcb0, command);
    StorePtr(state, 0xcb8, constants);
    StorePtr(state, 0xcc0, constants + 0x100);
    StorePtr(state, GnmStateConstants, constants);
    StorePtr(state, 0xe88, header + 0x2c8);
}

struct GnmConstantRow {
    float flag;
    float value;
    float kind;
    float weight;
};

constexpr std::size_t GnmConstantRowCount = 0x42;

constexpr std::array<GnmConstantRow, GnmConstantRowCount> GnmConstantRows() {
    std::array<GnmConstantRow, GnmConstantRowCount> rows{};
    std::size_t at = 0;
    const auto add = [&](float flag, float value, float kind, float weight = 1.0f) { rows[at++] = {flag, value, kind, weight}; };
    add(0, 1, 12, 0);
    add(10, 1, 12);
    add(0, 1, 12);
    for (int n = 1; n <= 16; ++n) add(0, static_cast<float>(n), 252);
    add(1, 3, 252);
    add(1, 2, 252);
    add(1, 1, 12);
    add(0, 1.0f / 8, 188);
    add(0, 7.0f / 64, 188);
    add(0, 1.0f / 64, 188);
    add(0, 1.0f / 8, 186);
    for (int n = 1; n <= 16; ++n) add(0, 1.0f / static_cast<float>(n), 27);
    add(0, 1.0f / 4, 188);
    add(0, 1, 202);
    for (int n = 2; n <= 4; ++n) add(0, 1.0f / static_cast<float>(n), 10);
    for (const float value : {1.0f / 16, 1.0f / 8, 1.0f / 4, 1.0f / 2, 3.0f / 4}) add(0, value, 30);
    add(0, 0.33f, 12);
    for (int n = 1; n <= 8; ++n) add(0, static_cast<float>(n) / 8, 188);
    add(0, 3.0f / 4, 190);
    add(1, 7.0f / 16, 29);
    add(1, 1.0f / 4, 541);
    add(1, 1.0f / 2, 524);
    add(1, 3.0f / 4, 524);
    return rows;
}

constexpr std::array<GnmConstantRow, GnmConstantRowCount> GnmConstantTable = GnmConstantRows();
static_assert(sizeof(GnmConstantTable) == 0x420);

void GnmFillConstantBlock(std::uint8_t* header, std::uint8_t* block) {
    StorePtr(header, 0x220, block);
    StorePtr(header, 0x228, block + 0x10);
    StorePtr(header, 0x230, block + 0x20);
    std::memcpy(block, GnmConstantTable.data(), sizeof(GnmConstantTable));
    StorePtr(header, 0x268, block + 0x130);
    StorePtr(header, 0x260, block + 0x160);
    StorePtr(header, 0x238, block + 0x190);
    StorePtr(header, 0x240, block + 0x2a0);
    StorePtr(header, 0x258, block + 0x2f0);
    StorePtr(header, 0x248, block + 0x340);
    StorePtr(header, 0x250, block + 0x3d0);
    StorePtr(header, 0x270, block + 0x3e0);
    StorePtr(header, 0x278, block + 0x400);
    std::uint8_t* const tail = block + (block - LoadPtr(header, 0x230)) + 0x20;
    StorePtr(header, 0x280, tail + 0x420);
    GnmStoreOnes(tail + 0x420, 8);
    GnmStoreFloats(tail + 0x440, {1, 1, 1, 1, 0.5f, 0.5f, 0.5f, 0});
    StorePtr(header, 0x288, tail + 0x460);
    GnmStoreFloats(tail + 0x460, {1.0f / 16, 1.0f / 16, 1.0f / 16, 1.0f / 16, 1, 1, 1, 1});
    GnmStoreFloats(tail + 0x480, {1, 1, 1, 1, 0.5f, 0.5f, 0.5f, 0});
    Store<std::uint64_t>(tail, 0x4a0, 0x100000001);
    std::memset(tail + 0x4a8, 0, 0x38);
    GnmBufferDescriptor(header + 0x178, tail + 0x4a0, 0x40);
    GnmBufferDescriptor(header + 0x1a8, tail + 0x4a0, 0x40);
    GnmBufferDescriptor(header + 0x1d8, tail + 0x4a0, 0x40);
    StorePtr(header, 0x208, tail + 0x4e0);
    Store<std::uint32_t>(tail, 0x4e0, 1);
    StorePtr(header, 0x210, tail + 0x4e4);
    Store<std::uint32_t>(tail, 0x4e8, 0xff000000);
    GnmTypedBufferDescriptor(header + 0x188, tail + 0x4e8, 0x4404, 1);
    Store<std::uint32_t>(tail, 0x4ec, 0xffffffff);
    GnmTypedBufferDescriptor(header + 0x1b8, tail + 0x4ec, 0x4404, 1);
    GnmTypedBufferDescriptor(header + 0x1e8, LoadPtr(header, 0x208), 0x4404, 1);
    Store<std::uint64_t>(header, 0x198, 0);
    Store<std::uint64_t>(header, 0x1c8, 0);
    Store<std::uint64_t>(header, 0x1f8, 0);
    StorePtr(header, 0x368, tail + 0x4f0);
    StorePtr(header, 0x370, tail + 0x500);
    Store<std::uint64_t>(tail, 0x508, 0x13f800000);
    StorePtr(header, 0x378, tail + 0x510);
    Store<std::uint32_t>(tail, 0x510, 0x3f800000);
    Store<std::uint32_t>(tail, 0x51c, 2);
    StorePtr(header, 0x380, tail + 0x520);
    Store<std::uint32_t>(tail, 0x524, 0x3f800000);
    Store<std::uint32_t>(tail, 0x52c, 4);
    StorePtr(header, 0x388, tail + 0x530);
    Store<std::uint64_t>(tail, 0x538, 0x33f800000);
    Store<std::uint32_t>(tail, 0x530, 0x3f800000);
    std::memset(tail + 0x540, 0, 0x10);
    std::memset(tail + 0x54c, 0, 0x10);
    std::memset(tail + 0x560, 0, 0x10);
    StorePtr(header, 0x218, tail + 0x540);
    std::memset(tail + 0x570, 0, 0x50);
    StorePtr(header, 0x358, tail + 0x570);
}

constexpr float GnmScreenWidth = 1920.0f;
constexpr float GnmScreenHeight = 1080.0f;

std::uint8_t* GnmFillVertexData(std::uint8_t* header, std::uint8_t* at) {
    at = AlignPtr(at + 0xb0, 8);
    Store<std::uint32_t>(at, 0, 4);
    StorePtr(header, 0x158, at);
    StorePtr(header, 0x160, at + 0x10);
    GnmStoreWords(at + 4, {1, 0, 0, 4, 4, 0, 0x10, 4});
    StorePtr(header, 0x168, at + 0x20);
    GnmStoreWords(at + 0x24, {1, 0, 1, 4});
    Store<std::uint64_t>(at, 0x34, 1);
    Store<std::uint32_t>(at, 0x3c, 0xff);
    StorePtr(header, 0x170, at + 0x30);
    GnmStoreFloats(at + 0x40, {2.0f / GnmScreenWidth, 0, 0, 0, 0, -2.0f / GnmScreenHeight, 0, 0});
    GnmStoreFloats(at + 0x60, {0, 0, 1, 0, -1, 1, 0, 1});
    StorePtr(header, 0x328, at + 0x40);
    GnmStoreFloats(at + 0x80, {0, 0, 1, -1});
    std::memset(at + 0x90, 0, 0x10);
    StorePtr(header, 0x330, at + 0x80);
    StorePtr(header, 0x3b8, at + 0xa0);
    const auto identity = [&](std::size_t offset) {
        for (std::size_t row = 0; row < 3; ++row) {
            Store<std::uint32_t>(at, offset + row * 0x14, 0x3f800000);
            std::memset(at + offset + row * 0x14 + 4, 0, 0x10);
        }
    };
    identity(0xa0);
    GnmStoreFloats(at + 0xdc, {1, 1, 0, 0});
    Store<std::uint32_t>(at, 0xec, 0x3f800000);
    std::memset(at + 0xf0, 0, 0x10);
    for (std::size_t offset = 0x100; offset < 0x160; offset += 0x20) {
        GnmStoreFloats(at + offset, {1, 0, 0, 1});
        std::memset(at + offset + 0x10, 0, 0x10);
    }
    StorePtr(header, 0x338, at + 0xa0);
    const auto frame = [&](std::size_t offset, std::uint32_t tailAddress) {
        identity(offset);
        Store<std::uint64_t>(at, offset + 0x3c, 0x3f8000003f800000);
        std::memset(at + offset + 0x44, 0, 0x10);
        Store<std::uint32_t>(at, offset + 0x54, 0x3f800000);
        std::memset(at + offset + 0x58, 0, 0x10);
        Store<std::uint32_t>(at, offset + 0x68, 0x3f800000);
        std::memset(at + offset + 0x6c, 0, 0x10);
        Store<std::uint32_t>(at, offset + 0x7c, 0x3f800000);
        if (tailAddress != 0) {
            std::memset(at + offset + 0x80, 0, 0x10);
        }
    };
    frame(0x160, 1);
    const std::uint8_t* matrix = LoadPtr(header, 0x3b8);
    const auto pick = [](std::size_t i) { return i / 2 * 0x10 + i % 2 * 4; };
    for (std::size_t i = 0; i < 8; ++i) {
        std::memcpy(at + 0x1f0 + 4 * i, matrix + pick(i), 4);
        std::memcpy(at + 0x210 + 4 * i, matrix + pick(i), 4);
    }
    StorePtr(header, 0x340, at + 0x160);
    frame(0x230, 0);
    GnmStoreWords(at + 0x2b0, {0, 0, 0, 1});
    matrix = LoadPtr(header, 0x3b8);
    for (std::size_t i = 0; i < 8; ++i) {
        std::memcpy(at + 0x2c0 + 4 * i, matrix + pick(i), 4);
        std::memcpy(at + 0x2e0 + 4 * i, matrix + pick(i), 4);
    }
    StorePtr(header, 0x348, at + 0x230);
    GnmStoreOnes(at + 0x300, 4);
    Store<std::uint64_t>(at, 0x310, 0);
    Store<std::uint32_t>(at, 0x318, 1);
    StorePtr(header, 0x350, at + 0x300);
    StorePtr(header, 0x3c0, at + 0x300);
    return at;
}

constexpr std::uint32_t GnmTexWrap = 0;
constexpr std::uint32_t GnmTexMirror = 1;
constexpr std::uint32_t GnmTexClampLastTexel = 2;
constexpr std::uint32_t GnmTexMirrorOnceLastTexel = 3;
constexpr std::uint32_t GnmTexClampBorder = 6;
constexpr std::uint32_t GnmTexMaxLod = 0xfff;
constexpr std::uint32_t GnmTexXyFilterBilinear = 1;
constexpr std::uint32_t GnmTexZFilterPoint = 1;
constexpr std::uint32_t GnmTexMipFilterPoint = 1;

void GnmSampler(std::uint8_t* at, std::uint32_t clampXy, std::uint32_t clampZ) {
    GnmStoreWords(at, {clampXy | clampXy << 3 | clampZ << 6, GnmTexMaxLod << 12, 0, 0});
}

std::uint32_t GnmSamplerFilters(std::uint32_t xyFilter) {
    return xyFilter << 20 | xyFilter << 22 | GnmTexZFilterPoint << 24 | GnmTexMipFilterPoint << 26;
}

std::uint64_t GnmBlendMode(std::uint32_t color, std::uint32_t alpha) { return color | alpha << 8; }

void GnmFillSamplers(std::uint8_t* header) {
    GnmSampler(header + 0x2c8, GnmTexWrap, GnmTexClampBorder);
    Store<std::uint32_t>(header, 0x2d0, GnmSamplerFilters(0));
    GnmSampler(header + 0x2d8, GnmTexClampLastTexel, GnmTexClampLastTexel);
    Store<std::uint32_t>(header, 0x2e0, GnmSamplerFilters(GnmTexXyFilterBilinear));
    GnmSampler(header + 0x2e8, GnmTexMirrorOnceLastTexel, GnmTexMirrorOnceLastTexel);
    Store<std::uint32_t>(header, 0x2f0, GnmSamplerFilters(GnmTexXyFilterBilinear));
    GnmSampler(header + 0x2f8, GnmTexMirror, GnmTexMirror);
    Store<std::uint32_t>(header, 0x300, GnmSamplerFilters(GnmTexXyFilterBilinear));
    GnmSampler(header + 0x308, GnmTexClampBorder, GnmTexClampBorder);
    Store<std::uint32_t>(header, 0x310, GnmSamplerFilters(GnmTexXyFilterBilinear));
    GnmSampler(header + 0x318, GnmTexWrap, GnmTexClampBorder);
    Store<std::uint32_t>(header, 0x320, GnmSamplerFilters(GnmTexXyFilterBilinear));
    Store(header, 0x390, GnmBlendMode(0, 12));
    Store(header, 0x398, GnmBlendMode(12, 12));
    Store(header, 0x3a0, GnmBlendMode(0, 4));
    Store(header, 0x3a8, GnmBlendMode(4, 6));
    Store(header, 0x3b0, GnmBlendMode(4, 12));
}

struct GnmTextureSpec {
    std::uint32_t type;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
    std::uint32_t pitch;
    std::uint32_t mips;
    std::uint32_t slices;
    std::uint32_t format;
    std::uint32_t tileMode;
    std::uint32_t gpuMode;
    std::uint32_t fragments;
    std::uint32_t reserved;
};

static_assert(sizeof(GnmTextureSpec) == 0x30);

constexpr std::uint32_t GnmTextureType2d = 9;
constexpr std::uint32_t GnmTileModeDisplayLinearAligned = 8;

const std::uint8_t* GnmRampSpec(std::uint8_t* spec) {
    const GnmTextureSpec ramp{GnmTextureType2d, 0x100, 1, 1, 0, 1, 1, GnmDataFormat(GnmFormat8, 0, GnmSel0, GnmSel0, GnmSel0, GnmSelX),
                              GnmTileModeDisplayLinearAligned, 0, 0, 0};
    std::memcpy(spec, &ramp, sizeof(ramp));
    return spec;
}

int GnmInit(std::uint8_t* instance, std::uint8_t* data, std::uint64_t size) {
    GnmLoadedShaders();
    static const bool constructed = (GnmConstructShaderObjects(), true);
    static_cast<void>(constructed);
    std::uint8_t* const header = AlignPtr(data, 0x10);
    GnmInitLayout layout{};
    layout.cursor = header + 0x3c8;
    Store<std::uint32_t>(instance, 0x10, 0x1400);
    StorePtr(instance, 8, header);
    Store<std::uint32_t>(header, 0, 0x3c8);
    Store(header, 4, Load<std::uint32_t>(instance, 0x14));
    std::uint8_t* const aligned = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(header) + 0x103c7) & ~std::uintptr_t{0xffff});
    const std::uintptr_t gap = layout.cursor == aligned ? 0x10000 : static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(aligned) -
                                                                                               reinterpret_cast<std::uintptr_t>(layout.cursor));
    layout.gpu = layout.cursor + gap;
    layout.code = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(header) + gap + 0x43c7) & ~std::uintptr_t{0x3fff});
    layout.tail = layout.code != nullptr ? layout.code + 0xc000 : layout.gpu;
    if (!GnmUploadShaders(layout)) return -1;
    std::uint8_t* const end = data + size;
    std::uint8_t* const block = AlignPtr(layout.tail, 0x10);
    std::uint8_t* const blockEnd = block != nullptr ? block + 0xbe0 : layout.tail;
    if (blockEnd > end) return -1;
    GnmFillConstantBlock(header, block);
    std::uint8_t* const tail = block + 0x5c0;
    std::uint8_t spec[0x30];
    if (GnmInitTextureSpec(header + 0x2a0, GnmRampSpec(spec)) != 0) return -1;
    std::uint8_t* at = tail;
    const std::uint64_t sizeAlign = GnmTextureSizeAlign(header + 0x2a0);
    if (static_cast<std::uint32_t>(sizeAlign) != 0) {
        const auto alignment = static_cast<std::uint32_t>(sizeAlign >> 32);
        const std::uintptr_t mask = 0xffffffff00000000ull | static_cast<std::uint32_t>(0u - alignment);
        at = reinterpret_cast<std::uint8_t*>(mask & (reinterpret_cast<std::uintptr_t>(at) + alignment - 1));
        StorePtr(header, 0x2c0, at);
        std::memset(at, 0xff, static_cast<std::uint32_t>(sizeAlign));
        at += static_cast<std::uint32_t>(sizeAlign);
    }
    GnmSetResourceBase(reinterpret_cast<std::uint32_t*>(header + 0x2a0), static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(LoadPtr(header, 0x2c0)) >> 8));
    std::memset(at, 0, 0x30);
    std::memcpy(at + 0x70, header + 0x1b8, 0x10);
    GnmSampler(at + 0x80, GnmTexClampLastTexel, GnmTexClampLastTexel);
    Store<std::uint32_t>(at, 0x88, GnmSamplerFilters(GnmTexXyFilterBilinear));
    if (GnmInitTextureSpec(at + 0x90, GnmRampSpec(spec)) != 0) return -1;
    GnmSetResourceBase(reinterpret_cast<std::uint32_t*>(at + 0x90), static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(LoadPtr(header, 0x2c0)) >> 8));
    std::memset(at + 0x30, 0, 0x40);
    StorePtr(header, 0x360, at);
    GnmFillVertexData(header, at);
    GnmFillSamplers(header);
    std::uint8_t* const commands = AlignPtr(blockEnd, 0x10);
    std::uint8_t* const commandsEnd = commands != nullptr ? commands + 0x1320 : blockEnd;
    GnmInitFrame frame{};
    std::uint8_t* const command = frame.command;
    std::uint8_t* const aux = frame.aux;
    std::uint8_t* const state = frame.state;
    if (commandsEnd > end) return -1;
    std::memset(commands, 0, 0x1320);
    StorePtr(aux, 0, header);
    StorePtr(aux, 0x18, commands);
    GnmCbInit(aux + 0x30, nullptr, 0);
    StorePtr(aux, 8, state);
    GnmCbInit(command + 8, commands, 0x1320);
    StorePtr(command, 0x48, aux);
    std::uint8_t* const constants = AlignPtr(commandsEnd, 0x10);
    if ((constants != nullptr ? constants + 0x100 : commandsEnd) > end) return -1;
    GnmInitState(state, command, constants, header);
    if (GnmBeginCommands(commands, command) == 0) return -1;
    if (GnmPrologue(header, commands, command + 8) == 0) return -1;
    if (GnmReserveLoop(command) == 0) return -1;
    if (GnmEmitLoop(command + 8) == 0) return -1;
    StorePtr(header, 0x40, GnmSyncReserved(command));
    if (GnmReserve(command, 4) == 0 || GnmCallLoop(command + 8) == 0) return -1;
    GnmFlush(command);
    StorePtr(header, 0x48, GnmSyncReserved(command));
    if (GnmReserve(command, 4) == 0 || GnmCallLoop(command + 8) == 0) return -1;
    GnmFlush(command);
    std::uint8_t* first = LoadPtr(command, 0x18);
    if (GnmReserveDefaults(command) == 0 || GnmReserve(command, 4) == 0) return -1;
    std::uint32_t emitted = GnmEmitDefaults(command + 8);
    emitted += GnmCallLoop(command + 8);
    if (emitted == 0) return -1;
    Store(header, 0x50, emitted >> 2);
    Store<std::uint32_t>(header, 0x54, 0);
    StorePtr(header, 0x58, first);
    Store<std::uint64_t>(header, 0x60, 0);
    GnmRestartAux(command, true);
    first = LoadPtr(command, 0x18);
    if (GnmReserveBlend(command) == 0 || GnmReserve(command, 4) == 0) return -1;
    emitted = GnmEmitBlend(command + 8);
    emitted += GnmCallLoop(command + 8);
    if (emitted == 0) return -1;
    Store(header, 0x68, emitted >> 2);
    Store<std::uint32_t>(header, 0x6c, 0);
    StorePtr(header, 0x70, first);
    Store<std::uint64_t>(header, 0x78, 0);
    GnmRestartAux(command, true);
    StorePtr(state, 0xd58, GnmShaderObject(GnmFetchShaderIndex));
    GnmBindVsStage(state, GnmShaderObject(GnmFetchShaderIndex), 0x88);
    Store<std::uint32_t>(state, 0x56c, 0);
    Store<std::uint64_t>(header, 0x98, 0x1000000000020);
    std::memset(header + 0xb8, 0, 0x20);
    StorePtr(header, 0xa0, LoadPtr(header, 0x340));
    StorePtr(header, 0xa8, LoadPtr(header, 0x340));
    StorePtr(header, 0xb0, header + 0x98);
    GnmFlush(command);
    GnmRecordVsDraw(command, state, header + 0x98);
    Store<std::uint64_t>(header, 0xd8, 0x1000000000020);
    std::memset(header + 0xf8, 0, 0x20);
    StorePtr(header, 0xe0, LoadPtr(header, 0x348));
    StorePtr(header, 0xe8, LoadPtr(header, 0x348));
    StorePtr(header, 0xf0, header + 0xd8);
    GnmFlush(command);
    GnmRecordVsDraw(command, state, header + 0xd8);
    GnmBindPsStage(state, header);
    Store<std::uint64_t>(header, 0x118, 0x1000000000001);
    std::memset(header + 0x138, 0, 0x20);
    GnmStoreOnes(header + 0x128, 4);
    Store<std::uint64_t>(header, 0x120, 0);
    GnmFlush(command);
    StorePtr(state, 0xe00, header + 0x128);
    GnmRecordPsDraw(command, state, header + 0x118);
    Store<std::uint64_t>(state, 0xe00, 0);
    GnmFlush(command);
    first = LoadPtr(command, 0x18);
    if (GnmReserveDraws(command) == 0 || GnmReserve(command, 4) == 0) return -1;
    emitted = GnmEmitDraws(command + 8);
    emitted += GnmCallLoop(command + 8);
    if (emitted == 0) return -1;
    Store(header, 0x80, emitted >> 2);
    Store<std::uint32_t>(header, 0x84, 0);
    StorePtr(header, 0x88, first);
    Store<std::uint64_t>(header, 0x90, 0);
    GnmRestartAux(command, false);
    return sceKernelMtypeprotect(layout.gpu, static_cast<std::size_t>(end - layout.gpu), 3, 0x32) != 0 ? -1 : 0;
}

int APS5_VABI GnmBackendInit(void* instance, void* data, std::uint64_t size) {
    return GnmInit(static_cast<std::uint8_t*>(instance), static_cast<std::uint8_t*>(data), size);
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

void GnmDmaData(std::uint8_t* cb, std::uint32_t dstSel, const void* dst, std::uint32_t srcSel, const void* src, std::uint32_t bytes,
                std::uint32_t blocking) {
    const auto to = reinterpret_cast<std::uintptr_t>(dst);
    const auto from = reinterpret_cast<std::uintptr_t>(src);
    GnmEmit(cb, {0xc0055000, (blocking << 31) + ((srcSel & 3) << 29 | (dstSel & 1) << 20) + 0x4004000, static_cast<std::uint32_t>(from),
                 static_cast<std::uint32_t>(from >> 32), static_cast<std::uint32_t>(to), static_cast<std::uint32_t>(to >> 32),
                 (bytes & 0x1fffff) | (dstSel & 8) << 26 | (dstSel & 4) << 25 | (srcSel & 4) << 24 | (srcSel & 8) << 25});
}

void GnmCount(std::uint8_t* command, std::uint32_t dwords) {
    GnmSyncReserved(command);
    Store(command, 0, Load<std::uint32_t>(command, 0) + dwords);
}

std::uint32_t GnmLoopDwords(const std::uint8_t* cb) { return Load<std::uint32_t>(LoadPtr(LoadPtr(cb, 0x40), 0), 0x2c); }

std::uint32_t GnmRoom(const std::uint8_t* cb, const std::uint8_t* from) {
    return (Load<std::uint32_t>(cb, GnmCbEnd) - static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(from))) & ~3u;
}

std::uint32_t GnmLow(const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer)); }

void GnmCallPatched(std::uint8_t* cb, const void* target, std::uint32_t dwords, std::uint8_t** cursorOut, std::uint32_t gap) {
    std::uint8_t* const header = LoadPtr(LoadPtr(cb, 0x40), 0);
    const std::uint32_t loopBytes = Load<std::uint32_t>(header, 0x2c) * 4;
    std::uint8_t* const loop = LoadPtr(header, 0x38);
    std::uint8_t* const start = GnmCbCur(cb);
    std::uint8_t local[0x40];
    GnmCbInit(local, start, GnmRoom(cb, start));
    GnmDmaData(local, 0, start, 0, loop, loopBytes, 1);
    GnmDmaData(local, 0, loop, 0, start + loopBytes, loopBytes, 1);
    GnmIndirectBuffer(local, target, dwords);
    std::uint8_t* const patch = GnmCbCur(local) + static_cast<std::uintptr_t>(gap) * 4;
    GnmDmaData(cb, 0, patch, 0, loop, loopBytes, 1);
    std::uint8_t* const resume = patch + loopBytes;
    GnmDmaData(cb, 0, loop, 0, resume, loopBytes, 1);
    if (cursorOut != nullptr) *cursorOut = GnmCbCur(cb);
    std::uint8_t* const end = GnmCbCur(cb) + static_cast<std::uintptr_t>(Load<std::uint32_t>(header, 0x2c) * 3 + gap) * 4;
    StorePtr(cb, GnmCbCursor, end);
    GnmDmaData(cb, 0, loop, 0, patch, loopBytes, 1);
    if (GnmCbCur(cb) < start || GnmCbCur(cb) > LoadPtr(cb, GnmCbEnd)) return;
    std::uint8_t* const aux = LoadPtr(cb, 0x40);
    GnmLinkPending(aux, LoadPtr(local, 0), GnmCbCur(local));
    StorePtr(aux, 0x70, end);
    GnmCbInit(aux + 0x30, resume, loopBytes);
    GnmCbInit(local, LoadPtr(aux, 0x40), GnmRoom(aux + 0x30, LoadPtr(aux, 0x40)));
    GnmNop(local, Load<std::uint32_t>(header, 0x2c));
}

void GnmOpenLocalChain(GnmChain& chain, std::uint8_t* state, std::uint8_t* header, std::uint8_t* begin, std::uint32_t bytes) {
    std::memset(&chain, 0, sizeof(chain));
    StorePtr(state, 0xcb0, chain.command);
    StorePtr(chain.aux, 0, header);
    StorePtr(chain.aux, 8, state);
    StorePtr(chain.aux, 0x18, begin);
    GnmCbInit(chain.aux + 0x30, nullptr, 0);
    GnmCbInit(chain.command + 8, begin, bytes);
    StorePtr(chain.command, 0x48, chain.aux);
}

void GnmLinkSequence(std::uint8_t* command, bool hasBuffer, std::uint8_t* state, std::uint8_t* ctx, std::uint8_t* resume) {
    std::uint8_t* const header = LoadPtr(command, 0x50);
    const std::uint32_t loop = Load<std::uint32_t>(header, 0x2c);
    std::uint8_t* const cb = command + 8;
    GnmChain chain;
    if (resume != nullptr) {
        GnmOpenLocalChain(chain, state, header, LoadPtr(ctx, 0x70), loop * 4);
        if (!hasBuffer) return;
        GnmIndirectBuffer(chain.command + 8, resume, loop);
        std::uint8_t local[0x40];
        GnmCbInit(local, LoadPtr(ctx, 0x78), loop * 12);
        std::uint8_t* const begin = LoadPtr(command, 0x80);
        const std::uint32_t bytes = GnmLow(LoadPtr(command, 0x88)) - GnmLow(begin);
        GnmIndirectBuffer(local, begin, bytes);
        StorePtr(local, GnmCbCursor, GnmCbCur(local) + static_cast<std::uintptr_t>(Load<std::uint32_t>(header, 0x2c)) * 4);
        GnmIndirectBuffer(local, begin, bytes);
        return;
    }
    std::uint8_t* const out = LoadPtr(ctx, 0x80);
    if (hasBuffer) {
        std::uint8_t* const aux = LoadPtr(command, 0x48);
        if (aux == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002e");
        std::uint8_t* const loopHeader = LoadPtr(aux, 0);
        const std::uint32_t loopBytes = Load<std::uint32_t>(loopHeader, 0x2c) * 4;
        std::uint8_t local[0x40];
        GnmCbInit(local, GnmCbCur(cb), GnmRoom(cb, GnmCbCur(cb)));
        GnmDmaData(local, 0, nullptr, 0, nullptr, loopBytes, 1);
        GnmEmit(local, {0xc0004200, 0});
        GnmIndirectBuffer(local, nullptr, 0);
        const std::uintptr_t step = static_cast<std::uintptr_t>(Load<std::uint32_t>(loopHeader, 0x2c)) * 4;
        std::uint8_t* const copyA = GnmCbCur(local) + step;
        std::uint8_t* const copyB = copyA + step;
        GnmDmaData(cb, 0, copyB, 0, copyA, loopBytes, 1);
        GnmEmit(cb, {0xc0004200, 0});
        GnmOpenChain(cb);
        const std::uint32_t twice = Load<std::uint32_t>(loopHeader, 0x2c) * 2;
        std::uint8_t* const mid = GnmCbCur(cb);
        std::uint8_t* const skipTo = mid + static_cast<std::uintptr_t>(twice) * 4;
        if (LoadPtr(cb, GnmCbEnd) >= skipTo) {
            if (twice != 0) StorePtr(cb, GnmCbCursor, skipTo);
            StorePtr(LoadPtr(command, 0x48), 0x70, skipTo);
        }
        GnmOpenChain(cb);
        GnmDmaData(cb, 0, copyB, 0, mid, loopBytes, 1);
        GnmOpenChain(cb);
        std::memcpy(mid, copyB, loopBytes);
        std::memcpy(copyA, copyB, loopBytes);
        StorePtr(ctx, 0x78, mid);
    } else {
        const std::uint32_t loopNow = GnmLoopDwords(cb);
        GnmCount(command, 9);
        GnmCount(command, loopNow);
        std::uint8_t* const first = GnmSyncReserved(command);
        if (first + static_cast<std::uintptr_t>(loopNow) * 8 <= LoadPtr(command, 0x10) && loopNow * 2 != 0) {
            Store(command, 0, Load<std::uint32_t>(command, 0) + loopNow * 2);
            GnmSyncReserved(command);
        }
        GnmCount(command, loopNow);
        GnmCount(command, 7);
        GnmCount(command, loopNow);
        GnmSyncReserved(command);
        StorePtr(ctx, 0x78, first);
    }
    std::uint8_t* const tail = GnmSyncReserved(command);
    GnmOpenLocalChain(chain, state, header, tail, GnmLow(LoadPtr(command, 0x10)) - GnmLow(tail));
    std::uint8_t* mark = nullptr;
    std::uint8_t* const local = chain.command;
    if (hasBuffer) {
        GnmCallPatched(local + 8, nullptr, 0, &mark, 0);
    } else {
        GnmCount(local, 0xe);
        mark = GnmSyncReserved(local);
        Store(local, 0, Load<std::uint32_t>(local, 0) + Load<std::uint32_t>(header, 0x2c) * 3);
        GnmCount(local, 7);
        GnmSyncReserved(local);
    }
    std::uint8_t* const resumeAt = GnmSyncReserved(local);
    if (hasBuffer) {
        GnmIndirectBuffer(local + 8, nullptr, 0);
    } else {
        GnmCount(local, 4);
        GnmSyncReserved(local);
    }
    std::uint8_t* const call = GnmSyncReserved(local);
    if (hasBuffer) {
        GnmIndirectBuffer(local + 8, nullptr, 0);
        GnmCallPatched(cb, call, loop, &mark, 0);
        GnmOpenChain(cb);
        GnmOpenChain(cb);
    } else {
        GnmCount(local, 4);
        GnmSyncReserved(local);
        const std::uint32_t loopNow = GnmLoopDwords(cb);
        GnmCount(command, 0xe);
        mark = GnmSyncReserved(command);
        Store(command, 0, Load<std::uint32_t>(command, 0) + loopNow * 3);
        GnmCount(command, 7);
        GnmCount(command, loopNow);
        GnmCount(command, loopNow);
        GnmSyncReserved(command);
    }
    StorePtr(out, 0, call);
    StorePtr(ctx, 0x70, resumeAt);
}

void GnmCallBlock(std::uint8_t* ctx, const void* target, std::uint32_t dwords) {
    std::uint8_t* const command = ctx + 0x100;
    if (Load<std::uint32_t>(ctx, 0) != 0) {
        GnmCallPatched(command + 8, target, dwords, nullptr, 0);
        return;
    }
    GnmCount(command, GnmLoopDwords(command + 8) * 3 + 0x15);
    GnmSyncReserved(command);
}

int GnmEndSequence(std::uint8_t* ctx, std::uint8_t* record) {
    const bool hasBuffer = Load<std::uint32_t>(ctx, 0) != 0;
    std::uint8_t* const command = ctx + 0x100;
    std::uint8_t* const cb = ctx + 0x108;
    std::uint8_t* start = nullptr;
    std::uint32_t size = 0;
    if (ctx[4] != 1) {
        const bool opened = Load<std::uint16_t>(ctx, 6) != 0;
        if (!opened) {
            std::uint8_t* const header = LoadPtr(ctx, 0x150);
            GnmCallBlock(ctx, LoadPtr(header, 0x70), Load<std::uint32_t>(header, 0x68));
            if ((ctx[5] & 2) != 0) {
                std::uint8_t* const targets = LoadPtr(ctx, 0x80);
                if (targets == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002e");
                GnmCallBlock(ctx, LoadPtr(targets, 0x10), GnmLoopDwords(cb));
            }
        }
        if (hasBuffer) {
            std::uint8_t* const before = GnmCbCur(cb);
            std::uint8_t* const header = LoadPtr(LoadPtr(ctx, 0x148), 0);
            GnmIndirectBuffer(cb, LoadPtr(header, 0x20), Load<std::uint32_t>(header, 0x28));
            std::uint8_t* const after = GnmCbCur(cb);
            if (after >= before && after <= LoadPtr(cb, GnmCbEnd)) {
                std::uint8_t* const aux = LoadPtr(ctx, 0x148);
                GnmLinkPending(aux, LoadPtr(cb, GnmCbBegin), after);
                Store<std::uint64_t>(aux, 0x70, 0);
                GnmCbInit(aux + 0x30, nullptr, 0);
            }
        } else {
            GnmCount(command, 4);
            GnmSyncReserved(command);
        }
        start = LoadPtr(ctx, 0x170);
        std::uint8_t* finish = nullptr;
        if (start != nullptr) {
            finish = LoadPtr(ctx, 0x178);
        } else {
            start = LoadPtr(cb, GnmCbBegin);
            finish = GnmSyncReserved(command);
        }
        size = static_cast<std::uint32_t>(finish - start);
        if (!opened) {
            std::uint8_t* const resume = GnmSyncReserved(command);
            std::uint8_t* const state = LoadPtr(ctx, 0x158);
            const std::uint32_t loop = GnmLoopDwords(cb);
            if (!hasBuffer) {
                GnmCount(command, loop + 4);
                GnmSyncReserved(command);
            } else {
                std::uint8_t* const cursor = GnmCbCur(cb);
                std::uint8_t* const pad = cursor + static_cast<std::uintptr_t>(loop) * 4;
                GnmIndirectBuffer(cb, pad, loop);
                std::memset(pad, 0, static_cast<std::size_t>(loop) * 4);
                GnmNop(cb, loop);
                std::uint8_t* const after = GnmCbCur(cb);
                if (after >= cursor && after <= LoadPtr(cb, GnmCbEnd)) {
                    std::uint8_t* const aux = LoadPtr(ctx, 0x148);
                    std::uint8_t* const pending = LoadPtr(aux, 0x70);
                    if (pending == nullptr) {
                        StorePtr(aux, 0x20, LoadPtr(cb, GnmCbBegin));
                        StorePtr(aux, 0x28, after);
                    } else if (pad > pending) {
                        GnmIndirectBuffer(aux + 0x30, pending, static_cast<std::uint32_t>((pad - pending) >> 2));
                    }
                    StorePtr(aux, 0x70, after);
                    GnmCbInit(aux + 0x30, pad, loop * 4);
                }
            }
            GnmLinkSequence(command, hasBuffer, state, ctx, resume);
        }
    }
    const std::uint32_t constants = Load<std::uint32_t>(ctx, 0x1d8) - Load<std::uint32_t>(ctx, 0x1c8);
    Store(ctx, 0xc, constants);
    const std::uint32_t used = GnmLow(GnmSyncReserved(command)) - Load<std::uint32_t>(ctx, 0x168);
    Store(ctx, 0x10, (hasBuffer ? 0 : constants) + used);
    std::uint8_t* const state = LoadPtr(ctx, 0x158);
    Store(ctx, 0x14, Load<std::uint32_t>(state, 0xcc8) - Load<std::uint32_t>(state, 0xcb8));
    if (record != nullptr && hasBuffer) {
        StorePtr(record, 0x28, start);
        Store(record, 0x30, size);
        StorePtr(record, 0x38, LoadPtr(ctx, 0x180));
        Store(record, 0x34, Load<std::uint32_t>(ctx, 0x188) - Load<std::uint32_t>(ctx, 0x180));
        const std::uint8_t* const targets = LoadPtr(ctx, 0x80);
        if (targets != nullptr) {
            for (std::uint32_t i = 0; i < 3; ++i) {
                const std::uint8_t* const target = LoadPtr(targets, i * 8);
                if (target != nullptr) Store(record, 0x4e + i * 2, static_cast<std::uint16_t>(target - start));
            }
        }
    }
    return 0;
}

int APS5_VABI GnmBackendEndSequence(void* ctx, void* record) {
    return GnmEndSequence(static_cast<std::uint8_t*>(ctx), static_cast<std::uint8_t*>(record));
}

void GnmShPointer(std::uint8_t* cb, std::uint32_t stage, std::uint32_t reg, const void* address) {
    const auto value = reinterpret_cast<std::uintptr_t>(address);
    GnmEmit(cb, {(stage == 0 ? 2u : 0u) + 0xc0027600, reg + GnmShBases[stage] - GnmShRegBase, static_cast<std::uint32_t>(value),
                 static_cast<std::uint32_t>(value >> 32)});
}

std::uint32_t GnmResourceBytes(const std::uint8_t* usage) { return ((usage[3] << 4) & 0x10) + 0x10; }

const std::uint8_t* GnmStageUsage(const std::uint8_t* stage, std::size_t offset) {
    const std::uint8_t* const usage = LoadPtr(stage, offset);
    if (usage == nullptr || usage[2] < 0x10) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    return usage;
}

void GnmCommitPsStage(std::uint8_t* stage, std::uint8_t* cb) {
    const std::uint8_t* const table = LoadPtr(stage, 0x530);
    if (table == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    GnmShPointer(cb, 1, table[2], LoadPtr(stage, 0x538));
    std::uint8_t* const base = LoadPtr(stage, 0x538);
    std::uint32_t bits = Load<std::uint32_t>(stage, 0);
    for (std::uint32_t i = 0; bits != 0; ++i, bits >>= 1) {
        if ((bits & 1) != 0) {
            const std::uint8_t* const usage = GnmStageUsage(stage, 0x10 + i * 40);
            if (base != nullptr) std::memcpy(base + (usage[2] - 0x10) * 4u, stage + 0x24 + i * 40, GnmResourceBytes(usage));
        }
        if (i > 0x12 || bits == 1) break;
    }
    bits = Load<std::uint32_t>(stage, 8);
    for (std::uint32_t i = 0; bits != 0; ++i, bits >>= 1) {
        if ((bits & 1) != 0) {
            const std::uint8_t* const usage = GnmStageUsage(stage, 0x330 + i * 16);
            if (base == nullptr || i > 0xe) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
            std::memcpy(base + (usage[2] - 0x10) * 4u, LoadPtr(stage, 0x338 + i * 16), GnmResourceBytes(usage));
        }
        if (bits == 1) break;
    }
    if (Load<std::uint32_t>(stage, 4) != 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    if (LoadPtr(stage, 0x560) != nullptr) GnmEmitUserData(cb, stage, 1);
}

void GnmCountPsStage(std::uint8_t* stage, std::uint8_t* command) {
    if (LoadPtr(stage, 0x530) != nullptr) Store(command, 0, Load<std::uint32_t>(command, 0) + 4);
    std::uint32_t bits = Load<std::uint32_t>(stage, 0);
    for (std::uint32_t i = 0; bits != 0 && i <= 0x13; ++i, bits >>= 1)
        if ((bits & 1) != 0) GnmStageUsage(stage, 0x10 + i * 40);
    bits = Load<std::uint32_t>(stage, 8);
    for (std::uint32_t i = 0; bits != 0 && i <= 0xf; ++i, bits >>= 1)
        if ((bits & 1) != 0) GnmStageUsage(stage, 0x330 + i * 16);
    if (Load<std::uint32_t>(stage, 4) != 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    GnmCountUserData(command, stage);
}

std::uint8_t* GnmOpenRecord(std::uint8_t* command, bool hasBuffer) {
    if (hasBuffer) {
        GnmOpenChain(command + 8);
        return GnmCbCur(command + 8);
    }
    GnmCount(command, GnmLoopDwords(command + 8));
    return GnmSyncReserved(command);
}

void GnmCommitRecord(std::uint8_t* command, bool hasBuffer, GnmChain& chain) {
    if (hasBuffer) {
        GnmCommitChain(command, chain);
        return;
    }
    std::uint8_t* const end = GnmSyncReserved(chain.command);
    std::uint8_t* const at = GnmReserveStart(command);
    if (at > end || LoadPtr(command, 0x10) < end) return;
    Store(command, 0, Load<std::uint32_t>(command, 0) + static_cast<std::uint32_t>((end - at) >> 2));
}

void GnmUserPointer(std::uint8_t* local, bool hasBuffer, std::uint32_t slot, const void* value) {
    std::uint8_t* const aux = LoadPtr(local, 0x48);
    if (aux == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    std::uint8_t* const state = LoadPtr(aux, 8);
    std::uint8_t* const stage = state + GnmStagePs;
    const std::size_t offset = 0xdc0 + slot * 4;
    StorePtr(state, offset, value);
    if (LoadPtr(stage, 0x560) == nullptr) return;
    if (((reinterpret_cast<std::uintptr_t>(state) + offset) & 3) == 0 && Load<std::uint32_t>(stage, 0x568) >= slot + 2) {
        Store(stage, 0x570 + slot * 4, Load<std::uint64_t>(state, offset));
        Store(stage, 0x56c, Load<std::uint32_t>(stage, 0x56c) | 3u << slot);
    }
    if (hasBuffer) GnmEmitUserData(local + 8, stage, 1);
    else GnmCountUserData(local, stage);
}

template <typename Body>
void GnmRecordBlock(std::uint8_t* command, bool hasBuffer, std::uint8_t* state, std::uint8_t* block, std::uint32_t flags, std::uint8_t* data,
                    Body body) {
    GnmSyncReserved(command);
    std::uint8_t* const header = LoadPtr(command, 0x50);
    if ((flags & 0x95) != 0x81 || block == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    std::uint8_t* const constants = LoadPtr(state, GnmStateConstants);
    std::uint8_t* const begin = GnmOpenRecord(command, hasBuffer);
    GnmChain chain;
    GnmOpenLocalChain(chain, state, header, begin, GnmLow(LoadPtr(command, 0x10)) - GnmLow(begin));
    std::uint8_t* const local = chain.command;
    std::uint8_t* const start = GnmReserveStart(local);
    if (hasBuffer) {
        GnmOpenChain(local + 8);
        if (LoadPtr(local, 0x10) >= LoadPtr(local, 0x18)) StorePtr(chain.aux, 0x70, LoadPtr(local, 0x18));
    } else {
        GnmCount(local, GnmLoopDwords(local + 8));
        GnmSyncReserved(local);
    }
    body(local);
    const auto dwords = static_cast<std::uint32_t>((GnmSyncReserved(local) - start) >> 2);
    if (hasBuffer) GnmCallLoop(local + 8);
    else GnmCount(local, 4);
    GnmCommitRecord(command, hasBuffer, chain);
    const std::uint32_t used = (Load<std::uint32_t>(state, GnmStateConstants) - GnmLow(constants)) >> 2;
    if (Load<std::uint64_t>(block, 0x30) == 0) {
        if (hasBuffer) {
            StorePtr(block, 0x10, data);
            StorePtr(block, 0x28, constants);
            StorePtr(block, 0x30, start);
        }
        Store(block, 0x20, static_cast<std::uint16_t>(used << 2));
        Store(block, 0x22, static_cast<std::uint16_t>(dwords << 2));
    }
    GnmSyncReserved(command);
}

std::uint8_t* GnmBlockOwner(std::uint8_t* block) {
    if (block == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    std::uint8_t* const owner = LoadPtr(block, 0x18);
    return owner != nullptr ? owner : block;
}

void GnmRecordPsPointer(std::uint8_t* command, bool hasBuffer, std::uint8_t* state, std::uint8_t* block, std::uint32_t flags, std::uint32_t slot) {
    std::uint8_t* const data = LoadPtr(GnmBlockOwner(block), 8);
    if (data == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    GnmRecordBlock(command, hasBuffer, state, block, flags, data, [&](std::uint8_t* local) { GnmUserPointer(local, hasBuffer, slot, data); });
}

void GnmBindVsBuffer(std::uint8_t* local, bool hasBuffer, const std::uint8_t* data) {
    std::uint8_t* const aux = LoadPtr(local, 0x48);
    if (aux == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    std::uint8_t* const state = LoadPtr(aux, 8);
    std::uint8_t* const stage = state + GnmStagePs;
    std::uint8_t* at = LoadPtr(state, GnmStateConstants);
    if (LoadPtr(stage, 0x530) != nullptr) {
        if ((stage[0x548] & 1) == 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
        at = AlignPtr(at, 4);
        StorePtr(stage, 0x538, at);
        at += Load<std::uint64_t>(stage, 0x540);
    }
    StorePtr(state, GnmStateConstants, at);
    if (hasBuffer && (stage[0] & 1) != 0) {
        const std::uint32_t high = Load<std::uint32_t>(data, 4);
        const std::uint64_t base = Load<std::uint32_t>(data, 0) | static_cast<std::uint64_t>(high & 0xfff) << 32;
        const std::uint32_t stride = (high >> 16) & 0x3fff;
        GnmBufferDescriptor(stage + 0x24, reinterpret_cast<const void*>(base), (stride != 0 ? stride : 1) * Load<std::uint32_t>(data, 8));
    }
    const std::uint8_t* const object = GnmShaderObject(8);
    GnmBindPsBuffer(stage, object, 0x78, 0x80, data + 0x10, false);
    const std::uint8_t* const extra = LoadPtr(data, 0x20);
    GnmBindPsBuffer(stage, object, 0x90, 0x98, extra != nullptr ? extra : LoadPtr(aux, 0) + 0x2a0, true);
    if (LoadPtr(stage, 0x560) != nullptr && Load<std::uint32_t>(stage, 0x568) >= 8) {
        Store(stage, 0x588, Load<std::uint64_t>(data, 0x28));
        stage[0x56c] |= 0xc0;
    }
    if (hasBuffer) GnmCommitPsStage(stage, local + 8);
    else GnmCountPsStage(stage, local);
}

void GnmRecordVsBuffer(std::uint8_t* command, bool hasBuffer, std::uint8_t* state, std::uint8_t* block, std::uint32_t flags) {
    std::uint8_t* data = LoadPtr(GnmBlockOwner(block), 8);
    if (data == nullptr) data = LoadPtr(command, 0x50) + 0x1d8;
    GnmRecordBlock(command, hasBuffer, state, block, flags, data, [&](std::uint8_t* local) { GnmBindVsBuffer(local, hasBuffer, data); });
}

constexpr std::uint8_t GnmBlendZero = 0;
constexpr std::uint8_t GnmBlendOne = 1;
constexpr std::uint8_t GnmBlendSrcAlpha = 4;
constexpr std::uint8_t GnmBlendOneMinusSrcAlpha = 5;
constexpr std::uint8_t GnmBlendDstAlpha = 6;
constexpr std::uint8_t GnmBlendOneMinusDstAlpha = 7;
constexpr std::uint8_t GnmBlendOneMinusDstColor = 9;
constexpr std::uint8_t GnmCombDstPlusSrc = 0;
constexpr std::uint8_t GnmCombMinDstSrc = 2;
constexpr std::uint8_t GnmCombMaxDstSrc = 3;
constexpr std::uint8_t GnmCombDstMinusSrc = 4;

struct GnmBlendFactor {
    std::uint8_t source;
    std::uint8_t op;
    std::uint8_t destination;
    bool set;
};

std::uint32_t GnmBlendColor(std::uint32_t control, const GnmBlendFactor& factor) {
    if (!factor.set) return control;
    return (control & 0xffffe000u) | (factor.source & 0x1fu) | ((factor.op << 5) & 0xffu) | (factor.destination & 0x1fu) << 8;
}

std::uint32_t GnmBlendAlpha(std::uint32_t control, const GnmBlendFactor& factor) {
    if (!factor.set) return control;
    return (control & 0xe000ffffu) | (factor.source & 0x1fu) << 16 | (factor.op & 7u) << 21 | (factor.destination & 0x1fu) << 24;
}

GnmBlendFactor GnmBlendPick(const GnmBlendFactor (&table)[13], std::uint32_t mode) { return mode <= 0xc ? table[mode] : GnmBlendFactor{}; }

std::uint32_t GnmBlendFirst(std::uint32_t value, bool premultiplied) {
    const std::uint8_t opaque = (value & 0xff00) == 0x700 ? 1 : 0;
    const std::uint32_t color = premultiplied ? (value >> 8) & 0xff : value & 0xff;
    const std::uint32_t alpha = premultiplied ? 4 : (value >> 8) & 0xff;
    const std::uint8_t mixed = static_cast<std::uint8_t>(opaque | GnmBlendSrcAlpha);
    const std::uint8_t inverse = static_cast<std::uint8_t>(opaque ^ GnmBlendOneMinusSrcAlpha);
    const GnmBlendFactor colors[13] = {{mixed, GnmCombDstPlusSrc, inverse, true},
                                       {GnmBlendOne, GnmCombDstPlusSrc, GnmBlendZero, true},
                                       {mixed, GnmCombDstPlusSrc, GnmBlendOne, true},
                                       {mixed, GnmCombDstMinusSrc, GnmBlendOne, true},
                                       {GnmBlendZero, GnmCombDstPlusSrc, GnmBlendOne, true},
                                       {GnmBlendOneMinusDstColor, GnmCombDstPlusSrc, inverse, true},
                                       {GnmBlendOne, GnmCombMaxDstSrc, GnmBlendOne, true},
                                       {GnmBlendOne, GnmCombMinDstSrc, GnmBlendOne, true},
                                       {},
                                       {},
                                       {},
                                       {},
                                       {GnmBlendZero, GnmCombDstPlusSrc, GnmBlendZero, true}};
    constexpr GnmBlendFactor Alphas[13] = {{GnmBlendSrcAlpha, GnmCombDstPlusSrc, GnmBlendOneMinusSrcAlpha, true},
                                           {GnmBlendOne, GnmCombDstPlusSrc, GnmBlendZero, true},
                                           {GnmBlendOne, GnmCombDstPlusSrc, GnmBlendOne, true},
                                           {GnmBlendOne, GnmCombDstMinusSrc, GnmBlendOne, true},
                                           {GnmBlendZero, GnmCombDstPlusSrc, GnmBlendOne, true},
                                           {GnmBlendOneMinusDstAlpha, GnmCombDstPlusSrc, GnmBlendOneMinusSrcAlpha, true},
                                           {GnmBlendOne, GnmCombMaxDstSrc, GnmBlendOne, true},
                                           {GnmBlendOne, GnmCombMinDstSrc, GnmBlendOne, true},
                                           {},
                                           {},
                                           {},
                                           {},
                                           {GnmBlendZero, GnmCombDstPlusSrc, GnmBlendZero, true}};
    return GnmBlendAlpha(GnmBlendColor(3u << 29, GnmBlendPick(colors, color)), GnmBlendPick(Alphas, alpha));
}

std::uint32_t GnmBlendSecond(std::uint32_t value) {
    constexpr GnmBlendFactor Colors[13] = {{GnmBlendDstAlpha, GnmCombDstPlusSrc, GnmBlendOneMinusDstAlpha, true},
                                           {GnmBlendDstAlpha, GnmCombDstPlusSrc, GnmBlendZero, true},
                                           {GnmBlendDstAlpha, GnmCombDstPlusSrc, GnmBlendOne, true},
                                           {GnmBlendDstAlpha, GnmCombDstMinusSrc, GnmBlendOne, true},
                                           {GnmBlendZero, GnmCombDstPlusSrc, GnmBlendOne, true},
                                           {GnmBlendOneMinusDstColor, GnmCombDstPlusSrc, GnmBlendOneMinusSrcAlpha, true},
                                           {GnmBlendDstAlpha, GnmCombMaxDstSrc, GnmBlendOne, true},
                                           {GnmBlendOneMinusDstAlpha, GnmCombMinDstSrc, GnmBlendOne, true},
                                           {},
                                           {},
                                           {},
                                           {},
                                           {GnmBlendZero, GnmCombDstPlusSrc, GnmBlendZero, true}};
    return GnmBlendColor(GnmBlendAlpha(3u << 29, {GnmBlendOne, GnmCombDstPlusSrc, GnmBlendZero, true}), GnmBlendPick(Colors, (value >> 8) & 0xff));
}

void GnmBindBlend(std::uint8_t* local, bool hasBuffer, const std::uint8_t* data, bool second) {
    std::uint8_t* const aux = LoadPtr(local, 0x48);
    if (aux == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    const std::uint8_t* const header = LoadPtr(aux, 0);
    const std::uint32_t value = Load<std::uint16_t>(data, 0);
    const std::uint32_t control = second ? GnmBlendSecond(value) : GnmBlendFirst(value, Load<std::uint32_t>(LoadPtr(aux, 8), 0xd68) == 0x30);
    if (hasBuffer) GnmContextReg(local + 8, 0x1e0, control & 0xffff1fff);
    else Store(local, 0, Load<std::uint32_t>(local, 0) + 3);
    const bool opaque = (value & 0xff00) == 0x700;
    std::size_t offset = (value & 0xff00) == 0x500 ? 0x380 : 0x368;
    if (!second) offset = (value & 0xff) == 5 ? (opaque ? 0x388 : 0x370) : (opaque ? 0x378 : 0x368);
    GnmUserPointer(local, hasBuffer, 4, LoadPtr(header, offset));
}

void GnmRecordBlend(std::uint8_t* command, bool hasBuffer, std::uint8_t* state, std::uint8_t* block, std::uint32_t flags, bool second) {
    std::uint8_t* const field = GnmBlockOwner(block) + 8;
    GnmRecordBlock(command, hasBuffer, state, block, flags, LoadPtr(field, 0),
                   [&](std::uint8_t* local) { GnmBindBlend(local, hasBuffer, field, second); });
}

void GnmRecordVertexBlock(std::uint8_t* command, bool hasBuffer, std::uint8_t* state, std::uint8_t* block, std::uint32_t flags) {
    std::uint8_t* const header = LoadPtr(command, 0x50);
    const std::uint8_t* const source = LoadPtr(header, 0x338);
    if (block == nullptr || (flags & 0x3d) != 9) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    std::uint8_t* const constants = LoadPtr(state, GnmStateConstants);
    std::uint8_t* const start = GnmOpenRecord(command, hasBuffer);
    GnmChain chain;
    GnmOpenLocalChain(chain, state, header, start, GnmLow(LoadPtr(command, 0x10)) - GnmLow(start));
    std::uint8_t* const local = chain.command;
    StorePtr(state, 0xda0, constants);
    if (LoadPtr(state, 0x560) != nullptr) {
        if (((reinterpret_cast<std::uintptr_t>(state) + 0xda0) & 3) == 0 && Load<std::uint32_t>(state, 0x568) >= 0xa) {
            StorePtr(state, 0x590, constants);
            state[0x56d] |= 3;
        }
        if (hasBuffer) GnmEmitUserData(local + 8, state, 2);
        else GnmCountUserData(local, state);
    }
    std::uint8_t* const stage = state + GnmStagePs;
    if (LoadPtr(stage, 0x560) != nullptr) {
        if (Load<std::uint32_t>(stage, 0x568) < 2) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
        StorePtr(stage, 0x570, constants + 0xc0);
        Store(stage, 0x56c, Load<std::uint32_t>(stage, 0x56c) | 3);
        if (hasBuffer) GnmEmitUserData(local + 8, stage, 1);
        else GnmCountUserData(local, stage);
    }
    if (hasBuffer) GnmCallLoop(local + 8);
    else GnmCount(local, 4);
    std::uint8_t* const end = GnmSyncReserved(local);
    const auto bytes = static_cast<std::uint32_t>(end - start);
    if (hasBuffer) {
        std::memcpy(constants, source, 0xc0);
        std::memcpy(constants + 0xc0, state + 0xe08, 0x10);
        Store(constants, 0xd0, Load<std::uint32_t>(state, 0xcf0));
        Store(constants, 0xd4, Load<std::uint32_t>(state, 0xcf4));
        Store<std::uint64_t>(constants, 0xd8, 0);
    }
    GnmCommitRecord(command, hasBuffer, chain);
    if (hasBuffer) {
        StorePtr(block, 8, start);
        StorePtr(block, 0x30, start);
        Store(block, 0x22, static_cast<std::uint16_t>(bytes & 0xfffc));
        StorePtr(block, 0x28, constants);
        Store<std::uint16_t>(block, 0x20, 0xe0);
    }
    StorePtr(state, GnmStateConstants, constants + 0xe0);
}

std::uint8_t* GnmPrepareBlock(std::uint8_t* ctx, std::size_t slot, std::uint64_t kind, std::uint8_t* data) {
    std::uint8_t* const block = LoadPtr(ctx, slot);
    Store(block, 0, kind);
    std::memset(block + 0x20, 0, 0x20);
    StorePtr(block, 8, data);
    StorePtr(block, 0x10, data);
    StorePtr(block, 0x18, block);
    return block;
}

void GnmBeginRender(std::uint8_t* ctx, std::uint32_t flags) {
    const bool hasBuffer = Load<std::uint32_t>(ctx, 0) != 0;
    std::uint8_t* const command = ctx + 0x100;
    std::uint8_t* const aux = LoadPtr(ctx, 0x148);
    if (aux == nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    std::uint8_t* const binding = LoadPtr(aux, 0);
    std::uint8_t* const bound = LoadPtr(aux, 8);
    std::uint8_t* const stage = bound != nullptr ? bound + GnmStagePs : bound;
    GnmBindVsStage(bound, GnmShaderObject(GnmFetchShaderIndex), 0x88);
    GnmBindPsStage(bound, binding);
    if (LoadPtr(bound, 0x530) != nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    std::uint8_t* at = LoadPtr(bound, GnmStateConstants);
    if (LoadPtr(stage, 0x530) != nullptr) {
        at = AlignPtr(at, 4);
        StorePtr(stage, 0x558, at);
        StorePtr(stage, 0x538, at);
        at += Load<std::uint64_t>(stage, 0x540);
    }
    if (hasBuffer) {
        GnmStoreOnes(at, 4);
        std::memcpy(at + 0x10, binding + 0x2d8, 0x10);
    }
    StorePtr(bound, GnmStateConstants, at + 0x20);
    Store<std::uint32_t>(stage, 0x56c, 0);
    if (LoadPtr(stage, 0x560) != nullptr && Load<std::uint32_t>(stage, 0x568) >= 4) {
        StorePtr(stage, 0x578, at);
        Store<std::uint32_t>(stage, 0x56c, 0xc);
        if (Load<std::uint32_t>(stage, 0x568) >= 8) {
            StorePtr(stage, 0x588, at + 0x10);
            Store<std::uint32_t>(stage, 0x56c, 0xcc);
        }
    }
    if (LoadPtr(stage, 0x530) != nullptr) StorePtr(stage, 0x538, LoadPtr(stage, 0x558));
    if (hasBuffer) GnmCommitPsStage(stage, command + 8);
    else GnmCountPsStage(stage, command);
    std::uint8_t* const header = LoadPtr(ctx, 0x150);
    std::uint8_t* const state = LoadPtr(ctx, 0x158);
    if (LoadPtr(ctx, 0x90) != nullptr) GnmRecordVertexBlock(command, hasBuffer, state, LoadPtr(ctx, 0x90), 9);
    GnmRecordPsPointer(command, hasBuffer, state, GnmPrepareBlock(ctx, 0xc8, 0x1000000000060, LoadPtr(header, 0x218)), 0x81, 8);
    GnmRecordPsPointer(command, hasBuffer, state, GnmPrepareBlock(ctx, 0xd0, 0x1000000000062, LoadPtr(header, 0x360)), 0x81, 12);
    GnmRecordPsPointer(command, hasBuffer, state, GnmPrepareBlock(ctx, 0xb8, 0x1000000000061, LoadPtr(header, 0x358)), 0x81, 10);
    GnmRecordPsPointer(command, hasBuffer, state, GnmPrepareBlock(ctx, 0xc0, 0x1000000000061, LoadPtr(header, 0x358)), 0x81, 10);
    GnmRecordVsBuffer(command, hasBuffer, state, GnmPrepareBlock(ctx, 0xa0, 0x1000000000050, header + 0x1a8), 0x81);
    GnmRecordVsBuffer(command, hasBuffer, state, GnmPrepareBlock(ctx, 0x98, 0x1000000000050, header + 0x1d8), 0x81);
    GnmRecordVsBuffer(command, hasBuffer, state, GnmPrepareBlock(ctx, 0xa8, 0x1000000000050, header + 0x178), 0x81);
    const std::uint8_t mode = static_cast<std::uint8_t>(flags);
    const std::size_t blend = mode == 0x4a ? 0x3a0 : mode == 0x40 ? 0x390 : 0x3a8;
    GnmRecordBlend(command, hasBuffer, state, GnmPrepareBlock(ctx, 0xd8, 0x1000000000063, LoadPtr(header, blend)), 0x81, false);
    GnmRecordBlend(command, hasBuffer, state, GnmPrepareBlock(ctx, 0xe0, 0x1000000000063, LoadPtr(header, blend)), 0x81, true);
    if (hasBuffer) GnmEmit(command + 8, {0xc0004200, 0});
    else GnmCount(command, 2);
}

int GnmBeginSequence(std::uint8_t* ctx, std::uint8_t* instance, std::uint8_t* param, std::uint32_t flags) {
    const bool hasBuffer = Load<std::uint32_t>(ctx, 0) != 0;
    std::uint32_t bytes = 0x3ffffc;
    std::uint32_t constantBytes = 0x10000000;
    std::uint8_t* buffer = nullptr;
    std::uint8_t* constants = nullptr;
    if (hasBuffer) {
        bytes = Load<std::uint32_t>(ctx, 0x10);
        constantBytes = Load<std::uint32_t>(ctx, 0x14);
        buffer = LoadPtr(ctx, 0x18);
        constants = LoadPtr(ctx, 0x20);
    } else {
        Store<std::uint64_t>(ctx, 0xc, 0);
        Store<std::uint32_t>(ctx, 0x14, 0);
    }
    std::uint8_t* const header = LoadPtr(instance, 8);
    std::uint8_t* const command = ctx + 0x100;
    std::uint8_t* const state = ctx + 0x1f0;
    StorePtr(state, 0xcb0, command);
    StorePtr(ctx, 0x150, header);
    std::memset(ctx + 0x158, 0, 0x10);
    StorePtr(ctx, 0x168, buffer);
    std::memset(ctx + 0x170, 0, 0x10);
    GnmCbInit(ctx + 0x180, nullptr, 0);
    std::memset(ctx + 0x1c0, 0, 0x30);
    StorePtr(ctx, 0x158, state);
    Store<std::uint64_t>(command, 0, 0);
    GnmCbInit(command + 8, buffer, bytes);
    StorePtr(command, 0x48, ctx + 0x150);
    Store<std::uint64_t>(state, 0x6e8, 0);
    Store<std::uint64_t>(state, 0xca0, 0);
    StorePtr(state, 0xca8, state + 0x6d8);
    Store<std::uint64_t>(state, 0xcd0, 0);
    Store<std::uint32_t>(state, 0xd50, 0);
    Store<std::uint64_t>(state, 0xd48, 0);
    std::memset(state + 0xce0, 0, 0x10);
    std::memset(state + 0xcf8, 0, 0x10);
    std::memset(state + 0xd68, 0, 0x10);
    Store<std::uint64_t>(state, 0xd78, 0);
    GnmStoreOnes(state + 0xe18, 8);
    Store<std::uint16_t>(state, 0xe38, 8);
    Store<std::uint64_t>(state, 0xe40, 0x3f8000003f800000);
    Store<std::uint32_t>(state, 0xe48, 0);
    GnmStoreOnes(state + 0xe50, 4);
    std::memset(state + 0xe60, 0, 0x10);
    Store<std::uint64_t>(state, 0xe70, 0);
    Store<std::uint64_t>(state, 0xe78, 0x3f8000003f800000);
    Store<std::uint32_t>(state, 0xe80, 0x3f800000);
    StorePtr(state, 0xcb0, command);
    StorePtr(state, 0xcb8, constants);
    StorePtr(state, 0xcc0, constants + constantBytes);
    StorePtr(state, GnmStateConstants, constants);
    Store<std::uint64_t>(state, 0xcd8, 0x4487000044f00000);
    StorePtr(state, 0xe88, header + 0x2c8);
    if (flags == 0x200) return 0;
    Store<std::uint32_t>(ctx, 0x54, 0);
    std::memset(ctx + 0x64, 0, 0x1c);
    const std::uint32_t mode = flags & 0xff;
    std::memset(state + 0xd58, 0, 0x10);
    GnmStoreOnes(state + 0xe08, 4);
    Store(state, 0xd68, mode);
    Store(state, 0xd48, Load<std::uint64_t>(header, 0x3b8));
    Store(state, 0xcf0, Load<std::uint64_t>(ctx, 0x28));
    std::uint8_t* const cb = command + 8;
    std::uint8_t* base = nullptr;
    const std::uint32_t previous = Load<std::uint32_t>(ctx, 0xc);
    if (hasBuffer) {
        GnmOpenChain(cb);
        base = GnmCbCur(cb);
        std::uint8_t* resume = base;
        if (previous != 0) {
            resume = base + ((static_cast<std::uintptr_t>(previous) + 3) & ~std::uintptr_t{3});
            StorePtr(cb, GnmCbCursor, resume);
        }
        StorePtr(LoadPtr(ctx, 0x148), 0x70, resume);
    } else {
        GnmCount(command, GnmLoopDwords(cb));
        base = GnmSyncReserved(command);
        if (previous != 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    }
    StorePtr(ctx, 0x1c8, base);
    StorePtr(ctx, 0x1d0, base + previous);
    StorePtr(ctx, 0x1d8, base);
    if (param != nullptr) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    Store<std::uint16_t>(ctx, 6, 1);
    StorePtr(state, 0xce0, LoadPtr(header, 0x328));
    Store<std::uint64_t>(state, 0xce8, 0);
    Store(state, 0xd68, mode == 0x10 ? 1u : mode);
    if ((flags & 0x100) == 0) NotImplemented_nid_no_patch("sceFontSelectGraphicsGnm_backend_4700002d");
    GnmBeginRender(ctx, flags);
    return 0;
}

int APS5_VABI GnmBackendBeginSequence(void* ctx, void* instance, void* param, std::uint32_t flags) {
    return GnmBeginSequence(static_cast<std::uint8_t*>(ctx), static_cast<std::uint8_t*>(instance), static_cast<std::uint8_t*>(param), flags);
}

std::uint8_t* GnmBlockRoot(std::uint8_t* block) {
    for (;;) {
        std::uint8_t* const next = LoadPtr(block, 0x18);
        if (next == nullptr || next == block) return block;
        block = next;
    }
}

int APS5_VABI GnmBackendCopyDrawState(void* ctx, void* param) {
    const std::uint8_t* const frame = LoadPtr(LoadPtr(ctx, 0x158), 0xd78);
    auto* const out = static_cast<std::uint8_t*>(param);
    std::memcpy(out + 0x08, frame + 0x28, 0x80);
    std::memcpy(out + 0x88, frame + 0x118, 0x40);
    std::memcpy(out + 0xc8, frame + 0x178, 0x40);
    std::memcpy(out + 0x110, frame + 0x228, 0x40);
    StorePtr(out, 0x108, GnmBlockRoot(LoadPtr(frame, 0x170)));
    StorePtr(out, 0x150, GnmBlockRoot(LoadPtr(frame, 0x220)));
    std::memcpy(out + 0x158, frame + 0xe8, 0x18);
    constexpr std::size_t Roots[7][2] = {{0x170, 0xd0}, {0x178, 0x110}, {0x198, 0x158}, {0x1a0, 0x160}, {0x180, 0x200}, {0x188, 0x1f8}, {0x190, 0x2a8}};
    for (const auto& [to, from] : Roots) StorePtr(out, to, GnmBlockRoot(LoadPtr(frame, from)));
    return 0;
}

int APS5_VABI GnmBackendPopDrawState(void* ctx, void* param) {
    const std::uint32_t depth = Load<std::uint32_t>(ctx, 0x54);
    if (depth == 0) return FontGsFatal;
    std::uint8_t* const state = LoadPtr(ctx, 0x158);
    Store(ctx, 0x54, depth - 1);
    const std::uint8_t* const shader = LoadPtr(param, 8);
    StorePtr(state, 0xd78, LoadPtr(LoadPtr(state, 0xd78), 0));
    StorePtr(state, 0xd70, shader);
    if (shader != nullptr) Store<std::uint32_t>(state, 0xd68, shader[2]);
    return 0;
}

int APS5_VABI GnmBackendCallCommands(void* ctx, void* param) {
    GnmCallBlock(static_cast<std::uint8_t*>(ctx), LoadPtr(param, 0x28), Load<std::uint32_t>(param, 0x30) >> 2);
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
    {{0x47000001, 0, reinterpret_cast<std::uint64_t>(&GnmBackendInit)},
     {0x47000002, 0, reinterpret_cast<std::uint64_t>(&GnmBackendTerminate)},
     {0x4700002d, 0, reinterpret_cast<std::uint64_t>(&GnmBackendBeginSequence)},
     {0x4700002e, 0, reinterpret_cast<std::uint64_t>(&GnmBackendEndSequence)},
     GnmUnportedEntry<0x470000c0>(),
     GnmUnportedEntry<0x470000cb>(),
     GnmUnportedEntry<0x470000ce>(),
     GnmUnportedEntry<0x470000d1>(),
     GnmUnportedEntry<0x470000d2>(),
     GnmUnportedEntry<0x470000d4>(),
     GnmUnportedEntry<0x470000e0>(),
     GnmUnportedEntry<0x470000e1>(),
     {0x470000ec, 0, reinterpret_cast<std::uint64_t>(&GnmBackendPopDrawState)},
     GnmUnportedEntry<0x470000d0>(),
     {0x470000e2, 0, reinterpret_cast<std::uint64_t>(&GnmBackendCopyDrawState)},
     GnmUnportedEntry<0x470000e3>(),
     {0x470000f0, 0, reinterpret_cast<std::uint64_t>(&GnmBackendCallCommands)},
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
    *output = nullptr;
    if (graphicsObject == nullptr || surface == nullptr) return FontGsInvalidParameter;
    const auto* const gnm = ReadGnmSurface(surface);
    if (gnm == nullptr || !(x >= 0.0f)) return FontGsInvalidParameter;
    const float right = x + width;
    if (!(right >= x) || !(right <= FontGnmDrawupLimit) || !(y >= 0.0f)) return FontGsInvalidParameter;
    const float bottom = y + height;
    if (!(bottom >= y) || !(bottom <= FontGnmDrawupLimit)) return FontGsInvalidParameter;
    const int error = sceFontGraphicsDrawupFillTextureImageObject(graphicsObject, gnm->fillObject, gnm->mode, output, x, y, width, height);
    if (error != 0) return error;
    MarkGnmRecord(*output);
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

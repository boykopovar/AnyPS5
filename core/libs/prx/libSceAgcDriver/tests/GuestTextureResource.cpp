#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using Shape = ShaderRecompiler::DescriptorImageShape;

struct Fields {
    std::uint64_t base40 = 0x123456ull;
    std::uint32_t minLod = 0;
    std::uint32_t format = 56;
    std::uint32_t width = 16;
    std::uint32_t height = 16;
    std::uint32_t dstSelX = 4;
    std::uint32_t dstSelY = 5;
    std::uint32_t dstSelZ = 6;
    std::uint32_t dstSelW = 7;
    std::uint32_t baseLevel = 0;
    std::uint32_t lastLevel = 0;
    std::uint32_t tileModeRaw = 0x00;
    std::uint32_t bcSwizzle = 0;
    std::uint32_t typeRaw = 9;
    std::uint32_t depth = 0;
    std::uint32_t baseArray = 0;
    std::uint32_t arrayPitch = 0;
    std::uint32_t maxMip = 0;
    std::uint32_t minLodWarn = 0;
    std::uint32_t perfMod = 0;
    bool cornerSample = false;
    bool mipStatsCntEn = false;
    bool prtDefColor = false;
    std::uint32_t mipStatsCntId = 0;
    bool msaaDepth = false;
    std::uint32_t maxUncompBlkSize = 0;
    std::uint32_t maxCompBlkSize = 0;
    bool metaPipeAligned = false;
    bool writeCompress = false;
    bool metaCompress = false;
    bool dccAlphaPos = false;
    bool dccColorTransf = false;
    std::uint64_t metaAddr = 0;
};

std::array<std::uint32_t, 8> pack(const Fields& f) {
    std::array<std::uint32_t, 8> words{};
    const auto widthMinus1 = f.width - 1u;
    const auto heightMinus1 = f.height - 1u;
    words[0] = static_cast<std::uint32_t>(f.base40 & 0xffffffffull);
    words[1] = static_cast<std::uint32_t>((f.base40 >> 32u) & 0xffull)
        | ((f.minLod & 0xfffu) << 8u)
        | ((f.format & 0x1ffu) << 20u)
        | ((widthMinus1 & 0x3u) << 30u);
    words[2] = ((widthMinus1 >> 2u) & 0xfffu) | ((heightMinus1 & 0x3fffu) << 14u);
    words[3] = (f.dstSelX & 0x7u) | ((f.dstSelY & 0x7u) << 3u) | ((f.dstSelZ & 0x7u) << 6u) | ((f.dstSelW & 0x7u) << 9u)
        | ((f.baseLevel & 0xfu) << 12u) | ((f.lastLevel & 0xfu) << 16u) | ((f.tileModeRaw & 0x1fu) << 20u)
        | ((f.bcSwizzle & 0x7u) << 25u) | ((f.typeRaw & 0xfu) << 28u);
    words[4] = (f.depth & 0x1fffu) | ((f.baseArray & 0x1fffu) << 16u);
    words[5] = (f.arrayPitch & 0xfu) | ((f.maxMip & 0xfu) << 4u) | ((f.minLodWarn & 0xfffu) << 8u)
        | ((f.perfMod & 0x7u) << 20u) | ((f.cornerSample ? 1u : 0u) << 23u) | ((f.mipStatsCntEn ? 1u : 0u) << 25u)
        | ((f.prtDefColor ? 1u : 0u) << 26u);
    words[6] = (f.mipStatsCntId & 0xffu) | ((f.msaaDepth ? 1u : 0u) << 10u) | ((f.maxUncompBlkSize & 0x3u) << 15u)
        | ((f.maxCompBlkSize & 0x3u) << 17u) | ((f.metaPipeAligned ? 1u : 0u) << 19u) | ((f.writeCompress ? 1u : 0u) << 20u)
        | ((f.metaCompress ? 1u : 0u) << 21u) | ((f.dccAlphaPos ? 1u : 0u) << 22u) | ((f.dccColorTransf ? 1u : 0u) << 23u)
        | static_cast<std::uint32_t>((f.metaAddr & 0xffull) << 24u);
    words[7] = static_cast<std::uint32_t>(f.metaAddr >> 8u);
    return words;
}

template<typename TAction>
void reject(TAction action, std::string_view reason) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(std::string_view(error.what()).find(reason) != std::string_view::npos, std::string("unexpected guest texture test error: ") + error.what());
        return;
    }
    throw std::runtime_error(std::string("expected guest texture rejection: ") + std::string(reason));
}

void rejectFields(const Fields& f, std::string_view reason) {
    const auto words = pack(f);
    reject([&] { DecodeTextureResource(words); }, reason);
}

}

void RunGuestTextureResourceTests() {
    Fields base;
    auto result = DecodeTextureResource(pack(base));
    Require(result.baseAddress == (base.base40 << 8u), "decoded base address changed");
    Require(result.width == 16 && result.height == 16, "decoded width or height changed");
    Require(result.depthOrLastArray == 0 && result.baseArray == 0, "decoded depth or base array changed");
    Require(result.mipCount == 1 && result.baseLevel == 0, "decoded mip count or base level changed");
    Require(result.tileMode == TextureTileMode::kLinear, "decoded tile mode changed");
    Require(result.dimension == TextureDimension::k2D, "decoded dimension changed");
    Require(result.format == 56, "decoded format changed");
    Require(result.dstSelX == 4 && result.dstSelY == 5 && result.dstSelZ == 6 && result.dstSelW == 7, "decoded destination selectors changed");

    Fields wide = base;
    wide.width = 8192;
    wide.height = 4096;
    result = DecodeTextureResource(pack(wide));
    Require(result.width == 8192 && result.height == 4096, "wide texture dimensions were split across dwords incorrectly");

    Fields tileModes = base;
    tileModes.tileModeRaw = 0x01;
    Require(DecodeTextureResource(pack(tileModes)).tileMode == TextureTileMode::kStandard256B, "tile mode 0x01 must decode to standard 256B");
    tileModes.tileModeRaw = 0x05;
    Require(DecodeTextureResource(pack(tileModes)).tileMode == TextureTileMode::kStandard4KB, "tile mode 0x05 must decode to standard 4KB");
    tileModes.tileModeRaw = 0x09;
    Require(DecodeTextureResource(pack(tileModes)).tileMode == TextureTileMode::kStandard64KB, "tile mode 0x09 must decode to standard 64KB");
    tileModes.tileModeRaw = 0x1b;
    rejectFields(tileModes, "pipe/bank XOR base");
    tileModes.base40 = 0x120000ull;
    Require(DecodeTextureResource(pack(tileModes)).tileMode == TextureTileMode::RenderTarget64KB, "tile mode 0x1b must decode to render target 64KB");
    constexpr std::array<std::pair<std::uint32_t, TextureTileMode>, 7> added{{{0x02, TextureTileMode::kD256B}, {0x06, TextureTileMode::kD4KB}, {0x0a, TextureTileMode::kD64KB}, {0x11, TextureTileMode::kS64KBT}, {0x12, TextureTileMode::kD64KBT}, {0x15, TextureTileMode::kS4KBX}, {0x16, TextureTileMode::kD4KBX}}};
    for (const auto& [raw, mode] : added) {
        tileModes.tileModeRaw = raw;
        Require(DecodeTextureResource(pack(tileModes)).tileMode == mode, "tile mode " + std::to_string(raw) + " decoded to the wrong swizzle");
    }
    tileModes.tileModeRaw = 0x16;
    tileModes.base40 = 0x120010ull;
    Require(DecodeTextureResource(pack(tileModes)).tileMode == TextureTileMode::kD4KBX, "a 4 KiB XOR swizzle must accept a 4 KiB aligned base");
    tileModes.base40 = 0x120011ull;
    rejectFields(tileModes, "pipe/bank XOR base");
    tileModes.tileModeRaw = 0x12;
    tileModes.base40 = 0x120010ull;
    rejectFields(tileModes, "pipe/bank XOR base");
    tileModes.base40 = base.base40;
    tileModes.tileModeRaw = 0x03;
    rejectFields(tileModes, "unsupported tile mode");

    Fields oneD = base;
    oneD.typeRaw = 8;
    oneD.height = 1;
    result = DecodeTextureResource(pack(oneD));
    Require(result.dimension == TextureDimension::k1D, "1D descriptor did not decode to 1D dimension");
    oneD.height = 2;
    rejectFields(oneD, "nonzero height, depth or base array");

    Fields twoD = base;
    twoD.depth = 1;
    rejectFields(twoD, "nonzero depth or base array");
    twoD = base;
    twoD.baseArray = 1;
    rejectFields(twoD, "nonzero depth or base array");

    Fields array = base;
    array.typeRaw = 13;
    array.depth = 3;
    array.baseArray = 1;
    result = DecodeTextureResource(pack(array));
    Require(result.dimension == TextureDimension::k2DArray && result.depthOrLastArray == 3 && result.baseArray == 1, "2D array descriptor decoded incorrectly");
    array.baseArray = 5;
    rejectFields(array, "base array past its last array slice");

    Fields oneDArray = base;
    oneDArray.typeRaw = 12;
    oneDArray.height = 1;
    oneDArray.depth = 3;
    oneDArray.baseArray = 2;
    result = DecodeTextureResource(pack(oneDArray));
    Require(result.dimension == TextureDimension::k1DArray && result.height == 1 && result.depthOrLastArray == 3 && result.baseArray == 2, "1D array descriptor decoded incorrectly");
    Require(DescribeSurface(result).layers == 4, "1D array surface does not hold every array slice");
    oneDArray.baseArray = 4;
    rejectFields(oneDArray, "1D array texture descriptor has a base array past its last array slice");
    oneDArray.baseArray = 0;
    oneDArray.height = 2;
    rejectFields(oneDArray, "1D array texture descriptor has a nonzero height");
    oneDArray.height = 1;
    oneDArray.base40 = 0x120000ull;
    oneDArray.tileModeRaw = 0x18;
    Require(DecodeTextureResource(pack(oneDArray)).tileMode == TextureTileMode::kZ64KBX, "1D array in SW_64KB_Z_X decoded incorrectly");
    oneDArray.tileModeRaw = 0x1b;
    Require(DecodeTextureResource(pack(oneDArray)).tileMode == TextureTileMode::kR64KBX, "1D array in SW_64KB_R_X decoded incorrectly");
    oneDArray.tileModeRaw = 0x05;
    rejectFields(oneDArray, "tile mode other than linear, Z or R");
    oneDArray.tileModeRaw = 0x19;
    rejectFields(oneDArray, "tile mode other than linear, Z or R");

    Fields cube = base;
    cube.typeRaw = 11;
    cube.width = 32;
    cube.height = 32;
    cube.depth = 5;
    cube.baseArray = 0;
    result = DecodeTextureResource(pack(cube));
    Require(result.dimension == TextureDimension::kCube, "cube descriptor did not decode to cube dimension");
    for (const auto first : {0u, 5u, 6u}) {
        auto singleCube = cube;
        singleCube.baseArray = first;
        singleCube.depth = first;
        result = DecodeTextureResource(pack(singleCube));
        Require(result.baseArray == first && result.depthOrLastArray == first + 5u, "a single cube descriptor did not expose six faces");
        const auto geometry = DescribeSurface(result);
        Require(geometry.imageLayers == first + 6u, "a single cube allocation omitted faces");
    }
    Fields cubeNotSquare = cube;
    cubeNotSquare.height = 16;
    rejectFields(cubeNotSquare, "not square");
    Fields cubeBadArray = cube;
    cubeBadArray.baseArray = 6;
    rejectFields(cubeBadArray, "base array past its last array slice");
    Fields cubeNotMultiple = cube;
    cubeNotMultiple.depth = 4;
    rejectFields(cubeNotMultiple, "multiple of 6");

    Fields badType = base;
    badType.typeRaw = 0;
    rejectFields(badType, "unsupported image type");

    Fields badSel = base;
    badSel.dstSelX = 2;
    rejectFields(badSel, "invalid destination channel selector");
    badSel = base;
    badSel.dstSelW = 3;
    rejectFields(badSel, "invalid destination channel selector");

    Fields zeroAddress = base;
    zeroAddress.base40 = 0;
    rejectFields(zeroAddress, "null base address");

    Fields minLod = base;
    minLod.maxMip = 4;
    minLod.baseLevel = 1;
    minLod.lastLevel = 3;
    minLod.minLod = 0x180;
    auto clamped = DecodeTextureResource(pack(minLod));
    Require(clamped.minLod == 0x180 && EffectiveMinLod(clamped) == 1.5f, "MIN_LOD 1.5 over levels 1..3 did not clamp at 1.5");
    minLod.minLod = 0x100;
    clamped = DecodeTextureResource(pack(minLod));
    Require(EffectiveMinLod(clamped) == 0.0f, "MIN_LOD at BASE_LEVEL cannot bind but was applied");
    minLod.minLod = 0x0c0;
    clamped = DecodeTextureResource(pack(minLod));
    Require(EffectiveMinLod(clamped) == 0.0f, "MIN_LOD below BASE_LEVEL cannot bind but was applied");
    minLod.minLod = 0xfff;
    clamped = DecodeTextureResource(pack(minLod));
    Require(EffectiveMinLod(clamped) == 3.0f, "MIN_LOD past the view's last level was not bounded by it");
    minLod.baseLevel = 0;
    minLod.minLod = 0x001;
    clamped = DecodeTextureResource(pack(minLod));
    Require(EffectiveMinLod(clamped) == 1.0f / 256.0f, "the smallest MIN_LOD step above level 0 was lost");
    Require(DecodeTextureResource(pack(base)).minLod == 0 && EffectiveMinLod(DecodeTextureResource(pack(base))) == 0.0f, "an unclamped descriptor gained a MIN_LOD");

    // Streaming feedback fields decode; nothing is reported back.
    Fields feedback = base;
    feedback.minLodWarn = 1;
    feedback.mipStatsCntId = 1;
    feedback.mipStatsCntEn = true;
    static_cast<void>(DecodeTextureResource(pack(feedback)));

    const auto unmodulated = DecodeTextureResource(pack(base));
    for (std::uint32_t perfMod = 0; perfMod < 8; ++perfMod) {
        Fields modulated = base;
        modulated.perfMod = perfMod;
        const auto decoded = DecodeTextureResource(pack(modulated));
        Require(decoded.baseAddress == unmodulated.baseAddress && decoded.width == unmodulated.width && decoded.height == unmodulated.height, "performance modulation changed texture storage");
        Require(decoded.depthOrLastArray == unmodulated.depthOrLastArray && decoded.baseArray == unmodulated.baseArray && decoded.mipCount == unmodulated.mipCount && decoded.baseLevel == unmodulated.baseLevel, "performance modulation changed texture subresources");
        Require(decoded.tileMode == unmodulated.tileMode && decoded.dimension == unmodulated.dimension && decoded.format == unmodulated.format, "performance modulation changed texture format or layout");
        Require(decoded.dstSelX == unmodulated.dstSelX && decoded.dstSelY == unmodulated.dstSelY && decoded.dstSelZ == unmodulated.dstSelZ && decoded.dstSelW == unmodulated.dstSelW, "performance modulation changed texture channel selectors");
        modulated.cornerSample = true;
        rejectFields(modulated, "corner sampling");
    }

    Fields badCorner = base;
    badCorner.cornerSample = true;
    rejectFields(badCorner, "corner sampling");

    Fields badPrt = base;
    badPrt.prtDefColor = true;
    rejectFields(badPrt, "partially resident default color");

    Fields badPitch = base;
    badPitch.arrayPitch = 1;
    rejectFields(badPitch, "nonzero array pitch");

    Fields badMsaa = base;
    badMsaa.msaaDepth = true;
    rejectFields(badMsaa, "MSAA");

    Fields blockSize = base;
    blockSize.maxUncompBlkSize = 1;
    blockSize.maxCompBlkSize = 1;
    Require(DecodeTextureResource(pack(blockSize)).baseAddress == DecodeTextureResource(pack(base)).baseAddress, "DCC block size overrides changed texture storage");

    Fields meta = base;
    meta.metaPipeAligned = true;
    meta.writeCompress = true;
    meta.dccColorTransf = true;
    meta.metaAddr = 1;
    const auto uncompressed = DecodeTextureResource(pack(meta));
    Require(uncompressed.dccAddress == 0 && !uncompressed.dccAlphaOnMsb, "DCC metadata without compression was decoded");
    meta.metaCompress = true;
    meta.dccAlphaPos = true;
    const auto compressed = DecodeTextureResource(pack(meta));
    Require(compressed.dccAddress == 0x100 && compressed.dccAlphaOnMsb, "DCC metadata was decoded wrongly");

    Fields storage = base;
    storage.base40 = 0x1000;
    storage.width = 1920;
    storage.height = 1080;
    storage.tileModeRaw = 27;
    storage.metaCompress = true;
    constexpr std::uint64_t storageBytes = 15u * 9u * 65536u;
    constexpr std::size_t extentKeys = 49152;
    std::vector<std::uint8_t> storageKeys(extentKeys + 256);
    const auto keysAddress = (reinterpret_cast<std::uintptr_t>(storageKeys.data()) + 255u) / 256u * 256u;
    const auto* keys = reinterpret_cast<const std::uint8_t*>(keysAddress);
    storage.metaAddr = keysAddress >> 8u;
    for (const bool pipeAligned : {true, false}) {
        storage.metaPipeAligned = pipeAligned;
        const auto image = DecodeTextureResource(pack(storage));
        const auto expected = pipeAligned ? extentKeys : static_cast<std::size_t>(storageBytes / 256u);
        std::fill(storageKeys.begin(), storageKeys.end(), std::uint8_t{0x00});
        MarkDccUncompressed(image.dccAddress, storageBytes, DccKeyCount(image, storageBytes));
        const auto stored = static_cast<std::size_t>(std::count(keys, keys + extentKeys, std::uint8_t{0xff}));
        Require(image.dccPipeAligned == pipeAligned && stored == expected && std::all_of(keys, keys + expected, [](std::uint8_t key) { return key == 0xff; }), std::string("a 1920x1080 SW_64KB_R_X storage image with ") + (pipeAligned ? "pipe-aligned" : "unaligned") + " DCC stored " + std::to_string(stored) + " uncompressed keys, expected " + std::to_string(expected));
    }

    Fields badSwizzle = base;
    badSwizzle.bcSwizzle = 1;
    rejectFields(badSwizzle, "BC swizzle");

    Fields badLevels = base;
    badLevels.baseLevel = 2;
    badLevels.lastLevel = 1;
    badLevels.maxMip = 1;
    rejectFields(badLevels, "base mip level past its last mip level");

    Fields partialMips = base;
    partialMips.lastLevel = 1;
    partialMips.maxMip = 2;
    const auto partial = DecodeTextureResource(pack(partialMips));
    Require(partial.lastLevel == 1 && partial.mipCount == 3, "a view over part of the mip chain decoded wrongly");

    Fields pastLast = base;
    pastLast.base40 = 0x55ea000ull;
    pastLast.format = 71;
    pastLast.width = 1920;
    pastLast.height = 1080;
    pastLast.tileModeRaw = 0x1b;
    pastLast.maxMip = 5;
    pastLast.baseLevel = 6;
    pastLast.lastLevel = 6;
    const auto tailView = DecodeTextureResource(pack(pastLast));
    Require(tailView.baseLevel == 6 && tailView.lastLevel == 6 && tailView.mipCount == 7, "a view one level past the last mip must address that level of the chain");
    auto allocated = tailView;
    allocated.mipCount = 6;
    const auto allocatedSurface = DescribeSurface(allocated);
    const auto viewSurface = DescribeSurface(tailView);
    Require(allocatedSurface.guestBytes == 0x1640000u && viewSurface.guestBytes == 0x1640000u, "a 1920x1080 64 bpp SW_64KB_R_X chain must take addrlib's 0x1640000 bytes with 6 and with 7 levels");
    constexpr std::array<std::uint64_t, 7> addrlibOffsets{0x650000u, 0x1d0000u, 0x90000u, 0x30000u, 0x10000u, 0u, 0u};
    for (std::uint32_t level = 0; level < 7; ++level) {
        Require(viewSurface.mips[level].tiledOffset == addrlibOffsets[level] && viewSurface.mips[level].tail == (level >= 5), "the 7-level chain must place each level at its addrlib offset");
        if (level < 6) Require(allocatedSurface.mips[level].tiledOffset == addrlibOffsets[level] && allocatedSurface.mips[level].tail == viewSurface.mips[level].tail && allocatedSurface.mips[level].tailX == viewSurface.mips[level].tailX && allocatedSurface.mips[level].tailY == viewSurface.mips[level].tailY, "the level past the last mip must not move the allocated levels");
    }
    Require(viewSurface.mips[5].tailX == 64 && viewSurface.mips[5].tailY == 0, "the last allocated level must sit in its addrlib tail slot");
    Require(viewSurface.mips[6].tailX == 0 && viewSurface.mips[6].tailY == 32, "the level past the last mip must sit in its addrlib tail slot");
    pastLast.lastLevel = 8;
    const auto deeperView = DecodeTextureResource(pack(pastLast));
    Require(deeperView.mipCount == 9, "a view past the last mip must cover every level it names");
    const auto deeperSurface = DescribeSurface(deeperView);
    Require(deeperSurface.guestBytes == 0x1640000u && deeperSurface.mips[7].tail && deeperSurface.mips[7].tailX == 32 && deeperSurface.mips[7].tailY == 0 && deeperSurface.mips[8].tail && deeperSurface.mips[8].tailX == 0 && deeperSurface.mips[8].tailY == 16, "levels 7 and 8 must sit in their addrlib tail slots");

    Fields pastLinear = base;
    pastLinear.maxMip = 1;
    pastLinear.baseLevel = 2;
    pastLinear.lastLevel = 2;
    rejectFields(pastLinear, "would move the surface's own");
    auto linearChain = partial;
    linearChain.mipCount = 2;
    const auto linearAllocated = DescribeSurface(linearChain).guestBytes;
    linearChain.mipCount = 3;
    Require(linearAllocated == 0x1800u && DescribeSurface(linearChain).guestBytes == 0x1c00u, "a third level must grow a 16x16 32 bpp linear chain from addrlib's 0x1800 to 0x1c00 bytes");

    Fields pastUntailed = pastLast;
    pastUntailed.maxMip = 0;
    pastUntailed.baseLevel = 1;
    pastUntailed.lastLevel = 1;
    rejectFields(pastUntailed, "would move the surface's own");
    auto untailedChain = tailView;
    untailedChain.mipCount = 1;
    const auto untailedAllocated = DescribeSurface(untailedChain).guestBytes;
    untailedChain.mipCount = 2;
    Require(untailedAllocated == 0xff0000u && DescribeSurface(untailedChain).guestBytes == 0x1470000u, "a second level must grow a 1920x1080 64 bpp SW_64KB_R_X surface from addrlib's 0xff0000 to 0x1470000 bytes");

    std::array<std::uint32_t, 4> shortWords{};
    reject([&] { DecodeTextureResource(shortWords); }, "8 dwords");

    Require(MatchesGuestDimension(Shape::Image1D, TextureDimension::k1D), "1D shape must match 1D dimension");
    Require(!MatchesGuestDimension(Shape::Image1D, TextureDimension::k2D), "1D shape must not match 2D dimension");
    Require(MatchesGuestDimension(Shape::Image2D, TextureDimension::k2D), "2D shape must match 2D dimension");
    Require(!MatchesGuestDimension(Shape::Image2D, TextureDimension::k2DArray), "2D shape must not match 2D array dimension");
    Require(MatchesGuestDimension(Shape::Image2DArray, TextureDimension::k2DArray), "2D array shape must match 2D array dimension");
    Require(MatchesGuestDimension(Shape::Image2DArray, TextureDimension::kCube), "a cube must be readable as a 2D array of its faces");
    Require(MatchesGuestDimension(Shape::ImageCube, TextureDimension::kCube), "cube shape must match cube dimension");
    Require(!MatchesGuestDimension(Shape::ImageCube, TextureDimension::k1D), "cube shape must not match 1D dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::k1D), "3D shape must never match a guest dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::k2D), "3D shape must never match a guest dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::k2DArray), "3D shape must never match a guest dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::kCube), "3D shape must never match a guest dimension");
}

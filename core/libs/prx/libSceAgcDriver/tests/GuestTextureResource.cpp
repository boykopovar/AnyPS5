#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
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

static void RunR128TextureTests() {
    constexpr std::array<std::uint32_t, 8> streamed{0xa0c18800u, 0xcad00000u, 0x01ffc1ffu, 0x905b2facu, 0u, 0u, 0u, 0u};
    const auto view = DecodeTextureResource(streamed);
    Require(view.baseAddress == 0xa0c1880000ull && view.format == 173u && view.tileMode == TextureTileMode::kStandard4KB && view.width == 512u && view.height == 512u && view.baseLevel == 0u && view.lastLevel == 9u && view.mipCount == 10u && view.allocatedMipCount == 10u && view.minLod == 0u,
        "a 128-bit BC3 T# (2048x2048, levels 2..11, words 4-7 zero) must read as the 512x512 chain of its 10 resident levels, got " + std::to_string(view.width) + "x" + std::to_string(view.height) + " levels " + std::to_string(view.baseLevel) + ".." + std::to_string(view.lastLevel) + " of " + std::to_string(view.mipCount));
    auto chainWords = streamed;
    chainWords[3] &= ~(0xfu << 12u);
    chainWords[5] = 11u << 4u;
    const auto full = DecodeTextureResource(chainWords);
    Require(full.width == 2048u && full.mipCount == 12u, "the same surface with MAX_MIP 11 must be the 12-level 2048x2048 chain");
    const auto chain = DescribeSurface(full);
    const auto resident = DescribeSurface(view);
    Require(chain.mips.size() == 12u && resident.mips.size() == 10u, "the chain and the resident levels must have 12 and 10 levels");
    for (std::uint32_t level = 0; level < 10u; ++level) {
        const auto& a = chain.mips[level + 2u];
        const auto& b = resident.mips[level];
        Require(a.tiledOffset == b.tiledOffset && a.tiledSize == b.tiledSize && a.width == b.width && a.height == b.height && a.blocksPerRow == b.blocksPerRow && a.tail == b.tail && a.tailX == b.tailX && a.tailY == b.tailY, "resident level " + std::to_string(level) + " must be level " + std::to_string(level + 2u) + " of the 12-level chain");
    }
    Require(resident.guestBytes <= chain.mips[1].tiledOffset && chain.mips[0].tiledOffset > chain.mips[1].tiledOffset, "the resident levels must end before the chain's levels 1 and 0, which come last in a GFX10 swizzled chain");
    auto levelZero = streamed;
    levelZero[3] &= ~(0xfu << 12u);
    const auto single = DecodeTextureResource(levelZero);
    Require(single.mipCount == 1u && single.lastLevel == 0u, "a T# with words 4-7 zero that starts at level 0 must stay one level");
    auto tailed = streamed;
    tailed[5] = 4u << 20u;
    reject([&] { DecodeTextureResource(tailed); }, "would move the surface's own");

    constexpr std::array<std::uint32_t, 8> tall{0xa0c1de00u, 0xcad00000u, 0x01ffc0ffu, 0x905b4facu, 0u, 0u, 0u, 0u};
    const auto tallView = DecodeTextureResource(tall);
    Require(tallView.width == 64u && tallView.height == 128u && tallView.lastLevel == 7u && tallView.mipCount == 8u, "a 128-bit 1024x2048 T# of levels 4..11 must read as its 64x128 chain of 8 levels");

    Fields clamped;
    clamped.base40 = 0x55ea000ull;
    clamped.format = 20;
    clamped.width = 1024;
    clamped.height = 1024;
    clamped.tileModeRaw = 0x1b;
    clamped.baseLevel = 2;
    clamped.lastLevel = 10;
    clamped.minLod = 2u * 256u + 64u;
    const auto clampedView = DecodeTextureResource(pack(clamped));
    Require(clampedView.width == 256u && clampedView.mipCount == 9u && clampedView.minLod == 64u, "reading a 128-bit T# from its resident levels must move its MIN_LOD clamp with the base level");
    clamped.minLod = 256u;
    Require(DecodeTextureResource(pack(clamped)).minLod == 0u, "a MIN_LOD clamp below the base level must vanish when read from the resident levels");
}

void RunGuestTextureResourceTests() {
    Fields base;
    const auto baseWords = pack(base);
    Require(!IsNullTextureResource(baseWords), "a nonzero 2D texture was classified as null");
    Fields nullBase = base;
    nullBase.base40 = 0;
    Require(IsNullTextureResource(pack(nullBase)), "a zero-base texture was not classified as null");
    Fields nullType = base;
    nullType.typeRaw = 0;
    Require(IsNullTextureResource(pack(nullType)), "a type-0 texture with a nonzero base was not classified as null");
    nullType.typeRaw = 1;
    Require(IsNullTextureResource(pack(nullType)), "a type-1 texture with a nonzero base was not classified as null");
    nullType.typeRaw = 2;
    Require(!IsNullTextureResource(pack(nullType)), "a reserved type-2 texture was classified as null");
    auto result = DecodeTextureResource(pack(base));
    Require(result.baseAddress == (base.base40 << 8u), "decoded base address changed");
    Require(result.width == 16 && result.height == 16, "decoded width or height changed");
    Require(result.depthOrLastArray == 0 && result.baseArray == 0, "decoded depth or base array changed");
    Require(result.mipCount == 1 && result.baseLevel == 0, "decoded mip count or base level changed");
    Require(result.tileMode == TextureTileMode::kLinear, "decoded tile mode changed");
    Require(result.dimension == TextureDimension::k2D, "decoded dimension changed");
    Require(result.format == 56, "decoded format changed");
    Fields fmask = base;
    fmask.format = 166u;
    fmask.tileModeRaw = 24u;
    fmask.dstSelY = fmask.dstSelZ = fmask.dstSelW = 0u;
    const auto fmaskResource = DecodeTextureResource(pack(fmask));
    Require(fmaskResource.format == 20u && fmaskResource.dimension == TextureDimension::k2D && fmaskResource.tileMode == TextureTileMode::kZ64KBX, "uncompressed 32-bit FMASK did not use an integer Z surface");
    auto compressedFmask = fmask;
    compressedFmask.metaCompress = compressedFmask.metaPipeAligned = true;
    compressedFmask.metaAddr = 0x123456789aull;
    const auto compressedResource = DecodeTextureResource(pack(compressedFmask));
    Require(compressedResource.fmaskCmaskAddress == compressedFmask.metaAddr << 8u && compressedResource.dccAddress == 0u && compressedResource.dccPipeAligned, "compressed FMASK CMASK was interpreted as color DCC");
    compressedFmask.metaPipeAligned = false;
    rejectFields(compressedFmask, "FMASK CMASK metadata must be pipe aligned");
    compressedFmask.metaPipeAligned = compressedFmask.writeCompress = true;
    rejectFields(compressedFmask, "FMASK CMASK compression is not implemented");
    Fields fmaskArray = fmask;
    fmaskArray.typeRaw = 13u;
    fmaskArray.depth = 2u;
    fmaskArray.baseArray = 1u;
    const auto fmaskArrayResource = DecodeTextureResource(pack(fmaskArray));
    Require(fmaskArrayResource.format == 20u && fmaskArrayResource.baseArray == 1u && fmaskArrayResource.depthOrLastArray == 2u, "FMASK normalization changed the array view");
    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 8> narrowFormats{{{156u, 5u}, {157u, 5u}, {158u, 5u}, {159u, 5u}, {160u, 5u}, {161u, 5u}, {163u, 11u}, {165u, 20u}}};
    for (const auto& [format, surface] : narrowFormats) {
        auto narrow = fmask;
        narrow.format = format;
        const auto narrowResource = DecodeTextureResource(pack(narrow));
        Require(narrowResource.format == surface && narrowResource.tileMode == TextureTileMode::kZ64KBX, "FMASK " + std::to_string(format) + " did not use an integer Z surface of its storage width");
        narrow.dstSelX = 4u;
        narrow.dstSelY = 5u;
        narrow.dstSelZ = 6u;
        narrow.dstSelW = 7u;
        Require(DecodeTextureResource(pack(narrow)).format == surface, "an expanded FMASK " + std::to_string(format) + " view with identity selectors was refused");
        auto compressedNarrow = narrow;
        compressedNarrow.metaCompress = compressedNarrow.metaPipeAligned = true;
        compressedNarrow.metaAddr = 0x123456789aull;
        if (format == 159u) Require(DecodeTextureResource(pack(compressedNarrow)).format == surface, "a two-fragment CMASK FMASK was not normalized to its 8-bit storage");
    }
    auto identityWord = fmask;
    identityWord.dstSelX = 4u;
    identityWord.dstSelY = 5u;
    identityWord.dstSelZ = 6u;
    identityWord.dstSelW = 7u;
    rejectFields(identityWord, "FMASK destination channel selector is not implemented");
    for (const auto format : {162u, 164u, 167u, 168u}) {
        auto unsupported = fmask;
        unsupported.format = format;
        rejectFields(unsupported, "FMASK format is not implemented");
    }
    for (const auto field : {&Fields::metaCompress, &Fields::writeCompress}) {
        auto compressed = fmask;
        compressed.*field = true;
        rejectFields(compressed, "FMASK CMASK compression is not implemented");
    }
    for (const auto selector : {5u, 6u, 7u}) {
        auto unsupported = fmask;
        unsupported.dstSelX = selector;
        rejectFields(unsupported, "FMASK destination channel selector is not implemented");
    }
    auto unsupportedFmask = fmask;
    unsupportedFmask.tileModeRaw = 0u;
    rejectFields(unsupportedFmask, "FMASK reads require a single-mip");
    unsupportedFmask = fmask;
    unsupportedFmask.lastLevel = unsupportedFmask.maxMip = 1u;
    rejectFields(unsupportedFmask, "FMASK reads require a single-mip");
    unsupportedFmask = fmask;
    unsupportedFmask.typeRaw = 10u;
    rejectFields(unsupportedFmask, "FMASK reads require a single-mip");
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
    const auto xorBase = DecodeTextureResource(pack(tileModes));
    Require(xorBase.baseAddress == 0x12340000ull && xorBase.pipeBankXor == 0x5600u, "a 64 KiB XOR swizzle did not split its base into the block base and its pipe/bank XOR");
    tileModes.base40 = 0x120000ull;
    Require(DecodeTextureResource(pack(tileModes)).pipeBankXor == 0u, "an aligned 64 KiB XOR base gained a pipe/bank XOR");
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
    const auto xor4Kb = DecodeTextureResource(pack(tileModes));
    Require(xor4Kb.baseAddress == 0x12001000ull && xor4Kb.pipeBankXor == 0x100u, "a 4 KiB XOR swizzle did not split its base into the block base and its pipe/bank XOR");
    tileModes.tileModeRaw = 0x12;
    tileModes.base40 = 0x120010ull;
    const auto xorT = DecodeTextureResource(pack(tileModes));
    Require(xorT.baseAddress == 0x12000000ull && xorT.pipeBankXor == 0x1000u, "a 64 KiB T swizzle did not split its base into the block base and its pipe/bank XOR");
    tileModes.tileModeRaw = 0x09;
    const auto standard = DecodeTextureResource(pack(tileModes));
    Require(standard.baseAddress == 0x12001000ull && standard.pipeBankXor == 0u, "a swizzle without XOR addressing split its base");
    tileModes.tileModeRaw = 0x1b;
    tileModes.base40 = 0x56ull;
    rejectFields(tileModes, "null base address");
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
    rejectFields(twoD, "custom pitch other than its default pitch");
    twoD.tileModeRaw = 0x09;
    rejectFields(twoD, "nonzero depth or base array");
    Fields pitched = base;
    pitched.width = 72;
    pitched.height = 72;
    pitched.depth = 127;
    result = DecodeTextureResource(pack(pitched));
    Require(result.dimension == TextureDimension::k2D && result.width == 72 && result.depthOrLastArray == 0, "linear 2D descriptor with its default pitch in DEPTH decoded incorrectly");
    pitched.width = 8;
    pitched.height = 8;
    pitched.depth = 63;
    result = DecodeTextureResource(pack(pitched));
    Require(result.width == 8 && result.depthOrLastArray == 0, "linear 2D descriptor with a one-unit default pitch decoded incorrectly");
    pitched.depth = 127;
    rejectFields(pitched, "custom pitch other than its default pitch");
    pitched.depth = 63;
    pitched.maxMip = 1;
    pitched.lastLevel = 1;
    rejectFields(pitched, "custom pitch other than its default pitch");
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
    Fields cubeExtraSlice = cube;
    cubeExtraSlice.depth = 6;
    result = DecodeTextureResource(pack(cubeExtraSlice));
    Require(result.baseArray == 0 && result.depthOrLastArray == 6, "a cube descriptor with a slice past its last whole cube lost a slice");
    Require(DescribeSurface(result).imageLayers == 7, "a cube allocation with a slice past its last whole cube omitted it");
    Fields cubePartial = cube;
    cubePartial.depth = 4;
    rejectFields(cubePartial, "fewer than 6 array slices");

    Fields multisampled = base;
    multisampled.typeRaw = 14;
    multisampled.width = 297;
    multisampled.height = 297;
    multisampled.tileModeRaw = 0x1b;
    multisampled.lastLevel = 2;
    multisampled.maxMip = 2;
    result = DecodeTextureResource(pack(multisampled));
    Require(result.dimension == TextureDimension::k2DMsaa && result.samples == 4 && result.width == 297 && result.height == 297, "4x multisampled descriptor decoded incorrectly");
    Require(result.mipCount == 1 && result.allocatedMipCount == 1 && result.baseLevel == 0 && result.lastLevel == 0, "a multisampled descriptor's sample count was decoded as mip levels");
    Require(DecodeTextureResource(pack(base)).samples == 1, "a single-sample descriptor decoded with several samples");
    const std::array<std::uint32_t, 8> canvas{0x021f2000u, 0x03800000u, 0x804a004au, 0xe1b20facu, 0x00000000u, 0x00400020u, 0x00000000u, 0x00000000u};
    result = DecodeTextureResource(canvas);
    Require(result.dimension == TextureDimension::k2DMsaa && result.samples == 4 && result.baseAddress == 0x21f200000ull && result.width == 297 && result.height == 297 && result.format == 56 && result.tileMode == TextureTileMode::kR64KBX, "the 4x canvas descriptor of PPSA99006 decoded incorrectly");
    Require(DescribeSurface(result).guestBytes == 0x190000, "4x render-target geometry must include fragment-aware padding for the PPSA99006 canvas");
    const auto canvasGeometry = DescribeSurface(result);
    const std::array<std::uint32_t, 8> sonic{0x405fc080u, 0xc4700000u, 0x821bc3bfu, 0xe1b30facu, 0x00000000u, 0x00400030u, 0x406b0000u, 0x0040819fu};
    const auto sonicResource = DecodeTextureResource(sonic);
    const auto sonicGeometry = DescribeSurface(sonicResource);
    Require(sonicResource.width == 3840u && sonicResource.height == 2160u && sonicResource.samples == 8u && sonicResource.format == 71u, "the PPSA99902 multisampled color descriptor decoded incorrectly");
    Require(sonicGeometry.guestBytes == 0x1fe00000ull, "the PPSA99902 8x surface footprint omits fragments: " + std::to_string(sonicGeometry.guestBytes));
    Require(canvasGeometry.guestBytes == 0x190000u, "the 297x297 4x surface footprint does not use fragment-aware blocks");
    auto singleSample = sonicResource;
    singleSample.samples = 1u;
    singleSample.dimension = TextureDimension::k2D;
    Require(DescribeSurface(singleSample).guestBytes == 0x3fc0000u, "single-sample surface geometry changed");
    struct FootprintCase {
        TextureTileMode tile;
        VkFormat format;
        std::uint32_t elementBytes, width, height, samples, blocksPerRow;
        std::uint64_t bytes;
    };
    const std::array footprints{
        FootprintCase{TextureTileMode::kR64KBX, VK_FORMAT_R8_UINT, 1u, 129u, 65u, 2u, 2u, 0x20000u},
        FootprintCase{TextureTileMode::kR64KBX, VK_FORMAT_R16_UINT, 2u, 65u, 129u, 4u, 1u, 0x30000u},
        FootprintCase{TextureTileMode::kR64KBX, VK_FORMAT_R32_UINT, 4u, 129u, 65u, 2u, 3u, 0x30000u},
        FootprintCase{TextureTileMode::kR64KBX, VK_FORMAT_R32_UINT, 4u, 65u, 129u, 2u, 2u, 0x40000u},
        FootprintCase{TextureTileMode::kR64KBX, VK_FORMAT_R32_UINT, 4u, 297u, 297u, 4u, 5u, 0x190000u},
        FootprintCase{TextureTileMode::kR64KBX, VK_FORMAT_R32_UINT, 4u, 65u, 129u, 8u, 3u, 0x90000u},
        FootprintCase{TextureTileMode::kR64KBX, VK_FORMAT_R32G32_SFLOAT, 8u, 3840u, 2160u, 8u, 120u, 0x1fe00000u},
        FootprintCase{TextureTileMode::kR64KBX, VK_FORMAT_R32G32B32A32_UINT, 16u, 16384u, 16384u, 8u, 1024u, 0x800000000ull},
        FootprintCase{TextureTileMode::kZ64KBX, VK_FORMAT_R16_UNORM, 2u, 129u, 65u, 2u, 2u, 0x20000u},
        FootprintCase{TextureTileMode::kZ64KBX, VK_FORMAT_R32_SFLOAT, 4u, 65u, 129u, 8u, 3u, 0x90000u},
        FootprintCase{TextureTileMode::kStandard4KB, VK_FORMAT_R32_UINT, 4u, 17u, 33u, 4u, 2u, 0x6000u},
        FootprintCase{TextureTileMode::kStandard64KB, VK_FORMAT_R32_UINT, 4u, 129u, 65u, 2u, 3u, 0x30000u}
    };
    for (const auto& test : footprints) {
        auto resource = sonicResource;
        const auto format = FindGuestTextureFormat(test.format, test.elementBytes);
        Require(format.has_value(), "the multisample footprint test format has no guest encoding");
        resource.format = format.value();
        resource.width = test.width;
        resource.height = test.height;
        resource.samples = test.samples;
        resource.tileMode = test.tile;
        const auto geometry = DescribeSurface(resource);
        const auto linearBytes = static_cast<std::uint64_t>(test.width) * test.height * test.samples * test.elementBytes;
        Require(geometry.guestBytes == test.bytes && geometry.layerBytes == test.bytes && geometry.sliceLinearBytes == linearBytes, "multisampled surface footprint or linear size disagrees with the reference");
        Require(geometry.layers == 1u && geometry.imageLayers == 1u && geometry.imageDepth == 1u && !geometry.thick && geometry.blockDepth == 1u && geometry.mips.size() == 1u, "multisampled surface geometry is not one thin 2D level");
        const auto& mip = geometry.mips.front();
        Require(mip.tiledOffset == 0u && mip.tiledSize == test.bytes && mip.linearOffset == 0u && mip.linearSize == linearBytes && mip.width == test.width && mip.height == test.height && mip.blocksPerRow == test.blocksPerRow && mip.pitchBytes == test.width * test.samples * test.elementBytes && !mip.tail && mip.tailX == 0u && mip.tailY == 0u, "multisampled mip geometry lost its sample pitch or block layout");
    }
    auto unsupportedSurface = sonicResource;
    for (const auto samples : {0u, 3u, 16u}) {
        unsupportedSurface.samples = samples;
        reject([&] { DescribeSurface(unsupportedSurface); }, "fragment count");
    }
    unsupportedSurface = sonicResource;
    unsupportedSurface.mipCount = 2u;
    reject([&] { DescribeSurface(unsupportedSurface); }, "mip levels");
    unsupportedSurface = sonicResource;
    unsupportedSurface.dimension = TextureDimension::k2DArray;
    reject([&] { DescribeSurface(unsupportedSurface); }, "dimension or array");
    unsupportedSurface = sonicResource;
    unsupportedSurface.tileMode = TextureTileMode::kStandard256B;
    reject([&] { DescribeSurface(unsupportedSurface); }, "4 KiB or 64 KiB");
    for (const auto format : {56u, 71u}) {
        for (const auto samples : {2u, 4u, 8u}) {
            for (const auto extent : {std::array{1u, 1u}, std::array{17u, 5u}, std::array{297u, 297u}}) {
                auto linear = sonicResource;
                linear.tileMode = TextureTileMode::kLinear;
                linear.format = format;
                linear.samples = samples;
                linear.width = extent[0];
                linear.height = extent[1];
                auto single = linear;
                single.samples = 1u;
                single.dimension = TextureDimension::k2D;
                const auto expected = DescribeSurface(single);
                const auto actual = DescribeSurface(linear);
                Require(actual.guestBytes == expected.guestBytes && actual.layerBytes == expected.layerBytes && actual.sliceLinearBytes == expected.sliceLinearBytes && actual.mips.size() == expected.mips.size(), "linear multisampled descriptors must retain the existing geometry path");
                const auto& mip = actual.mips.front();
                const auto& baseline = expected.mips.front();
                Require(mip.tiledSize == baseline.tiledSize && mip.linearSize == baseline.linearSize && mip.pitchBytes == baseline.pitchBytes && mip.blocksPerRow == baseline.blocksPerRow, "linear multisampled descriptors changed existing mip geometry");
            }
        }
    }
    Fields fewerFragments = multisampled;
    fewerFragments.maxMip = 1;
    rejectFields(fewerFragments, "same sample count");
    Fields singleSampleType = multisampled;
    singleSampleType.lastLevel = 0;
    singleSampleType.maxMip = 0;
    rejectFields(singleSampleType, "same sample count");
    Fields offsetLevel = multisampled;
    offsetLevel.baseLevel = 1;
    rejectFields(offsetLevel, "same sample count");
    Fields multisampledSlice = multisampled;
    multisampledSlice.depth = 1;
    rejectFields(multisampledSlice, "nonzero depth or base array");
    Fields multisampledArray = multisampled;
    multisampledArray.typeRaw = 15;
    rejectFields(multisampledArray, "unsupported image type");

    for (const auto format : {7u, 22u}) {
        for (const auto log2 : {1u, 2u, 3u}) {
            auto nativeDepth = base;
            nativeDepth.typeRaw = 14;
            nativeDepth.format = format;
            nativeDepth.tileModeRaw = 24;
            nativeDepth.lastLevel = log2;
            nativeDepth.maxMip = log2;
            nativeDepth.msaaDepth = true;
            const auto decoded = DecodeTextureResource(pack(nativeDepth));
            Require(decoded.iterate256 && decoded.samples == (1u << log2) && decoded.tileMode == TextureTileMode::kZ64KBX && decoded.format == format, "a native multisampled depth descriptor lost ITERATE_256 or its identity");
            Require(decoded.mipCount == 1 && decoded.allocatedMipCount == 1 && decoded.baseLevel == 0 && decoded.lastLevel == 0, "native multisampled depth retained sample counts as mip levels");
            auto wrongTile = nativeDepth;
            wrongTile.tileModeRaw = 27;
            rejectFields(wrongTile, "ITERATE_256");
            auto wrongFormat = nativeDepth;
            wrongFormat.format = 56;
            rejectFields(wrongFormat, "ITERATE_256");
            auto wrongType = nativeDepth;
            wrongType.typeRaw = 9;
            rejectFields(wrongType, "ITERATE_256");
            wrongType.typeRaw = 15;
            rejectFields(wrongType, "ITERATE_256");
            auto mismatchedSamples = nativeDepth;
            mismatchedSamples.maxMip = log2 - 1u;
            rejectFields(mismatchedSamples, "same sample count");
        }
    }
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
    rejectFields(badMsaa, "ITERATE_256");

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

    for (std::uint32_t swizzle = 0; swizzle <= 5u; ++swizzle) {
        Fields borderSwizzle = base;
        borderSwizzle.bcSwizzle = swizzle;
        Require(DecodeTextureResource(pack(borderSwizzle)).bcSwizzle == swizzle, "a texture descriptor lost its BC swizzle " + std::to_string(swizzle));
    }
    for (std::uint32_t swizzle : {6u, 7u}) {
        Fields badSwizzle = base;
        badSwizzle.bcSwizzle = swizzle;
        rejectFields(badSwizzle, "reserved BC swizzle");
    }

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
    pastUntailed.perfMod = 4;
    rejectFields(pastUntailed, "would move the surface's own");
    auto untailedChain = tailView;
    untailedChain.mipCount = 1;
    const auto untailedAllocated = DescribeSurface(untailedChain).guestBytes;
    untailedChain.mipCount = 2;
    Require(untailedAllocated == 0xff0000u && DescribeSurface(untailedChain).guestBytes == 0x1470000u, "a second level must grow a 1920x1080 64 bpp SW_64KB_R_X surface from addrlib's 0xff0000 to 0x1470000 bytes");

    RunR128TextureTests();

    std::array<std::uint32_t, 4> shortWords{};
    reject([&] { DecodeTextureResource(shortWords); }, "8 dwords");

    Require(MatchesGuestDimension(Shape::Image1D, TextureDimension::k1D), "1D shape must match 1D dimension");
    Require(!MatchesGuestDimension(Shape::Image1D, TextureDimension::k2D), "1D shape must not match 2D dimension");
    Require(MatchesGuestDimension(Shape::Image2D, TextureDimension::k2D), "2D shape must match 2D dimension");
    Require(!MatchesGuestDimension(Shape::Image2D, TextureDimension::k2DArray), "2D shape must not match 2D array dimension");
    Require(MatchesGuestDimension(Shape::Image2DArray, TextureDimension::k2DArray), "2D array shape must match 2D array dimension");
    Require(MatchesGuestDimension(Shape::Image2DMultisample, TextureDimension::k2DMsaa), "multisampled shape must match the multisampled dimension");
    Require(!MatchesGuestDimension(Shape::Image2DMultisample, TextureDimension::k2D), "multisampled shape must not match 2D dimension");
    Require(MatchesGuestDimension(Shape::Image2DMultisampleArray, TextureDimension::k2DMsaa), "multisampled array shape must match the multisampled dimension");
    Require(!MatchesGuestDimension(Shape::Image2DMultisampleArray, TextureDimension::k2DArray), "multisampled array shape must not match 2D array dimension");
    Require(!MatchesGuestDimension(Shape::Image2D, TextureDimension::k2DMsaa), "2D shape must not match the multisampled dimension");
    Require(MatchesGuestDimension(Shape::Image2DArray, TextureDimension::kCube), "a cube must be readable as a 2D array of its faces");
    Require(MatchesGuestDimension(Shape::ImageCube, TextureDimension::kCube), "cube shape must match cube dimension");
    Require(!MatchesGuestDimension(Shape::ImageCube, TextureDimension::k1D), "cube shape must not match 1D dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::k1D), "3D shape must never match a guest dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::k2D), "3D shape must never match a guest dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::k2DArray), "3D shape must never match a guest dimension");
    Require(!MatchesGuestDimension(Shape::Image3D, TextureDimension::kCube), "3D shape must never match a guest dimension");

    const auto layered = [](TextureDimension dimension, std::uint32_t lastLayer, std::uint32_t baseArray = 0) {
        GuestTextureResource resource{};
        resource.dimension = dimension;
        resource.depthOrLastArray = lastLayer;
        resource.baseArray = baseArray;
        return resource;
    };
    const auto twoSlices = layered(TextureDimension::k2DArray, 1);
    Require(ArrayLayersServeView(twoSlices, layered(TextureDimension::k2D, 0)), "a 2D texture must view the first layer of a 2D array color surface (slice-0 view with MIP0_DEPTH 1)");
    Require(ArrayLayersServeView(twoSlices, twoSlices), "a 2D array must view the same 2D array");
    Require(ArrayLayersServeView(layered(TextureDimension::k2DArray, 3), twoSlices), "a 2D array must view the first layers of a larger 2D array");
    Require(!ArrayLayersServeView(twoSlices, layered(TextureDimension::k2DArray, 3)), "a 2D array view must not reach past the surface's layers");
    Require(!ArrayLayersServeView(twoSlices, layered(TextureDimension::k2D, 0, 1)), "a 2D view of a later layer must not be served by the array's first layer");
    Require(ArrayLayersServeView(layered(TextureDimension::k2D, 0), layered(TextureDimension::k2D, 0)), "a 2D texture must view the same 2D surface");
    Require(!ArrayLayersServeView(layered(TextureDimension::k2D, 0), twoSlices), "a 2D surface must not serve a 2D array view");
    Require(!ArrayLayersServeView(layered(TextureDimension::k3D, 1), layered(TextureDimension::k2D, 0)), "a 3D surface's first slice must not serve a 2D texture");
    Require(!ArrayLayersServeView(layered(TextureDimension::kCube, 5), layered(TextureDimension::k2D, 0)), "a cube's first face must not serve a 2D texture");

    Fields streamed = base;
    streamed.minLodWarn = 0xabc;
    streamed.mipStatsCntEn = true;
    streamed.mipStatsCntId = 0x5a;
    Require(SampledTexturesShareEntry(pack(base), pack(streamed)), "T#s differing only in MIN_LOD_WARN, MIP_STATS_COUNTER_EN and MIP_STATS_COUNTER_ID must share a sampled texture entry");
    for (const auto& field : {&Fields::minLodWarn, &Fields::mipStatsCntId}) {
        Fields one = base;
        one.*field = 1;
        Require(SampledTexturesShareEntry(pack(base), pack(one)), "a T# differing in one streaming-feedback field must share a sampled texture entry");
    }
    Fields moved = streamed;
    moved.base40 = base.base40 + 1;
    Require(!SampledTexturesShareEntry(pack(base), pack(moved)), "T#s with different base addresses must not share a sampled texture entry");
    Fields reformatted = streamed;
    reformatted.format = 57;
    Require(!SampledTexturesShareEntry(pack(base), pack(reformatted)), "T#s with different formats must not share a sampled texture entry");
    Fields cornered = base;
    cornered.cornerSample = true;
    Require(!SampledTexturesShareEntry(pack(base), pack(cornered)), "T#s differing in a field next to the feedback fields must not share a sampled texture entry");

    Fields arrayFields = base;
    arrayFields.typeRaw = 13;
    arrayFields.depth = 1536;
    arrayFields.baseArray = 3;
    const auto arrayWords = pack(arrayFields);
    std::array<std::uint32_t, 8> narrowed{};
    Require(FirstLayerDescriptor(arrayWords, narrowed), "a 2D array T# naming more than its base slice must narrow to that slice");
    const auto slice = DecodeTextureResource(narrowed);
    const auto whole = DecodeTextureResource(arrayWords);
    Require(slice.dimension == TextureDimension::k2DArray && slice.baseArray == 3 && slice.depthOrLastArray == 3, "a narrowed 2D array T# must name its base slice alone");
    Require(slice.baseAddress == whole.baseAddress && slice.width == whole.width && slice.format == whole.format && slice.mipCount == whole.mipCount && slice.tileMode == whole.tileMode, "narrowing a 2D array T# must keep every other field");
    Require(!SampledTexturesShareEntry(arrayWords, narrowed), "a narrowed 2D array T# must not share the whole array's sampled texture entry");
    Fields single = arrayFields;
    single.depth = 3;
    Require(!FirstLayerDescriptor(pack(single), narrowed), "a 2D array T# of its base slice alone needs no narrowing");
    Require(!FirstLayerDescriptor(baseWords, narrowed), "a 2D T# needs no narrowing");
}

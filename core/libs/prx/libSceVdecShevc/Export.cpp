#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceVdecShevc/include/InventoryExports.hpp"
#include "prx/libSceVdecShevc/include/Api.hpp"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(offsetof(SdecHevcConfig, magic) == 0x00);
static_assert(offsetof(SdecHevcConfig, defaultDepthMode) == 0x04);
static_assert(offsetof(SdecHevcConfig, levelIdc) == 0x08);
static_assert(offsetof(SdecHevcConfig, frameCount) == 0x1c);
static_assert(offsetof(SdecHevcConfig, widthInMbs) == 0x20);
static_assert(offsetof(SdecHevcConfig, heightInMbs) == 0x24);
static_assert(offsetof(SdecHevcConfig, extraWidthPixels) == 0x28);
static_assert(offsetof(SdecHevcConfig, extraHeightPixels) == 0x2c);
static_assert(offsetof(SdecHevcConfig, depthLuma) == 0x30);
static_assert(offsetof(SdecHevcConfig, depthChroma) == 0x31);

namespace {

constexpr int SCE_SDEC_SW_HEVC_NO_DECODER = 1;
constexpr int SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM = 3;
constexpr std::uint32_t MaxDpbFrames = 16;
constexpr std::uint32_t MaxInstances = 16;
constexpr std::uint64_t PictureAlignment = 256;
constexpr std::uint64_t FixedSecondPart = 524544;
constexpr std::uint64_t MaxLayoutWidth = 16384;
constexpr std::uint64_t MaxLayoutHeight = 10927;
constexpr std::uint64_t MaxLayoutFrames = 0x2000;

struct LevelLimits {
    std::uint32_t maxLumaPs;
    std::uint32_t maxDimension;
};

struct LayoutSizes {
    std::uint64_t partA;
    std::uint64_t partB;
};

std::uint64_t ReadQuad(const void* source, std::size_t offset, const char* function) {
    std::uint64_t value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, static_cast<const unsigned char*>(source) + offset, sizeof(value))) {
        NotImplemented_nid_no_patch(function);
    }
    return value;
}

bool HasNoDecoder(const void* handle, const char* function) {
    return handle == nullptr || ReadQuad(handle, 0, function) == 0 || ReadQuad(handle, 0x10, function) == 0;
}

LevelLimits LevelLimitsFor(std::uint8_t levelIdc) {
    switch (levelIdc) {
        case 30: return {36864u, 543u};
        case 60: return {122880u, 991u};
        case 63: return {245760u, 1402u};
        case 90: return {552960u, 2103u};
        case 93: return {983040u, 2804u};
        case 120:
        case 123: return {2228224u, 4222u};
        case 180:
        case 183:
        case 186: return {35651584u, 16888u};
        default: return {8912896u, 8444u};
    }
}

std::uint64_t CeilDiv(std::uint64_t value, std::uint64_t divisor) {
    return (value + divisor - 1) / divisor;
}

std::uint64_t Up(std::uint64_t value, std::uint64_t multiple) {
    return CeilDiv(value, multiple) * multiple;
}

std::uint64_t SurfaceHevc(std::uint64_t elementSize, std::uint64_t width, std::uint64_t height) {
    const std::uint64_t granule = std::max<std::uint64_t>(256, 64 * elementSize);
    std::uint64_t pitchAlignment = std::max<std::uint64_t>(64, 8 * elementSize);
    while ((pitchAlignment * height) % granule != 0) pitchAlignment *= 2;
    return Up(width * elementSize, pitchAlignment) * height;
}

bool ValidRawDepths(const SdecHevcConfig& config) {
    return config.depthLuma == config.depthChroma || (config.depthLuma != 8 && config.depthChroma != 8);
}

std::uint64_t CappedPictureArea(const SdecHevcConfig& config, std::uint64_t cap) {
    const std::uint64_t width = 16 * static_cast<std::uint64_t>(config.widthInMbs) + config.extraWidthPixels;
    const std::uint64_t height = 16 * static_cast<std::uint64_t>(config.heightInMbs) + config.extraHeightPixels;
    if (width == 0 || height == 0) return 0;
    if (width > cap || height > cap) return cap + 1;
    return width * height;
}

std::uint32_t DpbFramesHevc(const SdecHevcConfig& config, const LevelLimits& limits) {
    if (config.frameCount >= 0) return std::min(static_cast<std::uint32_t>(config.frameCount), MaxDpbFrames);
    if (config.widthInMbs < 0 || config.heightInMbs < 0) return MaxDpbFrames;
    const std::uint64_t maxLumaPs = limits.maxLumaPs;
    const std::uint64_t pictureArea = CappedPictureArea(config, maxLumaPs);
    if (pictureArea <= maxLumaPs / 4) return 16;
    if (pictureArea <= maxLumaPs / 2) return 12;
    if (pictureArea <= (3 * maxLumaPs) / 4) return 8;
    return 6;
}

std::uint64_t FirstSumHevc(std::uint64_t width, std::uint64_t height, std::uint64_t frames, std::uint64_t instances, std::uint64_t depthBytes) {
    const std::uint64_t mb16 = CeilDiv(width, 16) * CeilDiv(height, 16);
    const std::uint64_t mb32 = CeilDiv(width, 32) * CeilDiv(height, 32);
    const std::uint64_t mb64 = CeilDiv(width, 64) * CeilDiv(height, 64);
    const std::uint64_t rows = CeilDiv(height, 16);
    const std::uint64_t ctbTerm = std::max({0x178ull * mb16, 0x598ull * mb32, 0x1618ull * mb64});
    return 0x352fa0ull
        + Up(ctbTerm, 32) * instances
        + 64 * frames + Up(40 * frames, 32) + Up(400 * frames, 32) + Up(28 * mb16, 32) * frames
        + 32 * instances + 0x2a00ull * instances + Up(16 * instances, 32) + Up(0x10d58ull * instances, 32) + Up(32 * instances, 64)
        + ((0x308ull * depthBytes + 0x7cull) & ~std::uint64_t{0x7f})
        + 0x2e0ull * rows * instances + Up(0x268ull * rows, 32) * instances;
}

bool LayoutAcceptsHevc(std::uint64_t width, std::uint64_t height, std::uint64_t frames) {
    return width != 8 && height != 8 && width <= MaxLayoutWidth && height <= MaxLayoutHeight && frames <= MaxLayoutFrames;
}

LayoutSizes ComputeLayoutHevc(std::uint64_t width, std::uint64_t height, std::uint64_t frames, std::uint64_t instances, std::uint32_t instanceMode, bool luma10, bool chroma10) {
    const std::uint64_t wc = CeilDiv(width, 16);
    const std::uint64_t hc = CeilDiv(height, 16);
    const std::uint64_t nTiles = wc * hc;
    const std::uint64_t sr = 256 * CeilDiv(wc, 4) * CeilDiv(hc, 4);
    const std::uint64_t bLuma = luma10 ? 2 : 1;
    const std::uint64_t bChroma = chroma10 ? 2 : 1;
    const std::uint64_t mLuma = luma10 ? 8 : 4;
    const std::uint64_t anyHighDepth = (luma10 || chroma10) ? 1 : 0;

    const std::uint64_t mMove = (instanceMode == 4 && width >= 512 && instances >= 1 && instances <= 5) ? 2 * sr : 0;
    const std::uint64_t bBase = Up(2032 + 196 * instances * std::min<std::uint64_t>(nTiles, 600) + instances * mMove, 256)
        + SurfaceHevc(1, width / 4, height / 8)
        + SurfaceHevc(1, width / 8, height / 4)
        + Up(width, 256) * bLuma * (3 * height / 2) * frames;

    const std::uint64_t tTable = nTiles < 16384
        ? SurfaceHevc(2, nTiles, 2)
        : SurfaceHevc(2, 16384, nTiles / 16384 + 2);
    const std::uint64_t smallTableWidth = (CeilDiv(wc, 2) * CeilDiv(hc, 2) + 2) % 16384;
    const std::uint64_t aTableGroup = 3 * SurfaceHevc(16, wc, hc) + 2 * tTable + SurfaceHevc(2, smallTableWidth, 1);
    const std::uint64_t aSurfaceGroup = SurfaceHevc(2, width, height) + 2 * SurfaceHevc(4, width / 4, height / 2);

    if (instanceMode == 3 || instanceMode == 4) {
        const std::uint64_t r1 = SurfaceHevc(mLuma, width / 4, 16 * CeilDiv(height, 64))
            + SurfaceHevc(mLuma, height / 4, 16 * CeilDiv(width, 64))
            + SurfaceHevc(2, width, height / 4)
            + SurfaceHevc(2, height, width / 4);
        const std::uint64_t r2 = SurfaceHevc(mLuma, width / 4, 4 * CeilDiv(height, 32) + 4)
            + SurfaceHevc(mLuma, height / 4, 4 * CeilDiv(width, 32) + 4)
            + SurfaceHevc(4, width / 2, height / 8)
            + SurfaceHevc(4, height / 2, width / 8);
        const std::uint64_t fixed0 = std::max(SurfaceHevc(2, width, height), r1);
        const std::uint64_t fixed1 = std::max(SurfaceHevc(4, width / 2, height / 2), r2);
        const std::uint64_t planeLuma = bLuma * Up(width, 256) * height;
        const std::uint64_t planeChroma = bChroma * Up(width, 256) * (height / 2);
        const std::uint64_t bStrip = Up(width, 256) * (2 * height + 2 * (height / 2));

        if (instanceMode == 4 && width >= 512) {
            constexpr std::uint64_t kGranule = 16384;
            const std::uint64_t p1 = fixed0 + planeLuma;
            const std::uint64_t p2 = fixed1 + planeChroma;
            const std::uint64_t x = Up(Up(p1, kGranule) + Up(p2, kGranule) + instances * aSurfaceGroup, kGranule) + kGranule;
            const std::uint64_t a = Up(instances * (12 * nTiles + 12 * sr - mMove) + x, 256) + instances * aTableGroup;
            const std::uint64_t b = bBase + bStrip + instances * (2 * SurfaceHevc(2, wc, hc) + 1280);
            return {a, b};
        }

        const std::uint64_t a = Up(12 * instances * (nTiles + sr), 256)
            + fixed0 + fixed1 + planeLuma + planeChroma
            + instances * (aTableGroup + aSurfaceGroup + (2 - anyHighDepth) * SurfaceHevc(2, wc, hc) + 1280);
        const std::uint64_t b = bBase + bStrip + instances * (anyHighDepth ? SurfaceHevc(2, wc, hc) : 0);
        return {a, b};
    }

    const std::uint64_t a = Up(12 * instances * (nTiles + sr), 256)
        + SurfaceHevc(2, width, height) + SurfaceHevc(4, width / 2, height / 2)
        + bLuma * Up(width, 256) * height + bChroma * Up(width, 256) * (height / 2)
        + instances * (aTableGroup + aSurfaceGroup + 2 * SurfaceHevc(2, wc, hc) + 1280);
    const std::uint64_t b = bBase + SurfaceHevc(mLuma, width / 4, height);
    return {a, b};
}

bool ComputeQuerySizesHevc(const SdecHevcConfig& config, const SdecRecord& record, const LevelLimits& limits, std::uint32_t dpbFrames, SdecMemory* outMemory) {
    if (record.instanceMode > 5) NotImplemented_nid_no_patch("sceSdecQueryMemorySwHevc");
    const std::uint32_t defaultDepth = config.defaultDepthMode == 1 ? 8u : 10u;
    const std::uint32_t dl = config.depthLuma == 0 ? defaultDepth : config.depthLuma;
    const std::uint32_t dc = config.depthChroma == 0 ? defaultDepth : config.depthChroma;
    const bool luma10 = dl > 8;
    const bool chroma10 = dc > 8;
    const std::uint64_t depthBytes = (std::max(dl, dc) + 7) / 8;
    const std::uint64_t instances = (record.instanceMode >= 2 && record.instanceMode <= 4)
        ? record.unknown8
        : (record.instanceMode == 1 ? 1ull : 2ull);
    const std::uint64_t frames = static_cast<std::uint64_t>(dpbFrames) + static_cast<std::uint64_t>(std::max<std::int32_t>(record.extraFrames, 0)) + 1;

    std::uint64_t firstSum = 0;
    std::uint64_t bestA = 0;
    std::uint64_t bestB = 0;
    bool acceptedAnyShape = false;

    if (config.widthInMbs < 0 || config.heightInMbs < 0) {
        const std::uint64_t maxPs = limits.maxLumaPs;
        const std::uint64_t maxDim = limits.maxDimension;
        for (std::uint64_t s = 8; s <= maxDim; s += 8) {
            const std::uint64_t aFirst = std::min(maxPs / s, maxDim);
            firstSum = std::max({firstSum, FirstSumHevc(aFirst, s, frames, instances, depthBytes), FirstSumHevc(s, aFirst, frames, instances, depthBytes)});

            const std::uint64_t aLayout = std::min((maxPs / s) & ~std::uint64_t{7}, maxDim);
            const std::uint64_t shapes[2][2] = {{s, aLayout}, {aLayout, s}};
            for (const auto& shape : shapes) {
                if (!LayoutAcceptsHevc(shape[0], shape[1], frames)) continue;
                const LayoutSizes layout = ComputeLayoutHevc(shape[0], shape[1], frames, instances, record.instanceMode, luma10, chroma10);
                bestA = std::max(bestA, layout.partA);
                bestB = std::max(bestB, layout.partB);
                acceptedAnyShape = true;
            }
        }
    } else {
        const std::uint64_t width = 16 * static_cast<std::uint64_t>(config.widthInMbs) + config.extraWidthPixels;
        const std::uint64_t height = 16 * static_cast<std::uint64_t>(config.heightInMbs) + config.extraHeightPixels;
        firstSum = FirstSumHevc(width, height, frames, instances, depthBytes);
        if (LayoutAcceptsHevc(width, height, frames)) {
            const LayoutSizes layout = ComputeLayoutHevc(width, height, frames, instances, record.instanceMode, luma10, chroma10);
            bestA = layout.partA;
            bestB = layout.partB;
            acceptedAnyShape = true;
        }
    }

    if (!acceptedAnyShape) return false;
    const std::uint64_t size1 = firstSum + 0x800ull - 128 * instances + 0x38ull;
    const std::uint64_t size2 = FixedSecondPart + bestA;
    const std::uint64_t size3 = bestB;
    *outMemory = {{{0, size1, 0, 0}, {0, size2, static_cast<std::uint32_t>(PictureAlignment), 0}, {0, size3, static_cast<std::uint32_t>(PictureAlignment), 0}}};
    return true;
}

} // namespace

extern "C" {

int APS5_VABI sceSdecCreateSwHevc(const void* param) {
    std::uint32_t first = 0;
    if (!GuestCopyIn_nid_no_patch(&first, param, sizeof(first))) NotImplemented_nid_no_patch(__func__);
    if (first != 1) return SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdecDestroySwHevc(void* handle) {
    if (handle == nullptr) return 0;
    if (ReadQuad(handle, 0x10, __func__) != 0) NotImplemented_nid_no_patch(__func__);
    if (ReadQuad(handle, 0x8, __func__) != 0) {
        const std::uint64_t cleared = 0;
        auto* field = static_cast<unsigned char*>(handle) + 0x8;
        if (!GuestCopyOut_nid_no_patch(field, &cleared, sizeof(cleared))) NotImplemented_nid_no_patch(__func__);
    }
    return 0;
}

int APS5_VABI sceSdecExecuteSwHevc(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdecGetFrameSwHevc(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceSdecGetVersionSwHevc(std::uint32_t* version) {
    if (version == nullptr) return;
    const std::uint32_t value = 0x13590001u;
    GuestCopyOut_nid_no_patch(version, &value, sizeof(value));
}

int APS5_VABI sceSdecQueryMemorySwHevc(const SdecHevcConfig* config, const SdecRecord* record, std::int32_t mode, SdecMemory* memory, std::uint32_t* dpbFrames, std::uint8_t* tooBig) {
    if (mode == 0 || config == nullptr || record == nullptr || memory == nullptr || dpbFrames == nullptr) return SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM;
    SdecHevcConfig localConfig{};
    SdecRecord localRecord{};
    if (!GuestCopyIn_nid_no_patch(&localConfig, config, sizeof(localConfig)) || !GuestCopyIn_nid_no_patch(&localRecord, record, sizeof(localRecord))) return SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM;
    if (!ValidRawDepths(localConfig)) return SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM;
    const LevelLimits limits = LevelLimitsFor(localConfig.levelIdc);
    const std::uint32_t frames = DpbFramesHevc(localConfig, limits);
    if (!GuestCopyOut_nid_no_patch(dpbFrames, &frames, sizeof(frames))) return SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM;
    if (localRecord.unknown8 > MaxInstances) return SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM;
    if (localConfig.magic != 1 || localConfig.widthInMbs == 0 || localConfig.heightInMbs == 0 ||
        (localConfig.extraWidthPixels != 0 && localConfig.extraWidthPixels != 8) ||
        (localConfig.extraHeightPixels != 0 && localConfig.extraHeightPixels != 8)) {
        return SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM;
    }
    if (localConfig.widthInMbs > 0 && localConfig.heightInMbs > 0 && CappedPictureArea(localConfig, limits.maxLumaPs) > limits.maxLumaPs) {
        if (tooBig != nullptr) {
            const std::uint8_t one = 1;
            GuestCopyOut_nid_no_patch(tooBig, &one, sizeof(one));
        }
        return SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM;
    }
    SdecMemory result{};
    if (!ComputeQuerySizesHevc(localConfig, localRecord, limits, frames, &result)) return SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM;
    return GuestCopyOut_nid_no_patch(memory, &result, sizeof(result)) ? 0 : SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM;
}

int APS5_VABI sceSdecResetSwHevc(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceVdecShevcUnknown_79qqGAVnWfA_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("79qqGAVnWfA", libSceVdecShevcUnknown_79qqGAVnWfA_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_PTO17yTeoSk_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PTO17yTeoSk", libSceVdecShevcUnknown_PTO17yTeoSk_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_hZXstu74B4I_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hZXstu74B4I", libSceVdecShevcUnknown_hZXstu74B4I_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_lLaAHswK1zQ_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lLaAHswK1zQ", libSceVdecShevcUnknown_lLaAHswK1zQ_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_wvq_plus_GPpswaw_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wvq+GPpswaw", libSceVdecShevcUnknown_wvq_plus_GPpswaw_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_xbRElcEVmdM_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("xbRElcEVmdM", libSceVdecShevcUnknown_xbRElcEVmdM_nid_no_patch);

}

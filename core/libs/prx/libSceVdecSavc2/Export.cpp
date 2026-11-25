#include "prx/libSceVdecSavc2/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libSceVdecSavc/include/SdecShared.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SdecInvalid = 3;
constexpr int SdecMemoryMismatch = 4;
constexpr std::uint32_t DefaultMaxDpbMbs = 184320;
constexpr std::uint32_t MaxDpbFrames = 16;
constexpr std::uint32_t MaxInstances = 16;
constexpr std::uint32_t MaxFieldInstances = 8;
constexpr std::uint32_t SdkLateThreshold = 0x6500000u;
constexpr std::uint64_t MaxLayoutWidthInMbs = 1024;
constexpr std::uint64_t MaxLayoutHeightInMbs = 682;
constexpr std::uint64_t MaxLayoutFrames = 0x2000;
constexpr std::uint64_t PictureAlignment = 256;
constexpr std::uint64_t HandleHeaderSize = 0x18;
constexpr std::uint64_t MinimumDecoderSize = 0x1f;

constexpr std::array<LevelLimits, 17> Levels{{
    {10, 396, 99}, {11, 900, 396}, {12, 2376, 396}, {13, 2376, 396}, {20, 2376, 396},
    {21, 4752, 792}, {22, 8100, 1620}, {30, 8100, 1620}, {31, 18000, 3600}, {32, 20480, 5120},
    {40, 32768, 8192}, {41, 32768, 8192}, {42, 34816, 8704}, {50, 110400, 22080}, {51, 184320, 36864},
    {52, 184320, 36864}, {111, 184320, 99},
}};

bool ValidDepths(const SdecConfig2& config) {
    return config.depthLuma == config.depthChroma || (config.depthLuma != 8 && config.depthChroma != 8);
}

std::uint32_t DpbFrames(const SdecConfig2& config) {
    if (config.frameCount >= 0) return std::min(static_cast<std::uint32_t>(config.frameCount), MaxDpbFrames);
    if (config.widthInMbs < 0 || config.heightInMbs < 0) return MaxDpbFrames;
    const LevelLimits* limits = FindLevel(Levels, config.level);
    const std::uint64_t maxDpbMbs = limits != nullptr && config.level <= 50 ? limits->maxDpbMbs : DefaultMaxDpbMbs;
    const std::uint64_t pictureMbs = static_cast<std::uint64_t>(config.widthInMbs) * static_cast<std::uint64_t>(config.heightInMbs);
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(maxDpbMbs / pictureMbs, MaxDpbFrames));
}

int CheckShape(const SdecConfig2& config, bool* pictureTooLarge) {
    if (pictureTooLarge != nullptr) *pictureTooLarge = false;
    if (config.reserved0 != 0 || config.widthInMbs == 0 || config.heightInMbs == 0) return SdecInvalid;
    const LevelLimits* limits = FindLevel(Levels, config.level);
    if (limits == nullptr) NotImplemented_nid_no_patch("sceSdec2 level outside the read H.264 levels");
    if (config.widthInMbs < 0 || config.heightInMbs < 0) NotImplemented_nid_no_patch("sceSdec2 memory sizes for a negative width or height");
    const std::uint64_t pictureMbs = static_cast<std::uint64_t>(config.widthInMbs) * static_cast<std::uint64_t>(config.heightInMbs);
    if (pictureMbs > limits->maxFrameMbs) {
        if (pictureTooLarge != nullptr) *pictureTooLarge = true;
        return SdecInvalid;
    }
    return 0;
}

std::uint64_t FirstSizePart(std::uint64_t width, std::uint64_t height, std::uint64_t frames, std::uint64_t extra, std::uint64_t instances, unsigned fieldShift, bool flagSet, bool lateSdk) {
    const std::uint64_t base = lateSdk ? 0x6b51c0ull : 0x6b8de0ull;
    const std::uint64_t hh = fieldShift != 0 ? (height + 1) / 2 : height;
    const std::uint64_t total = frames + extra;
    const std::uint64_t tc = fieldShift != 0 ? 2 * total : total;
    const std::uint64_t evenHhPlusOne = (hh + 1) & ~std::uint64_t{1};
    const std::uint64_t extraInstances = std::max<std::uint64_t>(instances, 2) - 1;
    return base +
        (frames << fieldShift) * ((128 * width + Up(width, 128)) * evenHhPlusOne) +
        6 * Up(152 * (width + (hh + 1) / 2 + 1), 32) +
        128 * hh * instances * width +
        384 * total +
        Up(112 * extraInstances, 32) +
        Up(total, 32) +
        Up(48 * total, 32) +
        Up(312 * instances, 32) +
        2 * Up(16 * instances, 32) +
        Up(0x3fec8ull * instances, 32) +
        Up(16 * instances, 64) +
        Up(24 * tc, 32) +
        (flagSet ? 0 : 224 * hh * instances * width);
}

std::uint64_t LayoutPartA(std::uint64_t width, std::uint64_t height, std::uint64_t instances, bool fieldPass, bool flagSet) {
    const std::uint64_t hf = fieldPass ? (height + 1) / 2 : height;
    const std::uint64_t pictureMbs = width * hf;
    const std::uint64_t perBuffer = 20 * pictureMbs + 8 * pictureMbs + 2 * (4 * pictureMbs + 20) + 2 * Up(4 * pictureMbs + 20, 16);
    const std::uint64_t m = flagSet ? std::min<std::uint64_t>(instances, 2) : instances;
    const std::uint64_t k = flagSet ? 1 : (instances > 1 ? instances - 1 : 1);
    return Up(32 + 768 * instances * pictureMbs + k * (116 * pictureMbs + 96) + m * perBuffer, PictureAlignment) +
        instances * (3 * Surface(4, width, hf) + 2 * Surface(8, 16 * width, hf)) +
        m * 6 * Surface(4, 2 * width, 2 * hf);
}

std::uint64_t LayoutPartB(std::uint64_t width, std::uint64_t height, std::uint64_t frames, bool fieldPass, bool flagSet) {
    const std::uint64_t first = Surface(4, 8 * width, 16 * height) + (flagSet ? 0 : Surface(4, 8 * width, 8 * height) - 512);
    const std::uint64_t second = Surface(4, 4 * width, 16 * height) + 2 * Surface(4, 2 * width, 16 * height) + 2 * Surface(4, width, 4 * height);
    const std::uint64_t scratch = std::max(first, second);
    return frames * Up(16 * width, fieldPass ? 256 : 64) * 24 * height + scratch + 3840;
}

bool ComputeSizes(const SdecConfig2& config, const SdecRecord& record, std::uint32_t dpbFrames, bool flagSet, SdecSizes* sizes) {
    if (record.instanceMode == 2 && config.fieldMode == 0 && record.unknown8 > MaxFieldInstances) {
        NotImplemented_nid_no_patch("sceSdec2 field layout with more than 8 instances");
    }
    const auto width = static_cast<std::uint64_t>(config.widthInMbs);
    const auto height = static_cast<std::uint64_t>(config.heightInMbs);
    const std::uint64_t frames = dpbFrames + static_cast<std::uint64_t>(std::max(record.extraFrames, 0)) + 1;
    if (width > MaxLayoutWidthInMbs || height > MaxLayoutHeightInMbs || frames > MaxLayoutFrames) return false;
    const std::uint64_t instances = record.instanceMode == 1 ? 1 : (record.instanceMode == 2 ? record.unknown8 : 2);
    const bool lateSdk = ApplicationCompiledSdkVersion() >= SdkLateThreshold;
    std::uint64_t extra = 0;
    if (lateSdk) {
        if (record.extraFrames >= 0 && record.extraFrames <= 16) {
            extra += static_cast<std::uint64_t>(16 - record.extraFrames);
        }
        if (frames <= 15) {
            extra += 16 - frames;
        }
    }
    const std::uint64_t first0 = FirstSizePart(width, height, frames, extra, instances, 0, flagSet, lateSdk);
    const std::uint64_t a0 = LayoutPartA(width, height, instances, false, flagSet);
    const std::uint64_t b0 = LayoutPartB(width, height, frames, false, flagSet);
    if (config.fieldMode == 0) {
        const std::uint64_t first1 = FirstSizePart(width, height, frames, extra, 2 * instances, 1, flagSet, lateSdk);
        const std::uint64_t a1 = LayoutPartA(width, height, 2 * instances, true, flagSet);
        const std::uint64_t b1 = LayoutPartB(width, height, frames, true, flagSet);
        sizes->first = std::max(first0, first1) + 0x38;
        sizes->second = 524544 + std::max(a0, a1);
        sizes->third = std::max(b0, b1);
    } else {
        sizes->first = first0 + 0x38;
        sizes->second = 524544 + a0;
        sizes->third = b0;
    }
    return true;
}

int CheckCreate(const SdecConfig2& config, const SdecRecord2& record, const SdecMemory& memory) {
    if (memory.blocks[0].size - HandleHeaderSize <= MinimumDecoderSize) return SdecInvalid;
    if (record.unknown10 == 0) return SdecInvalid;
    if (record.instanceMode > 2u) return SdecInvalid;
    if (record.instanceMode == 2u && record.instanceCount > MaxInstances) return SdecInvalid;
    if (record.unknown18 != -1 && (record.unknown18 < 0x100 || record.unknown18 > 0x2ff)) return SdecInvalid;
    if (record.unknown20 > 0x7fu) return SdecInvalid;
    if (!ValidDepths(config)) return SdecInvalid;
    const bool dividesByZero = config.frameCount < 0 && config.widthInMbs >= 0 && config.heightInMbs >= 0 &&
        (config.widthInMbs == 0 || config.heightInMbs == 0);
    if (dividesByZero) return SdecInvalid;
    if (const int status = CheckShape(config, nullptr); status != 0) return status;
    const SdecRecord queryRecord{record.reserved0, record.instanceMode, record.instanceCount, record.extraFrames};
    SdecSizes sizes{};
    if (!ComputeSizes(config, queryRecord, DpbFrames(config), true, &sizes)) {
        NotImplemented_nid_no_patch("sceSdecCreateSw2 for a picture past the layout limits");
    }
    if (memory.blocks[0].alignment != 0 ||
        memory.blocks[1].alignment != PictureAlignment ||
        memory.blocks[2].alignment != PictureAlignment) {
        return SdecMemoryMismatch;
    }
    if (memory.blocks[0].size < sizes.first ||
        memory.blocks[1].size < sizes.second ||
        memory.blocks[2].size < sizes.third) {
        return SdecMemoryMismatch;
    }
    return 0;
}

} // namespace

extern "C" {

int APS5_VABI sceSdecCreateSw2(const SdecConfig2* config, const void* attributes, const SdecRecord2* record, const SdecMemory* memory, void** handle) {
    if (config == nullptr) return SdecInvalid;
    SdecConfig2 localConfig{};
    const int savedErrno = errno;
    const bool configReadable = GuestCopyIn_nid_no_patch(&localConfig, config, sizeof(localConfig));
    errno = savedErrno;
    if (!configReadable || localConfig.reserved0 != 0) return SdecInvalid;
    if (attributes == nullptr || record == nullptr || memory == nullptr || handle == nullptr) return SdecInvalid;
    SdecRecord2 localRecord{};
    SdecMemory localMemory{};
    const bool readable = GuestCopyIn_nid_no_patch(&localRecord, record, sizeof(localRecord)) &&
        GuestCopyIn_nid_no_patch(&localMemory, memory, sizeof(localMemory));
    errno = savedErrno;
    if (!readable) return SdecInvalid;
    if (localMemory.blocks[0].base == 0 || localMemory.blocks[0].size < HandleHeaderSize) return SdecInvalid;
    if (const int status = CheckCreate(localConfig, localRecord, localMemory); status != 0) return status;
    NotImplemented_nid_no_patch("sceSdecCreateSw2 decoder setup");
}

int APS5_VABI sceSdecDestroySw2(void* decoder) {
    if (decoder == nullptr || DestroyedWithoutCodec(decoder)) return 0;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceSdecGetVersionSw2(std::uint32_t* version) {
    if (version == nullptr) return;
    const std::uint32_t value = 0x13590001u;
    const auto preserved = errno;
    GuestCopyOut_nid_no_patch(version, &value, sizeof(value));
    errno = preserved;
}

int APS5_VABI sceSdecQueryMemorySw2(const SdecConfig2* config, const SdecRecord* record, std::int32_t mode, SdecMemory* memory, std::uint32_t* dpbFrames, std::int32_t flag, std::uint8_t* tooBig) {
    if (config == nullptr || record == nullptr || memory == nullptr || dpbFrames == nullptr) return SdecInvalid;
    SdecConfig2 localConfig{};
    SdecRecord localRecord{};
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&localConfig, config, sizeof(localConfig)) &&
        GuestCopyIn_nid_no_patch(&localRecord, record, sizeof(localRecord));
    errno = savedErrno;
    if (!readable || mode == 0 || !ValidDepths(localConfig)) return SdecInvalid;
    const bool dividesByZero = localConfig.frameCount < 0 && localConfig.widthInMbs >= 0 && localConfig.heightInMbs >= 0 &&
        (localConfig.widthInMbs == 0 || localConfig.heightInMbs == 0);
    if (dividesByZero) return SdecInvalid;
    const std::uint32_t frames = DpbFrames(localConfig);
    const bool written = GuestCopyOut_nid_no_patch(dpbFrames, &frames, sizeof(frames));
    errno = savedErrno;
    if (!written || localRecord.unknown8 > MaxInstances) return SdecInvalid;
    bool pictureTooLarge = false;
    if (const int status = CheckShape(localConfig, &pictureTooLarge); status != 0) {
        if (pictureTooLarge && tooBig != nullptr) {
            const std::uint8_t one = 1;
            GuestCopyOut_nid_no_patch(tooBig, &one, sizeof(one));
            errno = savedErrno;
        }
        return status;
    }
    SdecSizes sizes{};
    if (!ComputeSizes(localConfig, localRecord, frames, flag != 0, &sizes)) return SdecInvalid;
    const SdecMemory result{{{0, sizes.first, 0, 0}, {0, sizes.second, static_cast<std::uint32_t>(PictureAlignment), 0}, {0, sizes.third, static_cast<std::uint32_t>(PictureAlignment), 0}}};
    const bool stored = GuestCopyOut_nid_no_patch(memory, &result, sizeof(result));
    errno = savedErrno;
    return stored ? 0 : SdecInvalid;
}

}

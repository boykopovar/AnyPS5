#include "prx/libSceVdecSavc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>

namespace {

constexpr int SdecInvalid = 3;
constexpr int SdecMemoryMismatch = 4;
constexpr std::uint32_t DefaultMaxDpbMbs = 184320;
constexpr std::uint32_t Level1b = 111;
constexpr std::uint32_t Level1bMaxFrameMbs = 99;
constexpr std::uint32_t MaxDpbFrames = 16;
constexpr std::uint64_t MaxLayoutWidthInMbs = 1024;
constexpr std::uint64_t MaxLayoutHeightInMbs = 682;
constexpr std::uint64_t MaxLayoutFrames = 0x2000;
constexpr std::uint64_t PictureAlignment = 256;
constexpr std::uint64_t HandleHeaderSize = 0x18;
constexpr std::uint64_t MinimumDecoderSize = 0x1f;
constexpr std::uint64_t FirstBlockSlack = 0x20;

struct LevelLimits {
    std::uint32_t level;
    std::uint32_t maxDpbMbs;
    std::uint32_t maxFrameMbs;
};

constexpr std::array<LevelLimits, 15> Levels{{
    {10, 396, 99}, {11, 900, 396}, {12, 2376, 396}, {13, 2376, 396}, {20, 2376, 396},
    {21, 4752, 792}, {22, 8100, 1620}, {30, 8100, 1620}, {31, 18000, 3600}, {32, 20480, 5120},
    {40, 32768, 8192}, {41, 32768, 8192}, {42, 34816, 8704}, {50, 110400, 22080}, {51, 184320, 36864},
}};

struct SdecSizes {
    std::uint64_t first;
    std::uint64_t second;
    std::uint64_t third;
};

const LevelLimits* FindLevel(std::uint32_t level) {
    const auto found = std::find_if(Levels.begin(), Levels.end(), [level](const LevelLimits& limits) { return limits.level == level; });
    return found == Levels.end() ? nullptr : &*found;
}

std::uint64_t Up(std::uint64_t value, std::uint64_t multiple) {
    return (value + multiple - 1) / multiple * multiple;
}

std::uint64_t Surface(std::uint64_t elementSize, std::uint64_t width, std::uint64_t height) {
    const std::uint64_t granule = std::max<std::uint64_t>(256, 64 * elementSize);
    std::uint64_t pitchAlignment = 64;
    while (pitchAlignment * height % granule != 0) pitchAlignment *= 2;
    return Up(width * elementSize, pitchAlignment) * height;
}

std::uint32_t DpbFrames(const SdecConfig& config) {
    if (config.frameCount >= 0) return std::min(static_cast<std::uint32_t>(config.frameCount), MaxDpbFrames);
    if (config.widthInMbs < 0 || config.heightInMbs < 0) return MaxDpbFrames;
    const LevelLimits* limits = FindLevel(config.level);
    const std::uint64_t maxDpbMbs = limits != nullptr ? limits->maxDpbMbs : DefaultMaxDpbMbs;
    const std::uint64_t pictureMbs = static_cast<std::uint64_t>(config.widthInMbs) * static_cast<std::uint64_t>(config.heightInMbs);
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(maxDpbMbs / pictureMbs, MaxDpbFrames));
}

int CheckShape(const SdecConfig& config) {
    if (config.reserved0 != 0 || config.widthInMbs == 0 || config.heightInMbs == 0) return SdecInvalid;
    const LevelLimits* limits = FindLevel(config.level);
    if (limits == nullptr && config.level != Level1b) return SdecInvalid;
    if (config.widthInMbs < 0 || config.heightInMbs < 0) NotImplemented_nid_no_patch("sceSdec memory sizes for a negative width or height");
    const std::uint64_t maxFrameMbs = limits != nullptr ? limits->maxFrameMbs : Level1bMaxFrameMbs;
    const std::uint64_t pictureMbs = static_cast<std::uint64_t>(config.widthInMbs) * static_cast<std::uint64_t>(config.heightInMbs);
    return pictureMbs > maxFrameMbs ? SdecInvalid : 0;
}

bool ComputeSizes(const SdecConfig& config, const SdecRecord& record, std::uint32_t dpbFrames, SdecSizes* sizes) {
    const auto width = static_cast<std::uint64_t>(config.widthInMbs);
    const auto height = static_cast<std::uint64_t>(config.heightInMbs);
    const std::uint64_t frames = dpbFrames + static_cast<std::uint64_t>(std::max(record.extraFrames, 0)) + 1;
    if (width > MaxLayoutWidthInMbs || height > MaxLayoutHeightInMbs || frames > MaxLayoutFrames) return false;
    const std::uint64_t instances = record.instanceMode == 1 ? 1 : 2;
    const std::uint64_t pictureMbs = width * height;
    sizes->first = 6217144 + 192 * pictureMbs + 6 * Up(152 * (width + (height + 1) / 2 + 1), 32) +
        Up(frames, 32) + Up(24 * frames, 32) + Up(40 * frames, 32) + Up(56 * frames, 32) +
        frames * ((128 * width + Up(width, 128)) * ((height + 1) & ~std::uint64_t{1}));
    const std::uint64_t perInstance = 512 * pictureMbs + 256 * pictureMbs + 20 * pictureMbs + 8 * pictureMbs +
        2 * (4 * pictureMbs + 20) + 2 * Up(4 * pictureMbs + 20, 16);
    const std::uint64_t surfaces = 3 * Surface(4, width, height) + 2 * Surface(8, 4 * width, 4 * height) + 6 * Surface(4, 2 * width, 2 * height);
    sizes->second = 4457728 + Up(128 + instances * perInstance + 116 * pictureMbs + 96, PictureAlignment) + instances * surfaces;
    sizes->third = 8388608 + frames * 384 * height * Up(width, 4) + 16 * height * std::max<std::uint64_t>(224, 3 * Up(16 * width, 32)) + 3328;
    return true;
}

int CheckCreate(const SdecConfig& config, const SdecRecord& record, const SdecMemory& memory) {
    if (memory.blocks[0].size - HandleHeaderSize <= MinimumDecoderSize) return SdecInvalid;
    if (record.instanceMode > 1) return SdecInvalid;
    if (const int status = CheckShape(config); status != 0) return status;
    SdecSizes sizes{};
    if (!ComputeSizes(config, record, DpbFrames(config), &sizes)) NotImplemented_nid_no_patch("sceSdecCreateSw for a picture past the layout limits");
    if (memory.blocks[0].alignment != 0 || memory.blocks[1].alignment != PictureAlignment || memory.blocks[2].alignment != PictureAlignment) return SdecMemoryMismatch;
    if (memory.blocks[0].size + FirstBlockSlack < sizes.first || memory.blocks[1].size < sizes.second || memory.blocks[2].size < sizes.third) return SdecMemoryMismatch;
    return 0;
}

bool DestroyedWithoutCodec(void* decoder) {
    const int savedErrno = errno;
    std::uint64_t codec = 0;
    std::uint64_t field = 0;
    bool done = GuestCopyIn_nid_no_patch(&codec, static_cast<unsigned char*>(decoder) + 0x10, sizeof(codec)) &&
                codec == 0 &&
                GuestCopyIn_nid_no_patch(&field, static_cast<unsigned char*>(decoder) + 0x8, sizeof(field));
    if (done && field != 0) {
        const std::uint64_t zero = 0;
        done = GuestCopyOut_nid_no_patch(static_cast<unsigned char*>(decoder) + 0x8, &zero, sizeof(zero));
    }
    errno = savedErrno;
    return done;
}

} // namespace

extern "C" {

int APS5_VABI sceSdecCreateSw(const SdecConfig* config, const void* attributes, const SdecRecord* record, const SdecMemory* memory, void** handle) {
    if (config == nullptr || attributes == nullptr || record == nullptr || memory == nullptr || handle == nullptr) return SdecInvalid;
    SdecConfig localConfig{};
    SdecRecord localRecord{};
    SdecMemory localMemory{};
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&localConfig, config, sizeof(localConfig)) &&
        GuestCopyIn_nid_no_patch(&localRecord, record, sizeof(localRecord)) &&
        GuestCopyIn_nid_no_patch(&localMemory, memory, sizeof(localMemory));
    errno = savedErrno;
    if (!readable || localConfig.reserved0 != 0) return SdecInvalid;
    if (localMemory.blocks[0].base == 0 || localMemory.blocks[0].size < HandleHeaderSize) return SdecInvalid;
    if (const int status = CheckCreate(localConfig, localRecord, localMemory); status != 0) {
        void* const none = nullptr;
        GuestCopyOut_nid_no_patch(handle, &none, sizeof(none));
        errno = savedErrno;
        return status;
    }
    NotImplemented_nid_no_patch("sceSdecCreateSw decoder setup");
}

int APS5_VABI sceSdecDestroySw(void* decoder) {
    if (decoder == nullptr || DestroyedWithoutCodec(decoder)) return 0;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceSdecGetVersionSw(std::uint32_t* version) {
    if (version == nullptr) return;
    const std::uint32_t value = 0u;
    const auto preserved = errno;
    GuestCopyOut_nid_no_patch(version, &value, sizeof(value));
    errno = preserved;
}

int APS5_VABI sceSdecQueryMemorySw(const SdecConfig* config, const SdecRecord* record, SdecMemory* memory, std::uint32_t* dpbFrames) {
    SdecConfig localConfig{};
    SdecRecord localRecord{};
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&localConfig, config, sizeof(localConfig)) &&
        GuestCopyIn_nid_no_patch(&localRecord, record, sizeof(localRecord));
    errno = savedErrno;
    if (!readable || dpbFrames == nullptr) return SdecInvalid;
    const bool dividesByZero = localConfig.frameCount < 0 && localConfig.widthInMbs >= 0 && localConfig.heightInMbs >= 0 &&
        (localConfig.widthInMbs == 0 || localConfig.heightInMbs == 0);
    if (dividesByZero) return SdecInvalid;
    const std::uint32_t frames = DpbFrames(localConfig);
    const bool written = GuestCopyOut_nid_no_patch(dpbFrames, &frames, sizeof(frames));
    errno = savedErrno;
    if (!written) return SdecInvalid;
    if (const int status = CheckShape(localConfig); status != 0) return status;
    SdecSizes sizes{};
    if (!ComputeSizes(localConfig, localRecord, frames, &sizes)) return SdecInvalid;
    const SdecMemory result{{{0, sizes.first, 0, 0}, {0, sizes.second, static_cast<std::uint32_t>(PictureAlignment), 0}, {0, sizes.third, static_cast<std::uint32_t>(PictureAlignment), 0}}};
    const bool stored = GuestCopyOut_nid_no_patch(memory, &result, sizeof(result));
    errno = savedErrno;
    return stored ? 0 : SdecInvalid;
}

}

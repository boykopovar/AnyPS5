#include "prx/libSceVideodec2/include/Api.hpp"
#include "prx/libSceVideodec2/include/PictureMetadata.hpp"
#include "prx/libSceVideodec/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

using namespace SceVideodec;

constexpr int VideodecErrorApiFail = static_cast<int>(0x80C10000);
constexpr int VideodecErrorStructSize = static_cast<int>(0x80C10002);
constexpr int VideodecErrorHandle = static_cast<int>(0x80C10003);
constexpr int VideodecErrorAuSize = static_cast<int>(0x80C10009);
constexpr int VideodecErrorAuPointer = static_cast<int>(0x80C1000A);
constexpr int VideodecErrorFrameBufferSize = static_cast<int>(0x80C1000B);
constexpr int VideodecErrorFrameBufferPointer = static_cast<int>(0x80C1000C);
constexpr int VideodecErrorConfigInfo = static_cast<int>(0x80C1000E);
constexpr int VideodecErrorArgumentPointer = static_cast<int>(0x80C1000F);
constexpr int VideodecErrorDecodeAu = static_cast<int>(0x80C10011);

constexpr std::uint32_t CodecAvc = 1;
constexpr std::uint32_t PitchAlignment = 64;

using Decoder2Crop = SceVideodec2::Internal::Crop;

static_assert(sizeof(ConfigInfo) == 40 && sizeof(ResourceInfo) == 56 && sizeof(Ctrl) == 24);
static_assert(offsetof(ConfigInfo, maxFrameWidth) == 20 && offsetof(ConfigInfo, videodecFlags) == 32);
static_assert(offsetof(ResourceInfo, cpuGpuMemorySize) == 24 && offsetof(ResourceInfo, maxFrameBufferSize) == 40);
static_assert(offsetof(ResourceInfo, frameBufferAlignment) == 48);
static_assert(sizeof(SceVideodec2::DecoderConfigInfo) == 40 && sizeof(SceVideodec2::DecoderMemoryInfo) == 72);
static_assert(sizeof(FrameBuffer) == 24 && sizeof(SceVideodec2::InputData) == 48 && sizeof(PictureInfo) == 112);
static_assert(sizeof(CodecInfo) == 64 && offsetof(PictureInfo, codec) == 48);

struct RestoreErrno {
    int value = errno;
    ~RestoreErrno() { errno = value; }
};

struct Decoder {
    std::mutex mutex;
    std::uint64_t handle = 0;
    std::vector<std::uint8_t> picture;
};

std::mutex decodersMutex;
std::map<const void*, std::shared_ptr<Decoder>> decoders;

std::shared_ptr<Decoder> FindDecoder(const void* handle) {
    if (handle == nullptr) return nullptr;
    std::lock_guard lock(decodersMutex);
    const auto found = decoders.find(handle);
    return found == decoders.end() ? nullptr : found->second;
}

bool WritableRows(void* destination, std::size_t pitch, std::size_t rowBytes, std::size_t rows) {
    const RestoreErrno restore;
    if (rows == 0 || rowBytes == 0) return true;
    const auto start = reinterpret_cast<std::uintptr_t>(destination);
    if (start == 0 || rowBytes > UINTPTR_MAX - start || (rows > 1 && pitch > (UINTPTR_MAX - start - rowBytes) / (rows - 1))) {
        return false;
    }
    if (pitch == rowBytes) {
        return GuestWritable_nid_no_patch(destination, rows * rowBytes);
    }
    auto* target = static_cast<std::uint8_t*>(destination);
    for (std::size_t row = 0; row < rows; ++row) {
        if (!GuestWritable_nid_no_patch(target + row * pitch, rowBytes)) return false;
    }
    return true;
}

std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

struct Geometry {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t pitch = 0;
    Decoder2Crop crop{};
};

SceVideodec2::DecoderConfigInfo MapConfig(const ConfigInfo& config) {
    SceVideodec2::DecoderConfigInfo decoderConfig{};
    decoderConfig.thisSize = sizeof(SceVideodec2::DecoderConfigInfo);
    decoderConfig.resourceType = 1;
    decoderConfig.codecType = CodecAvc;
    decoderConfig.profile = config.profile;
    decoderConfig.maxLevel = config.maxLevel;
    decoderConfig.maxFrameWidth = config.maxFrameWidth;
    decoderConfig.maxFrameHeight = config.maxFrameHeight;
    decoderConfig.maxDpbFrameCount = config.maxDpbFrameCount;
    decoderConfig.decodePipelineDepth = 1;
    return decoderConfig;
}

Geometry Measure(const SceVideodec2::OutputInfo& output, const SceVideodec2::Internal::PictureHeader& header) {
    if (header.bitDepthMinus8 != 0) throw std::runtime_error("Videodec: pictures deeper than 8 bits are not implemented");
    Geometry geometry;
    geometry.width = output.frameWidth;
    geometry.height = output.frameHeight;
    geometry.crop = header.crop;
    geometry.pitch = AlignUp(geometry.width, PitchAlignment);
    return geometry;
}

void FillPictureInfo(const Geometry& geometry, const SceVideodec2::OutputInfo& output, PictureInfo* picture) {
    picture->isValid = 0;
    picture->codecType = 0;
    picture->frameWidth = 0;
    picture->framePitch = 0;
    picture->frameHeight = 0;
    picture->isErrorPic = 0;
    picture->ptsData = 0;
    picture->attachedData = 0;
    std::memset(picture->codec.reserved, 0, sizeof(picture->codec.reserved));
    if (!output.isValid) return;
    picture->isValid = 1;
    picture->frameWidth = geometry.width;
    picture->framePitch = geometry.pitch;
    picture->frameHeight = geometry.height;
    picture->isErrorPic = output.isErrorFrame ? 1 : 0;
    picture->codec.avc.frameCropLeftOffset = geometry.crop.left / 2;
    picture->codec.avc.frameCropRightOffset = geometry.crop.right / 2;
    picture->codec.avc.frameCropTopOffset = geometry.crop.top / 2;
    picture->codec.avc.frameCropBottomOffset = geometry.crop.bottom / 2;
}

int CopyPicture(const SceVideodec2::OutputInfo& output, const Geometry& geometry, const FrameBuffer* frameBuffer) {
    const auto lumaBytes = static_cast<std::uint64_t>(geometry.pitch) * geometry.height;
    const auto bytes = lumaBytes * 3 / 2;
    if (frameBuffer->frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
    if (frameBuffer->frameBufferSize < bytes) return VideodecErrorFrameBufferSize;
    const auto base = reinterpret_cast<std::uintptr_t>(frameBuffer->frameBuffer);
    if (lumaBytes > std::numeric_limits<std::uintptr_t>::max() - base) return VideodecErrorFrameBufferPointer;
    const auto sourcePitch = static_cast<std::uint64_t>(output.framePitchInBytes);
    const auto* source = static_cast<const std::uint8_t*>(output.frameBuffer);
    auto* destination = static_cast<std::uint8_t*>(frameBuffer->frameBuffer);
    const auto* sourceChroma = source + sourcePitch * geometry.height;
    auto* destinationChroma = destination + static_cast<std::size_t>(lumaBytes);
    if (!WritableRows(destination, geometry.pitch, geometry.width, geometry.height) ||
        !WritableRows(destinationChroma, geometry.pitch, geometry.width, geometry.height / 2)) {
        return VideodecErrorFrameBufferPointer;
    }
    const RestoreErrno restore;
    if (!GuestCopyOutRows_nid_no_patch(destination, geometry.pitch, source, sourcePitch, geometry.width, geometry.height)) return VideodecErrorFrameBufferPointer;
    if (!GuestCopyOutRows_nid_no_patch(destinationChroma, geometry.pitch, sourceChroma, sourcePitch, geometry.width, geometry.height / 2)) return VideodecErrorFrameBufferPointer;
    return 0;
}

int PublishPicture(std::uint64_t handle, const SceVideodec2::OutputInfo& output, const FrameBuffer& frameBuffer, PictureInfo* picture, PictureInfo& localPicture) {
    SceVideodec2::Internal::PictureHeader header{};
    if (output.isValid) Videodec2PictureHeader_nid_no_patch(handle, output.frameBuffer, &header);
    const auto geometry = Measure(output, header);
    if (output.isValid) {
        const auto copyError = CopyPicture(output, geometry, &frameBuffer);
        if (copyError != 0) return copyError;
    }
    FillPictureInfo(geometry, output, &localPicture);
    localPicture.ptsData = header.pts;
    localPicture.attachedData = header.attached;
    if (!GuestCopyOut_nid_no_patch(picture, &localPicture, sizeof(localPicture))) return VideodecErrorArgumentPointer;
    return 0;
}

}

extern "C" {

int APS5_VABI sceVideodecCreateDecoder(const ConfigInfo* config, const ResourceInfo* resource, Ctrl* ctrl) {
    if (config == nullptr || resource == nullptr || ctrl == nullptr) return VideodecErrorArgumentPointer;
    std::uint64_t configSize = 0;
    if (!GuestCopyIn_nid_no_patch(&configSize, config, sizeof(configSize))) return VideodecErrorArgumentPointer;
    std::uint64_t resourceSize = 0;
    if (!GuestCopyIn_nid_no_patch(&resourceSize, resource, sizeof(resourceSize))) return VideodecErrorArgumentPointer;
    if (configSize < sizeof(ConfigInfo) || resourceSize < sizeof(ResourceInfo)) return VideodecErrorStructSize;
    ConfigInfo localConfig{};
    if (!GuestCopyIn_nid_no_patch(&localConfig, config, sizeof(localConfig))) return VideodecErrorArgumentPointer;
    ResourceInfo localResource{};
    if (!GuestCopyIn_nid_no_patch(&localResource, resource, sizeof(localResource))) return VideodecErrorArgumentPointer;
    Ctrl localCtrl{};
    localCtrl.thisSize = sizeof(Ctrl);
    localCtrl.version = 1;
    localCtrl.handle = nullptr;
    if (!GuestWritable_nid_no_patch(ctrl, sizeof(localCtrl))) return VideodecErrorArgumentPointer;
    const auto decoderConfig = MapConfig(localConfig);
    SceVideodec2::DecoderMemoryInfo decoderMemory{};
    decoderMemory.thisSize = sizeof(SceVideodec2::DecoderMemoryInfo);
    decoderMemory.cpuMemorySize = localResource.cpuMemorySize;
    decoderMemory.cpuMemory = localResource.cpuMemory;
    decoderMemory.cpuGpuMemorySize = localResource.cpuGpuMemorySize;
    decoderMemory.cpuGpuMemory = localResource.cpuGpuMemory;
    decoderMemory.maxFrameBufferSize = localResource.maxFrameBufferSize;
    decoderMemory.frameBufferAlignment = localResource.frameBufferAlignment;
    try {
        sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&decoderConfig, &decoderMemory);

    } catch (const std::exception&) {
        return VideodecErrorConfigInfo;
    }
    std::uint64_t handle = 0;
    try {
        if (sceVideodec2CreateDecoder_nid_postfix(&decoderConfig, &decoderMemory, &handle) != 0) return VideodecErrorApiFail;
    } catch (const std::exception&) {
        return VideodecErrorApiFail;
    }
    auto decoder = std::make_shared<Decoder>();
    decoder->handle = handle;
    decoder->picture.resize(decoderMemory.maxFrameBufferSize);
    {
        std::lock_guard lock(decodersMutex);
        decoders.emplace(decoder.get(), decoder);
    }
    localCtrl.handle = decoder.get();
    if (!GuestCopyOut_nid_no_patch(ctrl, &localCtrl, sizeof(localCtrl))) {
        {
            std::lock_guard lock(decodersMutex);
            decoders.erase(decoder.get());
        }
        try {
            sceVideodec2DeleteDecoder_nid_postfix(handle);
        } catch (const std::exception&) {
        }
        return VideodecErrorArgumentPointer;
    }
    return 0;
}

int APS5_VABI sceVideodecDecode(Ctrl* ctrl, const SceVideodec2::InputData* input, FrameBuffer* frameBuffer, PictureInfo* picture) {
    if (ctrl == nullptr || input == nullptr || picture == nullptr) return VideodecErrorArgumentPointer;
    if (frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
    std::uint64_t ctrlSize = 0;
    if (!GuestCopyIn_nid_no_patch(&ctrlSize, ctrl, sizeof(ctrlSize))) return VideodecErrorArgumentPointer;
    std::uint64_t inputSize = 0;
    if (!GuestCopyIn_nid_no_patch(&inputSize, input, sizeof(inputSize))) return VideodecErrorArgumentPointer;
    std::uint64_t pictureSize = 0;
    if (!GuestCopyIn_nid_no_patch(&pictureSize, picture, sizeof(pictureSize))) return VideodecErrorArgumentPointer;
    std::uint64_t frameBufferInfoSize = 0;
    if (!GuestCopyIn_nid_no_patch(&frameBufferInfoSize, frameBuffer, sizeof(frameBufferInfoSize))) return VideodecErrorFrameBufferPointer;
    if (ctrlSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    if (inputSize < sizeof(SceVideodec2::InputData)) return VideodecErrorStructSize;
    if (pictureSize < sizeof(PictureInfo)) return VideodecErrorStructSize;
    if (frameBufferInfoSize < sizeof(FrameBuffer)) return VideodecErrorStructSize;
    Ctrl localCtrl{};
    if (!GuestCopyIn_nid_no_patch(&localCtrl, ctrl, sizeof(localCtrl))) return VideodecErrorArgumentPointer;
    SceVideodec2::InputData localInput{};
    if (!GuestCopyIn_nid_no_patch(&localInput, input, sizeof(localInput))) return VideodecErrorArgumentPointer;
    PictureInfo localPicture{};
    if (!GuestCopyIn_nid_no_patch(&localPicture, picture, sizeof(localPicture))) return VideodecErrorArgumentPointer;
    FrameBuffer localFrameBuffer{};
    if (!GuestCopyIn_nid_no_patch(&localFrameBuffer, frameBuffer, sizeof(localFrameBuffer))) return VideodecErrorFrameBufferPointer;
    auto decoder = FindDecoder(localCtrl.handle);
    if (!decoder) return VideodecErrorHandle;
    if (localInput.auData == nullptr) return VideodecErrorAuPointer;
    if (localInput.auSize == 0) return VideodecErrorAuSize;
    if (localInput.auSize > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        !GuestReadable_nid_no_patch(localInput.auData, static_cast<std::size_t>(localInput.auSize))) {
        return VideodecErrorAuPointer;
    }
    if (localFrameBuffer.frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
    if (!GuestWritable_nid_no_patch(picture, sizeof(localPicture))) return VideodecErrorArgumentPointer;
    std::lock_guard decoderLock(decoder->mutex);
    if (decoder->handle == 0) return VideodecErrorHandle;
    SceVideodec2::FrameBuffer target{};
    target.thisSize = sizeof(SceVideodec2::FrameBuffer);
    target.frameBuffer = decoder->picture.data();
    target.frameBufferSize = decoder->picture.size();
    SceVideodec2::OutputInfo output{sizeof(SceVideodec2::OutputInfo)};
    try {
        sceVideodec2Decode_nid_postfix(decoder->handle, &localInput, &target, &output);

    } catch (const std::exception&) {
        return VideodecErrorDecodeAu;
    }
    return PublishPicture(decoder->handle, output, localFrameBuffer, picture, localPicture);
}

int APS5_VABI sceVideodecDeleteDecoder(Ctrl* ctrl) {
    if (ctrl == nullptr) return VideodecErrorArgumentPointer;
    std::uint64_t ctrlSize = 0;
    if (!GuestCopyIn_nid_no_patch(&ctrlSize, ctrl, sizeof(ctrlSize))) return VideodecErrorArgumentPointer;
    if (ctrlSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    Ctrl localCtrl{};
    if (!GuestCopyIn_nid_no_patch(&localCtrl, ctrl, sizeof(localCtrl))) return VideodecErrorArgumentPointer;
    std::shared_ptr<Decoder> decoder;
    {
        std::lock_guard lock(decodersMutex);
        const auto found = decoders.find(localCtrl.handle);
        if (found == decoders.end()) return VideodecErrorHandle;
        decoder = std::move(found->second);
        decoders.erase(found);
    }
    std::lock_guard decoderLock(decoder->mutex);
    if (decoder->handle == 0) return VideodecErrorHandle;
    const auto handle = decoder->handle;
    decoder->handle = 0;
    try {
        sceVideodec2DeleteDecoder_nid_postfix(handle);

    } catch (const std::exception&) {
        return VideodecErrorApiFail;
    }
    return 0;
}

int APS5_VABI sceVideodecFlush(Ctrl* ctrl, FrameBuffer* frameBuffer, PictureInfo* picture) {
    if (ctrl == nullptr || picture == nullptr) return VideodecErrorArgumentPointer;
    if (frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
    std::uint64_t ctrlSize = 0;
    if (!GuestCopyIn_nid_no_patch(&ctrlSize, ctrl, sizeof(ctrlSize))) return VideodecErrorArgumentPointer;
    std::uint64_t pictureSize = 0;
    if (!GuestCopyIn_nid_no_patch(&pictureSize, picture, sizeof(pictureSize))) return VideodecErrorArgumentPointer;
    std::uint64_t frameBufferInfoSize = 0;
    if (!GuestCopyIn_nid_no_patch(&frameBufferInfoSize, frameBuffer, sizeof(frameBufferInfoSize))) return VideodecErrorFrameBufferPointer;
    if (ctrlSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    if (pictureSize < sizeof(PictureInfo)) return VideodecErrorStructSize;
    if (frameBufferInfoSize < sizeof(FrameBuffer)) return VideodecErrorStructSize;
    Ctrl localCtrl{};
    if (!GuestCopyIn_nid_no_patch(&localCtrl, ctrl, sizeof(localCtrl))) return VideodecErrorArgumentPointer;
    PictureInfo localPicture{};
    if (!GuestCopyIn_nid_no_patch(&localPicture, picture, sizeof(localPicture))) return VideodecErrorArgumentPointer;
    FrameBuffer localFrameBuffer{};
    if (!GuestCopyIn_nid_no_patch(&localFrameBuffer, frameBuffer, sizeof(localFrameBuffer))) return VideodecErrorFrameBufferPointer;
    auto decoder = FindDecoder(localCtrl.handle);
    if (!decoder) return VideodecErrorHandle;
    if (localFrameBuffer.frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
    if (!GuestWritable_nid_no_patch(picture, sizeof(localPicture))) return VideodecErrorArgumentPointer;
    std::lock_guard decoderLock(decoder->mutex);
    if (decoder->handle == 0) return VideodecErrorHandle;
    SceVideodec2::FrameBuffer target{};
    target.thisSize = sizeof(SceVideodec2::FrameBuffer);
    target.frameBuffer = decoder->picture.data();
    target.frameBufferSize = decoder->picture.size();
    SceVideodec2::OutputInfo output{sizeof(SceVideodec2::OutputInfo)};
    try {
        sceVideodec2Flush_nid_postfix(decoder->handle, &target, &output);

    } catch (const std::exception&) {
        return VideodecErrorDecodeAu;
    }
    return PublishPicture(decoder->handle, output, localFrameBuffer, picture, localPicture);
}

int APS5_VABI sceVideodecReset(Ctrl* ctrl) {
    if (ctrl == nullptr) return VideodecErrorArgumentPointer;
    std::uint64_t ctrlSize = 0;
    if (!GuestCopyIn_nid_no_patch(&ctrlSize, ctrl, sizeof(ctrlSize))) return VideodecErrorArgumentPointer;
    if (ctrlSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    Ctrl localCtrl{};
    if (!GuestCopyIn_nid_no_patch(&localCtrl, ctrl, sizeof(localCtrl))) return VideodecErrorArgumentPointer;
    auto decoder = FindDecoder(localCtrl.handle);
    if (!decoder) return VideodecErrorHandle;
    std::lock_guard decoderLock(decoder->mutex);
    if (decoder->handle == 0) return VideodecErrorHandle;
    try {
        sceVideodec2Reset_nid_postfix(decoder->handle);

    } catch (const std::exception&) {
        return VideodecErrorApiFail;
    }
    return 0;
}
int APS5_VABI sceVideodecMapMemory(Ctrl* ctrl, const void* memory) {
    if (ctrl == nullptr || memory == nullptr) return VideodecErrorArgumentPointer;
    std::uint64_t ctrlSize = 0;
    if (!GuestCopyIn_nid_no_patch(&ctrlSize, ctrl, sizeof(ctrlSize))) return VideodecErrorArgumentPointer;
    if (ctrlSize != sizeof(Ctrl)) return VideodecErrorStructSize;
    std::uint64_t memorySize = 0;
    if (!GuestCopyIn_nid_no_patch(&memorySize, memory, sizeof(memorySize))) return VideodecErrorArgumentPointer;
    if (memorySize != 0x20 && memorySize != 0x28 && memorySize != 0x48) return VideodecErrorStructSize;
    void* handle = nullptr;
    if (!GuestCopyIn_nid_no_patch(&handle, &ctrl->handle, sizeof(handle))) return VideodecErrorArgumentPointer;
    if (handle == nullptr) return VideodecErrorHandle;
    if (memorySize != 0x20) {
        unsigned char fields[0x1c]{};
        if (!GuestCopyIn_nid_no_patch(fields, static_cast<const unsigned char*>(memory) + 0x8, sizeof(fields))) return VideodecErrorArgumentPointer;
        std::uint32_t flags = 0;
        std::memcpy(&flags, fields + 0x18, sizeof(flags));
        if (flags != 0) return VideodecErrorApiFail;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideodecQueryResourceInfo(const ConfigInfo* config, ResourceInfo* resource) {
    if (config == nullptr || resource == nullptr) return VideodecErrorArgumentPointer;
    std::uint64_t configSize = 0;
    if (!GuestCopyIn_nid_no_patch(&configSize, config, sizeof(configSize))) return VideodecErrorArgumentPointer;
    std::uint64_t resourceSize = 0;
    if (!GuestCopyIn_nid_no_patch(&resourceSize, resource, sizeof(resourceSize))) return VideodecErrorArgumentPointer;
    if (configSize < sizeof(ConfigInfo) || resourceSize < sizeof(ResourceInfo)) return VideodecErrorStructSize;
    ConfigInfo localConfig{};
    if (!GuestCopyIn_nid_no_patch(&localConfig, config, sizeof(localConfig))) return VideodecErrorArgumentPointer;
    ResourceInfo localResource{};
    if (!GuestCopyIn_nid_no_patch(&localResource, resource, sizeof(localResource))) return VideodecErrorArgumentPointer;
    const auto decoderConfig = MapConfig(localConfig);
    SceVideodec2::DecoderMemoryInfo decoderMemory{};
    decoderMemory.thisSize = sizeof(SceVideodec2::DecoderMemoryInfo);
    try {
        sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&decoderConfig, &decoderMemory);

    } catch (const std::exception&) {
        return VideodecErrorConfigInfo;
    }
    localResource.cpuMemorySize = decoderMemory.cpuMemorySize;
    localResource.cpuGpuMemorySize = decoderMemory.cpuGpuMemorySize;
    localResource.maxFrameBufferSize = decoderMemory.maxFrameBufferSize;
    localResource.frameBufferAlignment = decoderMemory.frameBufferAlignment;
    if (!GuestCopyOut_nid_no_patch(resource, &localResource, sizeof(localResource))) {
        return VideodecErrorArgumentPointer;
    }
    return 0;
}

}

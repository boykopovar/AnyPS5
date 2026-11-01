#include "prx/libSceVideodec2/include/Api.hpp"
#include "prx/libSceVideodec2/include/PictureMetadata.hpp"
#include "prx/libSceVideodec/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <map>
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

struct Decoder {
    std::uint64_t handle = 0;
    std::vector<std::uint8_t> picture;
};

std::mutex decodersMutex;
std::map<const void*, Decoder*> decoders;

Decoder* FindDecoder(const void* handle) {
    if (handle == nullptr) return nullptr;
    std::lock_guard lock(decodersMutex);
    const auto found = decoders.find(handle);
    return found == decoders.end() ? nullptr : found->second;
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
    const auto bytes = static_cast<std::uint64_t>(geometry.pitch) * geometry.height * 3 / 2;
    if (frameBuffer->frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
    if (frameBuffer->frameBufferSize < bytes) return VideodecErrorFrameBufferSize;
    const auto sourcePitch = static_cast<std::uint64_t>(output.framePitchInBytes);
    const auto* source = static_cast<const std::uint8_t*>(output.frameBuffer);
    auto* destination = static_cast<std::uint8_t*>(frameBuffer->frameBuffer);
    for (std::uint32_t row = 0; row < geometry.height; ++row) {
        if (!GuestCopyOut_nid_no_patch(destination + static_cast<std::uint64_t>(row) * geometry.pitch, source + static_cast<std::uint64_t>(row) * sourcePitch, geometry.width)) {
            return VideodecErrorFrameBufferPointer;
        }
    }
    const auto* sourceChroma = source + sourcePitch * geometry.height;
    auto* destinationChroma = destination + static_cast<std::uint64_t>(geometry.pitch) * geometry.height;
    for (std::uint32_t row = 0; row < geometry.height / 2; ++row) {
        if (!GuestCopyOut_nid_no_patch(destinationChroma + static_cast<std::uint64_t>(row) * geometry.pitch, sourceChroma + static_cast<std::uint64_t>(row) * sourcePitch, geometry.width)) {
            return VideodecErrorFrameBufferPointer;
        }
    }
    return 0;
}

}

extern "C" {

int APS5_VABI sceVideodecCreateDecoder(const ConfigInfo* config, const ResourceInfo* resource, Ctrl* ctrl) {
    if (config == nullptr || resource == nullptr || ctrl == nullptr) return VideodecErrorArgumentPointer;
    std::uint32_t configSize = 0;
    if (!GuestCopyIn_nid_no_patch(&configSize, config, sizeof(configSize))) return VideodecErrorArgumentPointer;
    std::uint32_t resourceSize = 0;
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
    if (!GuestCopyOut_nid_no_patch(ctrl, &localCtrl, sizeof(localCtrl))) return VideodecErrorArgumentPointer;
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
    if (sceVideodec2CreateDecoder_nid_postfix(&decoderConfig, &decoderMemory, &handle) != 0) return VideodecErrorApiFail;
    auto* decoder = new Decoder();
    decoder->handle = handle;
    decoder->picture.resize(decoderMemory.maxFrameBufferSize);
    {
        std::lock_guard lock(decodersMutex);
        decoders.emplace(decoder, decoder);
    }
    localCtrl.handle = decoder;
    if (!GuestCopyOut_nid_no_patch(ctrl, &localCtrl, sizeof(localCtrl))) {
        {
            std::lock_guard lock(decodersMutex);
            decoders.erase(decoder);
        }
        delete decoder;
        sceVideodec2DeleteDecoder_nid_postfix(handle);
        return VideodecErrorArgumentPointer;
    }
    return 0;
}

int APS5_VABI sceVideodecDecode(Ctrl* ctrl, const SceVideodec2::InputData* input, FrameBuffer* frameBuffer, PictureInfo* picture) {
    if (ctrl == nullptr || input == nullptr || picture == nullptr) return VideodecErrorArgumentPointer;
    if (frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
    Ctrl localCtrl{};
    if (!GuestCopyIn_nid_no_patch(&localCtrl, ctrl, sizeof(localCtrl))) return VideodecErrorArgumentPointer;
    SceVideodec2::InputData localInput{};
    if (!GuestCopyIn_nid_no_patch(&localInput, input, sizeof(localInput))) return VideodecErrorArgumentPointer;
    PictureInfo localPicture{};
    if (!GuestCopyIn_nid_no_patch(&localPicture, picture, sizeof(localPicture))) return VideodecErrorArgumentPointer;
    FrameBuffer localFrameBuffer{};
    if (!GuestCopyIn_nid_no_patch(&localFrameBuffer, frameBuffer, sizeof(localFrameBuffer))) return VideodecErrorFrameBufferPointer;
    if (localCtrl.thisSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    if (localInput.thisSize < sizeof(SceVideodec2::InputData)) return VideodecErrorStructSize;
    if (localPicture.thisSize < sizeof(PictureInfo)) return VideodecErrorStructSize;
    if (localFrameBuffer.thisSize < sizeof(FrameBuffer)) return VideodecErrorStructSize;
    auto* decoder = FindDecoder(localCtrl.handle);
    if (decoder == nullptr) return VideodecErrorHandle;
    if (localInput.auData == nullptr) return VideodecErrorAuPointer;
    if (localInput.auSize == 0) return VideodecErrorAuSize;
    if (localFrameBuffer.frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
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
    SceVideodec2::Internal::PictureHeader header{};
    if (output.isValid) Videodec2PictureHeader_nid_no_patch(decoder->handle, output.frameBuffer, &header);
    const auto geometry = Measure(output, header);
    FillPictureInfo(geometry, output, &localPicture);
    localPicture.ptsData = header.pts;
    localPicture.attachedData = header.attached;
    if (!GuestCopyOut_nid_no_patch(picture, &localPicture, sizeof(localPicture))) return VideodecErrorArgumentPointer;
    if (!output.isValid) return 0;
    return CopyPicture(output, geometry, &localFrameBuffer);
}

int APS5_VABI sceVideodecDeleteDecoder(Ctrl* ctrl) {
    if (ctrl == nullptr) return VideodecErrorArgumentPointer;
    Ctrl localCtrl{};
    if (!GuestCopyIn_nid_no_patch(&localCtrl, ctrl, sizeof(localCtrl))) return VideodecErrorArgumentPointer;
    if (localCtrl.thisSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    Decoder* decoder = nullptr;
    {
        std::lock_guard lock(decodersMutex);
        const auto found = decoders.find(localCtrl.handle);
        if (found == decoders.end()) return VideodecErrorHandle;
        decoder = found->second;
        decoders.erase(found);
    }
    const auto handle = decoder->handle;
    delete decoder;
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
    Ctrl localCtrl{};
    if (!GuestCopyIn_nid_no_patch(&localCtrl, ctrl, sizeof(localCtrl))) return VideodecErrorArgumentPointer;
    PictureInfo localPicture{};
    if (!GuestCopyIn_nid_no_patch(&localPicture, picture, sizeof(localPicture))) return VideodecErrorArgumentPointer;
    FrameBuffer localFrameBuffer{};
    if (!GuestCopyIn_nid_no_patch(&localFrameBuffer, frameBuffer, sizeof(localFrameBuffer))) return VideodecErrorFrameBufferPointer;
    if (localCtrl.thisSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    if (localPicture.thisSize < sizeof(PictureInfo)) return VideodecErrorStructSize;
    if (localFrameBuffer.thisSize < sizeof(FrameBuffer)) return VideodecErrorStructSize;
    auto* decoder = FindDecoder(localCtrl.handle);
    if (decoder == nullptr) return VideodecErrorHandle;
    if (localFrameBuffer.frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
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
    SceVideodec2::Internal::PictureHeader header{};
    if (output.isValid) Videodec2PictureHeader_nid_no_patch(decoder->handle, output.frameBuffer, &header);
    const auto geometry = Measure(output, header);
    FillPictureInfo(geometry, output, &localPicture);
    localPicture.ptsData = header.pts;
    localPicture.attachedData = header.attached;
    if (!GuestCopyOut_nid_no_patch(picture, &localPicture, sizeof(localPicture))) return VideodecErrorArgumentPointer;
    if (!output.isValid) return 0;
    return CopyPicture(output, geometry, &localFrameBuffer);
}

int APS5_VABI sceVideodecReset(Ctrl* ctrl) {
    if (ctrl == nullptr) return VideodecErrorArgumentPointer;
    Ctrl localCtrl{};
    if (!GuestCopyIn_nid_no_patch(&localCtrl, ctrl, sizeof(localCtrl))) return VideodecErrorArgumentPointer;
    if (localCtrl.thisSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    auto* decoder = FindDecoder(localCtrl.handle);
    if (decoder == nullptr) return VideodecErrorHandle;
    try {
        sceVideodec2Reset_nid_postfix(decoder->handle);

    } catch (const std::exception&) {
        return VideodecErrorApiFail;
    }
    return 0;
}
int APS5_VABI sceVideodecMapMemory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVideodecQueryResourceInfo(const ConfigInfo* config, ResourceInfo* resource) {
    if (config == nullptr || resource == nullptr) return VideodecErrorArgumentPointer;
    std::uint32_t configSize = 0;
    if (!GuestCopyIn_nid_no_patch(&configSize, config, sizeof(configSize))) return VideodecErrorArgumentPointer;
    std::uint32_t resourceSize = 0;
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

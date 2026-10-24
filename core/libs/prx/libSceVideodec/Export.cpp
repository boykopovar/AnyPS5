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

struct ConfigInfo {
    std::uint64_t thisSize;
    std::uint32_t codecType;
    std::uint32_t profile;
    std::uint32_t maxLevel;
    std::int32_t maxFrameWidth;
    std::int32_t maxFrameHeight;
    std::int32_t maxDpbFrameCount;
    std::uint64_t videodecFlags;
};

struct ResourceInfo {
    std::uint64_t thisSize;
    std::uint64_t cpuMemorySize;
    void* cpuMemory;
    std::uint64_t cpuGpuMemorySize;
    void* cpuGpuMemory;
    std::uint64_t maxFrameBufferSize;
    std::uint32_t frameBufferAlignment;
};

struct Ctrl {
    std::uint64_t thisSize;
    void* handle;
    std::uint64_t version;
};

struct FrameBuffer {
    std::uint64_t thisSize;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
};

struct AvcInfo {
    std::uint32_t numUnitsInTick;
    std::uint32_t timeScale;
    std::uint8_t fixedFrameRateFlag;
    std::uint8_t aspectRatioIdc;
    std::uint16_t sarWidth;
    std::uint16_t sarHeight;
    std::uint8_t colourPrimaries;
    std::uint8_t transferCharacteristics;
    std::uint8_t matrixCoefficients;
    std::uint8_t videoFullRangeFlag;
    std::uint32_t frameCropLeftOffset;
    std::uint32_t frameCropRightOffset;
    std::uint32_t frameCropTopOffset;
    std::uint32_t frameCropBottomOffset;
};

union CodecInfo {
    std::uint8_t reserved[64];
    AvcInfo avc;
};

struct PictureInfo {
    std::uint64_t thisSize;
    std::uint32_t isValid;
    std::uint32_t codecType;
    std::uint32_t frameWidth;
    std::uint32_t framePitch;
    std::uint32_t frameHeight;
    std::uint32_t isErrorPic;
    std::uint64_t ptsData;
    std::uint64_t attachedData;
    CodecInfo codec;
};

struct InputData {
    std::uint64_t thisSize;
    const std::uint8_t* auData;
    std::uint64_t auSize;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
};

struct Decoder2ConfigInfo {
    std::uint64_t thisSize;
    std::uint32_t resourceType;
    std::uint32_t codecType;
    std::uint32_t profile;
    std::uint32_t maxLevel;
    std::int32_t maxFrameWidth;
    std::int32_t maxFrameHeight;
    std::int32_t maxDpbFrameCount;
    std::uint32_t decodePipelineDepth;
};

struct Decoder2MemoryInfo {
    std::uint64_t thisSize;
    std::uint64_t cpuMemorySize;
    void* cpuMemory;
    std::uint64_t gpuMemorySize;
    void* gpuMemory;
    std::uint64_t cpuGpuMemorySize;
    void* cpuGpuMemory;
    std::uint64_t maxFrameBufferSize;
    std::uint32_t frameBufferAlignment;
};

struct Decoder2FrameBuffer {
    std::uint64_t thisSize;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
    bool isAccepted;
};

struct Decoder2OutputInfo {
    std::uint64_t thisSize;
    bool isValid;
    bool isErrorFrame;
    std::uint8_t pictureCount;
    std::uint32_t codecType;
    std::uint32_t frameWidth;
    std::uint32_t framePitch;
    std::uint32_t frameHeight;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
    std::uint32_t frameFormat;
    std::uint32_t framePitchInBytes;
};

struct Decoder2AvcPictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint8_t idrPictureFlag;
    std::uint8_t profileIdc;
    std::uint8_t levelIdc;
    std::uint32_t picWidthInLumaSamples;
    std::uint32_t picHeightInLumaSamples;
};

struct Decoder2Crop {
    std::uint32_t left;
    std::uint32_t right;
    std::uint32_t top;
    std::uint32_t bottom;
};

static_assert(sizeof(ConfigInfo) == 40 && sizeof(ResourceInfo) == 56 && sizeof(Ctrl) == 24);
static_assert(offsetof(ConfigInfo, maxFrameWidth) == 20 && offsetof(ConfigInfo, videodecFlags) == 32);
static_assert(offsetof(ResourceInfo, cpuGpuMemorySize) == 24 && offsetof(ResourceInfo, maxFrameBufferSize) == 40);
static_assert(offsetof(ResourceInfo, frameBufferAlignment) == 48);
static_assert(sizeof(Decoder2ConfigInfo) == 40 && sizeof(Decoder2MemoryInfo) == 72);
static_assert(sizeof(FrameBuffer) == 24 && sizeof(InputData) == 48 && sizeof(PictureInfo) == 112);
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

}

extern "C" {
int APS5_VABI sceVideodec2QueryDecoderMemoryInfo_nid_postfix(const Decoder2ConfigInfo*, Decoder2MemoryInfo*);
int APS5_VABI sceVideodec2CreateDecoder_nid_postfix(const Decoder2ConfigInfo*, const Decoder2MemoryInfo*, std::uint64_t*);
int APS5_VABI sceVideodec2DeleteDecoder_nid_postfix(std::uint64_t);
int APS5_VABI sceVideodec2Decode_nid_postfix(std::uint64_t, const InputData*, Decoder2FrameBuffer*, Decoder2OutputInfo*);
int APS5_VABI sceVideodec2Flush_nid_postfix(std::uint64_t, Decoder2FrameBuffer*, Decoder2OutputInfo*);
int APS5_VABI sceVideodec2Reset_nid_postfix(std::uint64_t);
int APS5_VABI sceVideodec2GetAvcPictureInfo_nid_postfix(const Decoder2OutputInfo*, Decoder2AvcPictureInfo*, Decoder2AvcPictureInfo*);
void Videodec2PictureCrop_nid_no_patch(std::uint64_t, const void*, Decoder2Crop*);
}

namespace {

struct Geometry {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t pitch = 0;
    Decoder2Crop crop{};
};

Decoder2ConfigInfo MapConfig(const ConfigInfo& config) {
    Decoder2ConfigInfo decoderConfig{};
    decoderConfig.thisSize = sizeof(Decoder2ConfigInfo);
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

Geometry Measure(const Decoder2OutputInfo& output, const Decoder2Crop& crop) {
    Geometry geometry;
    geometry.width = output.frameWidth;
    geometry.height = output.frameHeight;
    geometry.crop = crop;
    geometry.pitch = AlignUp(geometry.width, PitchAlignment);
    return geometry;
}

void FillPictureInfo(const Geometry& geometry, const Decoder2OutputInfo& output, PictureInfo* picture) {
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

int CopyPicture(const Decoder2OutputInfo& output, const Geometry& geometry, const FrameBuffer* frameBuffer) {
    const auto bytes = static_cast<std::uint64_t>(geometry.pitch) * geometry.height * 3 / 2;
    if (frameBuffer->frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
    if (frameBuffer->frameBufferSize < bytes) return VideodecErrorFrameBufferSize;
    const auto sourcePitch = static_cast<std::uint64_t>(output.framePitchInBytes);
    const auto* source = static_cast<const std::uint8_t*>(output.frameBuffer);
    auto* destination = static_cast<std::uint8_t*>(frameBuffer->frameBuffer);
    for (std::uint32_t row = 0; row < geometry.height; ++row)
        std::memcpy(destination + static_cast<std::uint64_t>(row) * geometry.pitch, source + static_cast<std::uint64_t>(row) * sourcePitch, geometry.width);
    const auto* sourceChroma = source + sourcePitch * geometry.height;
    auto* destinationChroma = destination + static_cast<std::uint64_t>(geometry.pitch) * geometry.height;
    for (std::uint32_t row = 0; row < geometry.height / 2; ++row)
        std::memcpy(destinationChroma + static_cast<std::uint64_t>(row) * geometry.pitch, sourceChroma + static_cast<std::uint64_t>(row) * sourcePitch, geometry.width);
    return 0;
}

}

extern "C" {

int APS5_VABI sceVideodecCreateDecoder(const ConfigInfo* config, const ResourceInfo* resource, Ctrl* ctrl) {
    if (config == nullptr || resource == nullptr || ctrl == nullptr) return VideodecErrorArgumentPointer;
    if (config->thisSize < sizeof(ConfigInfo) || resource->thisSize < sizeof(ResourceInfo)) return VideodecErrorStructSize;
    const auto decoderConfig = MapConfig(*config);
    Decoder2MemoryInfo decoderMemory{};
    decoderMemory.thisSize = sizeof(Decoder2MemoryInfo);
    decoderMemory.cpuMemorySize = resource->cpuMemorySize;
    decoderMemory.cpuMemory = resource->cpuMemory;
    decoderMemory.cpuGpuMemorySize = resource->cpuGpuMemorySize;
    decoderMemory.cpuGpuMemory = resource->cpuGpuMemory;
    decoderMemory.maxFrameBufferSize = resource->maxFrameBufferSize;
    decoderMemory.frameBufferAlignment = resource->frameBufferAlignment;
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
    ctrl->thisSize = sizeof(Ctrl);
    ctrl->handle = decoder;
    ctrl->version = 1;
    return 0;
}

int APS5_VABI sceVideodecDecode(Ctrl* ctrl, const InputData* input, FrameBuffer* frameBuffer, PictureInfo* picture) {
    if (ctrl == nullptr || input == nullptr || picture == nullptr) return VideodecErrorArgumentPointer;
    if (frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
    if (ctrl->thisSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    if (input->thisSize < sizeof(InputData)) return VideodecErrorStructSize;
    if (picture->thisSize < sizeof(PictureInfo)) return VideodecErrorStructSize;
    if (frameBuffer->thisSize < sizeof(FrameBuffer)) return VideodecErrorStructSize;
    auto* decoder = FindDecoder(ctrl->handle);
    if (decoder == nullptr) return VideodecErrorHandle;
    if (input->auData == nullptr) return VideodecErrorAuPointer;
    if (input->auSize == 0) return VideodecErrorAuSize;
    if (frameBuffer->frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
    Decoder2FrameBuffer target{};
    target.thisSize = sizeof(Decoder2FrameBuffer);
    target.frameBuffer = decoder->picture.data();
    target.frameBufferSize = decoder->picture.size();
    Decoder2OutputInfo output{sizeof(Decoder2OutputInfo)};
    try {
        sceVideodec2Decode_nid_postfix(decoder->handle, input, &target, &output);
    } catch (const std::exception&) {
        return VideodecErrorDecodeAu;
    }
    Decoder2Crop crop{};
    if (output.isValid) Videodec2PictureCrop_nid_no_patch(decoder->handle, output.frameBuffer, &crop);
    const auto geometry = Measure(output, crop);
    Decoder2AvcPictureInfo avc{sizeof(Decoder2AvcPictureInfo)};
    if (output.isValid) sceVideodec2GetAvcPictureInfo_nid_postfix(&output, &avc, nullptr);
    FillPictureInfo(geometry, output, picture);
    picture->ptsData = avc.ptsData;
    picture->attachedData = avc.attachedData;
    if (!output.isValid) return 0;
    return CopyPicture(output, geometry, frameBuffer);
}

int APS5_VABI sceVideodecDeleteDecoder(Ctrl* ctrl) {
    if (ctrl == nullptr) return VideodecErrorArgumentPointer;
    if (ctrl->thisSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    Decoder* decoder = nullptr;
    {
        std::lock_guard lock(decodersMutex);
        const auto found = decoders.find(ctrl->handle);
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
    if (ctrl->thisSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    if (picture->thisSize < sizeof(PictureInfo)) return VideodecErrorStructSize;
    if (frameBuffer->thisSize < sizeof(FrameBuffer)) return VideodecErrorStructSize;
    auto* decoder = FindDecoder(ctrl->handle);
    if (decoder == nullptr) return VideodecErrorHandle;
    if (frameBuffer->frameBuffer == nullptr) return VideodecErrorFrameBufferPointer;
    Decoder2FrameBuffer target{};
    target.thisSize = sizeof(Decoder2FrameBuffer);
    target.frameBuffer = decoder->picture.data();
    target.frameBufferSize = decoder->picture.size();
    Decoder2OutputInfo output{sizeof(Decoder2OutputInfo)};
    try {
        sceVideodec2Flush_nid_postfix(decoder->handle, &target, &output);
    } catch (const std::exception&) {
        return VideodecErrorDecodeAu;
    }
    Decoder2Crop crop{};
    if (output.isValid) Videodec2PictureCrop_nid_no_patch(decoder->handle, output.frameBuffer, &crop);
    const auto geometry = Measure(output, crop);
    Decoder2AvcPictureInfo avc{sizeof(Decoder2AvcPictureInfo)};
    if (output.isValid) sceVideodec2GetAvcPictureInfo_nid_postfix(&output, &avc, nullptr);
    FillPictureInfo(geometry, output, picture);
    picture->ptsData = avc.ptsData;
    picture->attachedData = avc.attachedData;
    if (!output.isValid) return 0;
    return CopyPicture(output, geometry, frameBuffer);
}

int APS5_VABI sceVideodecReset(Ctrl* ctrl) {
    if (ctrl == nullptr) return VideodecErrorArgumentPointer;
    if (ctrl->thisSize < sizeof(Ctrl)) return VideodecErrorStructSize;
    auto* decoder = FindDecoder(ctrl->handle);
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
    if (config->thisSize < sizeof(ConfigInfo) || resource->thisSize < sizeof(ResourceInfo)) return VideodecErrorStructSize;
    const auto decoderConfig = MapConfig(*config);
    Decoder2MemoryInfo decoderMemory{};
    decoderMemory.thisSize = sizeof(Decoder2MemoryInfo);
    try {
        sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&decoderConfig, &decoderMemory);
    } catch (const std::exception&) {
        return VideodecErrorConfigInfo;
    }
    resource->cpuMemorySize = decoderMemory.cpuMemorySize;
    resource->cpuGpuMemorySize = decoderMemory.cpuGpuMemorySize;
    resource->maxFrameBufferSize = decoderMemory.maxFrameBufferSize;
    resource->frameBufferAlignment = decoderMemory.frameBufferAlignment;
    return 0;
}

}

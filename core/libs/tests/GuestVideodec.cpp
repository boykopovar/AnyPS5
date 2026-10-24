#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include "prx/libc/include/General.hpp"
#include "H264Fixture.hpp"

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

static_assert(sizeof(PictureInfo) == 112 && sizeof(AvcInfo) == 36);
static_assert(sizeof(ConfigInfo) == 40 && sizeof(ResourceInfo) == 56 && sizeof(Ctrl) == 24);
static_assert(sizeof(Decoder2ConfigInfo) == 40 && sizeof(Decoder2MemoryInfo) == 72);

extern "C" {
int APS5_VABI sceVideodecCreateDecoder(const ConfigInfo*, const ResourceInfo*, Ctrl*);
int APS5_VABI sceVideodecDecode(Ctrl*, const InputData*, FrameBuffer*, PictureInfo*);
int APS5_VABI sceVideodecDeleteDecoder(Ctrl*);
int APS5_VABI sceVideodecFlush(Ctrl*, FrameBuffer*, PictureInfo*);
int APS5_VABI sceVideodecReset(Ctrl*);
int APS5_VABI sceVideodecQueryResourceInfo(const ConfigInfo*, ResourceInfo*);
int APS5_VABI sceVideodec2QueryDecoderMemoryInfo_nid_postfix(const Decoder2ConfigInfo*, Decoder2MemoryInfo*);
}

namespace {

using namespace H264Fixture;

constexpr int ErrorApiFail = static_cast<int>(0x80C10000);
constexpr int ErrorStructSize = static_cast<int>(0x80C10002);
constexpr int ErrorHandle = static_cast<int>(0x80C10003);
constexpr int ErrorAuSize = static_cast<int>(0x80C10009);
constexpr int ErrorAuPointer = static_cast<int>(0x80C1000A);
constexpr int ErrorFrameBufferSize = static_cast<int>(0x80C1000B);
constexpr int ErrorFrameBufferPointer = static_cast<int>(0x80C1000C);
constexpr int ErrorConfigInfo = static_cast<int>(0x80C1000E);
constexpr int ErrorArgumentPointer = static_cast<int>(0x80C1000F);

constexpr std::uint32_t Pitch = 64;
constexpr std::uint32_t PictureHeight = 48;
constexpr std::size_t FrameBytes = static_cast<std::size_t>(Pitch) * PictureHeight * 3 / 2;

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

struct Session {
    Ctrl ctrl{sizeof(Ctrl)};
    std::vector<std::uint8_t> buffer;

    Session() : buffer(FrameBytes) {
        const ConfigInfo config{sizeof(ConfigInfo), 1, 100, 42, static_cast<std::int32_t>(Width),
                                static_cast<std::int32_t>(CodedHeight), 4, 0};
        const ResourceInfo resource{sizeof(ResourceInfo), 16u << 20u, nullptr, 16u << 20u, nullptr,
                                    FrameBytes, 256};
        check(sceVideodecCreateDecoder(&config, &resource, &ctrl) == 0, "decoder creation failed");
        check(ctrl.handle != nullptr && ctrl.version == 1, "the control block was not filled");
    }

    ~Session() { sceVideodecDeleteDecoder(&ctrl); }

    FrameBuffer MakeTarget() { return FrameBuffer{sizeof(FrameBuffer), buffer.data(), buffer.size()}; }
};

void testDecode(const std::vector<std::vector<std::uint8_t>>& units, std::uint32_t cropTop, std::uint32_t cropBottom) {
    Session session;
    check(units.size() == PictureHashes.size(), "unexpected access unit count");
    std::vector<std::uint64_t> hashes;
    std::vector<std::uint64_t> timestamps;
    const auto collect = [&](PictureInfo& picture) {
        if (!picture.isValid) return;
        check(picture.framePitch == Pitch && picture.frameHeight == PictureHeight, "unexpected picture geometry");
        check(picture.frameWidth == Width, "unexpected picture width");
        check(picture.codecType == 0 && !picture.isErrorPic, "unexpected picture flags");
        check(picture.codec.avc.frameCropTopOffset == cropTop && picture.codec.avc.frameCropBottomOffset == cropBottom, "unexpected crop");
        hashes.push_back(hashNv12(session.buffer.data(), Pitch, PictureHeight));
        timestamps.push_back(picture.ptsData);
    };
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0xa0 + unit};
        auto target = session.MakeTarget();
        PictureInfo picture{sizeof(PictureInfo)};
        const auto status = sceVideodecDecode(&session.ctrl, &input, &target, &picture);
        check(status == 0, "decode of unit " + std::to_string(unit) + " failed with " + std::to_string(status));
        collect(picture);
    }
    for (;;) {
        auto target = session.MakeTarget();
        PictureInfo picture{sizeof(PictureInfo)};
        check(sceVideodecFlush(&session.ctrl, &target, &picture) == 0, "flush failed");
        if (!picture.isValid) break;
        collect(picture);
    }
    check(hashes.size() == PictureHashes.size(), "missing pictures");
    for (std::size_t index = 0; index < hashes.size(); ++index) {
        check(hashes[index] == PictureHashes[index], "picture " + std::to_string(index) + " differs from the reference");
        const auto unit = DisplayOrderUnits[index];
        check(timestamps[index] == 1000 + unit, "pictures out of display order");
    }
}

void testDecode() {
    testDecode(accessUnits(false), 0, (CodedHeight - Height) / 2);
}

void testDecodeCropped() {
    const auto stream = croppedStream();
    testDecode(accessUnits(stream.data(), stream.size(), false), 4, 4);
}

void testErrors() {
    const ConfigInfo config{sizeof(ConfigInfo), 1, 100, 42, static_cast<std::int32_t>(Width),
                            static_cast<std::int32_t>(CodedHeight), 4, 0};
    const ResourceInfo resource{sizeof(ResourceInfo), 16u << 20u, nullptr, 16u << 20u, nullptr, FrameBytes, 256};
    Ctrl ctrl{sizeof(Ctrl)};
    check(sceVideodecCreateDecoder(nullptr, &resource, &ctrl) == ErrorArgumentPointer, "a null configuration was accepted");
    check(sceVideodecCreateDecoder(&config, &resource, nullptr) == ErrorArgumentPointer, "a null control block was accepted");
    const ConfigInfo shortConfig{sizeof(ConfigInfo) - 8};
    check(sceVideodecCreateDecoder(&shortConfig, &resource, &ctrl) == ErrorStructSize, "a short configuration was accepted");
    check(sceVideodecCreateDecoder(&config, &resource, &ctrl) == 0, "decoder creation failed");
    Ctrl foreign{sizeof(Ctrl)};
    foreign.handle = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234));
    check(sceVideodecDeleteDecoder(&foreign) == ErrorHandle, "an unknown decoder was deleted");
    check(sceVideodecReset(&foreign) == ErrorHandle, "an unknown decoder was reset");
    const InputData empty{sizeof(InputData), nullptr, 0, 0, 0, 0};
    FrameBuffer frame{sizeof(FrameBuffer), nullptr, 0};
    PictureInfo picture{sizeof(PictureInfo)};
    check(sceVideodecDecode(nullptr, &empty, &frame, &picture) == ErrorArgumentPointer, "a null control block was accepted");
    check(sceVideodecDecode(&ctrl, &empty, &frame, &picture) == ErrorAuPointer, "a null access unit was accepted");
    const InputData sized{sizeof(InputData), reinterpret_cast<const std::uint8_t*>(&config), 0, 0, 0, 0};
    check(sceVideodecDecode(&ctrl, &sized, &frame, &picture) == ErrorAuSize, "an empty access unit was accepted");
    FrameBuffer missing{sizeof(FrameBuffer), nullptr, FrameBytes};
    const auto units = accessUnits(false);
    const InputData input{sizeof(InputData), units[0].data(), units[0].size(), 0, 0, 0};
    check(sceVideodecDecode(&ctrl, &input, &missing, &picture) == ErrorFrameBufferPointer, "a null frame buffer was accepted");
    std::vector<std::uint8_t> storage(static_cast<std::size_t>(FrameBytes));
    FrameBuffer shortBuffer{sizeof(FrameBuffer), storage.data(), FrameBytes - 1};
    int refusals = 0;
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData unit0{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0xa0 + unit};
        const auto status = sceVideodecDecode(&ctrl, &unit0, &shortBuffer, &picture);
        check(status == 0 || status == ErrorFrameBufferSize, "a decode failed");
        if (status == ErrorFrameBufferSize) ++refusals;
    }
    check(refusals > 0, "no picture was refused for the short frame buffer");
    check(sceVideodecDeleteDecoder(&ctrl) == 0, "the decoder was not deleted");
    check(sceVideodecDeleteDecoder(&ctrl) == ErrorHandle, "the decoder was deleted twice");
}

void testReset() {
    Session session;
    const auto units = accessUnits(false);
    const InputData input{sizeof(InputData), units[0].data(), units[0].size(), 1000, 0, 0xa0};
    auto target = session.MakeTarget();
    PictureInfo picture{sizeof(PictureInfo)};
    check(sceVideodecDecode(&session.ctrl, &input, &target, &picture) == 0, "decode failed");
    check(sceVideodecReset(&session.ctrl) == 0, "reset failed");
    check(sceVideodecFlush(&session.ctrl, &target, &picture) == 0, "flush failed");
    check(!picture.isValid, "a reset decoder produced a picture");
}

void testQueryResourceInfo(std::int32_t width, std::int32_t height, std::uint32_t profile, std::uint32_t level) {
    const ConfigInfo config{sizeof(ConfigInfo), 1, profile, level, width, height, 4, 0};
    ResourceInfo resource{sizeof(ResourceInfo)};
    check(sceVideodecQueryResourceInfo(&config, &resource) == 0, "the resource query failed");
    const Decoder2ConfigInfo mapped{sizeof(Decoder2ConfigInfo), 1, 1, profile, level, width, height, 4, 1};
    Decoder2MemoryInfo memory{sizeof(Decoder2MemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&mapped, &memory) == 0, "the Videodec2 memory query failed");
    check(resource.thisSize == sizeof(ResourceInfo), "the resource information size was changed");
    check(resource.cpuMemorySize == memory.cpuMemorySize, "the CPU memory size differs from libSceVideodec2");
    check(resource.cpuGpuMemorySize == memory.cpuGpuMemorySize, "the CPU-GPU memory size differs from libSceVideodec2");
    check(resource.maxFrameBufferSize == memory.maxFrameBufferSize, "the frame buffer size differs from libSceVideodec2");
    check(resource.frameBufferAlignment == memory.frameBufferAlignment, "the frame buffer alignment differs from libSceVideodec2");
}

void testQueryResourceInfo() {
    testQueryResourceInfo(static_cast<std::int32_t>(Width), static_cast<std::int32_t>(CodedHeight), 100, 42);
    testQueryResourceInfo(1920, 1080, 100, 42);
    testQueryResourceInfo(3840, 2160, 100, 51);
    const ConfigInfo config{sizeof(ConfigInfo), 1, 100, 42, static_cast<std::int32_t>(Width),
                            static_cast<std::int32_t>(CodedHeight), 4, 0};
    ResourceInfo resource{sizeof(ResourceInfo)};
    check(sceVideodecQueryResourceInfo(&config, &resource) == 0, "the resource query failed");
    check(resource.maxFrameBufferSize >= FrameBytes, "the frame buffer size does not hold a decoded picture");
    Ctrl ctrl{sizeof(Ctrl)};
    check(sceVideodecCreateDecoder(&config, &resource, &ctrl) == 0, "the queried resources were refused");
    check(sceVideodecDeleteDecoder(&ctrl) == 0, "the decoder was not deleted");
    check(sceVideodecQueryResourceInfo(nullptr, &resource) == ErrorArgumentPointer, "a null configuration was accepted");
    check(sceVideodecQueryResourceInfo(&config, nullptr) == ErrorArgumentPointer, "null resource information was accepted");
    const ResourceInfo sentinel{sizeof(ResourceInfo), 7, nullptr, 7, nullptr, 7, 7};
    auto refused = sentinel;
    auto shortConfig = config;
    shortConfig.thisSize = sizeof(ConfigInfo) - 8;
    check(sceVideodecQueryResourceInfo(&shortConfig, &refused) == ErrorStructSize, "a short configuration was accepted");
    auto shortResource = sentinel;
    shortResource.thisSize = sizeof(ResourceInfo) - 8;
    check(sceVideodecQueryResourceInfo(&config, &shortResource) == ErrorStructSize, "short resource information was accepted");
    auto unsized = config;
    unsized.maxFrameWidth = 0;
    check(sceVideodecQueryResourceInfo(&unsized, &refused) == ErrorConfigInfo, "a configuration without a frame size was accepted");
    check(refused.cpuMemorySize == 7 && refused.cpuGpuMemorySize == 7 && refused.maxFrameBufferSize == 7 && refused.frameBufferAlignment == 7,
          "a refused query wrote the resource information");
    check(shortResource.cpuMemorySize == 7 && shortResource.maxFrameBufferSize == 7, "a refused query wrote the resource information");
}

}

int main() {
    try {
        testDecode();
        testDecodeCropped();
        testErrors();
        testReset();
        testQueryResourceInfo();
        std::puts("Videodec tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

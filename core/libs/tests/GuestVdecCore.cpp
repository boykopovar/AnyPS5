#include "prx/libSceVdecCore/include/Api.hpp"
#include "prx/libSceVdecCore/include/Decoder.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80C00001);
constexpr int InvalidCodecType = static_cast<int>(0x80C00003);
constexpr std::uint32_t CodecAvc = 0;
constexpr std::uint32_t CodecHevc = 4;
constexpr std::uint32_t DecoderVdec2 = 2;
constexpr std::uint32_t DecoderVdec2Special2 = 8;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "VdecCore: %s\n", message);
        std::abort();
    }
}

void* Resolve(const char* name) {
#ifdef _WIN32
    const HMODULE module = GetModuleHandleA("libSceVdecCore.prx");
    Require(module != nullptr, "library module");
    return reinterpret_cast<void*>(GetProcAddress(module, name));
#else
    return dlsym(RTLD_DEFAULT, name);
#endif
}

VdecCoreConfigInfo Config(std::int32_t width, std::int32_t height, std::int32_t horizontalAlign, std::int32_t dpb) {
    VdecCoreConfigInfo config{};
    config.decoderType = DecoderVdec2;
    config.codecType = CodecAvc;
    config.maxFrameWidth = width;
    config.maxFrameHeight = height;
    config.horizontalAlign = horizontalAlign;
    config.maxDpbFrameCount = dpb;
    return config;
}

void CheckExports() {
    for (const char* name : {"sceVdecCoreQueryInstanceSize", "sceVdecCoreQueryFrameBufferInfo", "sceVdecCoreQueryComputeResourceInfo",
                             "sceVdecCoreCreateDecoder", "sceVdecCoreDeleteDecoder", "sceVdecCoreMapMemoryBlock"}) {
        Require(Resolve(name) != nullptr, name);
    }
}

void CheckInstanceSize() {
    VdecCoreConfigInfo config = Config(0, 0, 0, 0);
    std::uint64_t instance = 1, compute = 1, reserved = 1;
    Require(sceVdecCoreQueryInstanceSize(nullptr, &instance, &compute, &reserved) == InvalidArgument, "instance size without a config");
    config.codecType = 5;
    Require(sceVdecCoreQueryInstanceSize(&config, &instance, &compute, &reserved) == InvalidCodecType, "instance size for codec type 5");
    Require(instance == 1 && compute == 1 && reserved == 1, "a refused query leaves its outputs alone");
    config.codecType = CodecHevc;
    Require(sceVdecCoreQueryInstanceSize(&config, &instance, &compute, &reserved) == 0, "instance size");
    Require(instance == 0x40000 && compute == 0x10000 && reserved == 0, "instance, compute and reserved sizes");
    Require(sceVdecCoreQueryInstanceSize(&config, nullptr, nullptr, nullptr) == 0, "instance size with no outputs");
}

void RequireFrameBuffer(const VdecCoreConfigInfo& config, std::uint32_t size, std::uint32_t minCount, std::uint32_t align, std::uint32_t width, std::uint32_t height, const char* message) {
    VdecCoreFrameBufferInfo info{};
    Require(sceVdecCoreQueryFrameBufferInfo(&config, &info) == 0, message);
    Require(info.frameBufferSize == size, message);
    Require(info.frameBufferAlignment == 256, message);
    Require(info.minFrameBufferCount == minCount, message);
    Require(info.horizontalAlignment == align, message);
    Require(info.frameWidth == width && info.frameHeight == height, message);
}

void CheckFrameBufferInfo() {
    VdecCoreFrameBufferInfo info{};
    VdecCoreConfigInfo config = Config(0, 0, 0, 0);
    Require(sceVdecCoreQueryFrameBufferInfo(nullptr, &info) == InvalidArgument, "frame buffer info without a config");
    Require(sceVdecCoreQueryFrameBufferInfo(&config, nullptr) == InvalidArgument, "frame buffer info without an output");
    config.codecType = 5;
    config.maxDpbFrameCount = 17;
    Require(sceVdecCoreQueryFrameBufferInfo(&config, &info) == InvalidCodecType, "the codec type is checked before the DPB count");
    config.codecType = CodecAvc;
    Require(sceVdecCoreQueryFrameBufferInfo(&config, &info) == InvalidArgument, "DPB count 17");
    config.maxDpbFrameCount = -2;
    Require(sceVdecCoreQueryFrameBufferInfo(&config, &info) == InvalidArgument, "DPB count -2");

    RequireFrameBuffer(Config(0, 0, 0, 0), 0x340000, 17, 256, 1920, 1080, "1920x1080 defaults");
    RequireFrameBuffer(Config(64, 40, 256, 0), 0x10000, 17, 256, 64, 40, "64x40, DPB 0");
    RequireFrameBuffer(Config(64, 40, 256, -1), 0x10000, 17, 256, 64, 40, "64x40, DPB -1");
    RequireFrameBuffer(Config(64, 40, 256, 16), 0x10000, 17, 256, 64, 40, "64x40, DPB 16");
    RequireFrameBuffer(Config(64, 40, 256, 4), 0x10000, 5, 256, 64, 40, "64x40, DPB 4");
    RequireFrameBuffer(Config(3840, 2160, 0, 0), 0xbe0000, 17, 256, 3840, 2160, "3840x2160");
    RequireFrameBuffer(Config(1280, 720, 100, 0), 0x160000, 17, 100, 1280, 720, "1280x720 with a horizontal alignment of 100");
}

void CheckComputeResourceInfo() {
    Require(sceVdecCoreQueryComputeResourceInfo(nullptr) == InvalidArgument, "compute resource info without an output");
    VdecCoreComputeResourceInfo info{7, 7, 7, 7};
    Require(sceVdecCoreQueryComputeResourceInfo(&info) == 0, "compute resource info");
    Require(info.memoryType == 0 && info.size == 0x10000 && info.alignment == 0x10000 && info.reserved == 0, "compute resource sizes");
}

void CheckCreateAndDelete() {
    VdecCoreConfigInfo config = Config(0, 0, 0, 0);
    VdecCoreMemoryInfo memory{};
    void* decoder = &memory;
    Require(sceVdecCoreCreateDecoder(nullptr, &memory, nullptr, &decoder) == InvalidArgument, "create without a config");
    Require(sceVdecCoreCreateDecoder(&config, &memory, nullptr, nullptr) == InvalidArgument, "create without an output");
    config.decoderType = DecoderVdec2Special2 + 1;
    Require(sceVdecCoreCreateDecoder(&config, &memory, nullptr, &decoder) == InvalidArgument, "create with decoder type 9");
    config.decoderType = DecoderVdec2;
    config.codecType = 5;
    Require(sceVdecCoreCreateDecoder(&config, &memory, nullptr, &decoder) == InvalidCodecType, "create with codec type 5");
    Require(decoder == &memory, "a refused create leaves the output alone");

    config.codecType = CodecAvc;
    void* avc = nullptr;
    Require(sceVdecCoreCreateDecoder(&config, &memory, nullptr, &avc) == 0 && avc != nullptr, "create an AVC decoder");
    config.codecType = CodecHevc;
    config.decoderType = DecoderVdec2Special2;
    void* hevc = nullptr;
    Require(sceVdecCoreCreateDecoder(&config, nullptr, nullptr, &hevc) == 0 && hevc != nullptr && hevc != avc, "create an HEVC decoder of type 8");

    Require(sceVdecCoreDeleteDecoder(nullptr) == InvalidArgument, "delete without a decoder");
    Require(sceVdecCoreDeleteDecoder(avc) == 0, "delete the AVC decoder");
    Require(sceVdecCoreDeleteDecoder(hevc) == 0, "delete the HEVC decoder");
}

std::vector<void*> Slots(void* decoder) {
    std::array<void*, 16> slots{};
    const std::uint32_t count = VdecCoreFrameBufferSlots_nid_no_patch(decoder, slots.data(), static_cast<std::uint32_t>(slots.size()));
    Require(count <= slots.size(), "slot count");
    return std::vector<void*>(slots.begin(), slots.begin() + count);
}

void CheckMapMemoryBlock() {
    Require(sceVdecCoreMapMemoryBlock(nullptr, nullptr, 0) == InvalidArgument, "map without a decoder");
    const VdecCoreConfigInfo config = Config(64, 40, 256, 0);
    VdecCoreFrameBufferInfo info{};
    Require(sceVdecCoreQueryFrameBufferInfo(&config, &info) == 0 && info.frameBufferSize == 0x10000, "64x40 frame buffer size");
    void* decoder = nullptr;
    Require(sceVdecCoreCreateDecoder(&config, nullptr, nullptr, &decoder) == 0, "create the decoder to map into");
    Require(Slots(decoder).empty(), "a new decoder has no frame buffer slots");

    std::vector<std::uint8_t> memory(0x50000);
    std::uint8_t* base = memory.data();
    const std::array<VdecCoreMemoryBlock, 3> blocks{{
        {base, 0x28000, 256, 0},
        {base + 0x30000, 0x8000, 256, 0},
        {nullptr, 0x20000, 256, 0},
    }};
    Require(sceVdecCoreMapMemoryBlock(decoder, blocks.data(), static_cast<std::uint32_t>(blocks.size())) == 0, "map three blocks");
    const std::vector<void*> cut = Slots(decoder);
    Require(cut.size() == 2, "a 2.5-slot block gives two slots; a short block and a null block give none");
    Require(cut[0] == base && cut[1] == base + info.frameBufferSize, "slots start at the block and are one frame buffer apart");

    Require(sceVdecCoreMapMemoryBlock(decoder, nullptr, 3) == 0, "map with no blocks");
    Require(sceVdecCoreMapMemoryBlock(decoder, blocks.data(), 0) == 0, "map zero blocks");
    Require(Slots(decoder).size() == 2, "mapping nothing adds no slots");

    const VdecCoreMemoryBlock exact{base + 0x40000, 0x10000, 256, 0};
    Require(sceVdecCoreMapMemoryBlock(decoder, &exact, 1) == 0, "map a block of exactly one frame buffer");
    const std::vector<void*> grown = Slots(decoder);
    Require(grown.size() == 3 && grown[2] == base + 0x40000, "an exact block adds one slot after the earlier ones");
    Require(sceVdecCoreDeleteDecoder(decoder) == 0, "delete the mapped decoder");
}

}

int main() {
    CheckExports();
    CheckInstanceSize();
    CheckFrameBufferInfo();
    CheckComputeResourceInfo();
    CheckCreateAndDelete();
    CheckMapMemoryBlock();
    return 0;
}

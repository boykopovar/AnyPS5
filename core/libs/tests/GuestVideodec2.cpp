#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "prx/libc/include/General.hpp"
#include "H264Fixture.hpp"

struct DecoderConfigInfo {
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

struct DecoderMemoryInfo {
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

struct InputData {
    std::uint64_t thisSize;
    const std::uint8_t* auData;
    std::uint64_t auSize;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
};

struct FrameBuffer {
    std::uint64_t thisSize;
    void* frameBuffer;
    std::uint64_t frameBufferSize;
    bool isAccepted;
};

struct OutputInfo {
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

struct AvcPictureInfo {
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

struct Vp9PictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint32_t profile;
    std::uint32_t level;
    std::uint32_t keyFrameFlag;
    std::uint32_t intraOnly;
    std::uint8_t bitDepth;
    std::uint8_t colorRange;
    std::uint32_t colorSpace;
    std::uint32_t numFrames;
    std::uint32_t frameWidth;
    std::uint32_t frameHeight;
    std::uint32_t renderWidth;
    std::uint32_t renderHeight;
};

static_assert(sizeof(Vp9PictureInfo) == 88);
static_assert(offsetof(Vp9PictureInfo, colorSpace) == 60);

extern "C" {
int APS5_VABI sceVideodec2QueryDecoderMemoryInfo_nid_postfix(const DecoderConfigInfo*, DecoderMemoryInfo*);
int APS5_VABI sceVideodec2CreateDecoder_nid_postfix(const DecoderConfigInfo*, const DecoderMemoryInfo*, std::uint64_t*);
int APS5_VABI sceVideodec2DeleteDecoder_nid_postfix(std::uint64_t);
int APS5_VABI sceVideodec2Decode_nid_postfix(std::uint64_t, const InputData*, FrameBuffer*, OutputInfo*);
int APS5_VABI sceVideodec2Flush_nid_postfix(std::uint64_t, FrameBuffer*, OutputInfo*);
int APS5_VABI sceVideodec2Reset_nid_postfix(std::uint64_t);
int APS5_VABI sceVideodec2GetPictureInfo_nid_postfix(const OutputInfo*, AvcPictureInfo*, void*);
int APS5_VABI sceVideodec2GetVp9PictureInfo_nid_postfix(const OutputInfo*, Vp9PictureInfo*);
int APS5_VABI sceVideodec2GetAvcPictureInfo_nid_postfix(const OutputInfo*, AvcPictureInfo*, AvcPictureInfo*);
}

namespace {

using namespace H264Fixture;


constexpr std::uint32_t CodecHevc = 974921;
constexpr std::uint32_t CodecVp9 = 2382845;

constexpr std::array<std::uint8_t, 933> HevcStream{
    0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x10, 0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01, 0xff,
    0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x1e,
    0x95, 0x90, 0x09, 0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
    0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x1e, 0xa0, 0x20, 0x82, 0x96, 0x59, 0x59, 0x2e,
    0x4c, 0xaf, 0x01, 0x68, 0x08, 0x00, 0x00, 0x03, 0x00, 0x08, 0x00, 0x00, 0x03, 0x00, 0xf0, 0x40,
    0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xc1, 0x73, 0xd0, 0x89, 0x00, 0x00, 0x01, 0x28, 0x01, 0xaf,
    0x08, 0xe0, 0xbb, 0xcb, 0xc1, 0x71, 0x26, 0x0f, 0xbf, 0x6c, 0xcb, 0x4d, 0x2b, 0x2c, 0xf4, 0x08,
    0xe0, 0x17, 0xeb, 0x16, 0xcc, 0x07, 0xd8, 0xdd, 0x11, 0xf8, 0x5e, 0x73, 0x79, 0x57, 0xcb, 0x69,
    0xa3, 0x1c, 0x9c, 0x43, 0x14, 0xdb, 0x61, 0x0e, 0xc5, 0xc6, 0x9c, 0x9c, 0xf8, 0xf2, 0x07, 0x35,
    0xf1, 0x98, 0x6c, 0x1a, 0x86, 0x53, 0x5a, 0xcd, 0x4d, 0xda, 0xff, 0xfb, 0x01, 0xda, 0xc3, 0x61,
    0xb7, 0x8c, 0xae, 0xc9, 0x76, 0xe4, 0x05, 0xd6, 0xfd, 0xe4, 0xcd, 0x93, 0x2c, 0xb1, 0x41, 0x29,
    0xce, 0x22, 0xf7, 0x77, 0x64, 0x0f, 0x1f, 0xd2, 0xc3, 0x68, 0x32, 0x2f, 0x83, 0xd7, 0x53, 0xbe,
    0x23, 0xff, 0x8c, 0x46, 0x18, 0x33, 0x90, 0x28, 0xa3, 0xef, 0xd7, 0xb1, 0x46, 0x84, 0xa0, 0xc2,
    0xd4, 0xd9, 0xce, 0xdf, 0x0a, 0x05, 0xec, 0xad, 0x63, 0xa4, 0x78, 0x27, 0x80, 0x4e, 0x3b, 0x4c,
    0xa6, 0x42, 0xbc, 0xe3, 0x39, 0x4d, 0xc4, 0xd3, 0x44, 0xf1, 0x22, 0xd0, 0x64, 0x9f, 0xd0, 0x88,
    0x7f, 0x39, 0x6d, 0x06, 0xc7, 0x79, 0x75, 0x2a, 0x44, 0x3d, 0x21, 0x41, 0x53, 0x60, 0x8d, 0x08,
    0x9f, 0xd0, 0x34, 0xa6, 0x72, 0x70, 0x3b, 0xa9, 0xfa, 0x62, 0xb1, 0x19, 0x9b, 0x0b, 0xa2, 0xfc,
    0xb4, 0xb3, 0x2c, 0x58, 0xad, 0xc6, 0xf3, 0xea, 0xc4, 0xd0, 0xf9, 0xea, 0x9e, 0xa7, 0xa4, 0x87,
    0xb8, 0x19, 0x68, 0xf1, 0xc1, 0x6b, 0x87, 0xa5, 0x99, 0xa0, 0x69, 0x24, 0x28, 0x80, 0xba, 0xd2,
    0x57, 0xc6, 0xc0, 0x1e, 0x69, 0x1b, 0xce, 0x96, 0xcd, 0x85, 0x9d, 0x59, 0x32, 0x92, 0x71, 0xe7,
    0x15, 0xf2, 0x9b, 0x71, 0x4b, 0xd4, 0xb6, 0x99, 0xb0, 0x71, 0x9b, 0x9b, 0xdb, 0x33, 0x28, 0x86,
    0x5e, 0x44, 0xf3, 0x5d, 0x5d, 0x7b, 0x87, 0x6c, 0x99, 0x8a, 0x6d, 0xaf, 0xf8, 0xdd, 0x39, 0x14,
    0xc0, 0x10, 0x85, 0xaf, 0x3a, 0x77, 0x60, 0x8b, 0xb7, 0x5f, 0x67, 0x45, 0xb4, 0xa7, 0xe7, 0x44,
    0x77, 0x0a, 0xb9, 0xe1, 0xb2, 0xbe, 0xde, 0x1b, 0x5a, 0xb8, 0xc8, 0x9b, 0x1f, 0x15, 0x37, 0xdc,
    0xeb, 0x55, 0x12, 0x37, 0xa1, 0xab, 0x9c, 0xcd, 0xdd, 0x42, 0x5d, 0x90, 0x89, 0xc5, 0x99, 0x54,
    0x6e, 0x9a, 0xd2, 0x71, 0x07, 0xe3, 0x81, 0x20, 0xad, 0xd4, 0x7c, 0xad, 0x69, 0xaf, 0xc7, 0x2d,
    0x7c, 0x84, 0xf1, 0x5d, 0x43, 0x4a, 0x3c, 0xf6, 0xf8, 0x7e, 0x6a, 0xb2, 0xdd, 0x00, 0xda, 0x30,
    0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x30, 0x00, 0x00, 0x01, 0x02, 0x01, 0xd0, 0x19, 0x5f, 0x84,
    0x31, 0x84, 0x50, 0x62, 0xcd, 0x44, 0xa6, 0x4e, 0xbb, 0x2e, 0x87, 0xda, 0x21, 0x25, 0x1c, 0x13,
    0xfe, 0xc2, 0x97, 0xa3, 0xd5, 0xfd, 0xe2, 0x62, 0xd6, 0xe5, 0x50, 0x7a, 0xda, 0x98, 0x8d, 0xcf,
    0xbe, 0x40, 0x5d, 0x69, 0xb6, 0x5f, 0xdc, 0x32, 0x68, 0x05, 0x67, 0x70, 0xfd, 0xa0, 0xd3, 0xcd,
    0x88, 0xeb, 0x12, 0x2e, 0xac, 0xf5, 0x57, 0xa2, 0xc2, 0x81, 0x22, 0x6b, 0x8d, 0x69, 0x6d, 0xac,
    0x41, 0x05, 0x5b, 0x0b, 0x79, 0xe0, 0x86, 0x05, 0x86, 0x8b, 0x02, 0x51, 0x7d, 0x6e, 0x13, 0x63,
    0x36, 0xe6, 0x89, 0x22, 0x75, 0xd9, 0xb0, 0x39, 0x27, 0xcd, 0xac, 0x78, 0xe0, 0x3a, 0x6a, 0x17,
    0x15, 0x05, 0x83, 0x3a, 0xe9, 0x38, 0x7c, 0x75, 0x38, 0xf3, 0xd8, 0x42, 0x57, 0xa1, 0xef, 0x68,
    0xb8, 0xb5, 0x44, 0xfe, 0x4d, 0x66, 0xa1, 0x09, 0xc5, 0x62, 0xff, 0x87, 0x7e, 0xdb, 0x41, 0x6f,
    0x4d, 0x5f, 0x16, 0x49, 0xef, 0xca, 0xda, 0x67, 0x01, 0x29, 0x0b, 0xd6, 0xd8, 0xfc, 0x0b, 0x22,
    0xc8, 0x28, 0xff, 0xb4, 0xb3, 0xa9, 0xf8, 0x9b, 0x78, 0x7d, 0x33, 0xbf, 0xa6, 0x9b, 0x39, 0x64,
    0x57, 0x41, 0x1f, 0x27, 0x05, 0xfc, 0xcd, 0x44, 0xcd, 0xb2, 0xea, 0x79, 0xe0, 0x00, 0x00, 0x00,
    0x01, 0x46, 0x01, 0x50, 0x00, 0x00, 0x01, 0x02, 0x01, 0xe0, 0x44, 0x97, 0xe1, 0x8f, 0x40, 0x3d,
    0x73, 0xde, 0xdf, 0xf5, 0x1c, 0xff, 0xf6, 0xe2, 0x12, 0x16, 0x3d, 0xb6, 0x75, 0xad, 0x9e, 0xb5,
    0x2b, 0xe7, 0x4a, 0x40, 0xe9, 0x91, 0xd6, 0x86, 0x86, 0x2d, 0xcb, 0xe4, 0xb6, 0x58, 0x4a, 0x2d,
    0x7b, 0x1b, 0xdb, 0xc5, 0x48, 0x2d, 0x09, 0x4d, 0x32, 0x30, 0xde, 0xaa, 0x61, 0xc2, 0x27, 0x64,
    0x54, 0x71, 0x89, 0x32, 0x05, 0xc0, 0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x50, 0x00, 0x00, 0x01,
    0x00, 0x01, 0xe0, 0x24, 0xff, 0xfa, 0x2c, 0x6e, 0xdc, 0x78, 0x3d, 0x08, 0x6d, 0x3b, 0xcf, 0xcc,
    0xb6, 0x16, 0xeb, 0x9d, 0x5f, 0x46, 0x0b, 0x24, 0x0e, 0x97, 0x99, 0x06, 0x00, 0x00, 0x00, 0x01,
    0x46, 0x01, 0x30, 0x00, 0x00, 0x01, 0x02, 0x01, 0xd0, 0x28, 0x95, 0xd7, 0xdc, 0x41, 0xc0, 0x61,
    0x14, 0xd6, 0xee, 0xd4, 0xe8, 0x24, 0x3e, 0x7f, 0x82, 0x22, 0xb0, 0x20, 0xb9, 0xf1, 0x69, 0x9b,
    0xfe, 0xcc, 0xb7, 0x2e, 0xbd, 0xdb, 0x20, 0xfa, 0xe5, 0x97, 0xd9, 0x95, 0x1d, 0x7e, 0x49, 0x35,
    0x69, 0xdb, 0x40, 0xa3, 0x5c, 0x0f, 0x74, 0x6f, 0x00, 0x3d, 0x36, 0x05, 0xc0, 0xc4, 0x37, 0x15,
    0xf9, 0x01, 0x2d, 0xdf, 0xcb, 0x70, 0x3d, 0xfa, 0x47, 0xec, 0x80, 0x07, 0x48, 0x08, 0xc8, 0x54,
    0xd3, 0xe3, 0x00, 0xa3, 0xe4, 0xce, 0x40, 0x05, 0x0b, 0x51, 0x73, 0x56, 0x33, 0x82, 0xa8, 0x0a,
    0xc0, 0xd3, 0xc5, 0xf1, 0xeb, 0x32, 0x8e, 0x4e, 0xa7, 0x6f, 0xb2, 0x64, 0x43, 0x54, 0x71, 0xd4,
    0xaf, 0xc0, 0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x50, 0x00, 0x00, 0x01, 0x00, 0x01, 0xe0, 0x82,
    0x2f, 0x5f, 0xdc, 0x63, 0x70, 0x3f, 0xaf, 0x66, 0x75, 0x52, 0xe7, 0x7f, 0x62, 0xfb, 0x84, 0x9c,
    0xef, 0x60, 0x8f, 0xfa, 0x7e, 0x5f, 0x8b, 0x40, 0x15, 0x34, 0x69, 0xe2, 0xc4, 0xfc, 0xff, 0x53,
    0xb8, 0x1e, 0xc7, 0x20, 0x55, 0xdb, 0x5e, 0x05, 0xbd, 0x7a, 0x85, 0xb4, 0xb7, 0xc1, 0x8d, 0xdb,
    0x0f, 0xa6, 0xab, 0xb9, 0x00, 0x2a, 0xdb, 0x83, 0xa0, 0xb0, 0xc5, 0x76, 0x6e, 0xe1, 0xb8, 0x72,
    0xfd, 0x09, 0x61, 0x19, 0xe0,
};

constexpr std::array<std::uint64_t, 6> HevcHashes{
    0x7291c31ee57c5065ull, 0x37b15adc47810098ull, 0xc84737b65a65b247ull,
    0xe39bc891f8286267ull, 0x734caf1ba961b3c2ull, 0x7deb44bd1ed30531ull,
};

constexpr std::array<std::uint8_t, 1213> Vp9Stream{
    0x44, 0x4b, 0x49, 0x46, 0x00, 0x00, 0x20, 0x00, 0x56, 0x50, 0x39, 0x30, 0x40, 0x00, 0x28, 0x00,
    0x1e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xe9, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0x49, 0x83, 0x42,
    0x00, 0x03, 0xf0, 0x02, 0x76, 0x08, 0x38, 0x24, 0x1c, 0x18, 0x0e, 0x00, 0x01, 0xc0, 0x6f, 0xb1,
    0xfb, 0x17, 0xa6, 0xd2, 0x0f, 0x5f, 0xee, 0xf7, 0xec, 0xbf, 0xb4, 0x0c, 0x93, 0xbe, 0x87, 0x11,
    0x35, 0x1c, 0x51, 0x84, 0x8b, 0x70, 0xad, 0xe4, 0x00, 0x00, 0x79, 0x3b, 0xff, 0xff, 0xf5, 0x94,
    0xad, 0x57, 0x6e, 0xae, 0x8e, 0x1b, 0xfe, 0x64, 0xff, 0xc9, 0x64, 0xba, 0xca, 0x1d, 0x7f, 0x45,
    0x16, 0x95, 0x13, 0x8d, 0xd0, 0x61, 0x19, 0x80, 0xb7, 0xdf, 0xff, 0xff, 0xd6, 0x52, 0x1e, 0x7b,
    0x3c, 0x16, 0x3f, 0xff, 0xfb, 0x52, 0xe7, 0x26, 0xab, 0xd6, 0x3a, 0x6a, 0xff, 0xf8, 0x1b, 0x47,
    0xcd, 0xe2, 0xf5, 0x48, 0x65, 0x1e, 0x9f, 0x5c, 0x1a, 0x74, 0x0a, 0x99, 0xc0, 0x04, 0x41, 0x02,
    0xa9, 0x86, 0x36, 0x1f, 0xff, 0xf3, 0x4f, 0x71, 0xdf, 0x8a, 0xcd, 0x8a, 0x88, 0xdd, 0x37, 0xa2,
    0x27, 0x3a, 0x20, 0xb8, 0xf0, 0x4f, 0x67, 0x59, 0x8a, 0x82, 0xa2, 0xde, 0x08, 0xe3, 0xae, 0x87,
    0xff, 0xf0, 0x36, 0x8f, 0x9b, 0xc5, 0xea, 0x90, 0xca, 0x3d, 0x3e, 0xb8, 0x34, 0xe8, 0x15, 0x33,
    0x80, 0x08, 0x82, 0x05, 0x53, 0x0c, 0x6c, 0x3f, 0xff, 0xe6, 0x9e, 0xe3, 0xbf, 0x15, 0x9d, 0xec,
    0xdf, 0xe9, 0xce, 0x60, 0x88, 0x09, 0x48, 0x81, 0xa2, 0xb6, 0x27, 0x3e, 0xf9, 0xa9, 0x7e, 0xa7,
    0xe1, 0x9d, 0xdd, 0x3a, 0xa0, 0x39, 0x3f, 0x8a, 0xba, 0x73, 0x50, 0xde, 0xcf, 0x29, 0x5c, 0x2a,
    0x92, 0x01, 0xec, 0x06, 0xbe, 0x8b, 0x1e, 0x14, 0x34, 0x6b, 0xb1, 0x3a, 0x01, 0x47, 0x08, 0xe7,
    0xbe, 0xb0, 0xa7, 0x03, 0x77, 0xeb, 0xf0, 0xed, 0xd8, 0xa1, 0x4e, 0x7c, 0xd9, 0xe4, 0x7c, 0x9d,
    0x67, 0xfe, 0x36, 0xab, 0x5e, 0xa9, 0x5e, 0x81, 0x74, 0x68, 0xa1, 0x4b, 0x61, 0x81, 0xe2, 0xe4,
    0x4c, 0x6a, 0x4f, 0xe5, 0x65, 0x3f, 0x4c, 0x78, 0xea, 0x68, 0x5c, 0xf9, 0xc6, 0x58, 0x1f, 0x58,
    0xe0, 0x40, 0x4b, 0x88, 0x6d, 0x15, 0x94, 0x8c, 0x83, 0x4a, 0xa3, 0xf3, 0xd5, 0x80, 0x7f, 0x83,
    0x52, 0x7d, 0xbe, 0xa6, 0x9f, 0xb4, 0x89, 0xda, 0xb6, 0x5a, 0xdf, 0x1f, 0x78, 0xd8, 0xbf, 0xfe,
    0x7f, 0x52, 0xfa, 0xfd, 0xa4, 0xeb, 0xda, 0x91, 0x9c, 0xb6, 0xd0, 0xc3, 0xfd, 0x8e, 0x04, 0x04,
    0xb8, 0x86, 0xd1, 0x56, 0xa2, 0x24, 0xe6, 0xbc, 0x5b, 0xbf, 0x82, 0x36, 0xb5, 0xa7, 0x87, 0x73,
    0xd8, 0xbd, 0x30, 0x33, 0x3f, 0x7c, 0xbb, 0x4f, 0x70, 0xed, 0x3f, 0x8d, 0x2b, 0xe2, 0xce, 0x62,
    0x09, 0xfa, 0x04, 0x31, 0x67, 0xff, 0x09, 0xc1, 0x5c, 0x29, 0xf8, 0x77, 0xfa, 0xb6, 0xa7, 0xde,
    0xaa, 0xc1, 0x87, 0xd9, 0xb4, 0x10, 0x0d, 0xda, 0x5d, 0xf7, 0xdd, 0x7d, 0x08, 0x7a, 0xbe, 0x60,
    0xeb, 0x85, 0x00, 0x08, 0x21, 0x8e, 0xa0, 0x4d, 0xff, 0x73, 0x66, 0x55, 0xb8, 0x61, 0x69, 0xc5,
    0xf2, 0xef, 0x96, 0x27, 0x16, 0x2f, 0x0d, 0x93, 0xcd, 0xcd, 0xba, 0x2a, 0x80, 0x59, 0x90, 0x2b,
    0xc5, 0x43, 0x49, 0x25, 0x49, 0x01, 0x15, 0x9f, 0xf4, 0xdd, 0x02, 0x31, 0x8b, 0xf0, 0xbe, 0x28,
    0x87, 0xbd, 0x3f, 0xce, 0xb1, 0x6e, 0xd2, 0x7f, 0x77, 0x41, 0x09, 0x8a, 0x04, 0x17, 0x87, 0x57,
    0x1b, 0x72, 0x79, 0x69, 0x41, 0x7d, 0xc5, 0x78, 0xb7, 0x53, 0xe7, 0x80, 0xd2, 0x7f, 0x1f, 0xb9,
    0xd5, 0x0f, 0xff, 0xf5, 0xb8, 0xd0, 0xc5, 0x47, 0x30, 0x09, 0x36, 0x6e, 0xb1, 0xfd, 0xe1, 0xe9,
    0xf9, 0x81, 0x1d, 0x98, 0x00, 0x6d, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x86, 0x00, 0x40, 0x92, 0x9c, 0x10, 0x46, 0x00, 0x00, 0x03, 0x60, 0x00, 0x00, 0x76, 0x21,
    0x2d, 0x37, 0xff, 0xff, 0xc3, 0x7f, 0xff, 0xff, 0xff, 0xd8, 0x59, 0x6b, 0xff, 0xa9, 0xff, 0xff,
    0xff, 0xda, 0x3d, 0xca, 0x5b, 0xc7, 0xff, 0xfd, 0x52, 0xf6, 0x2e, 0x02, 0xe0, 0x37, 0xe3, 0x5f,
    0xff, 0xea, 0x82, 0x5b, 0xff, 0xff, 0xfa, 0x7b, 0x85, 0xdf, 0x66, 0xbf, 0xff, 0xe6, 0xde, 0xef,
    0xed, 0x97, 0xff, 0xf9, 0x38, 0x4a, 0x7f, 0x61, 0xb5, 0xeb, 0x56, 0x73, 0xa7, 0xff, 0xff, 0x31,
    0x8b, 0xa1, 0xcf, 0xff, 0x5b, 0x90, 0x07, 0x17, 0xff, 0xd6, 0xe1, 0x74, 0x39, 0xff, 0xeb, 0x72,
    0x5d, 0x14, 0xff, 0xf5, 0xb8, 0x95, 0x54, 0x7f, 0xf7, 0x5d, 0x1e, 0x26, 0x68, 0x00, 0x7f, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0x00, 0x40, 0x92, 0x9c, 0x10,
    0x45, 0x00, 0x00, 0x03, 0x60, 0x00, 0x00, 0x7d, 0xb8, 0xff, 0xff, 0xb4, 0x7f, 0xff, 0xff, 0xfe,
    0x4d, 0x81, 0xbf, 0xff, 0x98, 0x9f, 0xff, 0xff, 0xe4, 0xd8, 0x1b, 0xff, 0xf9, 0x89, 0xff, 0xff,
    0xfe, 0x4d, 0x81, 0xbf, 0xd6, 0xbf, 0xff, 0xf9, 0x36, 0x0c, 0x1f, 0xff, 0xfc, 0x10, 0x3e, 0xfe,
    0x1f, 0xa1, 0x76, 0x5c, 0x22, 0x5f, 0x3b, 0x56, 0x57, 0xa0, 0x3c, 0x43, 0xf0, 0xc8, 0x00, 0x00,
    0x00, 0x19, 0xbb, 0xff, 0xff, 0xd8, 0x43, 0x10, 0x26, 0x67, 0xff, 0xff, 0xfe, 0x4a, 0x07, 0x8f,
    0x80, 0x7f, 0xff, 0xff, 0x94, 0xd2, 0x59, 0x02, 0xff, 0xff, 0x0d, 0x89, 0x18, 0x02, 0xff, 0xff,
    0x0d, 0x84, 0xb2, 0x05, 0xff, 0xfe, 0x1b, 0x14, 0xdd, 0x42, 0xff, 0xff, 0x0d, 0x82, 0xc8, 0xb1,
    0xff, 0xf9, 0x5c, 0x32, 0xfe, 0x00, 0x78, 0x60, 0x00, 0x56, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0x00, 0x40, 0x92, 0x9c, 0x20, 0x46, 0x00, 0x00, 0x03, 0x60,
    0x00, 0x00, 0x73, 0xaf, 0xff, 0xff, 0xc3, 0x7f, 0xff, 0xff, 0xff, 0xd8, 0x19, 0x3f, 0xff, 0xe6,
    0x1b, 0xfb, 0x80, 0x95, 0x82, 0x3b, 0xd9, 0x4c, 0x81, 0xce, 0x52, 0x2b, 0xff, 0xff, 0xa0, 0xad,
    0xc9, 0x0e, 0x62, 0xff, 0xff, 0xff, 0x60, 0xf8, 0x75, 0x5f, 0xff, 0xff, 0x31, 0x92, 0xaa, 0x8f,
    0xfe, 0xeb, 0xa4, 0x4e, 0x5f, 0xff, 0x5b, 0x89, 0x55, 0x47, 0xff, 0x75, 0xd4, 0xd9, 0xef, 0xff,
    0xe4, 0x51, 0x64, 0x58, 0xff, 0xee, 0xba, 0xfc, 0x8d, 0x8f, 0x00, 0x95, 0x00, 0x00, 0x00, 0x04,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0x00, 0x40, 0x92, 0x9c, 0x10, 0x44, 0x00, 0x00,
    0x03, 0x60, 0x00, 0x00, 0x73, 0xaf, 0xff, 0xff, 0xfb, 0x50, 0x3f, 0xff, 0xff, 0xff, 0xea, 0xc5,
    0x0d, 0xff, 0xff, 0xe0, 0x09, 0xfc, 0xad, 0x23, 0x5f, 0xf8, 0xa3, 0x64, 0x9b, 0x40, 0x76, 0x8d,
    0xd6, 0xd1, 0x83, 0x6f, 0x24, 0xd5, 0xff, 0xff, 0xd4, 0xa9, 0x45, 0xac, 0xda, 0xdc, 0xd2, 0xbf,
    0xff, 0xff, 0xfd, 0x58, 0xbf, 0x7f, 0x8a, 0x7f, 0xff, 0xff, 0xad, 0xb9, 0x2d, 0x3c, 0xbf, 0xff,
    0xd4, 0xca, 0x46, 0x7d, 0x7f, 0xff, 0xe1, 0x64, 0x96, 0x9e, 0x5f, 0xff, 0xea, 0x65, 0x4d, 0xe9,
    0x97, 0xff, 0xfa, 0x99, 0x16, 0x71, 0xaf, 0xff, 0xfc, 0x2c, 0x99, 0x7f, 0x5b, 0x8b, 0x50, 0x9c,
    0xdc, 0x43, 0xa8, 0xb7, 0xd5, 0x68, 0x6a, 0xbc, 0x78, 0xef, 0xc2, 0x3d, 0xfb, 0x3f, 0xcc, 0x51,
    0x40, 0x86, 0x54, 0x02, 0xa4, 0xb2, 0xbd, 0x79, 0x93, 0xa2, 0x4a, 0x35, 0x36, 0xc8, 0x7c, 0xdb,
    0xb0, 0x1d, 0xc1, 0xd0, 0x1d, 0x51, 0x9a, 0xe4, 0x19, 0xe6, 0x08, 0x00, 0x95, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86, 0x00, 0x40, 0x92, 0x9c, 0x10, 0x46, 0x00,
    0x00, 0x03, 0x60, 0x00, 0x00, 0x73, 0xaf, 0xff, 0xff, 0xa9, 0xef, 0xff, 0xff, 0xff, 0xd8, 0x59,
    0x9f, 0xff, 0x53, 0xff, 0xff, 0xff, 0xb4, 0x78, 0x24, 0x37, 0xff, 0xfb, 0x89, 0x98, 0x26, 0xc0,
    0xe3, 0x5f, 0x50, 0x5c, 0x2f, 0xff, 0xff, 0xf1, 0xd2, 0xd2, 0x4d, 0xfd, 0xf2, 0x7f, 0xff, 0x9f,
    0xff, 0x17, 0x85, 0xd2, 0xab, 0x4b, 0xbe, 0x3d, 0xff, 0xff, 0xff, 0xdf, 0x55, 0xbf, 0xff, 0xff,
    0xff, 0xfe, 0x6e, 0xe3, 0xe4, 0x07, 0x53, 0xd2, 0x5f, 0xff, 0xff, 0xff, 0xbe, 0xab, 0x7f, 0xff,
    0xff, 0xff, 0xcd, 0xdc, 0x1a, 0xbf, 0xff, 0xfe, 0xfa, 0xad, 0xff, 0xff, 0xff, 0xcd, 0xdc, 0x4e,
    0x1d, 0x57, 0xff, 0xff, 0xe0, 0x44, 0xaa, 0xa3, 0xff, 0xd3, 0x89, 0x13, 0x97, 0xff, 0xe5, 0xa2,
    0x55, 0x51, 0xff, 0xe9, 0xc4, 0xba, 0x29, 0xff, 0xeb, 0x71, 0x2a, 0xa8, 0xff, 0xee, 0xb9, 0xc4,
    0x7f, 0xff, 0xfb, 0xea, 0xb7, 0xff, 0xff, 0xff, 0x37, 0x71, 0x29, 0xfa, 0x00,
};

constexpr std::array<std::uint64_t, 6> Vp9Hashes{
    0x552fd3c320b37408ull, 0xb2fb78bac8d2d7dfull, 0x312469345c491c3dull,
    0x9aeb0fb400c95a75ull, 0xe02e07646faa48beull, 0x1399072031abb375ull,
};

constexpr std::array<std::uint8_t, 234> HevcMain10Stream{
    0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x10, 0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01, 0xff,
    0xff, 0x02, 0x20, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x1e,
    0x95, 0x98, 0x09, 0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x02, 0x20, 0x00, 0x00, 0x03, 0x00,
    0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x1e, 0xa0, 0x20, 0x82, 0x93, 0x65, 0x95, 0x9a,
    0xe4, 0xca, 0xf0, 0x16, 0x80, 0x80, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x0f, 0x04, 0x00,
    0x00, 0x00, 0x01, 0x44, 0x01, 0xc1, 0x73, 0xd0, 0x89, 0x00, 0x00, 0x01, 0x28, 0x01, 0xaf, 0x08,
    0x60, 0xf2, 0xfb, 0x04, 0xef, 0x67, 0x67, 0xdc, 0xc7, 0x8e, 0xc7, 0x42, 0x63, 0xeb, 0x7d, 0x28,
    0x73, 0x95, 0x14, 0x0c, 0x71, 0x10, 0x8b, 0x65, 0x04, 0xbd, 0x03, 0xe6, 0x04, 0x35, 0xb9, 0x9b,
    0xec, 0x60, 0x2e, 0xea, 0xcf, 0xd0, 0xdd, 0x2d, 0x3a, 0x62, 0x33, 0x9d, 0x7a, 0x74, 0xb0, 0xeb,
    0x0c, 0x06, 0xe6, 0xa5, 0x30, 0xa7, 0xea, 0xed, 0xc2, 0xa5, 0x8e, 0x7c, 0xda, 0x1b, 0x9d, 0xe4,
    0xac, 0x2b, 0x38, 0xdb, 0x9c, 0xe8, 0x51, 0x34, 0x46, 0xd2, 0x04, 0x1a, 0x77, 0x87, 0x7a, 0x19,
    0x70, 0xf9, 0x4f, 0xcb, 0x03, 0x3c, 0x74, 0x25, 0xb9, 0x92, 0x3b, 0x95, 0xe9, 0xd8, 0xaa, 0x9b,
    0x6b, 0x9b, 0x1f, 0x70, 0x25, 0x2a, 0xe7, 0x6c, 0xc3, 0x7e, 0x55, 0x39, 0xf5, 0x06, 0x5a, 0x23,
    0xb1, 0x2e, 0xf6, 0x01, 0xac, 0x71, 0xb9, 0x20, 0xb4, 0xef, 0x1d, 0x42, 0x7e, 0x0e, 0x01, 0x1d,
    0xaf, 0x0a, 0x13, 0x04, 0x8e, 0x03, 0x4d, 0x1e, 0xc4, 0xe0,
};

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}


template <typename TAction>
void checkThrows(TAction action) {
    bool threw = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "expected a decoding exception");
}

void testFailures() {
    DecoderConfigInfo config{sizeof(DecoderConfigInfo), 0, 2, 100, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    std::uint64_t handle = 0;
    checkThrows([&] { sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle); });
    check(handle == 0, "unsupported codec created a decoder");
    config.codecType = 1;
    config.maxFrameWidth = 0;
    checkThrows([&] { sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory); });
    config.maxFrameWidth = Width;
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
    check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "decoder creation failed");
    std::vector<std::uint8_t> buffer(memory.maxFrameBufferSize);
    FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
    OutputInfo output{sizeof(OutputInfo)};
    const std::array<std::uint8_t, 5> malformed{0, 0, 0, 8, 0x65};
    InputData input{sizeof(InputData), malformed.data(), malformed.size(), 0, 0, 0};
    checkThrows([&] { sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output); });
    input.auSize = 2;
    checkThrows([&] { sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output); });
    check(sceVideodec2Reset_nid_postfix(handle) == 0, "reset failed");
    AvcPictureInfo avc{sizeof(AvcPictureInfo)};
    checkThrows([&] { sceVideodec2GetAvcPictureInfo_nid_postfix(nullptr, &avc, nullptr); });
    checkThrows([&] { sceVideodec2GetAvcPictureInfo_nid_postfix(&output, nullptr, nullptr); });
    output.frameBuffer = buffer.data();
    check(sceVideodec2GetAvcPictureInfo_nid_postfix(&output, &avc, nullptr) == 0 && !avc.isValid, "unknown frame buffer reported a picture");
    frame.frameBufferSize = 1;
    checkThrows([&] {
        for (const auto& unit : accessUnits(false)) {
            input.auData = unit.data();
            input.auSize = unit.size();
            sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output);
        }
        sceVideodec2Flush_nid_postfix(handle, &frame, &output);
    });
    check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
}

std::vector<std::vector<std::uint8_t>> hevcUnits(const std::uint8_t* stream, std::size_t size) {
    std::vector<std::size_t> starts;
    for (std::size_t index = 0; index + 4 < size; ++index) {
        if (stream[index] == 0 && stream[index + 1] == 0 && stream[index + 2] == 0 && stream[index + 3] == 1 && ((stream[index + 4] >> 1u) & 0x3fu) == 35) starts.push_back(index);
    }
    starts.push_back(size);
    std::vector<std::vector<std::uint8_t>> units;
    for (std::size_t unit = 0; unit + 1 < starts.size(); ++unit) units.emplace_back(stream + starts[unit], stream + starts[unit + 1]);
    return units;
}

std::vector<std::vector<std::uint8_t>> ivfFrames() {
    std::vector<std::vector<std::uint8_t>> frames;
    for (std::size_t index = 32; index + 12 <= Vp9Stream.size();) {
        const std::size_t length = Vp9Stream[index] | (Vp9Stream[index + 1] << 8u) | (Vp9Stream[index + 2] << 16u) | (static_cast<std::size_t>(Vp9Stream[index + 3]) << 24u);
        check(index + 12 + length <= Vp9Stream.size(), "truncated IVF frame");
        frames.emplace_back(Vp9Stream.begin() + static_cast<std::ptrdiff_t>(index + 12), Vp9Stream.begin() + static_cast<std::ptrdiff_t>(index + 12 + length));
        index += 12 + length;
    }
    return frames;
}

void testClip(std::uint32_t codec, std::uint32_t profile, const std::vector<std::vector<std::uint8_t>>& units, const std::array<std::uint64_t, 6>& hashes) {
    const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, codec, profile, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
    std::uint64_t handle = 0;
    check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "decoder creation failed");
    std::vector<std::uint8_t> buffer(memory.maxFrameBufferSize);
    std::size_t pictures = 0;
    const auto take = [&](const OutputInfo& output, const FrameBuffer& frame) {
        check(output.isValid == frame.isAccepted, "frame buffer acceptance disagrees with the output");
        if (!output.isValid) return false;
        check(pictures < hashes.size(), "more pictures than access units");
        check(output.codecType == codec && !output.isErrorFrame && output.frameWidth == Width && output.frameHeight == Height && output.framePitch == 256 && output.frameBuffer == buffer.data(), "unexpected picture geometry");
        AvcPictureInfo info{sizeof(AvcPictureInfo)};
        checkThrows([&] { sceVideodec2GetPictureInfo_nid_postfix(&output, &info, nullptr); });
        Vp9PictureInfo vp9{sizeof(Vp9PictureInfo)};
        if (codec == CodecVp9) {
            check(sceVideodec2GetVp9PictureInfo_nid_postfix(&output, &vp9) == 0 && vp9.isValid, "VP9 picture metadata missing");
            check(vp9.ptsData == 1000 + pictures && vp9.dtsData == pictures && vp9.attachedData == 0, "VP9 picture timestamps differ");
            check(vp9.profile == 0 && vp9.bitDepth == 8 && vp9.colorSpace == 0 && vp9.colorRange == 0 && vp9.numFrames == 1 && vp9.intraOnly == 0, "VP9 coded header metadata differs");
            check(vp9.frameWidth == Width && vp9.frameHeight == Height && vp9.renderWidth == Width && vp9.renderHeight == Height, "VP9 picture dimensions differ");
            check(vp9.keyFrameFlag == (pictures == 0 ? 1u : 0u), "VP9 key frame flag differs");
        } else checkThrows([&] { sceVideodec2GetVp9PictureInfo_nid_postfix(&output, &vp9); });
        check(hashNv12(buffer.data(), output.framePitch, output.frameHeight) == hashes[pictures], "picture " + std::to_string(pictures) + " differs from the reference");
        ++pictures;
        return true;
    };
    check(units.size() == hashes.size(), "unexpected access unit count");
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
        take(output, frame);
    }
    for (;;) {
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        if (!take(output, frame)) break;
    }
    check(pictures == hashes.size(), "missing pictures after flush");
    check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
}

void testVp9PictureMetadata() {
    const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, CodecVp9, 0, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "VP9 memory query failed");
    std::uint64_t handle = 0;
    check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "VP9 create failed");
    const auto units = ivfFrames();
    std::vector<std::uint8_t> firstBuffer(memory.maxFrameBufferSize), secondBuffer(memory.maxFrameBufferSize);
    OutputInfo firstOutput{sizeof(OutputInfo)}, secondOutput{sizeof(OutputInfo)};
    const auto decode = [&](const std::vector<std::uint8_t>& bytes, std::vector<std::uint8_t>& buffer, OutputInfo& output, std::uint64_t tag) {
        const InputData input{sizeof(InputData), bytes.data(), bytes.size(), tag, tag + 1, tag + 2};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "VP9 metadata decode failed");
    };
    const auto query = [&](const OutputInfo& output) {
        struct Guarded { Vp9PictureInfo info{sizeof(Vp9PictureInfo)}; std::uint64_t guard = 0x123456789abcdef0ull; } guarded;
        check(sceVideodec2GetVp9PictureInfo_nid_postfix(&output, &guarded.info) == 0, "VP9 metadata query failed");
        check(guarded.guard == 0x123456789abcdef0ull && guarded.info.thisSize == sizeof(Vp9PictureInfo), "VP9 info overwrote caller storage");
        return guarded.info;
    };
    Vp9PictureInfo info{sizeof(Vp9PictureInfo)};
    checkThrows([&] { sceVideodec2GetVp9PictureInfo_nid_postfix(nullptr, &info); });
    checkThrows([&] { sceVideodec2GetVp9PictureInfo_nid_postfix(&firstOutput, nullptr); });
    info.thisSize = sizeof(Vp9PictureInfo) - 1;
    checkThrows([&] { sceVideodec2GetVp9PictureInfo_nid_postfix(&firstOutput, &info); });
    check(!query(firstOutput).isValid, "VP9 unknown buffer reported a picture");
    std::vector<std::uint8_t> superframe;
    for (std::size_t index = 0; index < 2; ++index) superframe.insert(superframe.end(), units[index].begin(), units[index].end());
    superframe.push_back(0xc9);
    for (std::size_t index = 0; index < 2; ++index) {
        superframe.push_back(static_cast<std::uint8_t>(units[index].size()));
        superframe.push_back(static_cast<std::uint8_t>(units[index].size() >> 8u));
    }
    superframe.push_back(0xc9);
    decode(superframe, firstBuffer, firstOutput, 9000);
    check(firstOutput.isValid && hashNv12(firstBuffer.data(), firstOutput.framePitch, firstOutput.frameHeight) == Vp9Hashes[0], "VP9 first superframe picture differs");
    const auto firstInfo = query(firstOutput);
    check(firstInfo.isValid && firstInfo.numFrames == 2 && firstInfo.keyFrameFlag == 1 && firstInfo.ptsData == 9000 && firstInfo.dtsData == 9001 && firstInfo.attachedData == 9002, "VP9 first superframe metadata differs");
    FrameBuffer frame{sizeof(FrameBuffer), secondBuffer.data(), secondBuffer.size(), false};
    check(sceVideodec2Flush_nid_postfix(handle, &frame, &secondOutput) == 0 && secondOutput.isValid, "VP9 second superframe picture missing");
    check(hashNv12(secondBuffer.data(), secondOutput.framePitch, secondOutput.frameHeight) == Vp9Hashes[1], "VP9 second superframe picture differs");
    const auto secondInfo = query(secondOutput);
    check(secondInfo.numFrames == 2 && secondInfo.keyFrameFlag == 0 && secondInfo.ptsData == 9000, "VP9 second superframe metadata differs");
    firstOutput.frameWidth = 1;
    check(query(firstOutput).frameWidth == Width && query(firstOutput).keyFrameFlag == 1, "VP9 retained picture metadata changed");
    check(sceVideodec2Reset_nid_postfix(handle) == 0 && !query(firstOutput).isValid, "VP9 reset retained old picture ownership");
    auto hidden = units[0];
    hidden[0] &= static_cast<std::uint8_t>(~2u);
    decode(hidden, firstBuffer, firstOutput, 9100);
    check(!firstOutput.isValid, "VP9 hidden keyframe was presented");
    decode(std::vector<std::uint8_t>{0x88}, secondBuffer, secondOutput, 9200);
    info = query(secondOutput);
    check(info.isValid && info.ptsData == 9200 && info.keyFrameFlag == 0 && info.frameWidth == Width, "VP9 existing reference metadata differs");
    check(hashNv12(secondBuffer.data(), secondOutput.framePitch, secondOutput.frameHeight) == Vp9Hashes[0], "VP9 existing reference pixels differ");
    check(sceVideodec2Reset_nid_postfix(handle) == 0, "VP9 reset failed");
    checkThrows([&] { decode(std::vector<std::uint8_t>{0x88}, firstBuffer, firstOutput, 9300); });
    checkThrows([&] { decode(std::vector<std::uint8_t>{0}, firstBuffer, firstOutput, 9400); });
    checkThrows([&] { decode(std::vector<std::uint8_t>{0x82}, firstBuffer, firstOutput, 9500); });
    checkThrows([&] { decode(std::vector<std::uint8_t>{0xc1, 0, 0, 0xc1}, firstBuffer, firstOutput, 9550); });
    std::vector<bool> bits;
    for (const auto byte : units[0]) for (int shift = 7; shift >= 0; --shift) bits.push_back(((byte >> shift) & 1u) != 0);
    check(bits.size() > 69, "VP9 test keyframe header is truncated");
    bits[35] = true;
    bits[68] = true;
    std::vector<bool> render;
    for (const auto value : {static_cast<unsigned>(Width * 2 - 1), static_cast<unsigned>(Height * 2 - 1)})
        for (int shift = 15; shift >= 0; --shift) render.push_back(((value >> shift) & 1u) != 0);
    bits.insert(bits.begin() + 69, render.begin(), render.end());
    std::vector<std::uint8_t> resized(bits.size() / 8);
    for (std::size_t index = 0; index < bits.size(); ++index) if (bits[index]) resized[index / 8] |= static_cast<std::uint8_t>(1u << (7u - index % 8));
    decode(resized, firstBuffer, firstOutput, 9600);
    info = query(firstOutput);
    check(info.isValid && info.bitDepth == 8 && info.colorRange == 1 && info.frameWidth == Width && info.frameHeight == Height && info.renderWidth == Width * 2 && info.renderHeight == Height * 2, "VP9 render size or color range metadata differs");
    check(hashNv12(firstBuffer.data(), firstOutput.framePitch, firstOutput.frameHeight) == Vp9Hashes[0], "VP9 render metadata changed coded pixels");
    check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0 && !query(firstOutput).isValid, "VP9 delete retained picture ownership");
}

void testTenBit() {
    for (const auto codec : {CodecHevc, CodecVp9}) {
        for (const auto [profile, expected] : {std::pair<std::uint32_t, std::uint64_t>{1, 2048ull * 1088 * 3 / 2}, {2, 3840ull * 1088 * 3 / 2}}) {
            const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, codec, profile, 0, 1920, 1088, 4, 1};
            DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
            check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0 && memory.maxFrameBufferSize == expected, "1920x1088 frame size differs for profile " + std::to_string(profile));
        }
        const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, codec, 2, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
        DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
        check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
        std::uint64_t handle = 0;
        check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "ten-bit decoder creation failed");
        if (codec == CodecHevc) {
            std::vector<std::uint8_t> buffer(memory.maxFrameBufferSize);
            const InputData input{sizeof(InputData), HevcMain10Stream.data(), HevcMain10Stream.size(), 0, 0, 0};
            FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
            OutputInfo output{sizeof(OutputInfo)};
            check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "ten-bit decode failed");
            if (!output.isValid) check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "ten-bit flush failed");
            check(output.isValid && frame.isAccepted && output.frameWidth == Width && output.frameHeight == Height && output.framePitch == 128 && output.framePitchInBytes == 256 && output.frameFormat == 0, "ten-bit picture geometry differs");
            std::uint64_t hash = 0xcbf29ce484222325ull;
            std::uint16_t largest = 0;
            const auto mix = [&](const std::uint8_t* row) {
                for (std::uint32_t column = 0; column < Width; ++column) {
                    std::uint16_t word = 0;
                    std::memcpy(&word, row + column * 2, 2);
                    largest = std::max(largest, word);
                    hash = (hash ^ word) * 0x100000001b3ull;
                }
            };
            for (std::uint32_t row = 0; row < Height; ++row) mix(buffer.data() + static_cast<std::size_t>(row) * output.framePitchInBytes);
            for (std::uint32_t row = 0; row < Height / 2; ++row) mix(buffer.data() + static_cast<std::size_t>(output.framePitchInBytes) * Height + static_cast<std::size_t>(row) * output.framePitchInBytes);
            check(largest == 1023, "ten-bit samples are not in the low bits of their words");
            check(hash == 0x453fd1cd69e47611ull, "ten-bit picture differs from the reference");
        }
        check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
    }
}

void testDecode(bool lengthPrefixed, bool separateParameters = false) {
    const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 0, 1, 100, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
    std::uint64_t handle = 0;
    check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "decoder creation failed");
    std::vector<std::uint8_t> buffer(memory.maxFrameBufferSize);
    std::size_t pictures = 0;
    const auto take = [&](const OutputInfo& output, const FrameBuffer& frame) {
        check(output.isValid == frame.isAccepted, "frame buffer acceptance disagrees with the output");
        if (!output.isValid) return false;
        check(pictures < PictureHashes.size(), "more pictures than access units");
        check(!output.isErrorFrame && output.frameWidth == Width && output.frameHeight == CodedHeight && output.framePitch == 256 && output.frameBuffer == buffer.data(), "unexpected picture geometry");
        AvcPictureInfo info{sizeof(AvcPictureInfo)};
        check(sceVideodec2GetPictureInfo_nid_postfix(&output, &info, nullptr) == 0 && info.isValid, "picture info missing");
        const auto unit = DisplayOrderUnits[pictures];
        check(info.ptsData == 1000 + unit && info.dtsData == unit && info.attachedData == 0xa0 + unit, "picture timestamps out of display order");
        check(info.idrPictureFlag == (pictures == 0 ? 1 : 0) && info.profileIdc == 100, "picture flags mismatch");
        AvcPictureInfo avc{sizeof(AvcPictureInfo)};
        check(sceVideodec2GetAvcPictureInfo_nid_postfix(&output, &avc, nullptr) == 0 && avc.isValid, "avc picture info missing");
        check(avc.ptsData == 1000 + unit && avc.dtsData == unit && avc.attachedData == 0xa0 + unit && avc.idrPictureFlag == (pictures == 0 ? 1 : 0) && avc.profileIdc == 100 && avc.picWidthInLumaSamples == Width && avc.picHeightInLumaSamples == CodedHeight, "avc picture info mismatch");
        check(hashNv12(buffer.data(), output.framePitch, output.frameHeight) == PictureHashes[pictures], "picture " + std::to_string(pictures) + " differs from the reference");
        ++pictures;
        return true;
    };
    auto units = accessUnits(lengthPrefixed);
    check(units.size() == PictureHashes.size(), "unexpected access unit count");
    if (separateParameters) {
        auto& first = units.front();
        std::size_t slice = 0;
        for (std::size_t index = 0; index + 3 < first.size(); ++index) {
            if (first[index] != 0 || first[index + 1] != 0 || first[index + 2] != 1) continue;
            const auto type = first[index + 3] & 0x1fu;
            if (type >= 1 && type <= 5) {
                slice = index > 0 && first[index - 1] == 0 ? index - 1 : index;
                break;
            }
            index += 2;
        }
        check(slice != 0, "the first access unit has no slice after its parameter sets");
        const std::vector<std::uint8_t> parameters(first.begin(), first.begin() + static_cast<std::ptrdiff_t>(slice));
        first.erase(first.begin(), first.begin() + static_cast<std::ptrdiff_t>(slice));
        const InputData input{sizeof(InputData), parameters.data(), parameters.size(), 999, 0, 0x9f};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "a unit of parameter sets alone was not accepted");
        check(!output.isValid && !frame.isAccepted, "a unit of parameter sets alone produced a picture");
    }
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), 1000 + unit, unit, 0xa0 + unit};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
        take(output, frame);
    }
    for (;;) {
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        if (!take(output, frame)) break;
    }
    check(pictures == PictureHashes.size(), "missing pictures after flush");
    check(sceVideodec2Reset_nid_postfix(handle) == 0, "reset failed");
    check(sceVideodec2DeleteDecoder_nid_postfix(handle) == 0, "delete failed");
    checkThrows([&] { sceVideodec2DeleteDecoder_nid_postfix(handle); });
}

std::uint64_t createDecoder(std::uint32_t codec, std::uint32_t profile) {
    const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, codec, profile, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
    std::uint64_t handle = 0;
    check(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "decoder creation failed");
    return handle;
}

AvcPictureInfo query(std::vector<std::uint8_t>& buffer) {
    OutputInfo output{sizeof(OutputInfo)};
    output.frameBuffer = buffer.data();
    AvcPictureInfo info{sizeof(AvcPictureInfo)};
    check(sceVideodec2GetAvcPictureInfo_nid_postfix(&output, &info, nullptr) == 0, "picture info query failed");
    return info;
}

bool owned(std::vector<std::uint8_t>& buffer, std::uint64_t pts) {
    const auto info = query(buffer);
    return info.isValid && info.ptsData == pts;
}

std::uint64_t publishAvc(std::uint64_t handle, std::uint64_t base, std::vector<std::uint8_t>& buffer) {
    const auto units = accessUnits(false);
    std::size_t pictures = 0;
    const auto take = [&](const OutputInfo& output) {
        if (!output.isValid) return false;
        check(owned(buffer, base + DisplayOrderUnits[pictures]), "picture info does not follow the latest publication");
        ++pictures;
        return true;
    };
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), base + unit, unit, base + unit};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
        take(output);
    }
    for (;;) {
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        if (!take(output)) break;
    }
    check(pictures == PictureHashes.size(), "missing pictures after flush");
    return base + DisplayOrderUnits[pictures - 1];
}

void publishClip(std::uint64_t handle, const std::vector<std::vector<std::uint8_t>>& units, std::vector<std::uint8_t>& buffer) {
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        const InputData input{sizeof(InputData), units[unit].data(), units[unit].size(), unit, unit, 0};
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode failed");
    }
    for (;;) {
        FrameBuffer frame{sizeof(FrameBuffer), buffer.data(), buffer.size(), false};
        OutputInfo output{sizeof(OutputInfo)};
        check(sceVideodec2Flush_nid_postfix(handle, &frame, &output) == 0, "flush failed");
        if (!output.isValid) break;
    }
}

std::uint64_t frameBufferBytes() {
    const DecoderConfigInfo config{sizeof(DecoderConfigInfo), 1, 1, 100, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height), 4, 1};
    DecoderMemoryInfo memory{sizeof(DecoderMemoryInfo)};
    check(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "memory query failed");
    return memory.maxFrameBufferSize;
}

void testPictureOwnership() {
    std::vector<std::uint8_t> shared(frameBufferBytes()), other(frameBufferBytes());
    {
        const auto older = createDecoder(1, 100);
        const auto newer = createDecoder(1, 100);
        publishAvc(older, 1000, shared);
        const auto latest = publishAvc(newer, 5000, shared);
        check(owned(shared, latest), "an older decoder answered for a buffer a newer decoder wrote last");
        check(sceVideodec2Reset_nid_postfix(newer) == 0, "reset failed");
        check(!query(shared).isValid, "a stale picture survived the reset of the decoder that last wrote the buffer");
        check(sceVideodec2DeleteDecoder_nid_postfix(older) == 0 && sceVideodec2DeleteDecoder_nid_postfix(newer) == 0, "delete failed");
    }
    {
        const auto older = createDecoder(1, 100);
        const auto newer = createDecoder(1, 100);
        publishAvc(older, 1000, shared);
        publishAvc(newer, 5000, shared);
        check(sceVideodec2DeleteDecoder_nid_postfix(newer) == 0, "delete failed");
        check(!query(shared).isValid, "a stale picture survived the deletion of the decoder that last wrote the buffer");
        check(sceVideodec2DeleteDecoder_nid_postfix(older) == 0, "delete failed");
    }
    {
        const auto older = createDecoder(1, 100);
        const auto newer = createDecoder(1, 100);
        publishAvc(older, 1000, shared);
        const auto latest = publishAvc(newer, 5000, shared);
        check(sceVideodec2Reset_nid_postfix(older) == 0, "reset failed");
        check(owned(shared, latest), "resetting a decoder that no longer owns the buffer dropped its picture");
        check(sceVideodec2DeleteDecoder_nid_postfix(older) == 0, "delete failed");
        check(owned(shared, latest), "deleting a decoder that no longer owns the buffer dropped its picture");
        check(sceVideodec2DeleteDecoder_nid_postfix(newer) == 0, "delete failed");
        check(!query(shared).isValid, "a deleted decoder still owns its picture");
    }
    {
        const auto older = createDecoder(1, 100);
        const auto newer = createDecoder(1, 100);
        publishAvc(newer, 5000, shared);
        const auto latest = publishAvc(older, 1000, shared);
        const auto third = createDecoder(1, 100);
        const auto separate = publishAvc(third, 9000, other);
        check(owned(shared, latest) && owned(other, separate), "distinct buffers are not independent");
        check(sceVideodec2Reset_nid_postfix(older) == 0, "reset failed");
        check(!query(shared).isValid && owned(other, separate), "resetting one buffer's owner changed another buffer");
        check(sceVideodec2DeleteDecoder_nid_postfix(older) == 0 && sceVideodec2DeleteDecoder_nid_postfix(newer) == 0 && sceVideodec2DeleteDecoder_nid_postfix(third) == 0, "delete failed");
    }
    for (const auto codec : {CodecHevc, CodecVp9}) {
        const auto units = codec == CodecHevc ? hevcUnits(HevcStream.data(), HevcStream.size()) : ivfFrames();
        const auto avc = createDecoder(1, 100);
        const auto foreign = createDecoder(codec, codec == CodecHevc ? 1 : 0);
        publishAvc(avc, 1000, shared);
        publishClip(foreign, units, shared);
        checkThrows([&] { query(shared); });
        check(sceVideodec2DeleteDecoder_nid_postfix(avc) == 0, "delete failed");
        const auto newer = createDecoder(1, 100);
        const auto latest = publishAvc(newer, 5000, shared);
        check(owned(shared, latest), "a decoder of another codec answered for an AVC picture");
        check(sceVideodec2DeleteDecoder_nid_postfix(foreign) == 0 && sceVideodec2DeleteDecoder_nid_postfix(newer) == 0, "delete failed");
    }
}

void testConcurrentOwnership() {
    std::vector<std::uint8_t> shared(frameBufferBytes());
    const auto keeper = createDecoder(1, 100);
    const auto kept = publishAvc(keeper, 1000, shared);
    std::atomic<bool> done = false;
    std::string failure;
    std::thread reader([&] {
        try {
            while (!done.load()) {
                const auto info = query(shared);
                check(!info.isValid || info.ptsData == kept || (info.ptsData >= 5000 && info.ptsData < 5000 + PictureHashes.size()), "a query returned metadata no decoder published");
            }
        } catch (const std::exception& error) {
            failure = error.what();
        }
    });
    try {
        for (int round = 0; round < 8; ++round) {
            const auto writer = createDecoder(1, 100);
            publishAvc(writer, 5000, shared);
            if (round % 2 == 0) check(sceVideodec2Reset_nid_postfix(writer) == 0, "reset failed");
            check(sceVideodec2DeleteDecoder_nid_postfix(writer) == 0, "delete failed");
            check(!query(shared).isValid, "a removed writer left its picture behind");
        }
    } catch (...) {
        done = true;
        reader.join();
        throw;
    }
    done = true;
    reader.join();
    check(failure.empty(), failure);
    check(sceVideodec2DeleteDecoder_nid_postfix(keeper) == 0, "delete failed");
}

}

int main() {
    try {
        testFailures();
        testDecode(false);
        testDecode(true);
        testDecode(false, true);
        const auto hevc = hevcUnits(HevcStream.data(), HevcStream.size());
        testClip(CodecHevc, 1, hevc, HevcHashes);
        std::vector<std::vector<std::uint8_t>> hevcLengthPrefixed;
        for (const auto& unit : hevc) {
            std::vector<std::uint8_t> converted;
            std::vector<std::size_t> nals;
            for (std::size_t index = 0; index + 3 < unit.size(); ++index) {
                if (unit[index] == 0 && unit[index + 1] == 0 && unit[index + 2] == 1) {
                    nals.push_back(index + 3);
                    index += 2;
                }
            }
            for (std::size_t nal = 0; nal < nals.size(); ++nal) {
                auto end = nal + 1 < nals.size() ? nals[nal + 1] - 3 : unit.size();
                while (end > nals[nal] && unit[end - 1] == 0) --end;
                const auto length = end - nals[nal];
                for (int shift = 24; shift >= 0; shift -= 8) converted.push_back(static_cast<std::uint8_t>(length >> shift));
                converted.insert(converted.end(), unit.begin() + static_cast<std::ptrdiff_t>(nals[nal]), unit.begin() + static_cast<std::ptrdiff_t>(end));
            }
            hevcLengthPrefixed.push_back(std::move(converted));
        }
        testClip(CodecHevc, 1, hevcLengthPrefixed, HevcHashes);
        testClip(CodecVp9, 0, ivfFrames(), Vp9Hashes);
        testVp9PictureMetadata();
        testTenBit();
        testPictureOwnership();
        testConcurrentOwnership();
        std::puts("Videodec2 tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

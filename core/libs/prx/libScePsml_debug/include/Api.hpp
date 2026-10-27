#pragma once

#include "SceTypes.hpp"
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

struct MfsrRequirement {
 std::uint64_t blockBytes;
 std::uint64_t alignment;
 std::uint64_t blockCount;
};

struct MfsrBlock {
 std::uint64_t address;
 std::uint64_t bytes;
};

struct MfsrSharedResourcesParam {
 std::uint32_t type;
 std::uint32_t reserved;
 const MfsrBlock* blocks;
 std::uint64_t blockCount;
 std::uint64_t reserved2;
};

struct Mfsr2SharedResourcesParam {
 std::uint32_t type;
 std::uint32_t reserved0;
 const MfsrBlock* blocks;
 std::uint64_t blockCount;
 std::uint16_t width;
 std::uint16_t height;
 std::uint32_t reserved1;
};

struct Mfsr2ContextParam {
 std::uint32_t profile;
 std::uint32_t reserved0;
 void* sharedResources;
 const MfsrBlock* blocks;
 std::uint64_t blockCount;
 std::uint64_t reserved1;
 std::uint16_t outputWidth;
 std::uint16_t outputHeight;
 std::uint32_t inputColorSpace;
};

struct MfsrCommandBuffer {
 std::uint32_t* cursor;
 std::uint32_t sizeInDwords;
};

struct MfsrTextureDescriptor {
 std::uint32_t words[8];
};

struct Mfsr2DispatchParam {
 const MfsrTextureDescriptor* outputColor;
 const MfsrTextureDescriptor* color;
 const MfsrTextureDescriptor* motionVectors;
 const MfsrTextureDescriptor* depth;
 const MfsrTextureDescriptor* exposure;
 std::uint64_t reserved0;
 float preExposure;
 std::uint32_t renderX;
 std::uint32_t renderY;
 std::uint32_t renderWidth;
 std::uint32_t renderHeight;
 float jitter[2];
 float motionVectorScale[2];
 float motionVectorOffset[2];
 float depthRange[2];
 std::uint8_t resetHistory;
 std::uint8_t reserved1[3];
 std::uint32_t optionFlags;
 float sharpness;
};

struct MfsrContextParam {
 std::uint32_t reserved0[2];
 const MfsrBlock* blocks;
 std::uint64_t blockCount;
 std::uint16_t inputWidth;
 std::uint16_t inputHeight;
 std::uint32_t reserved1;
 const void* descriptor0;
 const void* descriptor1;
 std::uint32_t reserved2[2];
 void* sharedResources;
};

extern "C" {

std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement1100(void* requirement, const void* param);
std::int32_t APS5_VABI scePsmlMfsrCreateContext1100(void** context, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1100(void* context, void* commandBuffer, const void* param);
std::int32_t APS5_VABI scePsmlMfsrInit();
std::int32_t APS5_VABI scePsmlMfsrGetSharedResourcesInitRequirement(MfsrRequirement* requirement, const MfsrSharedResourcesParam* param);
std::int32_t APS5_VABI scePsmlMfsrCreateSharedResources(void** sharedResources, const MfsrSharedResourcesParam* param);
std::int32_t APS5_VABI scePsmlMfsrReleaseSharedResources(void* sharedResources);
std::int32_t APS5_VABI scePsmlMfsrGetContextBufferRequirement800M3_2(MfsrRequirement* requirement, const MfsrContextParam* param);
std::int32_t APS5_VABI scePsmlMfsrCreateContext800M3_2(void** context, const MfsrContextParam* param);
std::int32_t APS5_VABI scePsmlMfsrReleaseContext(void* context);
std::int32_t APS5_VABI scePsmlMfsrCreateContext1300(void** context, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1300(void* context, void* commandBuffer, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacketSizeInDwords(void* context, std::uint32_t* packetSizeInDwords);
std::int32_t APS5_VABI scePsmlMfsrSelectConfig(void* config);
std::int32_t APS5_VABI scePsmlMfsrGetMipmapBias(void* context, float* bias, std::uint32_t dimension);
std::int32_t APS5_VABI scePsmlMfsrRequestCapture(void* context);
std::int32_t APS5_VABI scePsmlMfsrIsCaptureInProgress(void* context);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket1000(void* context, void* commandBuffer, const void* param);
std::int32_t APS5_VABI scePsmlMfsrGetDispatchMfsrPacket900(void* context, void* commandBuffer, const void* param);
std::int32_t APS5_VABI scePsmlMfsr2ReleaseSharedResources(void* sharedResources);
std::int32_t APS5_VABI scePsmlMfsr2GetSharedResourcesInitRequirement(MfsrRequirement* requirement, const Mfsr2SharedResourcesParam* param);
std::int32_t APS5_VABI scePsmlMfsr2GetContextInitRequirement(MfsrRequirement* requirement, const Mfsr2ContextParam* param);
std::int32_t APS5_VABI scePsmlMfsr2ReleaseContext(void* context);
std::int32_t APS5_VABI scePsmlMfsr2GetDispatchPacketsSizeInDwords(void* context, std::uint32_t* packetSizeInDwords);
std::int32_t APS5_VABI scePsmlMfsr2CreateSharedResources(void** sharedResources, const Mfsr2SharedResourcesParam* param);
std::int32_t APS5_VABI scePsmlMfsr2GetDispatchPackets(void* context, const MfsrCommandBuffer* commandBuffer, const Mfsr2DispatchParam* param);
std::int32_t APS5_VABI scePsmlMfsr2CreateContext(void** context, const Mfsr2ContextParam* param);
std::int32_t APS5_VABI scePsmlMfsr2Init();
int APS5_VABI scePsmlMfsrEnableSpecularAA();
int APS5_VABI scePsmlMfsrEnableQuality8K();

}

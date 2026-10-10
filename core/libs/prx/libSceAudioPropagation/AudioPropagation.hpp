#ifndef CORE_LIBS_PRX_LIBSCEAUDIOPROPAGATION_AUDIOPROPAGATION_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOPROPAGATION_AUDIOPROPAGATION_HPP

#include <cstddef>
#include <cstdint>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace AudioPropagation {

inline constexpr std::uint32_t MaterialId = 0x010107d1;
inline constexpr std::size_t MaterialSize = 0x40;
inline constexpr std::uint32_t SystemMemoryId = 0x010107d4;
inline constexpr std::size_t SystemMemorySize = 0x30;
inline constexpr std::uint32_t RenderInfoId = 0x010107d6;
inline constexpr std::size_t RenderInfoSize = 0x30;
inline constexpr std::uint32_t RenderFormat = 2;
inline constexpr std::uint32_t RayId = 0x010107d7;
inline constexpr std::size_t RaySize = 0x58;
inline constexpr std::uint32_t PortalParamsId = 0x010107d8;
inline constexpr std::size_t PortalParamsSize = 0x60;
inline constexpr std::uint32_t PathEntryId = 0x010107d9;
inline constexpr std::size_t PathEntrySize = 0x28;

struct Attribute {
    std::uint32_t id;
    std::uint32_t reserved0;
    const void* value;
    std::size_t valueSize;
    std::uint32_t reserved1;
    std::uint32_t reserved2;
};

struct RenderInfo {
    AudioPropagationStructDescriptor desc;
    AudioPropagationHandle source;
    void* output;
    std::size_t outputSize;
    std::uint32_t format;
    std::uint32_t reserved;
};

struct PortalParams {
    AudioPropagationStructDescriptor desc;
    float rows[4][4];
    AudioPropagationHandle rooms[2];
};

static_assert(sizeof(AudioPropagationStructDescriptor) == 0x10);
static_assert(sizeof(AudioPropagationSystemMemory) == SystemMemorySize);
static_assert(sizeof(Attribute) == 0x20);
static_assert(sizeof(RenderInfo) == RenderInfoSize);
static_assert(offsetof(RenderInfo, source) == 0x10);
static_assert(offsetof(RenderInfo, format) == 0x28);
static_assert(sizeof(PortalParams) == PortalParamsSize);
static_assert(offsetof(PortalParams, rooms) == 0x50);

}

extern "C" {

std::int32_t APS5_VABI sceAudioPropagationSystemQueryMemory(const void* options, AudioPropagationSystemMemory* outMemory);
std::int32_t APS5_VABI sceAudioPropagationSystemCreate(const void* options, const AudioPropagationSystemMemory* memory, AudioPropagationHandle* outSystem);
std::int32_t APS5_VABI sceAudioPropagationSystemDestroy(AudioPropagationHandle system);
std::int32_t APS5_VABI sceAudioPropagationSystemSetAttributes(AudioPropagationHandle system, const AudioPropagation::Attribute* attributes, std::uint32_t count);
std::int32_t APS5_VABI sceAudioPropagationSystemRegisterMaterial(AudioPropagationHandle system, const AudioPropagationStructDescriptor* material, AudioPropagationHandle* outMaterial);
std::int32_t APS5_VABI sceAudioPropagationSystemUnregisterMaterial(AudioPropagationHandle material);
std::int32_t APS5_VABI sceAudioPropagationSystemGetRays(AudioPropagationHandle system, void* rays, std::uint32_t* count);
std::int32_t APS5_VABI sceAudioPropagationSystemSetRays(AudioPropagationHandle system, const void* rays, std::uint32_t count);
std::int32_t APS5_VABI sceAudioPropagationRoomCreate(AudioPropagationHandle system, AudioPropagationHandle* outRoom);
std::int32_t APS5_VABI sceAudioPropagationRoomDestroy(AudioPropagationHandle system, AudioPropagationHandle room);
std::int32_t APS5_VABI sceAudioPropagationPortalCreate(AudioPropagationHandle system, const AudioPropagation::PortalParams* params, AudioPropagationHandle* outPortal);
std::int32_t APS5_VABI sceAudioPropagationPortalDestroy(AudioPropagationHandle system, AudioPropagationHandle portal);
std::int32_t APS5_VABI sceAudioPropagationPortalSetAttributes(AudioPropagationHandle portal, const AudioPropagation::Attribute* attributes, std::uint32_t count);
std::int32_t APS5_VABI sceAudioPropagationSourceCreate(AudioPropagationHandle system, AudioPropagationHandle* outSource);
std::int32_t APS5_VABI sceAudioPropagationSourceDestroy(AudioPropagationHandle system, AudioPropagationHandle source);
std::int32_t APS5_VABI sceAudioPropagationSourceSetAttributes(AudioPropagationHandle source, const AudioPropagation::Attribute* attributes, std::uint32_t count);
std::int32_t APS5_VABI sceAudioPropagationSourceGetAudioPathCount(AudioPropagationHandle source, std::uint32_t* count);
std::int32_t APS5_VABI sceAudioPropagationSourceGetAudioPath(AudioPropagationHandle source, std::uint32_t index, AudioPropagationHandle* outPath);
std::int32_t APS5_VABI sceAudioPropagationSourceGetRays(AudioPropagationHandle source, void* rays, std::uint32_t* count);
std::int32_t APS5_VABI sceAudioPropagationSourceCalculateAudioPaths(AudioPropagationHandle source, const void* rays, std::uint32_t rayCount, std::uint32_t flags, void* paths, std::uint32_t pathCount);
std::int32_t APS5_VABI sceAudioPropagationSourceSetAudioPaths(AudioPropagationHandle source, const void* entries, std::uint32_t count);
std::int32_t APS5_VABI sceAudioPropagationSourceSetAudioPath(AudioPropagationHandle path, const void* data, float gain);
std::int32_t APS5_VABI sceAudioPropagationSourceRender(AudioPropagationHandle system, const AudioPropagation::RenderInfo* infos, std::uint32_t count);

}

#endif

#ifndef CORE_LIBS_PRX_LIBSCEAUDIOPROPAGATION_AUDIOPROPAGATION_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOPROPAGATION_AUDIOPROPAGATION_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace AudioPropagation {

inline constexpr std::int32_t ErrorInvalidArgument = static_cast<std::int32_t>(0x8A700001);
inline constexpr std::int32_t ErrorInvalidHandle = static_cast<std::int32_t>(0x8A700002);
inline constexpr std::int32_t ErrorNullPointer = static_cast<std::int32_t>(0x8A700003);

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

inline constexpr std::uint32_t SystemOptionsId = 0x010107d5;
inline constexpr std::size_t SystemOptionsSize = 56;

struct SystemOptions {
    AudioPropagationStructDescriptor desc;
    std::uint32_t maxSources;
    std::uint32_t maxMaterials;
    std::uint32_t parameter24;
    std::uint32_t reflectionOrder;
    std::uint32_t frameSamples;
    float propagationSpeed;
    float maxPathLength;
    std::uint32_t flags;
    std::uint8_t opaque[8];
};

static_assert(sizeof(SystemOptions) == SystemOptionsSize);
static_assert(offsetof(SystemOptions, frameSamples) == 32);
static_assert(offsetof(SystemOptions, flags) == 44);

struct SourceInfo {
    AudioPropagationHandle system;
    float position[3];
    std::uint32_t reserved20;
    float direction[3];
    std::uint32_t reserved36;
    std::uint32_t attribute2Mode;
    float attribute2Values[3];
    float attribute3Values[4];
    std::uint32_t pathCapacity;
    std::uint32_t parameter76;
};

struct SystemInfo {
    void* cpuMemory;
    std::size_t cpuMemorySize;
    std::uint32_t index;
    std::uint32_t frameSamples;
    std::uint32_t parameter24;
    std::uint32_t maxSources;
    std::uint32_t maxMaterials;
    std::uint32_t pathCapacityPerSource;
    float propagationSpeed;
    std::uint32_t reserved44;
};

static_assert(sizeof(SourceInfo) == 80);
static_assert(offsetof(SourceInfo, position) == 8);
static_assert(offsetof(SourceInfo, direction) == 24);
static_assert(offsetof(SourceInfo, attribute2Mode) == 40);
static_assert(offsetof(SourceInfo, attribute2Values) == 44);
static_assert(offsetof(SourceInfo, attribute3Values) == 56);
static_assert(offsetof(SourceInfo, pathCapacity) == 72);
static_assert(offsetof(SourceInfo, parameter76) == 76);
static_assert(sizeof(SystemInfo) == 48);
static_assert(offsetof(SystemInfo, cpuMemorySize) == 8);
static_assert(offsetof(SystemInfo, index) == 16);
static_assert(offsetof(SystemInfo, frameSamples) == 20);
static_assert(offsetof(SystemInfo, parameter24) == 24);
static_assert(offsetof(SystemInfo, maxSources) == 28);
static_assert(offsetof(SystemInfo, maxMaterials) == 32);
static_assert(offsetof(SystemInfo, pathCapacityPerSource) == 36);
static_assert(offsetof(SystemInfo, propagationSpeed) == 40);
struct ReportRegistration;

struct ReportArguments {
    std::uint32_t generalOffset;
    std::uint32_t floatingOffset;
    void* overflowArguments;
    void* savedRegisters;
};

struct ReportInfo {
    std::uint64_t size;
    AudioPropagationHandle handle;
    std::uint32_t category;
    std::int32_t result;
    std::int32_t operation;
    std::uint32_t reserved;
    const char* function;
    const char* format;
    ReportArguments arguments;
};

using ReportHandler = void (APS5_VABI*)(const ReportInfo* report, void* userData);

static_assert(sizeof(ReportArguments) == 24);
static_assert(offsetof(ReportArguments, generalOffset) == 0);
static_assert(offsetof(ReportArguments, floatingOffset) == 4);
static_assert(offsetof(ReportArguments, overflowArguments) == 8);
static_assert(offsetof(ReportArguments, savedRegisters) == 16);
static_assert(sizeof(ReportInfo) == 72);
static_assert(offsetof(ReportInfo, size) == 0);
static_assert(offsetof(ReportInfo, handle) == 8);
static_assert(offsetof(ReportInfo, category) == 16);
static_assert(offsetof(ReportInfo, result) == 20);
static_assert(offsetof(ReportInfo, operation) == 24);
static_assert(offsetof(ReportInfo, function) == 32);
static_assert(offsetof(ReportInfo, format) == 40);
static_assert(offsetof(ReportInfo, arguments) == 48);

inline constexpr std::uint32_t MaxPaths = 36;
inline constexpr std::uint32_t MaxPathPoints = 4;
inline constexpr std::uint32_t PathId = 0x010107d3;
inline constexpr std::uint32_t PathPointId = 0x010107d2;
inline constexpr std::uint32_t PathInvalid = 1;
inline constexpr std::uint32_t PathPortal = 2;


struct Attribute {
    std::uint32_t id;
    std::uint32_t reserved0;
    const void* value;
    std::size_t valueSize;
    std::uint32_t reserved1;
    std::uint32_t reserved2;
};

struct PathPoint {
    AudioPropagationStructDescriptor desc;
    AudioPropagationHandle room;
    float gain;
    float position[3];
    std::byte opaque[8];
};

struct AudioPath {
    AudioPropagationStructDescriptor desc;
    PathPoint points[4];
    std::uint32_t pointCount;
    std::uint32_t flags;
    std::byte opaque[8];
};

static_assert(sizeof(PathPoint) == 48);
static_assert(offsetof(PathPoint, room) == 16);
static_assert(offsetof(PathPoint, gain) == 24);
static_assert(offsetof(PathPoint, position) == 28);
static_assert(sizeof(AudioPath) == 224);
static_assert(offsetof(AudioPath, points) == 16);
static_assert(offsetof(AudioPath, pointCount) == 208);
static_assert(offsetof(AudioPath, flags) == 212);

struct RenderInfo {
    AudioPropagationStructDescriptor desc;
    AudioPropagationHandle source;
    float* output;
    std::size_t outputSize;
    std::uint32_t format;
    std::uint32_t channelCount;
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
static_assert(offsetof(RenderInfo, channelCount) == 0x2c);
static_assert(sizeof(PortalParams) == PortalParamsSize);
static_assert(offsetof(PortalParams, rooms) == 0x50);

}

extern "C" {

std::int32_t APS5_VABI sceAudioPropagationSystemQueryMemory(const AudioPropagation::SystemOptions* options, AudioPropagationSystemMemory* outMemory);
std::int32_t APS5_VABI sceAudioPropagationSystemCreate(const AudioPropagation::SystemOptions* options, const AudioPropagationSystemMemory* memory, AudioPropagationHandle* outSystem);
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
std::int32_t APS5_VABI sceAudioPropagationPathGetNumPoints(AudioPropagationHandle path, std::uint32_t* count);
std::int32_t APS5_VABI sceAudioPropagationSourceGetRays(AudioPropagationHandle source, void* rays, std::uint32_t* count);
std::int32_t APS5_VABI sceAudioPropagationSourceCalculateAudioPaths(AudioPropagationHandle source, const void* rays, std::uint32_t rayCount, std::uint32_t reflectionOrder, void* paths, std::uint32_t pathCount);
std::int32_t APS5_VABI sceAudioPropagationSourceSetAudioPaths(AudioPropagationHandle source, const void* entries, std::uint32_t count);
std::int32_t APS5_VABI sceAudioPropagationSourceSetAudioPath(AudioPropagationHandle path, const AudioPropagation::AudioPath* data, float gain);
std::int32_t APS5_VABI sceAudioPropagationSourceRender(AudioPropagationHandle system, AudioPropagation::RenderInfo* infos, std::uint32_t count);
int APS5_VABI sceAudioPropagationSourceQueryAudioPaths();

}

#endif

#include "AudioPropagation.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {

using namespace AudioPropagation;

constexpr std::uint64_t SystemMagic = [] {
    constexpr char text[] = "APSYSTEM";
    std::uint64_t value = 0;
    for (int index = 7; index >= 0; --index) value = (value << 8u) | static_cast<unsigned char>(text[index]);
    return value;
}();

struct SystemState {
    std::uint64_t magic;
    std::uint32_t frameSamples;
    std::uint32_t maxSources;
};

enum class Kind { Room, Portal, Source, Material };

struct Object {
    Kind kind;
    AudioPropagationHandle system;
};

struct Registry {
    std::mutex mutex;
    std::unordered_set<AudioPropagationHandle> systems;
    std::unordered_map<AudioPropagationHandle, Object> objects;
    std::unordered_map<AudioPropagationHandle, Object> orphans;
    AudioPropagationHandle next = 1;
};

Registry& registry() {
    static Registry state;
    return state;
}

void require(bool condition, const char* function) {
    if (!condition) throw std::invalid_argument(std::string(function) + ": invalid argument");
}

void requireDescriptor(const AudioPropagationStructDescriptor* desc, std::uint32_t id, std::size_t size, const char* function) {
    require(desc != nullptr && desc->id == id && desc->size == size, function);
}

void requireAttributes(const Attribute* attributes, std::uint32_t count, const char* function) {
    require(count == 0 || attributes != nullptr, function);
    for (std::uint32_t index = 0; index < count; ++index) {
        require(attributes[index].value != nullptr && attributes[index].valueSize != 0, function);
    }
}

bool liveSystem(Registry& state, AudioPropagationHandle system) {
    if (!state.systems.contains(system)) return false;
    return reinterpret_cast<const SystemState*>(system)->magic == SystemMagic;
}

void requireSystem(Registry& state, AudioPropagationHandle system, const char* function) {
    require(liveSystem(state, system), function);
}

const Object& requireObject(Registry& state, AudioPropagationHandle handle, Kind kind, const char* function) {
    const auto found = state.objects.find(handle);
    require(found != state.objects.end() && found->second.kind == kind && liveSystem(state, found->second.system), function);
    return found->second;
}

AudioPropagationHandle createObjectLocked(Registry& state, AudioPropagationHandle system, Kind kind, AudioPropagationHandle* out, const char* function) {
    require(out != nullptr, function);
    requireSystem(state, system, function);
    const auto handle = state.next++;
    state.objects.emplace(handle, Object{kind, system});
    *out = handle;
    return handle;
}

AudioPropagationHandle createObject(AudioPropagationHandle system, Kind kind, AudioPropagationHandle* out, const char* function) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    return createObjectLocked(state, system, kind, out, function);
}

void destroyObject(AudioPropagationHandle system, AudioPropagationHandle handle, Kind kind, const char* function) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto orphan = state.orphans.find(handle);
    if (orphan != state.orphans.end() && orphan->second.kind == kind && orphan->second.system == system) {
        state.orphans.erase(orphan);
        return;
    }
    require(requireObject(state, handle, kind, function).system == system, function);
    state.objects.erase(handle);
}

void requireRaysOutput(std::uint32_t* count, const char* function) {
    require(count != nullptr, function);
    *count = 0;
}

void requireNoPropagation(bool none, const char* function) {
    if (!none) throw std::runtime_error(std::string(function) + ": rays or audio paths are outside the no-propagation model");
}

}

extern "C" {

std::int32_t APS5_VABI sceAudioPropagationSystemQueryMemory(const SystemOptions* options, AudioPropagationSystemMemory* outMemory) {
    if (!options || !outMemory) return ErrorNullPointer;
    if (outMemory->desc.id != SystemMemoryId || outMemory->desc.size != SystemMemorySize) return static_cast<std::int32_t>(0x8A700007);
    outMemory->p_cpu_mem = nullptr;
    outMemory->size_cpu_mem = 0;
    outMemory->p_gpu_mem = nullptr;
    outMemory->size_gpu_mem = 0;
    if (options->desc.id != SystemOptionsId || options->desc.size != SystemOptionsSize) return static_cast<std::int32_t>(0x8A700007);
    if (options->maxSources == 0 || options->maxSources > 64 || options->maxMaterials == 0 || options->maxMaterials > 64 || options->parameter24 != 6 || options->reflectionOrder == 0 || options->reflectionOrder > 2) return ErrorInvalidArgument;
    const auto frameSamples = options->frameSamples;
    if (frameSamples != 256 && frameSamples != 512 && frameSamples != 1024 && frameSamples != 2048 && frameSamples != 4096) return ErrorInvalidArgument;
    if (!(options->propagationSpeed > 0x1p-23f) || options->propagationSpeed > 1372.0f || !(options->maxPathLength > 0x1p-23f) || options->maxPathLength > 1372.0f || options->flags > 31 || ((options->flags & 1) && frameSamples != 512)) return ErrorInvalidArgument;
    if (options->flags & 1) throw std::runtime_error(std::string(__func__) + ": GPU memory requirements depend on an unsupported native runtime query");
    const std::uint64_t sources = options->maxSources;
    const std::uint64_t paths = sources * (options->reflectionOrder == 1 ? 6 : 36);
    const float delaySeconds = options->maxPathLength / options->propagationSpeed;
    const float delaySamples = delaySeconds * 48000.0f;
    const auto delayCapacity = static_cast<std::uint64_t>(std::ceil(delaySamples));
    std::uint64_t bytes = sources * 1024 + paths * (32 * (options->reflectionOrder + 2) + 8928);
    bytes += frameSamples == 256 ? 2048 : 4 * frameSamples;
    bytes += 4 * delayCapacity * sources;
    bytes += bytes & 15;
    bytes += 160 * paths + 12352 + 4208 * options->maxMaterials;
    if (frameSamples == 256) bytes += sources * ((options->flags & 2) ? 36896 : 16416);
    if (options->flags & 4) bytes += 2000656;
    if (options->flags & 8) bytes += 288 * paths;
    if (options->flags & 16) bytes += 144 * paths;
    outMemory->size_cpu_mem = bytes + 5368 - ((bytes & 15) ^ 8);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSystemCreate(const SystemOptions* options, const AudioPropagationSystemMemory* memory, AudioPropagationHandle* outSystem) {
    require(options != nullptr && outSystem != nullptr, __func__);
    requireDescriptor(memory != nullptr ? &memory->desc : nullptr, SystemMemoryId, SystemMemorySize, __func__);
    auto requiredMemory = *memory;
    const auto result = sceAudioPropagationSystemQueryMemory(options, &requiredMemory);
    if (result != 0) return result;
    const auto address = reinterpret_cast<std::uintptr_t>(memory->p_cpu_mem);
    require(address != 0 && address % alignof(SystemState) == 0 && memory->size_cpu_mem >= requiredMemory.size_cpu_mem && memory->size_gpu_mem == 0, __func__);
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    require(!state.systems.contains(address), __func__);
    *static_cast<SystemState*>(memory->p_cpu_mem) = SystemState{SystemMagic, options->frameSamples, options->maxSources};
    state.systems.insert(address);
    *outSystem = address;
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSystemDestroy(AudioPropagationHandle system) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireSystem(state, system, __func__);
    std::erase_if(state.objects, [&](const auto& entry) {
        if (entry.second.system != system) return false;
        state.orphans.insert(entry);
        return true;
    });
    reinterpret_cast<SystemState*>(system)->magic = 0;
    state.systems.erase(system);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSystemSetAttributes(AudioPropagationHandle system, const Attribute* attributes, std::uint32_t count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireSystem(state, system, __func__);
    requireAttributes(attributes, count, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSystemRegisterMaterial(AudioPropagationHandle system, const AudioPropagationStructDescriptor* material, AudioPropagationHandle* outMaterial) {
    requireDescriptor(material, MaterialId, MaterialSize, __func__);
    createObject(system, Kind::Material, outMaterial, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSystemUnregisterMaterial(AudioPropagationHandle material) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto orphan = state.orphans.find(material);
    if (orphan != state.orphans.end() && orphan->second.kind == Kind::Material) {
        state.orphans.erase(orphan);
        return 0;
    }
    requireObject(state, material, Kind::Material, __func__);
    state.objects.erase(material);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSystemGetRays(AudioPropagationHandle system, void* rays, std::uint32_t* count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireSystem(state, system, __func__);
    require(rays != nullptr, __func__);
    requireRaysOutput(count, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSystemSetRays(AudioPropagationHandle system, const void* rays, std::uint32_t count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireSystem(state, system, __func__);
    require(count == 0 || rays != nullptr, __func__);
    requireNoPropagation(count == 0, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationRoomCreate(AudioPropagationHandle system, AudioPropagationHandle* outRoom) {
    createObject(system, Kind::Room, outRoom, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationRoomDestroy(AudioPropagationHandle system, AudioPropagationHandle room) {
    destroyObject(system, room, Kind::Room, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationPortalCreate(AudioPropagationHandle system, const PortalParams* params, AudioPropagationHandle* outPortal) {
    requireDescriptor(params != nullptr ? &params->desc : nullptr, PortalParamsId, PortalParamsSize, __func__);
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    for (const auto room : params->rooms) require(requireObject(state, room, Kind::Room, __func__).system == system, __func__);
    createObjectLocked(state, system, Kind::Portal, outPortal, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationPortalDestroy(AudioPropagationHandle system, AudioPropagationHandle portal) {
    destroyObject(system, portal, Kind::Portal, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationPortalSetAttributes(AudioPropagationHandle portal, const Attribute* attributes, std::uint32_t count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireObject(state, portal, Kind::Portal, __func__);
    requireAttributes(attributes, count, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceCreate(AudioPropagationHandle system, AudioPropagationHandle* outSource) {
    createObject(system, Kind::Source, outSource, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceDestroy(AudioPropagationHandle system, AudioPropagationHandle source) {
    destroyObject(system, source, Kind::Source, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceSetAttributes(AudioPropagationHandle source, const Attribute* attributes, std::uint32_t count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireObject(state, source, Kind::Source, __func__);
    requireAttributes(attributes, count, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceGetAudioPathCount(AudioPropagationHandle source, std::uint32_t* count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireObject(state, source, Kind::Source, __func__);
    require(count != nullptr, __func__);
    *count = 0;
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceGetAudioPath(AudioPropagationHandle source, std::uint32_t index, AudioPropagationHandle* outPath) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.objects.find(source);
    if (found == state.objects.end() || found->second.kind != Kind::Source || !liveSystem(state, found->second.system)) return ErrorInvalidHandle;
    if (!outPath) return ErrorNullPointer;
    static_cast<void>(index);
    return ErrorInvalidArgument;
}

std::int32_t APS5_VABI sceAudioPropagationSourceGetRays(AudioPropagationHandle source, void* rays, std::uint32_t* count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireObject(state, source, Kind::Source, __func__);
    require(rays != nullptr, __func__);
    requireRaysOutput(count, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceCalculateAudioPaths(AudioPropagationHandle source, const void* rays, std::uint32_t rayCount, std::uint32_t reflectionOrder, void* paths, std::uint32_t pathCount) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.objects.find(source);
    if (found == state.objects.end() || found->second.kind != Kind::Source || !liveSystem(state, found->second.system)) return ErrorInvalidHandle;
    if (!rays || !paths) return ErrorNullPointer;
    if (rayCount != 6 || reflectionOrder == 0 || pathCount == 0) return ErrorInvalidArgument;
    throw std::runtime_error(__func__);
}

std::int32_t APS5_VABI sceAudioPropagationSourceSetAudioPaths(AudioPropagationHandle source, const void* entries, std::uint32_t count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireObject(state, source, Kind::Source, __func__);
    static_cast<void>(entries);
    requireNoPropagation(count == 0, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceSetAudioPath(AudioPropagationHandle path, const AudioPath* data, float gain) {
    static_cast<void>(path);
    static_cast<void>(data);
    static_cast<void>(gain);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceRender(AudioPropagationHandle system, RenderInfo* infos, std::uint32_t count) {
    require(infos != nullptr && count != 0, __func__);
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireSystem(state, system, __func__);
    const auto& configuration = *reinterpret_cast<const SystemState*>(system);
    require(count <= configuration.maxSources, __func__);
    const auto requiredBytes = static_cast<std::size_t>(configuration.frameSamples) * 16 * sizeof(float);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto& info = infos[index];
        requireDescriptor(&info.desc, RenderInfoId, RenderInfoSize, __func__);
        require(requireObject(state, info.source, Kind::Source, __func__).system == system, __func__);
        require(info.output != nullptr && info.outputSize >= requiredBytes, __func__);
        if (info.format != RenderFormat) throw std::runtime_error(std::string(__func__) + ": render format " + std::to_string(info.format) + " is unknown");
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        infos[index].channelCount = 0;
    }
    return 0;
}

int APS5_VABI sceAudioPropagationPathGetNumPoints(AudioPropagationHandle path, std::uint32_t* count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioPropagationReportApi(std::uint32_t category, AudioPropagationHandle handle, std::int32_t result, std::int32_t operation, const char* function, const char* format, ...) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioPropagationResetAttributes(Attribute* attributes, std::uint32_t count) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioPropagationSourceQueryInfo(AudioPropagationHandle source, SourceInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioPropagationSystemLock(AudioPropagationHandle system) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioPropagationSystemQueryInfo(AudioPropagationHandle system, SystemInfo* info, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioPropagationReportRegisterHandler(std::uint32_t category, ReportHandler handler, void* userData, ReportRegistration** registration) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioPropagationReportUnregisterHandler(ReportRegistration* registration) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioPropagationSystemEnumHandles(AudioPropagationHandle* handles, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioPropagationSystemUnlock(AudioPropagationHandle system) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

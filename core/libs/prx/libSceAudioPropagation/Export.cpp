#include "AudioPropagation.hpp"

#include <cstdint>
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
    std::uint64_t reserved;
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

std::int32_t APS5_VABI sceAudioPropagationSystemQueryMemory(const void* options, AudioPropagationSystemMemory* outMemory) {
    require(options != nullptr, __func__);
    requireDescriptor(outMemory != nullptr ? &outMemory->desc : nullptr, SystemMemoryId, SystemMemorySize, __func__);
    outMemory->size_cpu_mem = sizeof(SystemState);
    outMemory->size_gpu_mem = 0;
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSystemCreate(const void* options, const AudioPropagationSystemMemory* memory, AudioPropagationHandle* outSystem) {
    require(options != nullptr && outSystem != nullptr, __func__);
    requireDescriptor(memory != nullptr ? &memory->desc : nullptr, SystemMemoryId, SystemMemorySize, __func__);
    const auto address = reinterpret_cast<std::uintptr_t>(memory->p_cpu_mem);
    require(address != 0 && address % alignof(SystemState) == 0 && memory->size_cpu_mem >= sizeof(SystemState) && memory->size_gpu_mem == 0, __func__);
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    require(!state.systems.contains(address), __func__);
    *static_cast<SystemState*>(memory->p_cpu_mem) = SystemState{SystemMagic, 0};
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
    requireObject(state, source, Kind::Source, __func__);
    static_cast<void>(outPath);
    throw std::invalid_argument(std::string(__func__) + ": index " + std::to_string(index) + " out of range (0 audio paths)");
}

std::int32_t APS5_VABI sceAudioPropagationSourceGetRays(AudioPropagationHandle source, void* rays, std::uint32_t* count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireObject(state, source, Kind::Source, __func__);
    require(rays != nullptr, __func__);
    requireRaysOutput(count, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceCalculateAudioPaths(AudioPropagationHandle source, const void* rays, std::uint32_t rayCount, std::uint32_t flags, void* paths, std::uint32_t pathCount) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireObject(state, source, Kind::Source, __func__);
    static_cast<void>(rays);
    static_cast<void>(flags);
    static_cast<void>(paths);
    requireNoPropagation(rayCount == 0 && pathCount == 0, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceSetAudioPaths(AudioPropagationHandle source, const void* entries, std::uint32_t count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireObject(state, source, Kind::Source, __func__);
    static_cast<void>(entries);
    requireNoPropagation(count == 0, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceRender(AudioPropagationHandle system, const AudioPropagationStructDescriptor* renderInfo) {
    static_cast<void>(system);
    static_cast<void>(renderInfo);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

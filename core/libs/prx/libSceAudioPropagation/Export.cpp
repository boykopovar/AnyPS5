#include "prx/libSceAudioPropagation/include/Api.hpp"
#include "prx/libSceAudioPropagation/AudioPropagation.hpp"
#include "AudioPropagation.hpp"
#include "prx/libc/include/General.hpp"

#include <cmath>
#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdint>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
    void* cpuMemory;
    std::size_t cpuMemorySize;
    std::uint32_t index;
    std::uint32_t frameSamples;
    std::uint32_t maxSources;
    std::uint32_t maxMaterials;
    std::uint32_t pathCapacityPerSource;
    float propagationSpeed;
};

struct SystemLockState {
    std::mutex mutex;
    std::condition_variable changed;
    std::thread::id owner;
    std::size_t depth = 0;
    bool destroying = false;
};

enum class Kind { Room, Portal, Source, Material, Path };

struct Object {
    Kind kind;
    AudioPropagationHandle system;
};

struct PathState {
    AudioPath data{};
    float gain = 0.0f;
};

struct Registry {
    std::mutex mutex;
    std::unordered_set<AudioPropagationHandle> systems;
    std::vector<AudioPropagationHandle> systemOrder;
    std::array<bool, 64> systemIndices{};
    std::unordered_map<AudioPropagationHandle, std::shared_ptr<SystemLockState>> systemLocks;
    std::unordered_map<AudioPropagationHandle, Object> objects;
    std::unordered_map<AudioPropagationHandle, Object> orphans;
    std::unordered_map<AudioPropagationHandle, std::array<AudioPropagationHandle, MaxPaths>> sourcePaths;
    std::unordered_map<AudioPropagationHandle, PathState> paths;
    std::mutex reportMutex;
    std::condition_variable reportChanged;
    std::array<std::array<ReportRegistration, 8>, 2> reportHandlers{};
    AudioPropagationHandle next = 1;
};

Registry& registry() {
    static Registry state;
    return state;
}

thread_local std::vector<ReportRegistration*> activeReportCallbacks;

struct ActiveReportCall {
    Registry& state;
    ReportRegistration& registration;

    ActiveReportCall(Registry& currentState, ReportRegistration& currentRegistration)
        : state(currentState), registration(currentRegistration) {
        activeReportCallbacks.push_back(&registration);
    }

    ~ActiveReportCall() {
        activeReportCallbacks.pop_back();
        std::lock_guard lock(state.reportMutex);
        --registration.activeCalls;
        state.reportChanged.notify_all();
    }
};

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
    if (kind == Kind::Source) {
        for (const auto path : state.sourcePaths.at(handle)) {
            state.paths.erase(path);
            state.objects.erase(path);
        }
        state.sourcePaths.erase(handle);
    }
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
    const auto freeIndex = std::find(state.systemIndices.begin(), state.systemIndices.end(), false);
    if (freeIndex == state.systemIndices.end()) return static_cast<std::int32_t>(0x8A700006);
    const auto index = static_cast<std::uint32_t>(freeIndex - state.systemIndices.begin());
    *static_cast<SystemState*>(memory->p_cpu_mem) = SystemState{
        SystemMagic,
        memory->p_cpu_mem,
        memory->size_cpu_mem,
        index,
        options->frameSamples,
        options->maxSources,
        options->maxMaterials,
        options->reflectionOrder == 1 ? 6u : 36u,
        options->propagationSpeed,
    };
    state.systems.insert(address);
    state.systemOrder.push_back(address);
    state.systemIndices[index] = true;
    state.systemLocks.emplace(address, std::make_shared<SystemLockState>());
    *outSystem = address;
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSystemDestroy(AudioPropagationHandle system) {
    auto& state = registry();
    std::shared_ptr<SystemLockState> systemLock;
    {
        std::lock_guard lock(state.mutex);
        requireSystem(state, system, __func__);
        systemLock = state.systemLocks.at(system);
    }
    {
        std::unique_lock systemGuard(systemLock->mutex);
        if (systemLock->destroying) return ErrorInvalidHandle;
        if (systemLock->owner == std::this_thread::get_id()) return ErrorInvalidArgument;
        systemLock->destroying = true;
        systemLock->changed.notify_all();
        systemLock->changed.wait(systemGuard, [&] { return systemLock->depth == 0; });
    }
    std::lock_guard lock(state.mutex);
    const auto lockFound = state.systemLocks.find(system);
    if (lockFound == state.systemLocks.end() || lockFound->second != systemLock || !liveSystem(state, system)) return ErrorInvalidHandle;
    std::erase_if(state.paths, [&](const auto& entry) { return state.objects.at(entry.first).system == system; });
    std::erase_if(state.sourcePaths, [&](const auto& entry) { return state.objects.at(entry.first).system == system; });
    std::erase_if(state.objects, [&](const auto& entry) {
        if (entry.second.system != system) return false;
        if (entry.second.kind != Kind::Path) state.orphans.insert(entry);
        return true;
    });
    const auto& configuration = *reinterpret_cast<const SystemState*>(system);
    state.systemIndices[configuration.index] = false;
    std::erase(state.systemOrder, system);
    state.systemLocks.erase(system);
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
    require(outSource != nullptr, __func__);
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    AudioPropagationHandle source = 0;
    createObjectLocked(state, system, Kind::Source, &source, __func__);
    std::array<AudioPropagationHandle, MaxPaths> handles{};
    try {
        for (auto& handle : handles) {
            createObjectLocked(state, system, Kind::Path, &handle, __func__);
            PathState path{};
            path.data.desc = {PathId, sizeof(AudioPath)};
            path.data.flags = PathInvalid;
            state.paths.emplace(handle, path);
        }
        state.sourcePaths.emplace(source, handles);
    } catch (...) {
        for (const auto handle : handles) {
            state.paths.erase(handle);
            state.objects.erase(handle);
        }
        state.objects.erase(source);
        throw;
    }
    *outSource = source;
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
    *count = MaxPaths;
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceGetAudioPath(AudioPropagationHandle source, std::uint32_t index, AudioPropagationHandle* outPath) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.objects.find(source);
    if (found == state.objects.end() || found->second.kind != Kind::Source || !liveSystem(state, found->second.system)) return ErrorInvalidHandle;
    if (!outPath) return ErrorNullPointer;
    if (index >= MaxPaths) return ErrorInvalidArgument;
    *outPath = state.sourcePaths.at(source)[index];
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationPathGetNumPoints(AudioPropagationHandle path, std::uint32_t* count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireObject(state, path, Kind::Path, __func__);
    require(count != nullptr, __func__);
    *count = state.paths.at(path).data.pointCount;
    return 0;
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
    NotImplemented_nid_no_patch(__func__);
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

std::int32_t APS5_VABI sceAudioPropagationSourceSetAudioPath(AudioPropagationHandle path, const AudioPath* data, float gain) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto system = requireObject(state, path, Kind::Path, __func__).system;
    requireDescriptor(data != nullptr ? &static_cast<const AudioPath*>(data)->desc : nullptr, PathId, sizeof(AudioPath), __func__);
    const auto replacement = *static_cast<const AudioPath*>(data);
    require(std::isfinite(gain) && gain >= 0.0f && replacement.pointCount <= MaxPathPoints && (replacement.flags & ~(PathInvalid | PathPortal)) == 0, __func__);
    for (std::uint32_t index = 0; index < replacement.pointCount; ++index) {
        const auto& point = replacement.points[index];
        requireDescriptor(&point.desc, PathPointId, sizeof(PathPoint), __func__);
        require(std::isfinite(point.gain) && point.gain >= 0.0f && std::isfinite(point.position[0]) && std::isfinite(point.position[1]) && std::isfinite(point.position[2]), __func__);
        if (point.room != 0) require(requireObject(state, point.room, Kind::Room, __func__).system == system, __func__);
    }
    state.paths.at(path) = PathState{replacement, gain};
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceRender(AudioPropagationHandle system, RenderInfo* infos, std::uint32_t count) {
    require(infos != nullptr && count != 0, __func__);
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireSystem(state, system, __func__);
    const auto& configuration = *reinterpret_cast<const SystemState*>(system);
    require(count <= configuration.maxSources, __func__);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto& info = infos[index];
        requireDescriptor(&info.desc, RenderInfoId, RenderInfoSize, __func__);
        require(requireObject(state, info.source, Kind::Source, __func__).system == system, __func__);
        require(info.output != nullptr && info.outputSize != 0, __func__);
        std::uint32_t channels = 0;
        switch (info.format) {
        case 0: channels = 4; break;
        case 1: channels = 9; break;
        case 2: channels = 16; break;
        case 4:
        case 5: channels = 8; break;
        default: throw std::runtime_error(std::string(__func__) + ": render format " + std::to_string(info.format) + " is unknown");
        }
        const auto requiredBytes = static_cast<std::size_t>(configuration.frameSamples) * channels * sizeof(float);
        require(info.outputSize >= requiredBytes && info.outputSize % (channels * sizeof(float)) == 0, __func__);
        for (const auto handle : state.sourcePaths.at(info.source)) {
            const auto& path = state.paths.at(handle);
            if (path.gain > 0.0f && path.data.pointCount != 0 && (path.data.flags & PathInvalid) == 0) {
                throw std::runtime_error(std::string(__func__) + ": nonempty audio path rendering is not implemented");
            }
        }
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        infos[index].channelCount = 0;
    }
    return 0;
}

int APS5_VABI sceAudioPropagationReportApi(std::uint32_t category, AudioPropagationHandle handle, std::int32_t result, std::int32_t operation, const char* function, const char* format, ...) {
    require(category < 2, __func__);

#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
#else
    std::va_list arguments;
    va_start(arguments, format);
#endif

    ReportInfo info{};
    info.size = sizeof(info);
    info.handle = handle;
    info.category = category;
    info.result = result;
    info.operation = operation;
    info.function = function;
    info.format = format;
    static_assert(sizeof(arguments) == sizeof(info.arguments));
    std::memcpy(&info.arguments, &arguments, sizeof(info.arguments));

    struct Invocation {
        ReportRegistration* registration;
        std::uint64_t generation;
        ReportHandler handler;
        void* userData;
    };
    auto& state = registry();
    std::vector<Invocation> invocations;
    {
        std::lock_guard lock(state.reportMutex);
        for (auto& registration : state.reportHandlers[category]) {
            if (registration.handler) invocations.push_back({&registration, registration.generation, registration.handler, registration.userData});
        }
    }
    for (const auto& invocation : invocations) {
        auto& registration = *invocation.registration;
        {
            std::lock_guard lock(state.reportMutex);
            if (registration.generation != invocation.generation || registration.handler != invocation.handler || registration.userData != invocation.userData) continue;
            ++registration.activeCalls;
        }
        ActiveReportCall activeCall(state, registration);
        ReportInfo handlerInfo = info;
        invocation.handler(&handlerInfo, invocation.userData);
    }
#ifdef _WIN32
    __builtin_sysv_va_end(arguments);
#else
    va_end(arguments);
#endif
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
    auto& state = registry();
    std::shared_ptr<SystemLockState> systemLock;
    {
        std::lock_guard lock(state.mutex);
        const auto found = state.systemLocks.find(system);
        if (found == state.systemLocks.end() || !liveSystem(state, system)) return ErrorInvalidHandle;
        systemLock = found->second;
    }
    std::unique_lock lock(systemLock->mutex);
    const auto currentThread = std::this_thread::get_id();
    systemLock->changed.wait(lock, [&] { return systemLock->destroying || systemLock->depth == 0 || systemLock->owner == currentThread; });
    if (systemLock->destroying) return ErrorInvalidHandle;
    systemLock->owner = currentThread;
    ++systemLock->depth;
    return 0;
}

int APS5_VABI sceAudioPropagationSystemQueryInfo(AudioPropagationHandle system, SystemInfo* info, std::size_t size) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    if (!liveSystem(state, system) || !info || size != sizeof(SystemInfo)) return ErrorInvalidHandle;
    const auto& configuration = *reinterpret_cast<const SystemState*>(system);
    info->cpuMemory = configuration.cpuMemory;
    info->cpuMemorySize = configuration.cpuMemorySize;
    info->index = configuration.index;
    info->frameSamples = configuration.frameSamples;
    info->parameter24 = 1;
    info->maxSources = configuration.maxSources;
    info->maxMaterials = configuration.maxMaterials;
    info->pathCapacityPerSource = configuration.pathCapacityPerSource;
    info->propagationSpeed = configuration.propagationSpeed;
    return 0;
}

int APS5_VABI sceAudioPropagationReportRegisterHandler(std::uint32_t category, ReportHandler handler, void* userData, ReportRegistration** registration) {
    if (category >= 2) return -4;
    if (!handler) return -2;
    auto& state = registry();
    std::lock_guard lock(state.reportMutex);
    for (auto& slot : state.reportHandlers[category]) {
        if (!slot.handler && slot.activeCalls == 0) {
            slot.userData = userData;
            slot.handler = handler;
            ++slot.generation;
            if (registration) *registration = &slot;
            return 0;
        }
    }
    return -3;
}

int APS5_VABI sceAudioPropagationReportUnregisterHandler(ReportRegistration* registration) {
    if (!registration) return -1;
    auto& state = registry();
    std::unique_lock lock(state.reportMutex);
    for (auto& category : state.reportHandlers) {
        for (auto& slot : category) {
            if (&slot == registration && slot.handler) {
                slot.handler = nullptr;
                slot.userData = nullptr;
                const auto activeHere = static_cast<std::size_t>(std::count(activeReportCallbacks.begin(), activeReportCallbacks.end(), &slot));
                state.reportChanged.wait(lock, [&] { return slot.activeCalls <= activeHere; });
                return 0;
            }
        }
    }
    return -1;
}

int APS5_VABI sceAudioPropagationSystemEnumHandles(AudioPropagationHandle* handles, std::uint32_t capacity) {
    if (!handles && capacity != 0) return ErrorInvalidArgument;
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto total = static_cast<std::uint32_t>(state.systemOrder.size());
    if (handles) {
        const auto copied = std::min(capacity, total);
        for (std::uint32_t index = 0; index < copied; ++index) {
            handles[index] = state.systemOrder[state.systemOrder.size() - index - 1];
        }
    }
    return static_cast<int>(total);
}

int APS5_VABI sceAudioPropagationSystemUnlock(AudioPropagationHandle system) {
    auto& state = registry();
    std::shared_ptr<SystemLockState> systemLock;
    {
        std::lock_guard lock(state.mutex);
        const auto found = state.systemLocks.find(system);
        if (found == state.systemLocks.end() || !liveSystem(state, system)) return ErrorInvalidHandle;
        systemLock = found->second;
    }
    {
        std::lock_guard lock(systemLock->mutex);
        if (systemLock->owner != std::this_thread::get_id() || systemLock->depth == 0) return ErrorInvalidArgument;
        if (--systemLock->depth == 0) {
            systemLock->owner = std::thread::id{};
            systemLock->changed.notify_all();
        }
    }
    return 0;
}

}

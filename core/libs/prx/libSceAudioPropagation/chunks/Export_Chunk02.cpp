extern "C" {
#include "prx/libSceAudioPropagation/include/Api.hpp"
#include "prx/libSceAudioPropagation/AudioPropagation.hpp"
#include "AudioPropagation.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

#include <cmath>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <condition_variable>
#include <cstring>
#include <cerrno>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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
    std::unordered_map<AudioPropagationHandle, SourceInfo> sourceInfos;
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

struct ActiveReportCallbacksTag {};

std::vector<ReportRegistration*>& activeReportCallbacks() {
    return HostThreadLocal<std::vector<ReportRegistration*>, ActiveReportCallbacksTag>();
}

struct ActiveReportCall {
    Registry& state;
    ReportRegistration& registration;

    ActiveReportCall(Registry& currentState, ReportRegistration& currentRegistration)
        : state(currentState), registration(currentRegistration) {
        activeReportCallbacks().push_back(&registration);
    }

    ~ActiveReportCall() {
        activeReportCallbacks().pop_back();
        std::lock_guard lock(state.reportMutex);
        --registration.activeCalls;
        state.reportChanged.notify_all();
    }
};

void require(bool condition, const char* function) {
    if (!condition) throw std::invalid_argument(std::string(function) + ": invalid argument");
}

template <typename T>
bool copyGuestIn(T& dst, const T* src);

template <typename T>
bool copyGuestOut(T* dst, const T& src);

void requireDescriptor(const AudioPropagationStructDescriptor* desc, std::uint32_t id, std::size_t size, const char* function) {
    require(desc != nullptr && desc->id == id && desc->size == size, function);
}

std::vector<Attribute> requireAttributes(const Attribute* attributes, std::uint32_t count, const char* function) {
    require(count == 0 || attributes != nullptr, function);
    constexpr std::uint32_t Chunk = 2048;
    std::vector<Attribute> copied;
    for (std::uint32_t done = 0; done < count;) {
        const std::uint32_t step = std::min(Chunk, count - done);
        copied.resize(done + step);
        const int savedErrno = errno;
        const bool read = GuestCopyIn_nid_no_patch(copied.data() + done, attributes + done, step * sizeof(Attribute));
        errno = savedErrno;
        require(read, function);
        done += step;
    }
    for (const auto& attribute : copied) require(attribute.value != nullptr && attribute.valueSize != 0, function);
    return copied;
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
    if (!copyGuestOut(out, handle)) return 0;
    state.objects.emplace(handle, Object{kind, system});
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
        state.sourceInfos.erase(handle);
    }
    state.objects.erase(handle);
}

std::int32_t writeRaysCount(std::uint32_t* count, const char* function) {
    require(count != nullptr, function);
    return copyGuestOut(count, std::uint32_t{0}) ? 0 : ErrorNullPointer;
}

void requireNoPropagation(bool none, const char* function) {
    if (!none) throw std::runtime_error(std::string(function) + ": rays or audio paths are outside the no-propagation model");
}

template <typename T>
bool copyGuestIn(T& dst, const T* src) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&dst, src, sizeof(T));
    errno = savedErrno;
    return copied;
}

template <typename T>
bool copyGuestOut(T* dst, const T& src) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(dst, &src, sizeof(T));
    errno = savedErrno;
    return copied;
}

SourceInfo DefaultSourceInfo(AudioPropagationHandle system) {
    SourceInfo info{};
    info.system = system;
    info.direction[2] = 1.0f;
    info.attribute2Mode = 3;
    info.attribute2Values[0] = 100.0f;
    info.attribute2Values[1] = 1.0f;
    info.attribute2Values[2] = 1.0f;
    info.attribute3Values[0] = 1.0f;
    info.attribute3Values[1] = 30.0f;
    info.attribute3Values[2] = 0.7f;
    info.attribute3Values[3] = 270.0f;
    info.pathCapacity = MaxPaths;
    return info;
}

bool StoreSourceAttribute(SourceInfo& info, const Attribute& attribute) {
    if (attribute.id > 3) return true;
    if (attribute.valueSize != 16) return false;
    std::array<std::uint32_t, 4> words{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(words.data(), attribute.value, sizeof(words));
    errno = savedErrno;
    if (!copied) return false;
    switch (attribute.id) {
        case 0: std::memcpy(info.position, words.data(), sizeof(info.position)); break;
        case 1: std::memcpy(info.direction, words.data(), sizeof(info.direction)); break;
        case 2:
            info.attribute2Mode = words[0];
            std::memcpy(info.attribute2Values, words.data() + 1, sizeof(info.attribute2Values));
            break;
        case 3: std::memcpy(info.attribute3Values, words.data(), sizeof(info.attribute3Values)); break;
        default: break;
    }
    return true;
}

struct PathResult {
    std::uint32_t pointCount;
    std::uint32_t flags;
};

static_assert(offsetof(AudioPath, flags) == offsetof(AudioPath, pointCount) + sizeof(std::uint32_t));

PathState validatedPath(Registry& state, AudioPropagationHandle path, const AudioPath* data, float gain, const char* function) {
    const auto system = requireObject(state, path, Kind::Path, function).system;
    AudioPath replacement{};
    require(data != nullptr && copyGuestIn(replacement, data), function);
    requireDescriptor(&replacement.desc, PathId, sizeof(AudioPath), function);
    require(std::isfinite(gain) && gain >= 0.0f && replacement.pointCount <= MaxPathPoints && (replacement.flags & ~(PathInvalid | PathPortal)) == 0, function);
    for (std::uint32_t index = 0; index < replacement.pointCount; ++index) {
        const auto& point = replacement.points[index];
        requireDescriptor(&point.desc, PathPointId, sizeof(PathPoint), function);
        require(std::isfinite(point.gain) && point.gain >= 0.0f && std::isfinite(point.position[0]) && std::isfinite(point.position[1]) && std::isfinite(point.position[2]), function);
        if (point.material != 0) require(requireObject(state, point.material, Kind::Material, function).system == system, function);
    }
    return PathState{replacement, gain};
}

}


int APS5_VABI sceAudioPropagationResetAttributes(Attribute* attributes, std::uint32_t count) {
    if (!attributes) return ErrorNullPointer;
    if (!count) return ErrorInvalidArgument;
    const auto address = reinterpret_cast<std::uintptr_t>(attributes);
    const auto readAttribute = [&](std::uint32_t index, Attribute& attribute) {
        if (index > (UINTPTR_MAX - address) / sizeof(Attribute)) return false;
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(&attribute, reinterpret_cast<const void*>(address + static_cast<std::size_t>(index) * sizeof(Attribute)), sizeof(attribute));
        errno = savedErrno;
        return copied;
    };
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    for (std::uint32_t index = 0; index < count; ++index) {
        Attribute attribute{};
        if (!readAttribute(index, attribute)) return ErrorInvalidArgument;
        if (!attribute.value) return ErrorNullPointer;
        if (attribute.id > 0x10000) {
            const bool handle = attribute.id == 0x10001 || attribute.id == 0x20000;
            if (!handle && attribute.id - 0x10002 >= 4) return ErrorInvalidArgument;
            if (attribute.valueSize != (attribute.id == 0x20000 ? 8 : 16)) return ErrorInvalidArgument;
            if (handle) {
                const std::size_t roomCount = attribute.id == 0x10001 ? 2 : 1;
                std::array<AudioPropagationHandle, 2> rooms{};
                const int savedErrno = errno;
                const bool copied = GuestCopyIn_nid_no_patch(rooms.data(), attribute.value, roomCount * sizeof(AudioPropagationHandle));
                errno = savedErrno;
                if (!copied) return ErrorInvalidArgument;
                for (std::size_t slot = 0; slot < roomCount; ++slot) {
                    const auto room = rooms[slot];
                    if (attribute.id == 0x20000 && room == 0) continue;
                    const auto found = state.objects.find(room);
                    if (found == state.objects.end() || found->second.kind != Kind::Room || !liveSystem(state, found->second.system)) return ErrorInvalidHandle;
                }
            }
        } else if (attribute.id <= 7) {
            constexpr std::array<std::size_t, 8> sizes{16, 16, 16, 16, 24, 8, 16, 8};
            if (attribute.valueSize != sizes[attribute.id]) return ErrorInvalidArgument;
        } else if (attribute.id == 10) {
            NotImplemented_nid_no_patch(__func__);
        } else if (attribute.id != 0x10000 || attribute.valueSize != 16) {
            return ErrorInvalidArgument;
        }
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        Attribute attribute{};
        if (!readAttribute(index, attribute)) return ErrorInvalidArgument;
        std::array<std::uint32_t, 4> value{};
        std::size_t bytes = 0;
        switch (attribute.id) {
            case 0: bytes = 12; break;
            case 1: value[2] = 0x3f800000; bytes = 12; break;
            case 2: value = {3, 0x42c80000, 0x3f800000, 0x3f800000}; bytes = 16; break;
            case 3: value = {0x3f800000, 0x41f00000, 0x3f333333, 0x43870000}; bytes = 16; break;
            case 4: bytes = 16; break;
            case 5: bytes = 4; break;
            case 6:
            case 7: bytes = 8; break;
            case 0x10001: bytes = 16; break;
            case 0x10002: value = {0x3f000000, 0x3f000000, 0x3f000000, 0}; bytes = 12; break;
            case 0x10003: value[2] = 0x3f800000; bytes = 12; break;
            case 0x10004: value[1] = 0x3f800000; bytes = 12; break;
            default: return ErrorInvalidArgument;
        }
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(const_cast<void*>(attribute.value), value.data(), bytes);
        errno = savedErrno;
        if (!copied) return ErrorNullPointer;
    }
    return 0;
}

int APS5_VABI sceAudioPropagationSourceQueryInfo(AudioPropagationHandle source, SourceInfo* info) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.objects.find(source);
    if (found == state.objects.end() || found->second.kind != Kind::Source || !liveSystem(state, found->second.system)) return ErrorInvalidHandle;
    if (!info) return ErrorNullPointer;
    const SourceInfo local = state.sourceInfos.at(source);
    return copyGuestOut(info, local) ? 0 : ErrorNullPointer;
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
    SystemInfo localInfo{};
    const int savedErrno = errno;
    const bool read = GuestCopyIn_nid_no_patch(&localInfo, info, sizeof(localInfo));
    errno = savedErrno;
    if (!read) return ErrorInvalidHandle;
    localInfo.cpuMemory = configuration.cpuMemory;
    localInfo.cpuMemorySize = configuration.cpuMemorySize;
    localInfo.index = configuration.index;
    localInfo.frameSamples = configuration.frameSamples;
    localInfo.parameter24 = 1;
    localInfo.maxSources = configuration.maxSources;
    localInfo.maxMaterials = configuration.maxMaterials;
    localInfo.pathCapacityPerSource = configuration.pathCapacityPerSource;
    localInfo.propagationSpeed = configuration.propagationSpeed;
    const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
    errno = savedErrno;
    if (!copied) return ErrorInvalidHandle;
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
            if (registration) {
                if (!copyGuestOut(registration, &slot)) {
                    slot.userData = nullptr;
                    slot.handler = nullptr;
                    --slot.generation;
                    return -1;
                }
            }
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
                const auto activeHere = static_cast<std::size_t>(std::count(activeReportCallbacks().begin(), activeReportCallbacks().end(), &slot));
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
            if (!copyGuestOut(handles + index, state.systemOrder[state.systemOrder.size() - index - 1])) return ErrorNullPointer;
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

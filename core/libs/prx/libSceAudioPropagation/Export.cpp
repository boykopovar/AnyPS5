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

struct PcmRecord {
    const void* data;
    std::uint32_t sizeBytes;
    std::uint32_t type;
    std::uint32_t reserved[2];
};

static_assert(sizeof(PcmRecord) == 24);

struct PathRender {
    bool active = false;
    const char* unsupported = nullptr;
    bool filtered = false;
    std::array<float, 6> absorption{};
    float delay = 0.0f;
    std::array<float, 4> target{};
    std::array<float, 4> applied{};
};

struct SourceRender {
    PcmRecord pcm{};
    bool hasPcm = false;
    std::vector<float> history;
    std::uint64_t written = 0;
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
    std::unordered_map<AudioPropagationHandle, std::array<float, 6>> materials;
    std::unordered_map<AudioPropagationHandle, SourceRender> sourceRenders;
    std::unordered_map<AudioPropagationHandle, PathRender> pathRenders;
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
            state.pathRenders.erase(path);
            state.objects.erase(path);
        }
        state.sourcePaths.erase(handle);
        state.sourceInfos.erase(handle);
        state.sourceRenders.erase(handle);
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

constexpr std::size_t MaterialBandTaps = 513;
constexpr std::size_t HistorySize = 1u << 18;

const std::array<std::array<float, MaterialBandTaps>, 6>& materialBandResponses() {
    static const auto responses = [] {
        constexpr double Pi = 3.14159265358979323846;
        constexpr int Grid = 1024;
        constexpr int Bins = Grid / 2 + 1;
        std::array<std::array<double, Bins>, 5> lowpass{};
        for (int edge = 0; edge < 5; ++edge) {
            const double centre = static_cast<double>((4 << edge) + 1);
            const double width = (centre - 1.0) / 3.0;
            for (int bin = 0; bin < Bins; ++bin) {
                const double u = std::clamp((bin - centre) / width, -1.0, 1.0);
                lowpass[edge][bin] = 0.5 - 0.5 * std::sin(0.5 * Pi * u);
            }
        }
        std::array<std::array<float, MaterialBandTaps>, 6> out{};
        for (int band = 0; band < 6; ++band) {
            std::array<double, Bins> response{};
            for (int bin = 0; bin < Bins; ++bin) {
                const double upper = band < 5 ? lowpass[band][bin] : 1.0;
                const double lower = band > 0 ? lowpass[band - 1][bin] : 0.0;
                response[bin] = upper - lower;
            }
            for (int tap = -256; tap <= 256; ++tap) {
                double sum = response[0] + response[Bins - 1] * ((tap & 1) != 0 ? -1.0 : 1.0);
                for (int bin = 1; bin < Bins - 1; ++bin) sum += 2.0 * response[bin] * std::cos(2.0 * Pi * bin * tap / Grid);
                const double window = 0.54 + 0.46 * std::cos(Pi * tap / 256.0);
                out[band][static_cast<std::size_t>(tap + 256)] = static_cast<float>(sum / Grid * window);
            }
        }
        return out;
    }();
    return responses;
}

float distanceGain(const SourceInfo& info, float d) {
    const float maxDistance = info.attribute2Values[0];
    const float factor = info.attribute2Values[1];
    const float reference = info.attribute2Values[2];
    const auto mode = info.attribute2Mode;
    if (mode >= 3) d = std::clamp(d, reference, std::max(reference, maxDistance));
    switch (mode % 3) {
    case 0: {
        const float denominator = reference + factor * (d - reference);
        return denominator > 0.0f ? reference / denominator : 1.0f;
    }
    case 1: {
        const float span = maxDistance - reference;
        if (span <= 0.0f) return 1.0f;
        return std::max(0.0f, 1.0f - factor * (d - reference) / span);
    }
    default:
        if (reference <= 0.0f || d <= 0.0f) return 1.0f;
        return std::pow(d / reference, -factor);
    }
}

float coneGain(const SourceInfo& info, const float* toward) {
    const float* direction = info.direction;
    const float lengthA = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
    const float lengthB = std::sqrt(toward[0] * toward[0] + toward[1] * toward[1] + toward[2] * toward[2]);
    if (lengthA <= 0.0f || lengthB <= 0.0f) return info.attribute3Values[0];
    const float cosine = std::clamp((direction[0] * toward[0] + direction[1] * toward[1] + direction[2] * toward[2]) / (lengthA * lengthB), -1.0f, 1.0f);
    const float angle = std::acos(cosine) * 180.0f / 3.14159265358979f;
    const float inner = info.attribute3Values[1] * 0.5f;
    const float outer = info.attribute3Values[3] * 0.5f;
    if (angle <= inner) return info.attribute3Values[0];
    if (angle >= outer || outer <= inner) return info.attribute3Values[2];
    const float t = (angle - inner) / (outer - inner);
    return info.attribute3Values[0] + t * (info.attribute3Values[2] - info.attribute3Values[0]);
}

void computePathRender(Registry& state, AudioPropagationHandle path, const PathState& updated) {
    auto& render = state.pathRenders[path];
    const auto& data = updated.data;
    render.active = false;
    render.unsupported = nullptr;
    render.filtered = false;
    if (updated.gain <= 0.0f || data.pointCount == 0 || (data.flags & PathInvalid) != 0) {
        render.target = {};
        return;
    }
    render.active = true;
    if ((data.flags & PathPortal) != 0) {
        render.unsupported = "a portal path";
        return;
    }
    if (data.pointCount != 3) {
        render.unsupported = "a path without exactly three points";
        return;
    }
    if (data.points[0].material != 0 || data.points[2].material != 0 || data.points[1].material == 0) {
        render.unsupported = "a path whose middle point has no material or whose end points have one";
        return;
    }
    AudioPropagationHandle owner = 0;
    for (const auto& [source, handles] : state.sourcePaths) {
        if (std::find(handles.begin(), handles.end(), path) != handles.end()) {
            owner = source;
            break;
        }
    }
    require(owner != 0, __func__);
    const auto& info = state.sourceInfos.at(owner);
    if (info.attribute2Mode > 5) {
        render.unsupported = "a rolloff model above 5";
        return;
    }
    const auto& system = *reinterpret_cast<const SystemState*>(state.objects.at(path).system);
    float length = 0.0f;
    float gain = updated.gain;
    for (std::uint32_t index = 0; index < data.pointCount; ++index) {
        const auto& point = data.points[index];
        gain *= point.gain;
        if (index + 1 < data.pointCount) {
            const auto& next = data.points[index + 1];
            const float dx = next.position[0] - point.position[0];
            const float dy = next.position[1] - point.position[1];
            const float dz = next.position[2] - point.position[2];
            length += std::sqrt(dx * dx + dy * dy + dz * dz);
        }
    }
    const auto& absorption = state.materials.at(data.points[1].material);
    render.absorption = absorption;
    const bool uniform = std::all_of(absorption.begin(), absorption.end(), [&](float value) { return value == absorption[0]; });
    if (uniform) {
        gain *= 1.0f - absorption[0];
    } else {
        render.filtered = true;
    }
    const float sourceDistance = std::sqrt(info.position[0] * info.position[0] + info.position[1] * info.position[1] + info.position[2] * info.position[2]);
    if (!std::isfinite(sourceDistance) || !std::isfinite(length)) throw std::runtime_error(std::string(__func__) + ": a source position or path length that is not finite is not measured");
    const float speed = system.propagationSpeed;
    render.delay = std::max(0.0f, length - sourceDistance) * 48000.0f / speed + 256.0f + (sourceDistance > 0.0f ? static_cast<float>(system.frameSamples) : 0.0f);
    const auto maximumDelay = static_cast<float>(HistorySize - system.frameSamples - MaterialBandTaps);
    if (!std::isfinite(render.delay) || render.delay > maximumDelay) throw std::runtime_error(std::string(__func__) + ": a path delay above " + std::to_string(static_cast<std::uint64_t>(maximumDelay)) + " samples is not supported");
    const float toward[3] = {data.points[1].position[0] - info.position[0], data.points[1].position[1] - info.position[1], data.points[1].position[2] - info.position[2]};
    gain *= distanceGain(info, length) * coneGain(info, toward);
    const auto& arrival = data.points[1].position;
    const auto& listener = data.points[2].position;
    float v[3] = {arrival[0] - listener[0], arrival[1] - listener[1], arrival[2] - listener[2]};
    const float norm = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (norm > 0.0f) {
        for (auto& component : v) component /= norm;
    }
    render.target = {gain, -v[0] * gain, v[1] * gain, v[2] * gain};
}

}

extern "C" {

std::int32_t APS5_VABI sceAudioPropagationSystemQueryMemory(const SystemOptions* options, AudioPropagationSystemMemory* outMemory) {
    if (!options || !outMemory) return ErrorNullPointer;
    AudioPropagationSystemMemory localMemory{};
    if (!copyGuestIn(localMemory, outMemory)) return ErrorNullPointer;
    if (localMemory.desc.id != SystemMemoryId || localMemory.desc.size != SystemMemorySize) return static_cast<std::int32_t>(0x8A700007);
    SystemOptions localOptions{};
    if (!copyGuestIn(localOptions, options)) return ErrorNullPointer;
    localMemory.p_cpu_mem = nullptr;
    localMemory.size_cpu_mem = 0;
    localMemory.p_gpu_mem = nullptr;
    localMemory.size_gpu_mem = 0;
    if (!copyGuestOut(outMemory, localMemory)) return ErrorNullPointer;
    if (localOptions.desc.id != SystemOptionsId || localOptions.desc.size != SystemOptionsSize) return static_cast<std::int32_t>(0x8A700007);
    if (localOptions.maxSources == 0 || localOptions.maxSources > 64 || localOptions.maxMaterials == 0 || localOptions.maxMaterials > 64 || localOptions.parameter24 != 6 || localOptions.reflectionOrder == 0 || localOptions.reflectionOrder > 2) return ErrorInvalidArgument;
    const auto frameSamples = localOptions.frameSamples;
    if (frameSamples != 256 && frameSamples != 512 && frameSamples != 1024 && frameSamples != 2048 && frameSamples != 4096) return ErrorInvalidArgument;
    if (!(localOptions.propagationSpeed > 0x1p-23f) || localOptions.propagationSpeed > 1372.0f || !(localOptions.maxPathLength > 0x1p-23f) || localOptions.maxPathLength > 1372.0f || localOptions.flags > 31 || ((localOptions.flags & 1) && frameSamples != 512)) return ErrorInvalidArgument;
    if (localOptions.flags & 1) throw std::runtime_error(std::string(__func__) + ": GPU memory requirements depend on an unsupported native runtime query");
    const std::uint64_t sources = localOptions.maxSources;
    const std::uint64_t paths = sources * (localOptions.reflectionOrder == 1 ? 6 : 36);
    const float delaySeconds = localOptions.maxPathLength / localOptions.propagationSpeed;
    const float delaySamples = delaySeconds * 48000.0f;
    const auto delayCapacity = static_cast<std::uint64_t>(std::ceil(delaySamples));
    std::uint64_t bytes = sources * 1024 + paths * (32 * (localOptions.reflectionOrder + 2) + 8928);
    bytes += frameSamples == 256 ? 2048 : 4 * frameSamples;
    bytes += 4 * delayCapacity * sources;
    bytes += bytes & 15;
    bytes += 160 * paths + 12352 + 4208 * localOptions.maxMaterials;
    if (frameSamples == 256) bytes += sources * ((localOptions.flags & 2) ? 36896 : 16416);
    if (localOptions.flags & 4) bytes += 2000656;
    if (localOptions.flags & 8) bytes += 288 * paths;
    if (localOptions.flags & 16) bytes += 144 * paths;
    localMemory.p_cpu_mem = nullptr;
    localMemory.size_cpu_mem = bytes + 5368 - ((bytes & 15) ^ 8);
    localMemory.p_gpu_mem = nullptr;
    localMemory.size_gpu_mem = 0;
    if (!copyGuestOut(outMemory, localMemory)) return ErrorNullPointer;
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSystemCreate(const SystemOptions* options, const AudioPropagationSystemMemory* memory, AudioPropagationHandle* outSystem) {
    require(options != nullptr && memory != nullptr && outSystem != nullptr, __func__);
    SystemOptions localOptions{};
    if (!copyGuestIn(localOptions, options)) return ErrorNullPointer;
    AudioPropagationSystemMemory localMemory{};
    if (!copyGuestIn(localMemory, memory)) return ErrorNullPointer;
    requireDescriptor(&localMemory.desc, SystemMemoryId, SystemMemorySize, __func__);
    auto requiredMemory = localMemory;
    const auto result = sceAudioPropagationSystemQueryMemory(&localOptions, &requiredMemory);
    if (result != 0) return result;
    const auto address = reinterpret_cast<std::uintptr_t>(localMemory.p_cpu_mem);
    require(address != 0 && address % alignof(SystemState) == 0 && localMemory.size_cpu_mem >= requiredMemory.size_cpu_mem && localMemory.size_gpu_mem == 0, __func__);
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    require(!state.systems.contains(address), __func__);
    const auto freeIndex = std::find(state.systemIndices.begin(), state.systemIndices.end(), false);
    if (freeIndex == state.systemIndices.end()) return static_cast<std::int32_t>(0x8A700006);
    const auto index = static_cast<std::uint32_t>(freeIndex - state.systemIndices.begin());
    if (!copyGuestOut(outSystem, static_cast<AudioPropagationHandle>(address))) return ErrorNullPointer;
    if (!copyGuestOut(static_cast<SystemState*>(localMemory.p_cpu_mem), SystemState{
        SystemMagic,
        localMemory.p_cpu_mem,
        localMemory.size_cpu_mem,
        index,
        localOptions.frameSamples,
        localOptions.maxSources,
        localOptions.maxMaterials,
        localOptions.reflectionOrder == 1 ? 6u : 36u,
        localOptions.propagationSpeed,
    })) return ErrorNullPointer;
    state.systems.insert(address);
    state.systemOrder.push_back(address);
    state.systemIndices[index] = true;
    state.systemLocks.emplace(address, std::make_shared<SystemLockState>());
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
    std::erase_if(state.sourceInfos, [&](const auto& entry) { return state.objects.at(entry.first).system == system; });
    std::erase_if(state.sourceRenders, [&](const auto& entry) { return state.objects.at(entry.first).system == system; });
    std::erase_if(state.pathRenders, [&](const auto& entry) { return state.objects.at(entry.first).system == system; });
    std::erase_if(state.materials, [&](const auto& entry) { return state.objects.at(entry.first).system == system; });
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
    std::array<float, 16> record{};
    require(GuestCopyIn_nid_no_patch(record.data(), material, sizeof(record)), __func__);
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto handle = createObjectLocked(state, system, Kind::Material, outMaterial, __func__);
    if (handle == 0) return ErrorNullPointer;
    std::array<float, 6> absorption{};
    std::copy(record.begin() + 8, record.begin() + 14, absorption.begin());
    state.materials[handle] = absorption;
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
    state.materials.erase(material);
    state.objects.erase(material);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSystemGetRays(AudioPropagationHandle system, void* rays, std::uint32_t* count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireSystem(state, system, __func__);
    require(rays != nullptr, __func__);
    return writeRaysCount(count, __func__);
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
    return createObject(system, Kind::Room, outRoom, __func__) != 0 ? 0 : ErrorNullPointer;
}

std::int32_t APS5_VABI sceAudioPropagationRoomDestroy(AudioPropagationHandle system, AudioPropagationHandle room) {
    destroyObject(system, room, Kind::Room, __func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationPortalCreate(AudioPropagationHandle system, const PortalParams* params, AudioPropagationHandle* outPortal) {
    require(params != nullptr, __func__);
    PortalParams localParams{};
    if (!copyGuestIn(localParams, params)) return ErrorNullPointer;
    requireDescriptor(&localParams.desc, PortalParamsId, PortalParamsSize, __func__);
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    for (const auto room : localParams.rooms) require(requireObject(state, room, Kind::Room, __func__).system == system, __func__);
    return createObjectLocked(state, system, Kind::Portal, outPortal, __func__) != 0 ? 0 : ErrorNullPointer;
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
    if (!copyGuestOut(outSource, source)) {
        state.objects.erase(source);
        return ErrorNullPointer;
    }
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
        state.sourceInfos.emplace(source, DefaultSourceInfo(system));
    } catch (...) {
        for (const auto handle : handles) {
            state.paths.erase(handle);
            state.objects.erase(handle);
        }
        state.sourceInfos.erase(source);
        state.sourcePaths.erase(source);
        state.objects.erase(source);
        throw;
    }
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
    const auto copied = requireAttributes(attributes, count, __func__);
    SourceInfo updated = state.sourceInfos.at(source);
    for (const auto& attribute : copied) require(StoreSourceAttribute(updated, attribute), __func__);
    state.sourceInfos.at(source) = updated;
    for (const auto& attribute : copied) {
        if (attribute.id != 4 || attribute.valueSize != sizeof(PcmRecord)) continue;
        PcmRecord pcm{};
        require(GuestCopyIn_nid_no_patch(&pcm, attribute.value, sizeof(pcm)), __func__);
        auto& render = state.sourceRenders[source];
        render.pcm = pcm;
        render.hasPcm = true;
    }
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceGetAudioPathCount(AudioPropagationHandle source, std::uint32_t* count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.objects.find(source);
    if (found == state.objects.end() || found->second.kind != Kind::Source || !liveSystem(state, found->second.system)) return ErrorInvalidHandle;
    require(count != nullptr, __func__);
    if (!copyGuestOut(count, static_cast<std::uint32_t>(MaxPaths))) return ErrorNullPointer;
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceGetAudioPath(AudioPropagationHandle source, std::uint32_t index, AudioPropagationHandle* outPath) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.objects.find(source);
    if (found == state.objects.end() || found->second.kind != Kind::Source || !liveSystem(state, found->second.system)) return ErrorInvalidHandle;
    if (!outPath) return ErrorNullPointer;
    if (index >= MaxPaths) return ErrorInvalidArgument;
    if (!copyGuestOut(outPath, state.sourcePaths.at(source)[index])) return ErrorNullPointer;
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceQueryAudioPaths(AudioPropagationHandle source, std::uint64_t* paths, std::size_t size) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.objects.find(source);
    if (found == state.objects.end() || found->second.kind != Kind::Source || !liveSystem(state, found->second.system)) return ErrorInvalidHandle;
    if (!paths) return ErrorNullPointer;
    static_cast<void>(size);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationPathGetNumPoints(AudioPropagationHandle path, std::uint32_t* count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    if (state.objects.find(path) == state.objects.end()) return ErrorInvalidHandle;
    requireObject(state, path, Kind::Path, __func__);
    require(count != nullptr, __func__);
    if (!copyGuestOut(count, state.paths.at(path).data.pointCount)) return ErrorNullPointer;
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceGetRays(AudioPropagationHandle source, void* rays, std::uint32_t* count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireObject(state, source, Kind::Source, __func__);
    require(rays != nullptr, __func__);
    return writeRaysCount(count, __func__);
}

std::int32_t APS5_VABI sceAudioPropagationSourceCalculateAudioPaths(AudioPropagationHandle source, const void* rays, std::uint32_t rayCount, std::uint32_t reflectionOrder, void* paths, std::uint32_t pathCount) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    const auto found = state.objects.find(source);
    if (found == state.objects.end() || found->second.kind != Kind::Source || !liveSystem(state, found->second.system)) return ErrorInvalidHandle;
    if (!rays || !paths) return ErrorNullPointer;
    if (rayCount == 0 && reflectionOrder != 0 && pathCount != 0) {
        auto* outputs = static_cast<std::byte*>(paths);
        for (std::uint32_t index = 0; index < pathCount; ++index) {
            AudioPropagationStructDescriptor desc{};
            require(copyGuestIn(desc, reinterpret_cast<const AudioPropagationStructDescriptor*>(outputs + index * sizeof(AudioPath) + offsetof(AudioPath, desc))), __func__);
            requireDescriptor(&desc, PathId, sizeof(AudioPath), __func__);
        }
        const PathResult none{0, PathInvalid};
        for (std::uint32_t index = 0; index < pathCount; ++index) {
            require(copyGuestOut(reinterpret_cast<PathResult*>(outputs + index * sizeof(AudioPath) + offsetof(AudioPath, pointCount)), none), __func__);
        }
        return 0;
    }
    if (rayCount != 6 || reflectionOrder == 0 || pathCount == 0) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceSetAudioPaths(AudioPropagationHandle source, const void* entries, std::uint32_t count) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireObject(state, source, Kind::Source, __func__);
    require(count == 0 || entries != nullptr, __func__);
    const auto& owned = state.sourcePaths.at(source);
    std::vector<std::pair<AudioPropagationHandle, PathState>> updates;
    for (std::uint32_t index = 0; index < count; ++index) {
        PathEntry entry{};
        require(copyGuestIn(entry, static_cast<const PathEntry*>(entries) + index), __func__);
        requireDescriptor(&entry.desc, PathEntryId, PathEntrySize, __func__);
        require(std::find(owned.begin(), owned.end(), entry.path) != owned.end(), __func__);
        require(std::none_of(updates.begin(), updates.end(), [&](const auto& update) { return update.first == entry.path; }), __func__);
        updates.emplace_back(entry.path, validatedPath(state, entry.path, entry.data, entry.gain, __func__));
    }
    for (const auto& [path, update] : updates) {
        state.paths.at(path) = update;
        computePathRender(state, path, update);
    }
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceSetAudioPath(AudioPropagationHandle path, const AudioPath* data, float gain) {
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    state.paths.at(path) = validatedPath(state, path, data, gain, __func__);
    computePathRender(state, path, state.paths.at(path));
    return 0;
}

std::int32_t APS5_VABI sceAudioPropagationSourceRender(AudioPropagationHandle system, RenderInfo* infos, std::uint32_t count) {
    require(infos != nullptr && count != 0, __func__);
    auto& state = registry();
    std::lock_guard lock(state.mutex);
    requireSystem(state, system, __func__);
    const auto& configuration = *reinterpret_cast<const SystemState*>(system);
    require(count <= configuration.maxSources, __func__);
    std::vector<std::pair<RenderInfo, bool>> validated;
    for (std::uint32_t index = 0; index < count; ++index) {
        RenderInfo info{};
        if (!copyGuestIn(info, infos + index)) return ErrorNullPointer;
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
        bool anyActive = false;
        for (const auto handle : state.sourcePaths.at(info.source)) {
            const auto found = state.pathRenders.find(handle);
            if (found == state.pathRenders.end()) continue;
            const auto& path = found->second;
            if (path.active && path.unsupported) throw std::runtime_error(std::string(__func__) + ": rendering " + path.unsupported + " is not measured");
            if (path.active || path.applied != std::array<float, 4>{}) anyActive = true;
        }
        if (anyActive) {
            if (info.format != 0) throw std::runtime_error(std::string(__func__) + ": render format " + std::to_string(info.format) + " with audio paths is not measured");
            if (configuration.frameSamples != 1024) throw std::runtime_error(std::string(__func__) + ": audio path rendering with " + std::to_string(configuration.frameSamples) + "-sample frames is not measured");
            const auto found = state.sourceRenders.find(info.source);
            if (found != state.sourceRenders.end() && found->second.hasPcm && found->second.pcm.type > 1) throw std::runtime_error(std::string(__func__) + ": PCM type " + std::to_string(found->second.pcm.type) + " is not measured");
        }
        validated.emplace_back(info, anyActive);
    }
    const std::uint32_t frame = configuration.frameSamples;
    for (const auto& [info, anyActive] : validated) {
        auto& source = state.sourceRenders[info.source];
        std::vector<float> input(frame, 0.0f);
        if (source.hasPcm && source.pcm.data != nullptr) {
            if (source.pcm.type == 1) {
                std::vector<std::int16_t> raw(std::min<std::size_t>(frame, source.pcm.sizeBytes / 2));
                if (!raw.empty()) {
                    if (!GuestCopyIn_nid_no_patch(raw.data(), source.pcm.data, raw.size() * 2)) throw std::runtime_error(std::string(__func__) + ": the source PCM could not be read");
                    for (std::size_t n = 0; n < raw.size(); ++n) input[n] = raw[n] / 32767.0f;
                }
            } else {
                const std::size_t samples = std::min<std::size_t>(frame, source.pcm.sizeBytes / 4);
                if (samples != 0 && !GuestCopyIn_nid_no_patch(input.data(), source.pcm.data, samples * 4)) throw std::runtime_error(std::string(__func__) + ": the source PCM could not be read");
            }
        }
        if (source.history.empty()) source.history.assign(HistorySize, 0.0f);
        const std::uint64_t start = source.written;
        for (std::uint32_t n = 0; n < frame; ++n) source.history[(start + n) & (HistorySize - 1)] = input[n];
        source.written += frame;
        if (!anyActive) continue;
        const auto read = [&](std::int64_t index) { return index < 0 ? 0.0f : source.history[static_cast<std::uint64_t>(index) & (HistorySize - 1)]; };
        std::vector<float> mix(static_cast<std::size_t>(frame) * 4, 0.0f);
        require(GuestCopyIn_nid_no_patch(mix.data(), info.output, mix.size() * sizeof(float)), __func__);
        for (const auto handle : state.sourcePaths.at(info.source)) {
            const auto found = state.pathRenders.find(handle);
            if (found == state.pathRenders.end()) continue;
            auto& path = found->second;
            const auto from = path.applied;
            const auto to = path.active ? path.target : std::array<float, 4>{};
            if (from == std::array<float, 4>{} && to == std::array<float, 4>{}) continue;
            constexpr int Half = static_cast<int>(MaterialBandTaps / 2);
            std::array<float, MaterialBandTaps> taps{};
            if (path.filtered) {
                taps[Half] = 1.0f;
                const auto& responses = materialBandResponses();
                for (std::size_t band = 0; band < 6; ++band) {
                    for (std::size_t tap = 0; tap < MaterialBandTaps; ++tap) taps[tap] -= path.absorption[band] * responses[band][tap];
                }
            }
            for (std::uint32_t n = 0; n < frame; ++n) {
                const double at = static_cast<double>(start + n) - path.delay;
                const auto whole = static_cast<std::int64_t>(std::floor(at));
                const float fraction = static_cast<float>(at - static_cast<double>(whole));
                float sample = 0.0f;
                if (path.filtered) {
                    for (int tap = -Half; tap <= Half; ++tap) {
                        const float weight = taps[static_cast<std::size_t>(tap + Half)];
                        if (weight == 0.0f) continue;
                        const float a = read(whole - tap);
                        const float b = read(whole - tap + 1);
                        sample += weight * (a + (b - a) * fraction);
                    }
                } else {
                    const float a = read(whole);
                    const float b = read(whole + 1);
                    sample = a + (b - a) * fraction;
                }
                const float t = static_cast<float>(n) / static_cast<float>(frame);
                for (std::uint32_t channel = 0; channel < 4; ++channel) {
                    const float coefficient = from[channel] + (to[channel] - from[channel]) * t;
                    mix[static_cast<std::size_t>(channel) * frame + n] += coefficient * sample;
                }
            }
            path.applied = to;
        }
        require(GuestCopyOut_nid_no_patch(info.output, mix.data(), mix.size() * sizeof(float)), __func__);
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        if (!copyGuestOut(&(infos + index)->channelCount, std::uint32_t{0})) return ErrorNullPointer;
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

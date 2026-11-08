#include "prx/libSceAudioPropagation/include/Api.hpp"
#include "prx/libSceAudioPropagation/AudioPropagation.hpp"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "AudioPropagation: %s\n", message);
        std::abort();
    }
}

template<typename TException, typename TCall>
void RequireThrows(TCall call, const char* message) {
    try {
        call();
    } catch (const TException&) {
        return;
    } catch (...) {
        Require(false, message);
    }
    Require(false, message);
}

void* Resolve(const char* name) {
#ifdef _WIN32
    const HMODULE module = GetModuleHandleA("libSceAudioPropagation.prx");
    Require(module != nullptr, "library module");
    return reinterpret_cast<void*>(GetProcAddress(module, name));
#else
    return dlsym(RTLD_DEFAULT, name);
#endif
}

AudioPropagationSystemMemory Memory() {
    AudioPropagationSystemMemory memory{};
    memory.desc = {AudioPropagation::SystemMemoryId, AudioPropagation::SystemMemorySize};
    return memory;
}

AudioPropagation::Attribute Attribute(const void* value, std::size_t size) {
    AudioPropagation::Attribute attribute{};
    attribute.id = 0x20000;
    attribute.value = value;
    attribute.valueSize = size;
    return attribute;
}

struct ReportProbe {
    std::uint32_t calls = 0;
    bool fieldsMatch = false;
    int integerArgument = 0;
    const char* stringArgument = nullptr;
    double floatingArgument = 0.0;
};

#ifdef _WIN32
using TestVaList = __builtin_sysv_va_list;
#else
using TestVaList = std::va_list;
#endif

static_assert(sizeof(TestVaList) == sizeof(AudioPropagation::ReportArguments));

TestVaList& ReportArguments(AudioPropagation::ReportInfo* report) {
    return *reinterpret_cast<TestVaList*>(&report->arguments);
}

struct MixedReportProbe {
    std::uint32_t calls = 0;
    int integer = 0;
    double floating = 0.0;
    const char* text = nullptr;
};

void APS5_VABI CaptureMixedReport(const AudioPropagation::ReportInfo* report, void* userData) {
    auto& probe = *static_cast<MixedReportProbe*>(userData);
    auto& arguments = ReportArguments(const_cast<AudioPropagation::ReportInfo*>(report));
    probe.integer = va_arg(arguments, int);
    probe.floating = va_arg(arguments, double);
    probe.text = va_arg(arguments, const char*);
    ++probe.calls;
}

struct FloatingReportProbe {
    std::uint32_t calls = 0;
    std::array<double, 9> values{};
};

void APS5_VABI CaptureFloatingReport(const AudioPropagation::ReportInfo* report, void* userData) {
    auto& probe = *static_cast<FloatingReportProbe*>(userData);
    auto& arguments = ReportArguments(const_cast<AudioPropagation::ReportInfo*>(report));
    for (auto& value : probe.values) value = va_arg(arguments, double);
    ++probe.calls;
}

struct IntegerReportProbe {
    std::uint32_t calls = 0;
    int integer = 0;
    const char* text = nullptr;
};

void APS5_VABI CaptureIntegerReport(const AudioPropagation::ReportInfo* report, void* userData) {
    auto& probe = *static_cast<IntegerReportProbe*>(userData);
    auto& arguments = ReportArguments(const_cast<AudioPropagation::ReportInfo*>(report));
    probe.integer = va_arg(arguments, int);
    probe.text = va_arg(arguments, const char*);
    ++probe.calls;
}

void APS5_VABI CountReport(const AudioPropagation::ReportInfo*, void* userData) {
    ++*static_cast<std::uint32_t*>(userData);
}

void APS5_VABI CaptureReport(const AudioPropagation::ReportInfo* report, void* userData) {
    auto& probe = *static_cast<ReportProbe*>(userData);
    ++probe.calls;
    probe.fieldsMatch = report && report->size == sizeof(*report) && report->handle == 0x1234 &&
        report->category == 0 && report->result == -7 && report->operation == 9 &&
        report->function && std::strcmp(report->function, "sceAudioPropagationTest") == 0 &&
        report->format && std::strcmp(report->format, "%d %s %f") == 0;
    std::memcpy(&probe.integerArgument, report->arguments.overflowArguments, sizeof(probe.integerArgument));
    std::memcpy(&probe.stringArgument, static_cast<const std::byte*>(report->arguments.overflowArguments) + sizeof(std::uint64_t), sizeof(probe.stringArgument));
    std::memcpy(&probe.floatingArgument, static_cast<const std::byte*>(report->arguments.savedRegisters) + 48, sizeof(probe.floatingArgument));
}

struct SelfUnregisterProbe {
    AudioPropagation::ReportRegistration* registration = nullptr;
    std::uint32_t calls = 0;
};

void APS5_VABI SelfUnregisterReport(const AudioPropagation::ReportInfo*, void* userData) {
    auto& probe = *static_cast<SelfUnregisterProbe*>(userData);
    ++probe.calls;
    Require(sceAudioPropagationReportUnregisterHandler(probe.registration) == 0, "report self-unregister failed");
}

struct BlockingReportProbe {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    std::atomic<std::uint32_t> calls{0};
};

void APS5_VABI BlockingReport(const AudioPropagation::ReportInfo*, void* userData) {
    auto& probe = *static_cast<BlockingReportProbe*>(userData);
    probe.calls.fetch_add(1, std::memory_order_relaxed);
    probe.entered.store(true, std::memory_order_release);
    while (!probe.release.load(std::memory_order_acquire)) std::this_thread::yield();
}

}

int main() {
    const AudioPropagation::SystemOptions options{{AudioPropagation::SystemOptionsId, AudioPropagation::SystemOptionsSize}, 2, 2, 6, 1, 256, 343.0f, 343.0f, 0, {}};

    auto memory = Memory();
    Require(sceAudioPropagationSystemQueryMemory(&options, &memory) == 0, "query memory failed");
    Require(memory.size_cpu_mem == 557264, "native CPU memory size mismatch");
    std::vector<std::uint64_t> arenaStorage((memory.size_cpu_mem + 7) / 8);
    auto* arena = reinterpret_cast<std::uint8_t*>(arenaStorage.data());
    Require(memory.size_gpu_mem == 0 && memory.p_cpu_mem == nullptr && memory.p_gpu_mem == nullptr, "query wrote more than the sizes");
    Require(sceAudioPropagationSystemQueryMemory(nullptr, &memory) == AudioPropagation::ErrorNullPointer, "null options");
    Require(sceAudioPropagationSystemQueryMemory(&options, nullptr) == AudioPropagation::ErrorNullPointer, "null memory");
    auto wrong = Memory();
    wrong.desc.size = 0x28;
    Require(sceAudioPropagationSystemQueryMemory(&options, &wrong) == static_cast<std::int32_t>(0x8A700007), "wrong descriptor size");
    wrong = Memory();
    wrong.desc.id = AudioPropagation::RayId;
    Require(sceAudioPropagationSystemQueryMemory(&options, &wrong) == static_cast<std::int32_t>(0x8A700007), "wrong descriptor id");

    auto probeOptions = options;
    auto probeMemory = Memory();
    probeMemory.p_cpu_mem = arena;
    probeMemory.p_gpu_mem = arena;
    probeOptions.maxSources = 0;
    Require(sceAudioPropagationSystemQueryMemory(&probeOptions, &probeMemory) == AudioPropagation::ErrorInvalidArgument, "zero source capacity accepted");
    Require(probeMemory.p_cpu_mem == nullptr && probeMemory.p_gpu_mem == nullptr && probeMemory.size_cpu_mem == 0 && probeMemory.size_gpu_mem == 0, "invalid options did not clear memory output");
    probeOptions = options;
    probeOptions.parameter24 = 7;
    Require(sceAudioPropagationSystemQueryMemory(&probeOptions, &probeMemory) == AudioPropagation::ErrorInvalidArgument, "unsupported parameter accepted");
    probeOptions = options;
    probeOptions.frameSamples = 768;
    Require(sceAudioPropagationSystemQueryMemory(&probeOptions, &probeMemory) == AudioPropagation::ErrorInvalidArgument, "unsupported frame size accepted");
    probeOptions = options;
    probeOptions.propagationSpeed = 0x1p-23f;
    Require(sceAudioPropagationSystemQueryMemory(&probeOptions, &probeMemory) == AudioPropagation::ErrorInvalidArgument, "invalid propagation speed accepted");
    probeOptions = options;
    probeOptions.frameSamples = 512;
    Require(sceAudioPropagationSystemQueryMemory(&probeOptions, &probeMemory) == 0 && probeMemory.size_cpu_mem == 524432, "512 sample memory size mismatch");
    probeOptions = options;
    probeOptions.reflectionOrder = 2;
    Require(sceAudioPropagationSystemQueryMemory(&probeOptions, &probeMemory) == 0 && probeMemory.size_cpu_mem == 1110608, "second order memory size mismatch");
    probeOptions = options;
    probeOptions.flags = 30;
    Require(sceAudioPropagationSystemQueryMemory(&probeOptions, &probeMemory) == 0 && probeMemory.size_cpu_mem == 2604064, "option memory sizes mismatch");
    probeOptions.flags = 1;
    Require(sceAudioPropagationSystemQueryMemory(&probeOptions, &probeMemory) == AudioPropagation::ErrorInvalidArgument, "GPU frame size constraint ignored");
    probeOptions.frameSamples = 512;
    RequireThrows<std::runtime_error>([&] { sceAudioPropagationSystemQueryMemory(&probeOptions, &probeMemory); }, "unknown GPU memory query returned a size");

    AudioPropagationHandle system = 0;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemCreate(&options, &memory, &system); }, "create without cpu memory");
    memory.p_cpu_mem = arena + 1;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemCreate(&options, &memory, &system); }, "misaligned cpu memory");
    memory.p_cpu_mem = arena;
    memory.size_gpu_mem = 16;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemCreate(&options, &memory, &system); }, "unrequested gpu memory");
    memory.size_gpu_mem = 0;
    const auto required = memory.size_cpu_mem;
    memory.size_cpu_mem = required - 1;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemCreate(&options, &memory, &system); }, "cpu memory too small");
    memory.size_cpu_mem = required;
    Require(sceAudioPropagationSystemCreate(&options, &memory, &system) == 0, "create failed");
    Require(system == reinterpret_cast<std::uintptr_t>(arena), "system handle is not its memory");
    AudioPropagation::SystemInfo systemInfo{};
    systemInfo.reserved44 = 0xdeadbeef;
    Require(sceAudioPropagationSystemQueryInfo(system, &systemInfo, sizeof(systemInfo)) == 0, "system query info failed");
    Require(systemInfo.cpuMemory == arena && systemInfo.cpuMemorySize == memory.size_cpu_mem && systemInfo.index == 0, "system query info memory or index");
    Require(systemInfo.frameSamples == options.frameSamples && systemInfo.parameter24 == 1 && systemInfo.maxSources == options.maxSources, "system query info options");
    Require(systemInfo.maxMaterials == options.maxMaterials && systemInfo.pathCapacityPerSource == 6 && systemInfo.propagationSpeed == options.propagationSpeed, "system query info capacities");
    Require(systemInfo.reserved44 == 0xdeadbeef, "system query info overwrote reserved output");
    Require(sceAudioPropagationSystemQueryInfo(system, &systemInfo, sizeof(systemInfo) - 1) == AudioPropagation::ErrorInvalidHandle, "system query info accepted wrong size");
    Require(sceAudioPropagationSystemQueryInfo(system, nullptr, sizeof(systemInfo)) == AudioPropagation::ErrorInvalidHandle, "system query info accepted null output");
    Require(sceAudioPropagationSystemUnlock(system) == AudioPropagation::ErrorInvalidArgument, "unbalanced system unlock was accepted");
    Require(sceAudioPropagationSystemLock(system) == 0, "system lock failed");
    Require(sceAudioPropagationSystemLock(system) == 0, "recursive system lock failed");
    Require(sceAudioPropagationSystemDestroy(system) == AudioPropagation::ErrorInvalidArgument, "same-thread destroy under system lock was accepted");
    std::atomic<std::int32_t> crossThreadUnlock{0};
    std::thread wrongUnlocker([&] { crossThreadUnlock.store(sceAudioPropagationSystemUnlock(system), std::memory_order_release); });
    wrongUnlocker.join();
    Require(crossThreadUnlock.load(std::memory_order_acquire) == AudioPropagation::ErrorInvalidArgument, "cross-thread system unlock was accepted");
    Require(sceAudioPropagationSystemUnlock(system) == 0, "first recursive system unlock failed");
    Require(sceAudioPropagationSystemUnlock(system) == 0, "system unlock failed");
    Require(sceAudioPropagationSystemLock(system + 16) == AudioPropagation::ErrorInvalidHandle, "invalid system lock result");
    Require(sceAudioPropagationSystemUnlock(system + 16) == AudioPropagation::ErrorInvalidHandle, "invalid system unlock result");
    Require(sceAudioPropagationSystemEnumHandles(nullptr, 0) == 1, "system handle count");
    Require(sceAudioPropagationSystemEnumHandles(nullptr, 1) == AudioPropagation::ErrorInvalidArgument, "null handle array with capacity");
    AudioPropagationHandle enumerated[2]{};
    Require(sceAudioPropagationSystemEnumHandles(enumerated, 1) == 1 && enumerated[0] == system, "single system enumeration");
    AudioPropagationHandle twice = 0;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemCreate(&options, &memory, &twice); }, "second system in the same memory");

    const std::uint64_t value = 1;
    const auto attribute = Attribute(&value, sizeof(value));
    const auto empty = Attribute(nullptr, sizeof(value));
    Require(sceAudioPropagationSystemSetAttributes(system, &attribute, 1) == 0, "system attributes failed");
    Require(sceAudioPropagationSystemSetAttributes(system, nullptr, 0) == 0, "empty system attributes failed");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemSetAttributes(system, &empty, 1); }, "attribute without value");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemSetAttributes(system + 16, &attribute, 1); }, "unknown system");

    std::uint8_t materialBytes[AudioPropagation::MaterialSize]{};
    auto* material = reinterpret_cast<AudioPropagationStructDescriptor*>(materialBytes);
    AudioPropagationHandle materialHandle = 0;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemRegisterMaterial(system, material, &materialHandle); }, "material without descriptor");
    *material = {AudioPropagation::MaterialId, AudioPropagation::MaterialSize};
    Require(sceAudioPropagationSystemRegisterMaterial(system, material, &materialHandle) == 0 && materialHandle != 0, "register material failed");
    Require(sceAudioPropagationSystemUnregisterMaterial(materialHandle) == 0, "unregister material failed");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemUnregisterMaterial(materialHandle); }, "unregister twice");

    AudioPropagationHandle rooms[2]{};
    Require(sceAudioPropagationRoomCreate(system, &rooms[0]) == 0 && sceAudioPropagationRoomCreate(system, &rooms[1]) == 0, "room create failed");
    Require(rooms[0] != 0 && rooms[0] != rooms[1], "room handles are not distinct");
    AudioPropagation::PortalParams portalParams{};
    portalParams.desc = {AudioPropagation::PortalParamsId, AudioPropagation::PortalParamsSize};
    portalParams.rooms[0] = rooms[0];
    portalParams.rooms[1] = materialHandle;
    AudioPropagationHandle portal = 0;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationPortalCreate(system, &portalParams, &portal); }, "portal to a non-room");
    portalParams.rooms[1] = rooms[1];
    Require(sceAudioPropagationPortalCreate(system, &portalParams, &portal) == 0, "portal create failed");
    const AudioPropagation::Attribute portalAttributes[4]{attribute, attribute, attribute, attribute};
    Require(sceAudioPropagationPortalSetAttributes(portal, portalAttributes, 4) == 0, "portal attributes failed");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationPortalSetAttributes(rooms[0], portalAttributes, 4); }, "portal attributes on a room");

    AudioPropagationHandle source = 0;
    Require(sceAudioPropagationSourceCreate(system, &source) == 0, "source create failed");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceCreate(system, nullptr); }, "source without output");
    Require(sceAudioPropagationSourceSetAttributes(source, portalAttributes, 3) == 0, "source attributes failed");
    std::uint32_t count = 7;
    Require(sceAudioPropagationSourceGetAudioPathCount(source, &count) == 0 && count == AudioPropagation::MaxPaths, "source path pool size");
    AudioPropagationHandle path = 0;
    Require(sceAudioPropagationSourceGetAudioPath(source, 0, &path) == 0 && path != 0, "path lookup immediately after source creation");
    AudioPropagationHandle repeated = 0;
    Require(sceAudioPropagationSourceGetAudioPath(source, 0, &repeated) == 0 && repeated == path, "path handle is unstable");
    AudioPropagationHandle lastPath = 0;
    Require(sceAudioPropagationSourceGetAudioPath(source, AudioPropagation::MaxPaths - 1, &lastPath) == 0 && lastPath != path, "last path slot");
    Require(sceAudioPropagationSourceGetAudioPath(source, AudioPropagation::MaxPaths, &repeated) == AudioPropagation::ErrorInvalidArgument, "out of range path slot");
    Require(repeated == path, "rejected path lookup changed the output");
    Require(sceAudioPropagationSourceGetAudioPath(source, 0, nullptr) == AudioPropagation::ErrorNullPointer, "null path output");
    Require(sceAudioPropagationSourceGetAudioPath(0, 0, nullptr) == AudioPropagation::ErrorInvalidHandle, "path lookup handle precedence");
    Require(sceAudioPropagationPathGetNumPoints(path, &count) == 0 && count == 0, "initial path has points");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationPathGetNumPoints(source, &count); }, "source used as a path");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationPathGetNumPoints(path, nullptr); }, "null point-count output");
    AudioPropagation::AudioPath configured{};
    configured.desc = {AudioPropagation::PathId, sizeof(configured)};
    configured.pointCount = 2;
    configured.flags = AudioPropagation::PathInvalid;
    for (auto& point : configured.points) {
        point.desc = {AudioPropagation::PathPointId, sizeof(point)};
        point.gain = 1.0f;
    }
    configured.points[0].room = rooms[0];
    configured.points[0].opaque[0] = std::byte{0x7f};
    configured.opaque[0] = std::byte{0x55};
    configured.points[0].position[0] = 3.0f;
    Require(sceAudioPropagationSourceSetAudioPath(path, &configured, 1.0f) == 0, "path configuration");
    configured.pointCount = 0;
    Require(sceAudioPropagationPathGetNumPoints(path, &count) == 0 && count == 2, "path configuration aliases caller memory");
    configured.pointCount = 2;
    auto badPath = configured;
    badPath.desc.size--;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceSetAudioPath(path, &badPath, 1.0f); }, "wrong path size");
    badPath = configured;
    badPath.pointCount = AudioPropagation::MaxPathPoints + 1;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceSetAudioPath(path, &badPath, 1.0f); }, "too many path points");
    badPath = configured;
    AudioPropagationHandle pathMaterial = 0;
    Require(sceAudioPropagationSystemRegisterMaterial(system, material, &pathMaterial) == 0, "path material creation");
    badPath.points[0].room = pathMaterial;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceSetAudioPath(path, &badPath, 1.0f); }, "material used as a path room");
    Require(sceAudioPropagationSystemUnregisterMaterial(pathMaterial) == 0, "path material cleanup");
    badPath = configured;
    badPath.points[0].position[0] = std::numeric_limits<float>::infinity();
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceSetAudioPath(path, &badPath, 1.0f); }, "nonfinite point position");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceSetAudioPath(path, &configured, std::numeric_limits<float>::quiet_NaN()); }, "nonfinite path gain");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceSetAudioPath(path, nullptr, 1.0f); }, "null path data");
    Require(sceAudioPropagationPathGetNumPoints(path, &count) == 0 && count == 2, "rejected update changed path state");

    std::uint8_t rays[AudioPropagation::RaySize * 2]{};
    count = 7;
    Require(sceAudioPropagationSystemGetRays(system, rays, &count) == 0 && count == 0, "system requested rays");
    count = 7;
    Require(sceAudioPropagationSourceGetRays(source, rays, &count) == 0 && count == 0, "source requested rays");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceGetRays(source, rays, nullptr); }, "rays without count");
    Require(sceAudioPropagationSystemSetRays(system, rays, 0) == 0, "empty ray results failed");
    RequireThrows<std::runtime_error>([&] { sceAudioPropagationSystemSetRays(system, rays, 1); }, "unrequested ray results");
    Require(sceAudioPropagationSourceCalculateAudioPaths(source, rays, 0, 1, nullptr, 0) == AudioPropagation::ErrorNullPointer, "path calculation null output");
    Require(sceAudioPropagationSourceCalculateAudioPaths(0, nullptr, 0, 0, nullptr, 0) == AudioPropagation::ErrorInvalidHandle, "path calculation handle precedence");
    Require(sceAudioPropagationSourceCalculateAudioPaths(source, rays, 0, 1, rays, 6) == AudioPropagation::ErrorInvalidArgument, "path calculation empty rays");
    Require(sceAudioPropagationSourceCalculateAudioPaths(source, rays, 6, 0, rays, 6) == AudioPropagation::ErrorInvalidArgument, "path calculation zero reflection order");
    Require(sceAudioPropagationSourceCalculateAudioPaths(source, rays, 6, 1, rays, 0) == AudioPropagation::ErrorInvalidArgument, "path calculation zero output capacity");
    Require(sceAudioPropagationSourceSetAudioPaths(source, nullptr, 0) == 0, "empty audio paths failed");
    RequireThrows<std::runtime_error>([&] { sceAudioPropagationSourceSetAudioPaths(source, rays, 1); }, "set audio paths");

    std::vector<std::uint64_t> otherArenaStorage(arenaStorage.size());
    auto* otherArena = reinterpret_cast<std::uint8_t*>(otherArenaStorage.data());
    auto otherMemory = Memory();
    Require(sceAudioPropagationSystemQueryMemory(&options, &otherMemory) == 0, "second query memory failed");
    otherMemory.p_cpu_mem = otherArena;
    AudioPropagationHandle otherSystem = 0;
    Require(sceAudioPropagationSystemCreate(&options, &otherMemory, &otherSystem) == 0, "second system create failed");
    AudioPropagationHandle enumeratedSystems[2]{};
    Require(sceAudioPropagationSystemEnumHandles(enumeratedSystems, 2) == 2 && enumeratedSystems[0] == otherSystem && enumeratedSystems[1] == system, "system enumeration is not newest first");
    Require(sceAudioPropagationSystemQueryInfo(otherSystem, &systemInfo, sizeof(systemInfo)) == 0 && systemInfo.index == 1, "second system slot index");
    AudioPropagationHandle otherSource = 0;
    Require(sceAudioPropagationSourceCreate(otherSystem, &otherSource) == 0, "second system source create failed");
    AudioPropagationHandle foreignRoom = 0;
    Require(sceAudioPropagationRoomCreate(otherSystem, &foreignRoom) == 0, "foreign room create failed");
    badPath = configured;
    badPath.points[0].room = foreignRoom;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceSetAudioPath(path, &badPath, 1.0f); }, "room from another system");
    Require(sceAudioPropagationRoomDestroy(otherSystem, foreignRoom) == 0, "foreign room cleanup");
    Require(sceAudioPropagationPathGetNumPoints(path, &count) == 0 && count == 2, "cross-system rejection changed path state");
    AudioPropagationHandle secondSource = 0;
    Require(sceAudioPropagationSourceCreate(system, &secondSource) == 0, "second source create failed");

    constexpr std::size_t frameValues = 256 * 16;
    float outputs[2][frameValues + 16];
    float expected[2][frameValues + 16];
    const auto fill = [&] {
        for (std::size_t channel = 0; channel < 2; ++channel) {
            for (std::size_t sample = 0; sample < frameValues + 16; ++sample) {
                outputs[channel][sample] = static_cast<float>(sample + 1) / (channel == 0 ? 128.0f : -128.0f);
            }
        }
        std::memcpy(expected, outputs, sizeof(outputs));
    };
    const auto untouched = [&] { return std::memcmp(outputs, expected, sizeof(outputs)) == 0; };
    AudioPropagation::RenderInfo infos[2]{};
    const auto reset = [&] {
        infos[0] = {{AudioPropagation::RenderInfoId, AudioPropagation::RenderInfoSize}, source, outputs[0], sizeof(outputs[0]), AudioPropagation::RenderFormat, 0};
        infos[1] = {{AudioPropagation::RenderInfoId, AudioPropagation::RenderInfoSize}, secondSource, outputs[1], sizeof(outputs[1]), AudioPropagation::RenderFormat, 0};
        fill();
    };
    reset();
    Require(sceAudioPropagationSourceRender(system, infos, 1) == 0, "render failed");
    Require(untouched(), "empty source erased an existing mix");
    infos[0].channelCount = 99;
    Require(sceAudioPropagationSourceRender(system, infos, 1) == 0 && infos[0].channelCount == 0, "fixed render format clears the channel count");
    Require(untouched(), "render wrote past the output size");
    reset();
    infos[0].outputSize = sizeof(float) * (frameValues - 1);
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 2); }, "short frame output accepted");
    Require(untouched(), "short output modified a render buffer");
    reset();
    infos[0].outputSize = sizeof(float) * frameValues;
    Require(sceAudioPropagationSourceRender(system, infos, 2) == 0, "render of two sources failed");
    Require(untouched(), "empty sources modified separate destination mixes or their tails");
    reset();
    infos[1].output = outputs[0];
    Require(sceAudioPropagationSourceRender(system, infos, 2) == 0, "render into a shared mix failed");
    Require(untouched(), "empty sources modified a shared destination mix");
    reset();
    infos[0].outputSize = sizeof(float) * frameValues;
    infos[1].output = outputs[0] + 16;
    infos[1].outputSize = sizeof(float) * frameValues;
    Require(sceAudioPropagationSourceRender(system, infos, 2) == 0, "render into overlapping mixes failed");
    Require(untouched(), "empty sources modified overlapping destination mixes");
    reset();
    std::memset(outputs, 0, sizeof(outputs));
    std::memcpy(expected, outputs, sizeof(outputs));
    Require(sceAudioPropagationSourceRender(system, infos, 2) == 0, "render into silence failed");
    Require(untouched(), "empty sources modified a silent destination mix");

    reset();
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 0); }, "render without infos");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 3); }, "render exceeds source capacity");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, nullptr, 1); }, "render with null infos");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system + 16, infos, 1); }, "render on an unknown system");
    infos[1].desc.id = AudioPropagation::RayId;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 2); }, "render info with a wrong descriptor id");
    Require(untouched(), "rejected render wrote an output");
    reset();
    infos[0].desc.size = 0x28;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 1); }, "render info with a wrong descriptor size");
    reset();
    infos[0].source = rooms[0];
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 1); }, "render of a room");
    reset();
    infos[0].source = otherSource;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 1); }, "render of another system's source");
    reset();
    infos[0].output = nullptr;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 1); }, "render without output");
    reset();
    infos[0].outputSize = 0;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 1); }, "render with an empty output");
    for (const std::uint32_t format : {0u, 1u, 2u, 4u, 5u}) {
        reset();
        infos[0].format = format;
        const std::size_t channels = format == 0 ? 4 : format == 1 ? 9 : format == 2 ? 16 : 8;
        infos[0].outputSize = sizeof(float) * options.frameSamples * channels;
        Require(sceAudioPropagationSourceRender(system, infos, 1) == 0, "render in a known format failed");
        Require(untouched(), "empty source modified a mix in a known format");
        infos[0].outputSize -= sizeof(float) * channels;
        RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 1); }, "known format accepted an undersized frame");
        Require(untouched(), "undersized known-format output modified a mix");
    }
    reset();
    infos[0].format = 1;
    infos[0].outputSize = sizeof(float) * (options.frameSamples * 9 + 1);
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 1); }, "render of a partial nine-channel frame");
    Require(untouched(), "render of a partial frame wrote an output");
    for (const std::uint32_t format : {3u, 6u}) {
        reset();
        infos[1].format = format;
        RequireThrows<std::runtime_error>([&] { sceAudioPropagationSourceRender(system, infos, 2); }, "render with an unknown format");
        Require(untouched(), "render with an unknown format wrote an output");
    }

    reset();
    configured.flags = 0;
    Require(sceAudioPropagationSourceSetAudioPath(path, &configured, 1.0f) == 0, "active path configuration");
    RequireThrows<std::runtime_error>([&] { sceAudioPropagationSourceRender(system, infos, 2); }, "active path silently rendered");
    Require(untouched(), "unsupported active-path render modified output");
    configured.flags = AudioPropagation::PathInvalid;
    Require(sceAudioPropagationSourceSetAudioPath(path, &configured, 1.0f) == 0, "invalidating path");

    Require(sceAudioPropagationSourceDestroy(system, secondSource) == 0, "second source destroy failed");
    reset();
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, infos, 2); }, "render of a destroyed source");
    Require(sceAudioPropagationSystemDestroy(otherSystem) == 0, "second system destroy failed");
    Require(sceAudioPropagationSystemEnumHandles(enumeratedSystems, 2) == 1 && enumeratedSystems[0] == system, "destroyed system remained enumerated");

    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationRoomDestroy(system, source); }, "room destroy of a source");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceDestroy(system + 16, source); }, "source destroy with another system");
    Require(sceAudioPropagationPortalDestroy(system, portal) == 0, "portal destroy failed");
    Require(sceAudioPropagationSourceDestroy(system, source) == 0, "source destroy failed");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationPathGetNumPoints(path, &count); }, "path survived source destruction");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceSetAudioPath(lastPath, &configured, 1.0f); }, "path update after source destruction");
    Require(sceAudioPropagationRoomDestroy(system, rooms[0]) == 0, "room destroy failed");

    AudioPropagationHandle lateSource = 0;
    AudioPropagationHandle lateMaterial = 0;
    Require(sceAudioPropagationSourceCreate(system, &lateSource) == 0, "late source create failed");
    AudioPropagationHandle latePath = 0;
    Require(sceAudioPropagationSourceGetAudioPath(lateSource, 0, &latePath) == 0 && latePath != path, "path handle reuse");
    Require(sceAudioPropagationSystemRegisterMaterial(system, material, &lateMaterial) == 0, "late material register failed");
    Require(sceAudioPropagationSystemDestroy(system) == 0, "system destroy failed");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationPathGetNumPoints(latePath, &count); }, "path survived system destruction");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceSetAttributes(lateSource, &attribute, 1); }, "source used after its system");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationRoomDestroy(system, lateSource); }, "orphan destroyed as the wrong kind");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceDestroy(system + 16, lateSource); }, "orphan destroyed with another system");
    Require(sceAudioPropagationRoomDestroy(system, rooms[1]) == 0, "orphaned room destroy is not a no-op");
    Require(sceAudioPropagationSourceDestroy(system, lateSource) == 0, "orphaned source destroy is not a no-op");
    Require(sceAudioPropagationSystemUnregisterMaterial(lateMaterial) == 0, "orphaned material unregister is not a no-op");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceDestroy(system, lateSource); }, "orphan destroyed twice");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemDestroy(system); }, "destroy twice");
    Require(sceAudioPropagationSystemCreate(&options, &memory, &system) == 0, "memory reuse after destroy failed");
    Require(sceAudioPropagationSystemQueryInfo(system, &systemInfo, sizeof(systemInfo)) == 0 && systemInfo.index == 0, "system slot was not reused");
    Require(sceAudioPropagationSystemLock(system) == 0, "system lock before destroy failed");
    std::atomic<bool> destroyStarted{false};
    std::atomic<bool> destroyFinished{false};
    std::thread destroyThread([&] {
        destroyStarted.store(true, std::memory_order_release);
        Require(sceAudioPropagationSystemDestroy(system) == 0, "concurrent system destroy failed");
        destroyFinished.store(true, std::memory_order_release);
    });
    while (!destroyStarted.load(std::memory_order_acquire)) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    Require(!destroyFinished.load(std::memory_order_acquire), "system destroy ignored the held lock");
    Require(sceAudioPropagationSystemUnlock(system) == 0, "system unlock before destroy failed");
    destroyThread.join();
    Require(destroyFinished.load(std::memory_order_acquire), "system destroy did not finish");

    Require(sceAudioPropagationSystemCreate(&options, &memory, &system) == 0, "create for stale lock waiter failed");
    Require(sceAudioPropagationSystemLock(system) == 0, "system lock before stale waiter failed");
    std::atomic<bool> waiterStarted{false};
    std::atomic<bool> waiterFinished{false};
    std::atomic<std::int32_t> waiterResult{0};
    std::thread waiter([&] {
        waiterStarted.store(true, std::memory_order_release);
        const auto result = sceAudioPropagationSystemLock(system);
        waiterResult.store(result, std::memory_order_release);
        if (result == 0) Require(sceAudioPropagationSystemUnlock(system) == 0, "stale lock waiter cleanup failed");
        waiterFinished.store(true, std::memory_order_release);
    });
    while (!waiterStarted.load(std::memory_order_acquire)) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    std::atomic<bool> reuseDestroyStarted{false};
    std::thread reuseDestroyer([&] {
        reuseDestroyStarted.store(true, std::memory_order_release);
        Require(sceAudioPropagationSystemDestroy(system) == 0, "destroy with a blocked lock waiter failed");
    });
    while (!reuseDestroyStarted.load(std::memory_order_acquire)) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    Require(sceAudioPropagationSystemUnlock(system) == 0, "unlock before address reuse failed");
    reuseDestroyer.join();
    waiter.join();
    Require(waiterFinished.load(std::memory_order_acquire) && waiterResult.load(std::memory_order_acquire) == AudioPropagation::ErrorInvalidHandle, "waiter acquired a destroyed system lock");
    Require(sceAudioPropagationSystemCreate(&options, &memory, &system) == 0, "create at reused system address failed");
    Require(sceAudioPropagationSystemLock(system) == 0 && sceAudioPropagationSystemUnlock(system) == 0, "new system lock was affected by a stale waiter");
    Require(sceAudioPropagationSystemDestroy(system) == 0, "destroy after address reuse failed");

    Require(sceAudioPropagationSystemLock(0) == AudioPropagation::ErrorInvalidHandle, "system lock invalid handle");

    ReportProbe probe{};
    AudioPropagation::ReportRegistration* registration = nullptr;
    Require(sceAudioPropagationReportRegisterHandler(2, CaptureReport, &probe, &registration) == -4, "invalid report category");
    Require(sceAudioPropagationReportRegisterHandler(0, nullptr, &probe, &registration) == -2, "null report handler");
    Require(sceAudioPropagationReportRegisterHandler(0, CaptureReport, &probe, &registration) == 0 && registration, "report handler registration");
    Require(sceAudioPropagationReportApi(0, 0x1234, -7, 9, "sceAudioPropagationTest", "%d %s %f", 42, "ok", 3.25) == 0, "report dispatch");
    Require(probe.calls == 1 && probe.fieldsMatch && probe.integerArgument == 42 && std::strcmp(probe.stringArgument, "ok") == 0 && probe.floatingArgument == 3.25, "report info or va_list capture");
    Require(sceAudioPropagationReportUnregisterHandler(registration) == 0, "report handler unregister");
    Require(sceAudioPropagationReportUnregisterHandler(registration) == -1, "duplicate report handler unregister");
    Require(sceAudioPropagationReportUnregisterHandler(nullptr) == -1, "null report registration");

    MixedReportProbe firstMixed{};
    MixedReportProbe secondMixed{};
    AudioPropagation::ReportRegistration* firstMixedRegistration = nullptr;
    AudioPropagation::ReportRegistration* secondMixedRegistration = nullptr;
    Require(sceAudioPropagationReportRegisterHandler(0, CaptureMixedReport, &firstMixed, &firstMixedRegistration) == 0, "first mixed report registration");
    Require(sceAudioPropagationReportRegisterHandler(0, CaptureMixedReport, &secondMixed, &secondMixedRegistration) == 0, "second mixed report registration");
    Require(sceAudioPropagationReportApi(0, 0, 0, 0, "mixed", "%d %f %s", 27, 6.5, "independent") == 0, "mixed report dispatch");
    for (const auto* probeValue : {&firstMixed, &secondMixed}) {
        Require(probeValue->calls == 1 && probeValue->integer == 27 && probeValue->floating == 6.5 && std::strcmp(probeValue->text, "independent") == 0, "handlers did not receive independent mixed varargs state");
    }
    Require(sceAudioPropagationReportUnregisterHandler(firstMixedRegistration) == 0, "first mixed report unregister");
    Require(sceAudioPropagationReportUnregisterHandler(secondMixedRegistration) == 0, "second mixed report unregister");

    FloatingReportProbe floatingProbe{};
    AudioPropagation::ReportRegistration* floatingRegistration = nullptr;
    Require(sceAudioPropagationReportRegisterHandler(0, CaptureFloatingReport, &floatingProbe, &floatingRegistration) == 0, "floating report registration");
    Require(sceAudioPropagationReportApi(0, 0, 0, 0, "floating", "%f%f%f%f%f%f%f%f%f", 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0) == 0, "stack-spilled floating report dispatch");
    Require(floatingProbe.calls == 1, "stack-spilled floating callback count");
    for (std::size_t index = 0; index < floatingProbe.values.size(); ++index) Require(floatingProbe.values[index] == static_cast<double>(index + 1), "stack-spilled floating argument mismatch");
    Require(sceAudioPropagationReportUnregisterHandler(floatingRegistration) == 0, "floating report unregister");

    IntegerReportProbe integerProbe{};
    AudioPropagation::ReportRegistration* integerRegistration = nullptr;
    Require(sceAudioPropagationReportRegisterHandler(0, CaptureIntegerReport, &integerProbe, &integerRegistration) == 0, "integer-only report registration");
    Require(sceAudioPropagationReportApi(0, 0, 0, 0, "integer-only", "%d %s", -19, "no-floats") == 0, "integer-only report dispatch");
    Require(integerProbe.calls == 1 && integerProbe.integer == -19 && std::strcmp(integerProbe.text, "no-floats") == 0, "integer-only varargs mismatch");
    Require(sceAudioPropagationReportUnregisterHandler(integerRegistration) == 0, "integer-only report unregister");

    AudioPropagation::ReportRegistration* registrations[8]{};
    for (auto& slot : registrations) Require(sceAudioPropagationReportRegisterHandler(1, CaptureReport, &probe, &slot) == 0 && slot, "report category slot allocation");
    Require(sceAudioPropagationReportRegisterHandler(1, CaptureReport, &probe, nullptr) == -3, "report category capacity");
    for (auto* slot : registrations) Require(sceAudioPropagationReportUnregisterHandler(slot) == 0, "report category slot release");

    std::uint32_t oldHandlerCalls = 0;
    std::uint32_t newHandlerCalls = 0;
    AudioPropagation::ReportRegistration* oldRegistration = nullptr;
    AudioPropagation::ReportRegistration* newRegistration = nullptr;
    Require(sceAudioPropagationReportRegisterHandler(0, CountReport, &oldHandlerCalls, &oldRegistration) == 0, "old report registration");
    Require(sceAudioPropagationReportUnregisterHandler(oldRegistration) == 0, "old report unregister");
    Require(sceAudioPropagationReportRegisterHandler(0, CountReport, &newHandlerCalls, &newRegistration) == 0 && newRegistration == oldRegistration, "native report slot address was not reused");
    Require(sceAudioPropagationReportUnregisterHandler(oldRegistration) == 0, "stale registration did not release the reused native slot");
    Require(sceAudioPropagationReportApi(0, 0, 0, 0, "stale-registration", "") == 0 && newHandlerCalls == 0, "stale registration left the reused handler active");

    SelfUnregisterProbe selfUnregister{};
    Require(sceAudioPropagationReportRegisterHandler(0, SelfUnregisterReport, &selfUnregister, &selfUnregister.registration) == 0, "self-unregister handler registration");
    Require(sceAudioPropagationReportApi(0, 0, 0, 0, "test", "") == 0 && selfUnregister.calls == 1, "self-unregister callback dispatch");
    Require(sceAudioPropagationReportApi(0, 0, 0, 0, "test", "") == 0 && selfUnregister.calls == 1, "self-unregistered callback still dispatched");

    BlockingReportProbe blocking{};
    AudioPropagation::ReportRegistration* blockingRegistration = nullptr;
    Require(sceAudioPropagationReportRegisterHandler(0, BlockingReport, &blocking, &blockingRegistration) == 0, "blocking handler registration");
    std::thread reportThread([&] {
        Require(sceAudioPropagationReportApi(0, 0, 0, 0, "test", "") == 0, "blocking report dispatch");
    });
    while (!blocking.entered.load(std::memory_order_acquire)) std::this_thread::yield();
    std::atomic<bool> unregisterStarted{false};
    std::atomic<bool> unregisterFinished{false};
    std::thread unregisterThread([&] {
        unregisterStarted.store(true, std::memory_order_release);
        Require(sceAudioPropagationReportUnregisterHandler(blockingRegistration) == 0, "concurrent report unregister failed");
        unregisterFinished.store(true, std::memory_order_release);
    });
    while (!unregisterStarted.load(std::memory_order_acquire)) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    Require(!unregisterFinished.load(std::memory_order_acquire), "handler unregistered before in-flight callback completed");
    blocking.release.store(true, std::memory_order_release);
    reportThread.join();
    unregisterThread.join();
    Require(unregisterFinished.load(std::memory_order_acquire), "handler unregistration did not finish");
    Require(sceAudioPropagationReportApi(0, 0, 0, 0, "test", "") == 0 && blocking.calls.load() == 1, "unregistered handler dispatched again");

    std::uint32_t unreturnedRegistrationCalls = 0;
    Require(sceAudioPropagationReportRegisterHandler(1, CountReport, &unreturnedRegistrationCalls, nullptr) == 0, "registration without output pointer failed");
    Require(sceAudioPropagationReportApi(1, 0, 0, 0, "unreturned-registration", "") == 0 && unreturnedRegistrationCalls == 1, "registration without output pointer was not retained");

    Require(Resolve("sceAudioPropagationSystemCreate") != nullptr && Resolve("sceAudioPropagationSourceSetAudioPath") != nullptr, "export lookup");
    for (const char* name : {"sceAudioPropagationAudioPathInit", "sceAudioPropagationAudioPathPointInit", "sceAudioPropagationMaterialInit", "sceAudioPropagationPortalSettingsInit", "sceAudioPropagationRayInit", "sceAudioPropagationSourceRenderInfoInit", "sceAudioPropagationSourceSetAudioPathsParamInit", "sceAudioPropagationSystemMemoryInit", "sceAudioPropagationSystemOptionInit", "sceAudioPropagationSystem"}) {
        Require(Resolve(name) == nullptr, name);
    }

    std::puts("AudioPropagation tests passed");
    return 0;
}

#include "prx/libSceAudioPropagation/AudioPropagation.hpp"

#include <cstdint>
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

extern "C" {
int APS5_VABI sceAudioPropagationSystemLock();
}

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
    RequireThrows<std::runtime_error>([&] { sceAudioPropagationSourceCalculateAudioPaths(source, rays, 6, 1, rays, 6); }, "unsupported path calculation");
    Require(sceAudioPropagationSourceSetAudioPaths(source, nullptr, 0) == 0, "empty audio paths failed");
    RequireThrows<std::runtime_error>([&] { sceAudioPropagationSourceSetAudioPaths(source, rays, 1); }, "set audio paths");

    std::vector<std::uint64_t> otherArenaStorage(arenaStorage.size());
    auto* otherArena = reinterpret_cast<std::uint8_t*>(otherArenaStorage.data());
    auto otherMemory = Memory();
    Require(sceAudioPropagationSystemQueryMemory(&options, &otherMemory) == 0, "second query memory failed");
    otherMemory.p_cpu_mem = otherArena;
    AudioPropagationHandle otherSystem = 0;
    Require(sceAudioPropagationSystemCreate(&options, &otherMemory, &otherSystem) == 0, "second system create failed");
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
    Require(sceAudioPropagationSystemDestroy(system) == 0, "second destroy failed");

    RequireThrows<std::runtime_error>([&] { sceAudioPropagationSystemLock(); }, "system lock throw");

    Require(Resolve("sceAudioPropagationSystemCreate") != nullptr && Resolve("sceAudioPropagationSourceSetAudioPath") != nullptr, "export lookup");
    for (const char* name : {"sceAudioPropagationAudioPathInit", "sceAudioPropagationAudioPathPointInit", "sceAudioPropagationMaterialInit", "sceAudioPropagationPortalSettingsInit", "sceAudioPropagationRayInit", "sceAudioPropagationSourceRenderInfoInit", "sceAudioPropagationSourceSetAudioPathsParamInit", "sceAudioPropagationSystemMemoryInit", "sceAudioPropagationSystemOptionInit", "sceAudioPropagationSystem"}) {
        Require(Resolve(name) == nullptr, name);
    }

    std::puts("AudioPropagation tests passed");
    return 0;
}

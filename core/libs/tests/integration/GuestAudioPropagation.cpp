#include "prx/libSceAudioPropagation/AudioPropagation.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr std::size_t ArenaSize = 64;
constexpr std::size_t OutputSamples = 64;
constexpr std::uint8_t OutputFill = 0xab;
constexpr std::uint64_t AttributeValue = 1;

struct MaterialStorage {
    alignas(AudioPropagationStructDescriptor) std::uint8_t bytes[AudioPropagation::MaterialSize]{};
};

const std::uint64_t* Options() {
    static const std::uint64_t options[7]{};
    return options;
}

const AudioPropagationStructDescriptor* MaterialDescriptor() {
    static const MaterialStorage storage = [] {
        MaterialStorage material;
        *reinterpret_cast<AudioPropagationStructDescriptor*>(material.bytes) = {AudioPropagation::MaterialId, AudioPropagation::MaterialSize};
        return material;
    }();
    return reinterpret_cast<const AudioPropagationStructDescriptor*>(storage.bytes);
}

AudioPropagationSystemMemory Memory() {
    AudioPropagationSystemMemory memory{};
    memory.desc = {AudioPropagation::SystemMemoryId, AudioPropagation::SystemMemorySize};
    return memory;
}

AudioPropagationSystemMemory QueriedMemory() {
    auto memory = Memory();
    RequireEqual(sceAudioPropagationSystemQueryMemory(Options(), &memory), 0, "query memory");
    return memory;
}

AudioPropagation::Attribute Attribute(const void* value, std::size_t size) {
    AudioPropagation::Attribute attribute{};
    attribute.id = 0x20000;
    attribute.value = value;
    attribute.valueSize = size;
    return attribute;
}

AudioPropagation::Attribute ValidAttribute() {
    return Attribute(&AttributeValue, sizeof(AttributeValue));
}

AudioPropagation::PortalParams PortalParams(AudioPropagationHandle first, AudioPropagationHandle second) {
    AudioPropagation::PortalParams params{};
    params.desc = {AudioPropagation::PortalParamsId, AudioPropagation::PortalParamsSize};
    params.rooms[0] = first;
    params.rooms[1] = second;
    return params;
}

class SystemGuard {
public:
    SystemGuard() = default;

    ~SystemGuard() {
        if (handle == 0) return;
        try {
            sceAudioPropagationSystemDestroy(handle);
        } catch (...) {
        }
    }

    SystemGuard(const SystemGuard&) = delete;
    SystemGuard& operator=(const SystemGuard&) = delete;

    AudioPropagationHandle handle = 0;
};

enum class ObjectKind { Room, Portal, Source, Material };

class SystemFixture {
public:
    SystemFixture() : memory(QueriedMemory()) {
        memory.p_cpu_mem = arena;
        RequireEqual(sceAudioPropagationSystemCreate(Options(), &memory, &system), 0, "create the system");
    }

    ~SystemFixture() {
        for (auto object = objects.rbegin(); object != objects.rend(); ++object) Release(object->first, object->second);
        try {
            sceAudioPropagationSystemDestroy(system);
        } catch (...) {
        }
    }

    SystemFixture(const SystemFixture&) = delete;
    SystemFixture& operator=(const SystemFixture&) = delete;

    AudioPropagationHandle Handle() const noexcept {
        return system;
    }

    const AudioPropagationSystemMemory& SystemMemory() const noexcept {
        return memory;
    }

    AudioPropagationHandle Room() {
        AudioPropagationHandle room = 0;
        RequireEqual(sceAudioPropagationRoomCreate(system, &room), 0, "create a room");
        Track(ObjectKind::Room, room);
        return room;
    }

    AudioPropagationHandle Source() {
        AudioPropagationHandle source = 0;
        RequireEqual(sceAudioPropagationSourceCreate(system, &source), 0, "create a source");
        Track(ObjectKind::Source, source);
        return source;
    }

    AudioPropagationHandle Material() {
        AudioPropagationHandle material = 0;
        RequireEqual(sceAudioPropagationSystemRegisterMaterial(system, MaterialDescriptor(), &material), 0, "register a material");
        Track(ObjectKind::Material, material);
        return material;
    }

    AudioPropagationHandle Portal(AudioPropagationHandle first, AudioPropagationHandle second) {
        const auto params = PortalParams(first, second);
        AudioPropagationHandle portal = 0;
        RequireEqual(sceAudioPropagationPortalCreate(system, &params, &portal), 0, "create a portal");
        Track(ObjectKind::Portal, portal);
        return portal;
    }

    void Track(ObjectKind kind, AudioPropagationHandle handle) {
        objects.emplace_back(kind, handle);
    }

private:
    void Release(ObjectKind kind, AudioPropagationHandle handle) noexcept {
        try {
            switch (kind) {
                case ObjectKind::Room: sceAudioPropagationRoomDestroy(system, handle); break;
                case ObjectKind::Portal: sceAudioPropagationPortalDestroy(system, handle); break;
                case ObjectKind::Source: sceAudioPropagationSourceDestroy(system, handle); break;
                case ObjectKind::Material: sceAudioPropagationSystemUnregisterMaterial(handle); break;
            }
        } catch (...) {
        }
    }

    alignas(16) std::uint8_t arena[ArenaSize]{};
    AudioPropagationSystemMemory memory;
    AudioPropagationHandle system = 0;
    std::vector<std::pair<ObjectKind, AudioPropagationHandle>> objects;
};

class RenderTarget {
public:
    RenderTarget(AudioPropagationHandle first, AudioPropagationHandle second) {
        std::memset(outputs, OutputFill, sizeof(outputs));
        const AudioPropagationHandle sources[2]{first, second};
        for (int index = 0; index < 2; ++index) {
            infos[index] = {{AudioPropagation::RenderInfoId, AudioPropagation::RenderInfoSize}, sources[index], outputs[index],
                            sizeof(outputs[index]), AudioPropagation::RenderFormat, 0};
        }
    }

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    bool Zeroed(int index) const {
        for (const float sample : outputs[index]) {
            if (sample != 0.0f) return false;
        }
        return true;
    }

    bool Untouched() const {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(outputs);
        for (std::size_t index = 0; index < sizeof(outputs); ++index) {
            if (bytes[index] != OutputFill) return false;
        }
        return true;
    }

    AudioPropagation::RenderInfo infos[2]{};
    float outputs[2][OutputSamples]{};
};

void RequireCreateRejected(const AudioPropagationSystemMemory& memory, const char* message) {
    SystemGuard guard;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemCreate(Options(), &memory, &guard.handle); }, message);
}

void RequireRenderRejected(AudioPropagationHandle system, const RenderTarget& target, std::uint32_t count, const char* message) {
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(system, target.infos, count); }, message);
}

const Case queryMemory{"QueryMemory_ValidDescriptor_ReportsOnlyTheCpuSize", [] {
    auto memory = Memory();
    RequireEqual(sceAudioPropagationSystemQueryMemory(Options(), &memory), 0, "query memory result");
    Require(memory.size_cpu_mem != 0 && memory.size_cpu_mem <= ArenaSize, "cpu size outside the arena");
    RequireEqual(memory.size_gpu_mem, std::size_t{0}, "query reported gpu memory");
    Require(memory.p_cpu_mem == nullptr && memory.p_gpu_mem == nullptr, "query wrote more than the sizes");
}};

const Case queryNullOptions{"QueryMemory_NullOptions_ThrowsInvalidArgument", [] {
    auto memory = Memory();
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemQueryMemory(nullptr, &memory); }, "null options");
}};

const Case queryNullMemory{"QueryMemory_NullMemory_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemQueryMemory(Options(), nullptr); }, "null memory");
}};

const Case queryWrongSize{"QueryMemory_WrongDescriptorSize_ThrowsInvalidArgument", [] {
    auto wrong = Memory();
    wrong.desc.size = 0x28;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemQueryMemory(Options(), &wrong); }, "wrong descriptor size");
}};

const Case queryWrongId{"QueryMemory_WrongDescriptorId_ThrowsInvalidArgument", [] {
    auto wrong = Memory();
    wrong.desc.id = AudioPropagation::RayId;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemQueryMemory(Options(), &wrong); }, "wrong descriptor id");
}};

const Case createWithoutCpuMemory{"SystemCreate_NullCpuMemory_ThrowsInvalidArgument", [] {
    const auto memory = QueriedMemory();
    RequireCreateRejected(memory, "create without cpu memory");
}};

const Case createMisaligned{"SystemCreate_MisalignedCpuMemory_ThrowsInvalidArgument", [] {
    alignas(16) std::uint8_t arena[ArenaSize]{};
    auto memory = QueriedMemory();
    memory.p_cpu_mem = arena + 1;
    RequireCreateRejected(memory, "misaligned cpu memory");
}};

const Case createWithGpuMemory{"SystemCreate_UnrequestedGpuMemory_ThrowsInvalidArgument", [] {
    alignas(16) std::uint8_t arena[ArenaSize]{};
    auto memory = QueriedMemory();
    memory.p_cpu_mem = arena;
    memory.size_gpu_mem = 16;
    RequireCreateRejected(memory, "unrequested gpu memory");
}};

const Case createTooSmall{"SystemCreate_CpuMemoryTooSmall_ThrowsInvalidArgument", [] {
    alignas(16) std::uint8_t arena[ArenaSize]{};
    auto memory = QueriedMemory();
    memory.p_cpu_mem = arena;
    memory.size_cpu_mem -= 1;
    RequireCreateRejected(memory, "cpu memory too small");
}};

const Case createValid{"SystemCreate_ValidMemory_ReturnsTheMemoryAsHandle", [] {
    alignas(16) std::uint8_t arena[ArenaSize]{};
    auto memory = QueriedMemory();
    memory.p_cpu_mem = arena;
    SystemGuard system;
    RequireEqual(sceAudioPropagationSystemCreate(Options(), &memory, &system.handle), 0, "create result");
    RequireEqual(system.handle, reinterpret_cast<std::uintptr_t>(arena), "system handle is not its memory");
}};

const Case createTwice{"SystemCreate_SecondSystemInTheSameMemory_ThrowsInvalidArgument", [] {
    const SystemFixture fixture;
    RequireCreateRejected(fixture.SystemMemory(), "second system in the same memory");
}};

const Case systemAttributes{"SystemSetAttributes_ValidAndEmptyLists_Succeed", [] {
    const SystemFixture fixture;
    const auto attribute = ValidAttribute();
    RequireEqual(sceAudioPropagationSystemSetAttributes(fixture.Handle(), &attribute, 1), 0, "system attributes");
    RequireEqual(sceAudioPropagationSystemSetAttributes(fixture.Handle(), nullptr, 0), 0, "empty system attributes");
}};

const Case systemAttributeWithoutValue{"SystemSetAttributes_AttributeWithoutValue_ThrowsInvalidArgument", [] {
    const SystemFixture fixture;
    const auto empty = Attribute(nullptr, sizeof(AttributeValue));
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemSetAttributes(fixture.Handle(), &empty, 1); }, "attribute without value");
}};

const Case systemAttributesUnknownSystem{"SystemSetAttributes_UnknownSystem_ThrowsInvalidArgument", [] {
    const SystemFixture fixture;
    const auto attribute = ValidAttribute();
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemSetAttributes(fixture.Handle() + 16, &attribute, 1); }, "unknown system");
}};

const Case materialWithoutDescriptor{"RegisterMaterial_MissingDescriptor_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const MaterialStorage material;
    AudioPropagationHandle handle = 0;
    RequireThrows<std::invalid_argument>([&] {
        sceAudioPropagationSystemRegisterMaterial(fixture.Handle(), reinterpret_cast<const AudioPropagationStructDescriptor*>(material.bytes), &handle);
    }, "material without descriptor");
    if (handle != 0) fixture.Track(ObjectKind::Material, handle);
}};

const Case materialRegister{"RegisterMaterial_ValidDescriptor_ReturnsAHandleThatUnregisters", [] {
    SystemFixture fixture;
    AudioPropagationHandle material = 0;
    RequireEqual(sceAudioPropagationSystemRegisterMaterial(fixture.Handle(), MaterialDescriptor(), &material), 0, "register result");
    fixture.Track(ObjectKind::Material, material);
    Require(material != 0, "register material returned a null handle");
    RequireEqual(sceAudioPropagationSystemUnregisterMaterial(material), 0, "unregister material");
}};

const Case materialUnregisterTwice{"UnregisterMaterial_Twice_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto material = fixture.Material();
    RequireEqual(sceAudioPropagationSystemUnregisterMaterial(material), 0, "first unregister");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemUnregisterMaterial(material); }, "unregister twice");
}};

const Case roomCreate{"RoomCreate_TwoRooms_ReturnsDistinctNonZeroHandles", [] {
    SystemFixture fixture;
    const auto first = fixture.Room();
    const auto second = fixture.Room();
    Require(first != 0 && first != second, "room handles are not distinct");
}};

const Case portalToNonRoom{"PortalCreate_UnregisteredMaterialAsRoom_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto room = fixture.Room();
    const auto material = fixture.Material();
    RequireEqual(sceAudioPropagationSystemUnregisterMaterial(material), 0, "unregister the material");
    const auto params = PortalParams(room, material);
    AudioPropagationHandle portal = 0;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationPortalCreate(fixture.Handle(), &params, &portal); }, "portal to a non-room");
    if (portal != 0) fixture.Track(ObjectKind::Portal, portal);
}};

const Case portalCreate{"PortalCreate_TwoRooms_Succeeds", [] {
    SystemFixture fixture;
    const auto first = fixture.Room();
    const auto second = fixture.Room();
    const auto params = PortalParams(first, second);
    AudioPropagationHandle portal = 0;
    RequireEqual(sceAudioPropagationPortalCreate(fixture.Handle(), &params, &portal), 0, "portal create");
    fixture.Track(ObjectKind::Portal, portal);
}};

const Case portalAttributes{"PortalSetAttributes_FourAttributes_Succeeds", [] {
    SystemFixture fixture;
    const auto first = fixture.Room();
    const auto portal = fixture.Portal(first, fixture.Room());
    const auto attribute = ValidAttribute();
    const AudioPropagation::Attribute attributes[4]{attribute, attribute, attribute, attribute};
    RequireEqual(sceAudioPropagationPortalSetAttributes(portal, attributes, 4), 0, "portal attributes");
}};

const Case portalAttributesOnRoom{"PortalSetAttributes_RoomHandle_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto room = fixture.Room();
    fixture.Portal(room, fixture.Room());
    const auto attribute = ValidAttribute();
    const AudioPropagation::Attribute attributes[4]{attribute, attribute, attribute, attribute};
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationPortalSetAttributes(room, attributes, 4); }, "portal attributes on a room");
}};

const Case sourceCreate{"SourceCreate_ValidSystem_Succeeds", [] {
    SystemFixture fixture;
    AudioPropagationHandle source = 0;
    RequireEqual(sceAudioPropagationSourceCreate(fixture.Handle(), &source), 0, "source create");
    fixture.Track(ObjectKind::Source, source);
}};

const Case sourceWithoutOutput{"SourceCreate_NullOutput_ThrowsInvalidArgument", [] {
    const SystemFixture fixture;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceCreate(fixture.Handle(), nullptr); }, "source without output");
}};

const Case sourceAttributes{"SourceSetAttributes_ThreeAttributes_Succeeds", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    const auto attribute = ValidAttribute();
    const AudioPropagation::Attribute attributes[3]{attribute, attribute, attribute};
    RequireEqual(sceAudioPropagationSourceSetAttributes(source, attributes, 3), 0, "source attributes");
}};

const Case sourcePathCount{"SourceGetAudioPathCount_NewSource_ReportsZero", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    std::uint32_t count = 7;
    RequireEqual(sceAudioPropagationSourceGetAudioPathCount(source, &count), 0, "audio path count result");
    RequireEqual(count, 0u, "source reports audio paths");
}};

const Case sourcePath{"SourceGetAudioPath_NoAudioPaths_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    AudioPropagationHandle path = 0;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceGetAudioPath(source, 0, &path); }, "audio path without paths");
}};

const Case systemRays{"SystemGetRays_NoPropagation_ReportsZeroRays", [] {
    const SystemFixture fixture;
    std::uint8_t rays[AudioPropagation::RaySize * 2]{};
    std::uint32_t count = 7;
    RequireEqual(sceAudioPropagationSystemGetRays(fixture.Handle(), rays, &count), 0, "system get rays result");
    RequireEqual(count, 0u, "system requested rays");
}};

const Case sourceRays{"SourceGetRays_NoPropagation_ReportsZeroRays", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    std::uint8_t rays[AudioPropagation::RaySize * 2]{};
    std::uint32_t count = 7;
    RequireEqual(sceAudioPropagationSourceGetRays(source, rays, &count), 0, "source get rays result");
    RequireEqual(count, 0u, "source requested rays");
}};

const Case sourceRaysWithoutCount{"SourceGetRays_NullCount_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    std::uint8_t rays[AudioPropagation::RaySize * 2]{};
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceGetRays(source, rays, nullptr); }, "rays without count");
}};

const Case setEmptyRays{"SystemSetRays_NoResults_Succeeds", [] {
    const SystemFixture fixture;
    std::uint8_t rays[AudioPropagation::RaySize * 2]{};
    RequireEqual(sceAudioPropagationSystemSetRays(fixture.Handle(), rays, 0), 0, "empty ray results");
}};

const Case setUnrequestedRays{"SystemSetRays_UnrequestedResults_ThrowsRuntimeError", [] {
    const SystemFixture fixture;
    std::uint8_t rays[AudioPropagation::RaySize * 2]{};
    RequireThrows<std::runtime_error>([&] { sceAudioPropagationSystemSetRays(fixture.Handle(), rays, 1); }, "unrequested ray results");
}};

const Case calculateWithoutRays{"SourceCalculateAudioPaths_NoRays_Succeeds", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    std::uint8_t rays[AudioPropagation::RaySize * 2]{};
    RequireEqual(sceAudioPropagationSourceCalculateAudioPaths(source, rays, 0, 1, nullptr, 0), 0, "empty path calculation");
}};

const Case calculateWithRays{"SourceCalculateAudioPaths_WithRays_ThrowsRuntimeError", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    std::uint8_t rays[AudioPropagation::RaySize * 2]{};
    RequireThrows<std::runtime_error>([&] { sceAudioPropagationSourceCalculateAudioPaths(source, rays, 1, 1, nullptr, 0); }, "path calculation with rays");
}};

const Case setEmptyAudioPaths{"SourceSetAudioPaths_NoEntries_Succeeds", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    RequireEqual(sceAudioPropagationSourceSetAudioPaths(source, nullptr, 0), 0, "empty audio paths");
}};

const Case setAudioPaths{"SourceSetAudioPaths_WithEntries_ThrowsRuntimeError", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    std::uint8_t rays[AudioPropagation::RaySize * 2]{};
    RequireThrows<std::runtime_error>([&] { sceAudioPropagationSourceSetAudioPaths(source, rays, 1); }, "set audio paths");
}};

const Case secondSystem{"SystemCreate_SecondArena_CreatesAnIndependentDestroyableSystem", [] {
    const SystemFixture first;
    SystemFixture second;
    second.Source();
    RequireEqual(sceAudioPropagationSystemDestroy(second.Handle()), 0, "second system destroy");
}};

const Case renderOne{"SourceRender_OneSource_ZeroesOnlyItsOutput", [] {
    SystemFixture fixture;
    const auto first = fixture.Source();
    const RenderTarget target(first, fixture.Source());
    RequireEqual(sceAudioPropagationSourceRender(fixture.Handle(), target.infos, 1), 0, "render");
    Require(target.Zeroed(0), "render left the output unwritten");
    const auto* tail = reinterpret_cast<const std::uint8_t*>(target.outputs[1]);
    RequireEqual(tail[0], OutputFill, "render wrote past the output size");
}};

const Case renderTwo{"SourceRender_TwoSources_WritesEachOutputForItsSize", [] {
    SystemFixture fixture;
    const auto first = fixture.Source();
    RenderTarget target(first, fixture.Source());
    target.infos[0].outputSize = sizeof(float) * 16;
    RequireEqual(sceAudioPropagationSourceRender(fixture.Handle(), target.infos, 2), 0, "render of two sources");
    Require(target.outputs[0][15] == 0.0f && target.outputs[0][16] != 0.0f && target.Zeroed(1), "render did not write each output for its size");
}};

const Case renderWithoutInfos{"SourceRender_ZeroInfos_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto first = fixture.Source();
    const RenderTarget target(first, fixture.Source());
    RequireRenderRejected(fixture.Handle(), target, 0, "render without infos");
}};

const Case renderNullInfos{"SourceRender_NullInfos_ThrowsInvalidArgument", [] {
    const SystemFixture fixture;
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceRender(fixture.Handle(), nullptr, 1); }, "render with null infos");
}};

const Case renderUnknownSystem{"SourceRender_UnknownSystem_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto first = fixture.Source();
    const RenderTarget target(first, fixture.Source());
    RequireRenderRejected(fixture.Handle() + 16, target, 1, "render on an unknown system");
}};

const Case renderWrongId{"SourceRender_WrongDescriptorIdInLaterInfo_ThrowsWithoutWriting", [] {
    SystemFixture fixture;
    const auto first = fixture.Source();
    RenderTarget target(first, fixture.Source());
    target.infos[1].desc.id = AudioPropagation::RayId;
    RequireRenderRejected(fixture.Handle(), target, 2, "render info with a wrong descriptor id");
    Require(target.Untouched(), "rejected render wrote an output");
}};

const Case renderWrongSize{"SourceRender_WrongDescriptorSize_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto first = fixture.Source();
    RenderTarget target(first, fixture.Source());
    target.infos[0].desc.size = 0x28;
    RequireRenderRejected(fixture.Handle(), target, 1, "render info with a wrong descriptor size");
}};

const Case renderRoom{"SourceRender_RoomHandle_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto first = fixture.Source();
    RenderTarget target(first, fixture.Source());
    target.infos[0].source = fixture.Room();
    RequireRenderRejected(fixture.Handle(), target, 1, "render of a room");
}};

const Case renderForeignSource{"SourceRender_SourceOfAnotherSystem_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    SystemFixture other;
    const auto first = fixture.Source();
    RenderTarget target(first, fixture.Source());
    target.infos[0].source = other.Source();
    RequireRenderRejected(fixture.Handle(), target, 1, "render of another system's source");
}};

const Case renderNullOutput{"SourceRender_NullOutput_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto first = fixture.Source();
    RenderTarget target(first, fixture.Source());
    target.infos[0].output = nullptr;
    RequireRenderRejected(fixture.Handle(), target, 1, "render without output");
}};

const Case renderEmptyOutput{"SourceRender_EmptyOutput_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto first = fixture.Source();
    RenderTarget target(first, fixture.Source());
    target.infos[0].outputSize = 0;
    RequireRenderRejected(fixture.Handle(), target, 1, "render with an empty output");
}};

const Case renderUnknownFormat{"SourceRender_UnknownFormat_ThrowsRuntimeErrorWithoutWriting", [] {
    SystemFixture fixture;
    const auto first = fixture.Source();
    RenderTarget target(first, fixture.Source());
    target.infos[1].format = 1;
    RequireThrows<std::runtime_error>([&] { sceAudioPropagationSourceRender(fixture.Handle(), target.infos, 2); }, "render with an unknown format");
    Require(target.Untouched(), "render with an unknown format wrote an output");
}};

const Case renderDestroyedSource{"SourceRender_DestroyedSource_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto first = fixture.Source();
    const auto second = fixture.Source();
    const RenderTarget target(first, second);
    RequireEqual(sceAudioPropagationSourceDestroy(fixture.Handle(), second), 0, "second source destroy");
    RequireRenderRejected(fixture.Handle(), target, 2, "render of a destroyed source");
}};

const Case roomDestroyOfSource{"RoomDestroy_SourceHandle_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationRoomDestroy(fixture.Handle(), source); }, "room destroy of a source");
}};

const Case sourceDestroyOtherSystem{"SourceDestroy_AnotherSystem_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceDestroy(fixture.Handle() + 16, source); }, "source destroy with another system");
}};

const Case portalDestroy{"PortalDestroy_LivePortal_Succeeds", [] {
    SystemFixture fixture;
    const auto first = fixture.Room();
    const auto portal = fixture.Portal(first, fixture.Room());
    RequireEqual(sceAudioPropagationPortalDestroy(fixture.Handle(), portal), 0, "portal destroy");
}};

const Case sourceDestroy{"SourceDestroy_LiveSource_Succeeds", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    RequireEqual(sceAudioPropagationSourceDestroy(fixture.Handle(), source), 0, "source destroy");
}};

const Case roomDestroy{"RoomDestroy_LiveRoom_Succeeds", [] {
    SystemFixture fixture;
    const auto room = fixture.Room();
    RequireEqual(sceAudioPropagationRoomDestroy(fixture.Handle(), room), 0, "room destroy");
}};

const Case systemDestroy{"SystemDestroy_WithLiveObjects_Succeeds", [] {
    SystemFixture fixture;
    fixture.Room();
    fixture.Source();
    fixture.Material();
    RequireEqual(sceAudioPropagationSystemDestroy(fixture.Handle()), 0, "system destroy");
}};

const Case orphanSourceAttributes{"SourceSetAttributes_AfterItsSystemIsDestroyed_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    RequireEqual(sceAudioPropagationSystemDestroy(fixture.Handle()), 0, "system destroy");
    const auto attribute = ValidAttribute();
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceSetAttributes(source, &attribute, 1); }, "source used after its system");
}};

const Case orphanWrongKind{"RoomDestroy_OrphanedSource_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    RequireEqual(sceAudioPropagationSystemDestroy(fixture.Handle()), 0, "system destroy");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationRoomDestroy(fixture.Handle(), source); }, "orphan destroyed as the wrong kind");
}};

const Case orphanOtherSystem{"SourceDestroy_OrphanWithAnotherSystem_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    RequireEqual(sceAudioPropagationSystemDestroy(fixture.Handle()), 0, "system destroy");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceDestroy(fixture.Handle() + 16, source); }, "orphan destroyed with another system");
}};

const Case orphanRoomDestroy{"RoomDestroy_OrphanedRoom_Succeeds", [] {
    SystemFixture fixture;
    const auto room = fixture.Room();
    RequireEqual(sceAudioPropagationSystemDestroy(fixture.Handle()), 0, "system destroy");
    RequireEqual(sceAudioPropagationRoomDestroy(fixture.Handle(), room), 0, "orphaned room destroy is not a no-op");
}};

const Case orphanSourceDestroy{"SourceDestroy_OrphanedSource_Succeeds", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    RequireEqual(sceAudioPropagationSystemDestroy(fixture.Handle()), 0, "system destroy");
    RequireEqual(sceAudioPropagationSourceDestroy(fixture.Handle(), source), 0, "orphaned source destroy is not a no-op");
}};

const Case orphanMaterialUnregister{"UnregisterMaterial_OrphanedMaterial_Succeeds", [] {
    SystemFixture fixture;
    const auto material = fixture.Material();
    RequireEqual(sceAudioPropagationSystemDestroy(fixture.Handle()), 0, "system destroy");
    RequireEqual(sceAudioPropagationSystemUnregisterMaterial(material), 0, "orphaned material unregister is not a no-op");
}};

const Case orphanDestroyTwice{"SourceDestroy_OrphanTwice_ThrowsInvalidArgument", [] {
    SystemFixture fixture;
    const auto source = fixture.Source();
    RequireEqual(sceAudioPropagationSystemDestroy(fixture.Handle()), 0, "system destroy");
    RequireEqual(sceAudioPropagationSourceDestroy(fixture.Handle(), source), 0, "first orphan destroy");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSourceDestroy(fixture.Handle(), source); }, "orphan destroyed twice");
}};

const Case destroyTwice{"SystemDestroy_Twice_ThrowsInvalidArgument", [] {
    const SystemFixture fixture;
    RequireEqual(sceAudioPropagationSystemDestroy(fixture.Handle()), 0, "first destroy");
    RequireThrows<std::invalid_argument>([&] { sceAudioPropagationSystemDestroy(fixture.Handle()); }, "destroy twice");
}};

const Case memoryReuse{"SystemCreate_MemoryOfADestroyedSystem_CanBeReused", [] {
    const SystemFixture fixture;
    RequireEqual(sceAudioPropagationSystemDestroy(fixture.Handle()), 0, "first destroy");
    SystemGuard system;
    RequireEqual(sceAudioPropagationSystemCreate(Options(), &fixture.SystemMemory(), &system.handle), 0, "memory reuse after destroy");
    RequireEqual(sceAudioPropagationSystemDestroy(std::exchange(system.handle, 0)), 0, "second destroy");
}};

} // namespace

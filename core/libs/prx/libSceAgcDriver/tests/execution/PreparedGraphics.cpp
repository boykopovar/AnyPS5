#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "CompiledVariant.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void Reject(TAction action, const char* expected) {
    try { action(); }
    catch (const std::exception& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, error.what());
        return;
    }
    throw std::runtime_error(std::string("invalid graphics ABI was accepted; expected: ") + expected);
}

struct Fixture {
    struct Header {
        Shader shader{};
        ShaderUserData users{};
    } header;
    alignas(256) std::array<std::uint32_t, 65> code{};
    std::shared_ptr<AgcDriver::DriverDetail::ShaderSnapshot> snapshot;

    void Initialize(std::uint8_t type) {
        header.shader.type = type;
        header.shader.user_data = &header.users;
        code.fill(0xffffffffu);
        code.back() = type == 5u ? 0xbefd2106u : 0xbf810000u;
        const auto address = reinterpret_cast<std::uintptr_t>(code.data());
        const auto headerAddress = reinterpret_cast<std::uintptr_t>(&header);
        snapshot = std::make_shared<AgcDriver::DriverDetail::ShaderSnapshot>();
        snapshot->type = type;
        snapshot->codeAddress = address;
        snapshot->headerAddress = headerAddress;
        snapshot->code.assign(code.begin(), code.end());
        snapshot->header.resize(sizeof(header));
        std::memcpy(snapshot->header.data(), &header, sizeof(header));
    }

    void Bind(AgcDriver::QueueState& queue, std::uint32_t program, std::uint32_t resources) const {
        const auto address = snapshot->codeAddress + 256u;
        queue.shader[program] = static_cast<std::uint32_t>(address >> 8u);
        queue.shader[program + 1u] = static_cast<std::uint32_t>(address >> 40u);
        queue.shader[resources] = 16u << 1u;
    }
};

void Check(AgcDriver::VulkanDevice& device, AgcDriver::Graphics::ShaderPath path, const std::filesystem::path& dump) {
    using namespace AgcDriver::DriverDetail;
    using namespace ShaderRecompiler;
    Fixture front;
    Fixture back;
    Fixture domain;
    Fixture fragment;
    const bool tessellation = path == AgcDriver::Graphics::ShaderPath::Tessellation;
    const bool mesh = path == AgcDriver::Graphics::ShaderPath::Geometry;
    front.Initialize(tessellation ? 5u : mesh ? 4u : 2u);
    back.Initialize(tessellation ? 7u : 6u);
    domain.Initialize(2u);
    fragment.Initialize(1u);
    const std::array<std::uint32_t, 7> pixelCode{0xc8020002u, 0xc8060102u, 0xc80a0202u, 0xc80e0302u, 0xf800180fu, 0x03020100u, 0xbf810000u};
    fragment.snapshot->code.resize(64);
    auto& fragmentCode = fragment.snapshot->code.Mutable();
    fragmentCode.insert(fragmentCode.end(), pixelCode.begin(), pixelCode.end());
    ShaderRegistry registry;
    for (const auto* fixture : {&front, &back, &domain, &fragment}) registry.emplace(fixture->snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{fixture->snapshot});
    AgcDriver::QueueState queue{};
    queue.context[0x8e] = 0xfu;
    queue.context[0x8f] = 0xfu;
    front.Bind(queue, tessellation ? 0x148u : 0xc8u, tessellation ? 0x10bu : 0x8bu);
    if (tessellation || mesh) back.Bind(queue, tessellation ? 0x108u : 0x88u, tessellation ? 0x10bu : 0x8bu);
    if (tessellation) domain.Bind(queue, 0xc8u, 0x8bu);
    fragment.Bind(queue, 0x8u, 0xbu);
    alignas(8) const std::array<std::uint32_t, 2> merged{};
    const auto mergedAddress = reinterpret_cast<std::uintptr_t>(merged.data());
    const auto pointerBase = tessellation ? 0x102u : 0x82u;
    queue.shader[pointerBase] = static_cast<std::uint32_t>(mergedAddress);
    queue.shader[pointerBase + 1u] = static_cast<std::uint32_t>(mergedAddress >> 32u);
    DrawDecode prepared{};
    prepared.state.stages.path = path;
    prepared.state.stages.vertexWaveSize = tessellation || mesh ? 64u : 32u;
    prepared.state.stages.fragmentWaveSize = 32u;
    if (mesh) prepared.state.stages.mesh = MeshConfiguration{4, 1, 3, 3, 1, 64, 128, 0, 4};
    if (tessellation) prepared.state.stages.tessellation = TessellationConfiguration{3, 4, 1, 2, 2};
    prepared.pixel.wave32 = true;
    prepared.pixel.interpolatorCount = 2;
    prepared.pixel.interpolatorSettings[0] = 0x403u;
    prepared.pixel.interpolatorSettings[1] = 0x220u;
    prepared.pixel.targetOutputMode[0] = 9;
    prepared.pixel.targetExportMapping.fill(0xe4u);
    DecodeGraphicsPrograms(prepared, queue, registry, true, true);
    DrawDecode draw{};
    draw.state = prepared.state;
    draw.pixel = prepared.pixel;
    DecodeGraphicsPrograms(draw, queue, registry, false, true);
    Require(prepared.programs.size() == draw.programs.size() && prepared.roles == draw.roles, "prepared graphics programs differ from draw programs");
    for (std::size_t index = 0; index < prepared.programs.size(); ++index) {
        const auto& expected = prepared.programs[index];
        const auto& actual = draw.programs[index];
        Require(expected.codeOffset == 64u && expected.binary.code.size() == (prepared.roles[index] == ProgramRole::Fragment ? pixelCode.size() : 1u) && expected.binary.code[0] == (prepared.roles[index] == ProgramRole::Fragment ? pixelCode[0] : prepared.roles[index] == ProgramRole::Local ? 0xbefd2106u : 0xbf810000u), "graphics entry point did not trim the code prefix");
        Require(expected.binary.stage == actual.binary.stage && expected.firstUserSgpr == actual.firstUserSgpr && expected.userData.size() == actual.userData.size(), "graphics preparation changed the user SGPR ABI");
    }
    auto target = device.Target();
    std::vector<std::uint32_t> capabilities(target.supportedCapabilities.begin(), target.supportedCapabilities.end());
    std::vector<std::string_view> extensions(target.supportedExtensions.begin(), target.supportedExtensions.end());
    if (mesh) {
        capabilities.push_back(spv::CapabilityMeshShadingEXT);
        extensions.push_back("SPV_EXT_mesh_shader");
        target.supportedCapabilities = capabilities;
        target.supportedExtensions = extensions;
        target.mesh = MeshTargetLimits{{128, 1, 1}, 128, 32768, 256, 256, 128, 32768, 1, 1};
        target.spirvVersion = 0x00010400u;
    }
    const auto stages = PrepareGraphicsStages(prepared, target);
    Require(stages.size() == (tessellation ? 4u : 2u), "graphics preparation compiled the wrong stages");
    if (!mesh && !tessellation && target.registrationModules) {
        const auto prebuild = [&](const std::vector<PreparedGraphicsStage>& group, std::initializer_list<std::size_t> picked) {
            std::vector<AgcDriver::Graphics::CompiledShader> shaders;
            for (const auto index : picked) {
                Require(group.at(index).registration != nullptr, "registration modules: a prepared stage has no registration result");
                shaders.push_back({group[index].stage, group[index].registration.get(), group[index].pushOffset});
            }
            const auto before = AgcDriver::Graphics::PipelineLibraryCountersOf(device.Device());
            Require(device.PrebuildGraphicsLibraries(prepared.state.stages, shaders), "registration modules: the shader libraries were not prebuilt");
            const auto after = AgcDriver::Graphics::PipelineLibraryCountersOf(device.Device());
            Require(after.linked == before.linked, "registration modules: a prebuild linked a pipeline");
            return std::pair{after.built[1] - before.built[1], after.built[2] - before.built[2]};
        };
        const auto vertexOnly = prebuild(stages, {0u});
        Require(vertexOnly.first == 1u, "registration modules: the vertex stage alone did not build its pre-rasterization library");
        const auto pixelOnly = prebuild(stages, {1u});
        Require(pixelOnly.second == 1u, "registration modules: the pixel stage alone did not build its fragment library");
        const auto pair = prebuild(stages, {0u, 1u});
        Require(pair.first == 0u && pair.second == 0u, "registration modules: the pair of stages did not find the libraries prebuilt one stage at a time");
        const auto again = PrepareGraphicsStages(prepared, target);
        Require(again.front().registration != stages.front().registration, "registration modules: the second preparation reused the first result object");
        const auto repeated = prebuild(again, {0u, 1u});
        Require(repeated.first == 0u && repeated.second == 0u, "registration modules: another materialization of the same stages did not find the prebuilt libraries");
    }
    for (const auto& stage : stages) stage.snapshot->prepared->entries.push_back(stage.entry);
    const auto& pixelArtifact = GetPreparedArtifact(*stages.back().entry.handle);
    Require(!pixelArtifact.fragmentParameters.empty() && pixelArtifact.fragmentParameters.front().sourceLocation == 3u, "prepared fragment lost interpolant mapping");
    if (!mesh && !tessellation) {
        const auto frontCount = front.snapshot->prepared->entries.size();
        const auto pixelCount = fragment.snapshot->prepared->entries.size();
        Reject([&] {
            ShaderPreparationTransaction transaction;
            transaction.Edit(*front.snapshot).entries.push_back(stages.front().entry);
            transaction.Edit(*fragment.snapshot).entries.push_back(stages.back().entry);
            auto unsupported = target;
            unsupported.tessellation.reset();
            ResolvePreparedGraphics(*front.snapshot, fragment.snapshot, 7, unsupported);
            transaction.Commit();
        }, "tessellation shaders are unavailable");
        Require(front.snapshot->prepared->entries.size() == frontCount && fragment.snapshot->prepared->entries.size() == pixelCount && front.snapshot->prepared->rectangles.empty() && front.snapshot->prepared->fragments.empty() && !front.snapshot->prepared->rectangleRequested, "failed rectangle compilation published part of the stage group");
        for (const bool primitiveFirst : {false, true}) {
            front.snapshot->prepared->rectangles.clear();
            front.snapshot->prepared->rectangleProgress.clear();
            front.snapshot->prepared->fragments.clear();
            front.snapshot->prepared->rectangleRequested = false;
            if (primitiveFirst) ResolvePreparedGraphics(*front.snapshot, {}, 7, target);
            ResolvePreparedGraphics(*front.snapshot, fragment.snapshot, 0, target);
            if (!primitiveFirst) ResolvePreparedGraphics(*front.snapshot, {}, 17, target);
            const auto vertexId = GetPreparedArtifact(*stages.front().entry.handle).variantId;
            const auto rectangle = PreparedRectangle(*front.snapshot, vertexId, pixelArtifact.variantId);
            Require(!rectangle.control.spirv.empty() && !rectangle.evaluation.spirv.empty(), "separate helpers did not prepare rectangle shaders");
            const auto count = front.snapshot->prepared->rectangles.size();
            ResolvePreparedGraphics(*front.snapshot, fragment.snapshot, 7, target);
            Require(front.snapshot->prepared->rectangles.size() == count, "repeated helpers duplicated rectangle shaders");
        }
        auto changed = prepared;
        changed.state.stages.vertexWaveSize = 64;
        changed.pixel.interpolatorSettings[0] = 0x404u;
        const auto newStages = PrepareGraphicsStages(changed, target);
        front.snapshot->prepared->entries.push_back(newStages.front().entry);
        ResolvePreparedGraphics(*front.snapshot, {}, 7, target);
        const auto newVertexId = GetPreparedArtifact(*newStages.front().entry.handle).variantId;
        Require(newVertexId != GetPreparedArtifact(*stages.front().entry.handle).variantId, "rectangle test did not change the vertex variant");
        static_cast<void>(PreparedRectangle(*front.snapshot, newVertexId, pixelArtifact.variantId));
        fragment.snapshot->prepared->entries.push_back(newStages.back().entry);
        ResolvePreparedGraphics(*front.snapshot, fragment.snapshot, 0, target);
        const auto newPixelId = GetPreparedArtifact(*newStages.back().entry.handle).variantId;
        Require(newPixelId != pixelArtifact.variantId, "rectangle test did not change the fragment variant");
        static_cast<void>(PreparedRectangle(*front.snapshot, newVertexId, newPixelId));
        auto temporary = std::make_shared<ShaderSnapshot>();
        temporary->prepared->entries = fragment.snapshot->prepared->entries;
        ResolvePreparedGraphics(*front.snapshot, temporary, 0, target);
        std::weak_ptr<const ShaderSnapshot> expired = temporary;
        temporary.reset();
        Require(expired.expired(), "helper link retained a shader snapshot");
        ResolvePreparedGraphics(*front.snapshot, {}, 7, target);
        Require(front.snapshot->prepared->fragments.size() == 1, "expired helper link was not removed");
        Require(front.snapshot->prepared->rectangleProgress.size() == 1 && !front.snapshot->prepared->rectangleProgress.front().fragment.expired(), "expired rectangle progress was not removed");
        auto drawOnly = prepared;
        drawOnly.pixel.interpolatorSettings[0] = 0x405u;
        const auto drawStages = PrepareGraphicsStages(drawOnly, target);
        auto unlinked = std::make_shared<ShaderSnapshot>();
        unlinked->type = 1;
        unlinked->prepared->entries.push_back(drawStages.back().entry);
        const auto drawPixelId = GetPreparedArtifact(*drawStages.back().entry.handle).variantId;
        const auto drawVertexId = GetPreparedArtifact(*drawStages.front().entry.handle).variantId;
        Require(drawVertexId == GetPreparedArtifact(*stages.front().entry.handle).variantId && drawPixelId != pixelArtifact.variantId && drawPixelId != newPixelId, "draw-only fragment test did not isolate a new fragment variant");
        const auto drawRectangle = DrawRectangle(*front.snapshot, unlinked, drawVertexId, drawPixelId, target);
        Require(!drawRectangle.control.spirv.empty() && !drawRectangle.evaluation.spirv.empty(), "a rect-list draw did not prepare the rectangle of a fragment variant prepared at draw");
        static_cast<void>(PreparedRectangle(*front.snapshot, drawVertexId, drawPixelId));
    }
    if (!dump.empty()) {
        for (std::size_t index = 0; index < stages.size(); ++index) {
            const auto& words = GetPreparedArtifact(*stages[index].entry.handle).spirv;
            const auto name = dump / (std::to_string(static_cast<unsigned>(path)) + "-" + std::to_string(index) + ".spv");
            std::ofstream output(name, std::ios::binary);
            output.write(reinterpret_cast<const char*>(words.data()), static_cast<std::streamsize>(words.size() * sizeof(std::uint32_t)));
            Require(static_cast<bool>(output), "cannot write prepared graphics SPIR-V");
        }
    }
    auto incomplete = prepared;
    incomplete.roles.pop_back();
    Reject([&] { static_cast<void>(PrepareGraphicsStages(incomplete, target)); }, "program roles are incomplete");
    auto wrongEntry = prepared;
    wrongEntry.programs.front().binary.codeAddress += 4;
    Reject([&] { static_cast<void>(PrepareGraphicsStages(wrongEntry, target)); }, "entry point differs");
    std::vector<LinkedProgram> linked;
    std::vector<MemoryRegion> memory;
    for (std::size_t index = 0; index < draw.programs.size(); ++index) {
        const auto& program = draw.programs[index];
        linked.push_back({draw.roles[index], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
        memory.insert(memory.end(), program.memory.begin(), program.memory.end());
    }
    for (std::size_t index = 0; index < draw.programs.size(); ++index) {
        if (draw.roles[index] == ProgramRole::GeometryBack) continue;
        const auto& program = draw.programs[index];
        const bool pixel = program.binary.stage == ShaderStage::Fragment;
        std::optional<ShaderVertexStageInfo> vertex;
        if (!pixel) vertex = AgcDriver::Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, nullptr, true);
        RecompileRequest request{program.binary, {pixel ? 32u : prepared.state.stages.vertexWaveSize, program.firstUserSgpr, program.userData, {}, pixel ? std::optional(draw.pixel) : std::nullopt, vertex, memory}, target, {0, 0, 0, mesh ? MeshDrawPushOffsetBytes : 128u}, GraphicsCompileContext{program.firstUserSgpr, linked, prepared.state.stages.mesh, prepared.state.stages.tessellation, {0, 3, 4, 1}}};
        static_cast<void>(InvocationFor(*program.snapshot, program.codeOffset, request));
        request.layout.pushConstantOffsetBytes = 4;
        request.layout.pushConstantSizeBytes -= 4;
        static_cast<void>(InvocationFor(*program.snapshot, program.codeOffset, request));
        request.layout.pushConstantOffsetBytes = 2;
        Reject([&] { static_cast<void>(InvocationFor(*program.snapshot, program.codeOffset, request)); }, "not dword-aligned");
        request.layout = {0, 0, 0, mesh ? MeshDrawPushOffsetBytes : 128u};
        if (pixel) request.context.pixel->interpolatorSettings[0] ^= 1u;
        else if (tessellation) ++request.graphics->tessellation->outputControlPoints;
        else if (mesh) ++request.graphics->mesh->maxVertices;
        else ++request.context.userDataBaseRegister;
        const auto predicted = program.snapshot->prepared->entries.size();
        static_cast<void>(InvocationFor(*program.snapshot, program.codeOffset, request));
        Require(program.snapshot->prepared->entries.size() == predicted + 1, "a draw state that registration did not predict was not prepared");
        static_cast<void>(InvocationFor(*program.snapshot, program.codeOffset, request));
        Require(program.snapshot->prepared->entries.size() == predicted + 1, "an artifact prepared at a draw was not reused");
    }
    if (tessellation) {
        Fixture late;
        late.Initialize(5u);
        registry.emplace(late.snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{late.snapshot});
        auto unbound = queue;
        unbound.shader[0x148u] = 0u;
        unbound.shader[0x149u] = 0u;
        DrawDecode resolved{};
        resolved.state = prepared.state;
        resolved.pixel = prepared.pixel;
        DecodeGraphicsPrograms(resolved, unbound, registry, true, true);
        Require(resolved.programs.size() == 3u && resolved.roles.front() == ProgramRole::Hull, "an unbound local program was not left to the draw");
        Require(PrepareGraphicsStages(resolved, target).size() == 3u, "graphics preparation without the local program compiled the wrong stages");
        auto bound = queue;
        late.Bind(bound, 0x148u, 0x10bu);
        DrawDecode lateDraw{};
        lateDraw.state = prepared.state;
        lateDraw.pixel = prepared.pixel;
        DecodeGraphicsPrograms(lateDraw, bound, registry, false, true);
        const auto& program = lateDraw.programs.front();
        Require(program.snapshot == late.snapshot && late.snapshot->prepared->entries.empty(), "the late local program was decoded from another shader");
        const auto vertex = AgcDriver::Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, nullptr, true);
        RecompileRequest request{program.binary, {prepared.state.stages.vertexWaveSize, program.firstUserSgpr, program.userData, {}, std::nullopt, vertex, memory}, target, {0, 0, 0, 128u}, GraphicsCompileContext{program.firstUserSgpr, linked, std::nullopt, prepared.state.stages.tessellation, {0, 3, 4, 1}}};
        static_cast<void>(InvocationFor(*late.snapshot, program.codeOffset, request));
        static_cast<void>(InvocationFor(*late.snapshot, program.codeOffset, request));
        Require(late.snapshot->prepared->entries.size() == 1u, "the local program was not prepared once at its first draw");
        ++request.graphics->tessellation->outputControlPoints;
        static_cast<void>(InvocationFor(*late.snapshot, program.codeOffset, request));
        Require(late.snapshot->prepared->entries.size() == 2u, "a second state of the local program was not prepared at its draw");
        registry.erase(late.snapshot->codeAddress);
    }
    ShaderRegistry invalidRegistry;
    for (const auto& [address, snapshots] : registry) {
        for (const auto& snapshot : snapshots) {
            auto compute = *snapshot;
            compute.type = 0;
            invalidRegistry[address].push_back(std::make_shared<const ShaderSnapshot>(std::move(compute)));
        }
    }
    DrawDecode invalid{};
    invalid.state = prepared.state;
    Reject([&] { DecodeGraphicsPrograms(invalid, queue, invalidRegistry, true, true); }, "registered");
    const auto frontBase = tessellation ? 0x148u : 0xc8u;
    queue.shader[frontBase + 1u] |= 0x100u;
    Reject([&] { DecodeGraphicsPrograms(invalid, queue, registry, true, true); }, "reserved graphics program address");
}

void PrebuildPolicyReadback(AgcDriver::VulkanDevice& device, bool disabled) {
    using namespace AgcDriver::DriverDetail;
    using namespace ShaderRecompiler;
    constexpr std::uint32_t extent = 64u;
    alignas(256) static std::array<std::uint32_t, extent * extent> pixels{};
    alignas(256) static constexpr std::array<std::array<float, 4>, 3> positions{{{-1, -1, 0, 1}, {3, -1, 0, 1}, {-1, 3, 0, 1}}};
    constexpr std::array<std::uint32_t, 6> vertexCode{0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
    constexpr std::array<std::uint32_t, 4> pixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};
    DrawDecode decoded{};
    decoded.state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, {}, {}};
    decoded.pixel.wave32 = true;
    decoded.pixel.targetOutputMode[0] = 9u;
    decoded.pixel.targetExportMapping.fill(0xe4u);
    const auto append = [&](ShaderStage stage, std::span<const std::uint32_t> code, std::vector<std::uint32_t> users) {
        auto snapshot = std::make_shared<ShaderSnapshot>();
        snapshot->codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
        snapshot->headerAddress = 0u;
        snapshot->type = stage == ShaderStage::Vertex ? 2u : 1u;
        snapshot->code.assign(code.begin(), code.end());
        DrawProgram program{{stage, snapshot->codeAddress, snapshot->code, 0u, {}}, 0u, 0u, std::move(users), {{{snapshot->codeAddress, std::as_bytes(std::span(snapshot->code))}, {0u, {}}}}, snapshot, 0u};
        decoded.programs.push_back(std::move(program));
        decoded.roles.push_back(stage == ShaderStage::Vertex ? ProgramRole::Main : ProgramRole::Fragment);
    };
    const auto address = reinterpret_cast<std::uintptr_t>(positions.data());
    append(ShaderStage::Vertex, vertexCode, {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u) | (16u << 16u), 3u, 0x01016facu});
    append(ShaderStage::Fragment, pixelCode, {});
    const auto target = device.Target();
    const auto stages = PrepareGraphicsStages(decoded, target);
    Require(stages.size() == 2u, "prebuild policy lost a required graphics stage");
    for (const auto& stage : stages) {
        Require(stage.entry.handle != nullptr && stage.snapshot != nullptr, "prebuild policy lost a required source handle");
        Require(disabled ? stage.registration == nullptr : stage.registration != nullptr, "prebuild policy retained or skipped registration-default materialization");
        stage.snapshot->prepared->entries.push_back(stage.entry);
    }
    std::vector<LinkedProgram> linked;
    std::vector<MemoryRegion> memory;
    for (std::size_t index = 0u; index < decoded.programs.size(); ++index) {
        const auto& program = decoded.programs[index];
        linked.push_back({decoded.roles[index], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
        memory.insert(memory.end(), program.memory.begin(), program.memory.end());
    }
    std::vector<std::shared_ptr<const RecompileResult>> owners;
    std::vector<AgcDriver::Graphics::CompiledShader> shaders;
    for (std::size_t index = 0u; index < decoded.programs.size(); ++index) {
        const auto& program = decoded.programs[index];
        const auto& stage = stages[index];
        const bool pixel = stage.stage == ShaderStage::Fragment;
        std::optional<ShaderVertexStageInfo> vertex;
        if (!pixel) {
            vertex = AgcDriver::Graphics::DecodeVertexStageInfo({}, 0u, program.userData, nullptr, true, 8u);
            vertex->paClVsOutCntl = PositionExportLayout(program.snapshot.get(), 0u, program.binary.code, 0u);
        }
        const auto room = AgcDriver::Graphics::PipelinePushSlotBytes - stage.pushOffset % AgcDriver::Graphics::PipelinePushSlotBytes;
        Require(stage.pushOffset % sizeof(std::uint32_t) == 0u && stage.entry.handle->artifact->bindings.pushConstantSizeBytes <= room, "prebuild policy changed the prepared push constant range");
        RecompileRequest request{program.binary, {32u, 0u, program.userData, {}, pixel ? std::optional(decoded.pixel) : std::nullopt, vertex, memory}, target, {0u, 0u, stage.pushOffset, room}, GraphicsCompileContext{0u, linked, {}, {}, {0u, 3u, 4u, 1u}}};
        const auto invocation = InvocationFor(*program.snapshot, 0u, request);
        const auto capture = invocation.Capture(SrtRuntime{program.userData, program.binary.codeAddress});
        Require(capture->source == stage.entry.handle->source && program.snapshot->prepared->entries.front().handle == stage.entry.handle, "prebuild policy replaced required prepared source ownership");
        owners.push_back(invocation.Materialize(*capture));
        Require(owners.back()->variantId == GetPreparedArtifact(*stage.entry.handle).variantId && program.snapshot->prepared->entries.size() == 1u && owners.back()->pushConstants.size() <= room, "prebuild policy changed the prepared shader ABI or ownership");
        shaders.push_back({stage.stage, owners.back().get(), stage.pushOffset});
    }
    auto state = decoded.state;
    pixels.fill(0x40404040u);
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), {extent, extent}, VK_FORMAT_R8G8B8A8_UNORM, sizeof(pixels), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.renderExtent = {extent, extent};
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(extent), static_cast<float>(extent), -static_cast<float>(extent), 0, 1};
    state.scissor = {{0u, 0u}, {extent, extent}};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.negativeOneToOne = false;
    state.blend.colorWriteMask = 15u;
    state.blends = {state.blend};
    device.Draw(state, {0u, 3u, 0u, 1u, 0u, false}, shaders);
    device.WaitIdle();
    Require(std::ranges::all_of(pixels, [](const auto value) { return value == 0xffffffffu; }), "prebuild policy changed native prepared-shader readback");
}

void RetainedPixelMatchesNullProgram(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::DriverDetail;
    Fixture front;
    Fixture retained;
    front.Initialize(2u);
    retained.Initialize(1u);
    const auto null = std::make_shared<ShaderSnapshot>(PrepareNullPixelProgram(device));
    ShaderRegistry registry;
    for (const auto& snapshot : {front.snapshot, retained.snapshot, null}) registry.emplace(snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{snapshot});
    AgcDriver::QueueState queue{};
    queue.context[0x8e] = queue.context[0x8f] = queue.context[0x203] = 0;
    queue.context[0x2dc] = 0xaa00;
    queue.context[0x1c4] = 1;
    front.Bind(queue, 0xc8u, 0x8bu);
    queue.shader[0x8bu] = 0;
    retained.Bind(queue, 0x008u, 0x00bu);
    queue.shader[0x00bu] = (31u << 1u) | (1u << 27u);
    for (std::uint32_t i = 0; i < 30; ++i) queue.shader[0x00cu + i] = 0xdead0000u + i;
    queue.context[0x1b6] = queue.context[0x1b3] = queue.context[0x1b4] = queue.context[0x1c5] = 0xffffffffu;
    auto captured = queue;
    captured.shader[0x00bu] = 20u << 1u;
    DrawDecode capturedDraw{};
    capturedDraw.state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
    DecodeGraphicsPrograms(capturedDraw, captured, registry, false, true);
    Require(capturedDraw.programs.back().snapshot == null && capturedDraw.programs.back().userData.empty(), "a retained pixel program with a valid 20-word resource count consumed unrelated userdata");
    for (const auto wave : {64u, 32u}) {
        DrawDecode draw{};
        draw.state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
        draw.state.stages.vertexWaveSize = 32u;
        draw.state.stages.fragmentWaveSize = wave;
        std::vector<AgcDriver::Graphics::RegisterRead> reads;
        AgcDriver::Graphics::RegisterReadLog() = &reads;
        try {
            DecodeGraphicsPrograms(draw, queue, registry, false, true);
            draw.pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, {}, AgcDriver::Graphics::PixelProgramSkipped(queue));
        } catch (...) {
            AgcDriver::Graphics::RegisterReadLog() = nullptr;
            throw;
        }
        AgcDriver::Graphics::RegisterReadLog() = nullptr;
        Require(draw.roles.size() == 2 && draw.roles.back() == ShaderRecompiler::ProgramRole::Fragment, "null substitution changed stage linking");
        Require(draw.programs.back().snapshot == null && draw.programs.back().userData.empty(), "an unused retained pixel program consumed unrelated userdata");
        Require(draw.pixel.interpolatorCount == 0 && !draw.pixel.depthExportEnable && !draw.pixel.stencilRefExportEnable && !draw.pixel.sampleMaskExportEnable && !draw.pixel.pixelKillEnable, "the substituted pixel program inherited stale inputs or exports");
        for (const auto mode : draw.pixel.targetOutputMode) Require(mode == 0, "the substituted pixel program inherited stale color formats");
        for (const auto read : reads) {
            Require(AgcDriver::Graphics::DrawKeyCovers(read), "a pixel substitution dependency is absent from the draw key");
            Require(read.bank != AgcDriver::Graphics::RegisterBank::Shader || read.offset < 0x00bu || read.offset > 0x02bu, "the substituted program read fragment resources or userdata");
        }
        const auto stages = PrepareGraphicsStages(draw, device.Target());
        Require(stages.size() == 2 && stages.back().entry.handle != nullptr, "a retained-format null program was not prepared");
        std::vector<ShaderRecompiler::LinkedProgram> linked;
        std::vector<ShaderRecompiler::MemoryRegion> memory;
        for (std::size_t i = 0; i < draw.programs.size(); ++i) {
            const auto& program = draw.programs[i];
            linked.push_back({draw.roles[i], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
            memory.insert(memory.end(), program.memory.begin(), program.memory.end());
        }
        const auto& fragment = draw.programs.back();
        const auto offset = stages.back().pushOffset;
        const auto room = AgcDriver::Graphics::PipelinePushSlotBytes - offset % AgcDriver::Graphics::PipelinePushSlotBytes;
        ShaderRecompiler::RecompileRequest request{fragment.binary, {wave, 0, {}, {}, draw.pixel, {}, memory, RegisteredFloatMode(*null)}, device.Target(), {0, 0, offset, room}, ShaderRecompiler::GraphicsCompileContext{0, linked, {}, {}, {}}};
        static_cast<void>(InvocationFor(*null, fragment.codeOffset, request));
        const auto handle = SourceHandleFor(*null, fragment.codeOffset, request);
        const auto entries = null->prepared->entries.size();
        static_cast<void>(InvocationFor(*null, fragment.codeOffset, request));
        Require(null->prepared->entries.size() == entries && SourceHandleFor(*null, fragment.codeOffset, request) == handle, "a repeated null program did not reuse its prepared variant");
    }
    queue.shader[0x00bu] = 2;
    queue.shader[0x00cu] = 0x12345678u;
    for (const auto trigger : {0u, 1u, 2u}) {
        auto active = queue;
        if (trigger == 0) active.context[0x8e] = active.context[0x8f] = 0xf;
        if (trigger == 1) active.context[0x203] = 0x400;
        if (trigger == 2) active.context[0x2dc] = 0xaa01;
        DrawDecode draw{};
        draw.state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
        DecodeGraphicsPrograms(draw, active, registry, false, true);
        Require(draw.programs.back().snapshot == retained.snapshot && draw.programs.back().userData == std::vector<std::uint32_t>{0x12345678u}, "an enabled consumer did not restore the retained pixel program");
    }
}

void CheckMergedHull(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::DriverDetail;
    using namespace ShaderRecompiler;
    Fixture merged;
    Fixture domain;
    Fixture fragment;
    merged.Initialize(3u);
    domain.Initialize(2u);
    fragment.Initialize(1u);
    ShaderRegistry registry;
    for (const auto* fixture : {&merged, &domain, &fragment}) registry.emplace(fixture->snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{fixture->snapshot});
    AgcDriver::QueueState queue{};
    queue.context[0x8e] = 0xfu;
    queue.context[0x8f] = 0xfu;
    merged.Bind(queue, 0x148u, 0x10bu);
    domain.Bind(queue, 0xc8u, 0x8bu);
    fragment.Bind(queue, 0x8u, 0xbu);
    const auto unset = queue;
    const auto null = [&] {
        auto state = queue;
        state.shader[0x102u] = 0u;
        state.shader[0x103u] = 0u;
        return state;
    }();
    alignas(8) const std::array<std::uint32_t, 2> pointer{};
    const auto pointerAddress = reinterpret_cast<std::uintptr_t>(pointer.data());
    queue.shader[0x102u] = static_cast<std::uint32_t>(pointerAddress);
    queue.shader[0x103u] = static_cast<std::uint32_t>(pointerAddress >> 32u);
    DrawDecode prepared{};
    prepared.state.stages.path = AgcDriver::Graphics::ShaderPath::Tessellation;
    prepared.state.stages.vertexWaveSize = 64u;
    prepared.state.stages.fragmentWaveSize = 32u;
    prepared.state.stages.tessellation = TessellationConfiguration{3, 4, 1, 2, 2};
    prepared.pixel.wave32 = true;
    prepared.pixel.targetOutputMode[0] = 9;
    prepared.pixel.targetExportMapping.fill(0xe4u);
    DecodeGraphicsPrograms(prepared, queue, registry, true, true);
    DrawDecode draw{};
    draw.state = prepared.state;
    draw.state.stages.tessellation->mergedHull = false;
    draw.pixel = prepared.pixel;
    DecodeGraphicsPrograms(draw, queue, registry, false, true);
    for (const auto* decoded : {&prepared, &draw}) {
        Require(decoded->state.stages.tessellation->mergedHull, "a registered type-3 hull program was not decoded as a merged hull");
        Require(decoded->roles == std::vector<ProgramRole>{ProgramRole::Local, ProgramRole::Hull, ProgramRole::Domain, ProgramRole::Fragment}, "a merged hull program did not give the local and hull roles");
        const auto& local = decoded->programs[0];
        const auto& hull = decoded->programs[1];
        Require(local.snapshot == merged.snapshot && hull.snapshot == merged.snapshot && local.codeOffset == 64u && hull.codeOffset == 64u, "the local and hull stages of a merged hull program refer to another program");
        Require(local.binary.stage == ShaderStage::Local && hull.binary.stage == ShaderStage::TessellationControl && local.firstUserSgpr == 8u && hull.firstUserSgpr == 0u && hull.userData.size() == local.userData.size() + 8u, "a merged hull program's stages lost the merged user SGPR layout");
    }
    Require(draw.programs[1].userData[0] == static_cast<std::uint32_t>(pointerAddress) && draw.programs[1].userData[1] == static_cast<std::uint32_t>(pointerAddress >> 32u), "a merged hull draw did not pass its user-data address");
    Require(UserDataWords(draw.programs, draw.roles, 0x10eu) == std::vector<std::pair<std::size_t, std::size_t>>{{0u, 2u}, {1u, 10u}}, "an indirect draw's user-data register does not reach both stages of a merged hull program");
    std::vector<MemoryRegion> programMemory;
    AppendProgramMemory(programMemory, draw.programs);
    Require(programMemory.size() == 6u, "a merged hull draw's program memory does not list each code and header region once");
    try {
        const RequestMemoryView view(programMemory);
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("a merged hull draw's program memory is not a valid request memory: ") + error.what());
    }
    for (const auto* state : {&unset, &null}) {
        DrawDecode without{};
        without.state = prepared.state;
        without.pixel = prepared.pixel;
        DecodeGraphicsPrograms(without, *state, registry, false, true);
        Require(without.roles == draw.roles && without.programs[1].userData.size() == draw.programs[1].userData.size() && without.programs[1].userData[0] == 0u && without.programs[1].userData[1] == 0u, "a merged hull draw without a user-data address was not decoded with zero s[0:1]");
    }
    auto unmapped = queue;
    unmapped.shader[0x102u] = 0x10u;
    unmapped.shader[0x103u] = 0u;
    DrawDecode stale{};
    stale.state = prepared.state;
    stale.pixel = prepared.pixel;
    DecodeGraphicsPrograms(stale, unmapped, registry, false, true);
    Require(stale.programs[1].userData[0] == 0x10u && stale.programs[1].userData[1] == 0u, "a merged hull draw did not pass a user-data address it does not need unchecked");
    const auto stages = PrepareGraphicsStages(prepared, device.Target());
    Require(stages.size() == 4u && stages[0].entry.handle != stages[1].entry.handle, "a merged hull program was not prepared as separate local and hull stages");
    for (const auto& stage : stages) stage.snapshot->prepared->entries.push_back(stage.entry);
    std::vector<LinkedProgram> linked;
    std::vector<MemoryRegion> memory;
    AppendProgramMemory(memory, draw.programs);
    for (std::size_t index = 0; index < draw.programs.size(); ++index) {
        const auto& program = draw.programs[index];
        linked.push_back({draw.roles[index], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
    }
    const auto predicted = merged.snapshot->prepared->entries.size();
    for (std::size_t index = 0; index < 2u; ++index) {
        const auto& program = draw.programs[index];
        const auto vertex = AgcDriver::Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, nullptr, true);
        RecompileRequest request{program.binary, {64u, program.firstUserSgpr, program.userData, {}, std::nullopt, vertex, memory}, device.Target(), {0, 0, 0, 128u}, GraphicsCompileContext{program.firstUserSgpr, linked, std::nullopt, draw.state.stages.tessellation, {0, 3, 4, 1}}};
        static_cast<void>(InvocationFor(*program.snapshot, program.codeOffset, request));
        Require(merged.snapshot->prepared->entries.size() == predicted, "a merged hull stage prepared at ABI resolution was prepared again at its draw");
        request.graphics->tessellation->mergedHull = false;
        Require(!MatchesPreparedShader(request, *stages[index].entry.handle), "a separate hull configuration matched a merged hull artifact");
    }
    Fixture front;
    front.Initialize(5u);
    registry.emplace(front.snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{front.snapshot});
    Fixture back;
    back.Initialize(7u);
    registry.emplace(back.snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{back.snapshot});
    auto separate = queue;
    front.Bind(separate, 0x148u, 0x10bu);
    back.Bind(separate, 0x108u, 0x10bu);
    DrawDecode halves{};
    halves.state = prepared.state;
    halves.pixel = prepared.pixel;
    DecodeGraphicsPrograms(halves, separate, registry, false, true);
    Require(!halves.state.stages.tessellation->mergedHull && halves.programs[0].snapshot == front.snapshot && halves.programs[1].snapshot == back.snapshot, "separate hull halves were decoded as a merged hull program");
    auto halvesUnset = unset;
    front.Bind(halvesUnset, 0x148u, 0x10bu);
    back.Bind(halvesUnset, 0x108u, 0x10bu);
    DrawDecode halvesWithout{};
    halvesWithout.state = prepared.state;
    halvesWithout.pixel = prepared.pixel;
    Reject([&] { DecodeGraphicsPrograms(halvesWithout, halvesUnset, registry, false, true); }, "merged shader user-data address is null");
}

void CheckMergedHullVertexTables(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::DriverDetail;
    using namespace ShaderRecompiler;
    struct Header {
        Shader shader{};
        ShaderUserData users{};
        std::array<std::uint16_t, 11> direct{};
        ShaderSemantic semantic{};
    } header;
    alignas(256) std::array<std::uint32_t, 65> code{};
    code.fill(0xffffffffu);
    code.back() = 0xbf810000u;
    header.shader.type = 3;
    header.shader.user_data = &header.users;
    header.direct.fill(0xffffu);
    header.direct[8] = 10u;
    header.direct[10] = 12u;
    header.users.direct_resource_offset = header.direct.data();
    header.users.direct_resource_count = static_cast<std::uint16_t>(header.direct.size());
    header.semantic.size_in_elements = 4u;
    header.shader.input_semantics = &header.semantic;
    header.shader.num_input_semantics = 1u;
    auto merged = std::make_shared<ShaderSnapshot>();
    merged->type = 3;
    merged->codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
    merged->headerAddress = reinterpret_cast<std::uintptr_t>(&header);
    merged->code.assign(code.begin(), code.end());
    merged->header.resize(sizeof(header));
    std::memcpy(merged->header.data(), &header, sizeof(header));
    Fixture domain;
    Fixture fragment;
    domain.Initialize(2u);
    fragment.Initialize(1u);
    ShaderRegistry registry;
    registry.emplace(merged->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{merged});
    for (const auto* fixture : {&domain, &fragment}) registry.emplace(fixture->snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{fixture->snapshot});
    AgcDriver::QueueState queue{};
    queue.context[0x8e] = 0xfu;
    queue.context[0x8f] = 0xfu;
    const auto entry = merged->codeAddress + 256u;
    queue.shader[0x148u] = static_cast<std::uint32_t>(entry >> 8u);
    queue.shader[0x149u] = static_cast<std::uint32_t>(entry >> 40u);
    queue.shader[0x10bu] = 16u << 1u;
    domain.Bind(queue, 0xc8u, 0x8bu);
    fragment.Bind(queue, 0x8u, 0xbu);
    alignas(16) const std::array<std::uint32_t, 4> bufferTable{};
    alignas(16) const std::array<std::uint32_t, 4> attributeTable{};
    const auto bufferAddress = reinterpret_cast<std::uintptr_t>(bufferTable.data());
    const auto attributeAddress = reinterpret_cast<std::uintptr_t>(attributeTable.data());
    for (std::uint32_t slot = 2u; slot < 6u; ++slot) queue.shader[0x10cu + slot] = slot % 2u == 0u ? 0x12345600u : 0x00000077u;
    queue.shader[0x10cu + 10u] = static_cast<std::uint32_t>(bufferAddress);
    queue.shader[0x10cu + 11u] = static_cast<std::uint32_t>(bufferAddress >> 32u);
    queue.shader[0x10cu + 12u] = static_cast<std::uint32_t>(attributeAddress);
    queue.shader[0x10cu + 13u] = static_cast<std::uint32_t>(attributeAddress >> 32u);
    DrawDecode draw{};
    draw.state.stages.path = AgcDriver::Graphics::ShaderPath::Tessellation;
    draw.state.stages.vertexWaveSize = 64u;
    draw.state.stages.fragmentWaveSize = 32u;
    draw.state.stages.tessellation = TessellationConfiguration{3, 3, 1, 2, 2};
    draw.pixel.wave32 = true;
    draw.pixel.targetOutputMode[0] = 9;
    draw.pixel.targetExportMapping.fill(0xe4u);
    DecodeGraphicsPrograms(draw, queue, registry, false, true);
    Require(draw.state.stages.tessellation->mergedHull && draw.programs[1].firstUserSgpr == 0u, "the merged hull draw with vertex tables was not decoded as a merged hull");
    for (std::size_t index = 0; index < 2u; ++index) {
        const auto& program = draw.programs[index];
        std::vector<AgcDriver::Graphics::DecodeRead> reads;
        const auto vertex = AgcDriver::Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, &reads, false, 8u - program.firstUserSgpr);
        Require(vertex.fetchEmbedded && vertex.fetchAttribReg == 12u && vertex.fetchBufferReg == 10u && vertex.resourcesNum == 1u, "a merged hull stage did not decode its vertex tables");
        Require(reads.size() == 2u && reads[0].address == attributeAddress && reads[1].address == bufferAddress, "a merged hull stage read its vertex tables from user-data words other than its slots");
    }
}

void NullPixelMatchesRegistration(AgcDriver::VulkanDevice& device) {
    using namespace AgcDriver::DriverDetail;
    Fixture front;
    front.Initialize(2u);
    const auto null = std::make_shared<ShaderSnapshot>(PrepareNullPixelProgram(device));
    const auto registered = null->prepared->entries;
    Require(!registered.empty(), "the null pixel program was not prepared at registration");
    ShaderRegistry registry;
    registry.emplace(front.snapshot->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{front.snapshot});
    registry.emplace(null->codeAddress, std::vector<std::shared_ptr<const ShaderSnapshot>>{null});
    AgcDriver::QueueState queue{};
    queue.context[0x8e] = 0xfu;
    queue.context[0x8f] = 0xfu;
    front.Bind(queue, 0xc8u, 0x8bu);
    alignas(8) const std::array<std::uint32_t, 2> merged{};
    const auto mergedAddress = reinterpret_cast<std::uintptr_t>(merged.data());
    queue.shader[0x82u] = static_cast<std::uint32_t>(mergedAddress);
    queue.shader[0x83u] = static_cast<std::uint32_t>(mergedAddress >> 32u);
    queue.shader[0x008u] = 0u;
    queue.shader[0x009u] = 0u;
    queue.shader[0x00bu] = 4u << 1u;
    queue.context[0x1b3u] = 0x30u;
    queue.context[0x1b4u] = 0x30u;
    DrawDecode draw{};
    draw.state.stages.path = AgcDriver::Graphics::ShaderPath::Vertex;
    draw.state.stages.vertexWaveSize = 32u;
    draw.state.stages.fragmentWaveSize = 64u;
    std::array<std::uint8_t, 8> mappings{};
    mappings.fill(0xe4u);
    draw.pixel = AgcDriver::Graphics::DecodePixelStageInfo(queue.context, mappings, true);
    DecodeGraphicsPrograms(draw, queue, registry, false, true);
    const auto& fragment = draw.programs.back();
    Require(fragment.snapshot == null && fragment.userData.empty(), "a null pixel draw took the last pixel shader's user SGPRs");
    const auto stages = PrepareGraphicsStages(draw, device.Target());
    const auto& handle = stages.back().entry.handle;
    Require(std::ranges::any_of(registered, [&](const auto& entry) { return entry.handle == handle; }), "a null pixel draw did not match the variant prepared at registration");
}

}

int main(int argc, char** argv) {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Require(argc <= 2, "invalid test arguments");
        if (argc == 2 && std::string_view(argv[1]).starts_with("--prebuild-")) {
            const auto mode = std::string_view(argv[1]).substr(11u);
            const auto* value = std::getenv("APS5_NO_LIBRARY_PREBUILD");
            Require(mode == "unset" ? value == nullptr : (mode == "0" || mode == "1") && value != nullptr && mode == value, "prebuild policy test has the wrong startup environment");
            if (!device->Target().registrationModules) return VulkanTestSkipped;
            PrebuildPolicyReadback(*device, mode == "1");
            std::cout << "prebuild policy native readback passed\n";
            return 0;
        }
        const auto dump = argc == 2 ? std::filesystem::path(argv[1]) : std::filesystem::path{};
        RetainedPixelMatchesNullProgram(*device);
        NullPixelMatchesRegistration(*device);
        Check(*device, AgcDriver::Graphics::ShaderPath::Vertex, dump);
        Check(*device, AgcDriver::Graphics::ShaderPath::Geometry, dump);
        Check(*device, AgcDriver::Graphics::ShaderPath::Tessellation, dump);
        CheckMergedHull(*device);
        CheckMergedHullVertexTables(*device);
        std::cout << "prepared graphics ABI tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <vector>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

using Testing::Require;

template<typename TAction>
void Reject(TAction action, const char* expected) {
    try { action(); }
    catch (const std::exception& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, error.what());
        return;
    }
    Testing::Fail(std::string("invalid graphics ABI was accepted; expected: ") + expected);
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
        code.back() = 0xbf810000u;
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
    fragment.snapshot->code.insert(fragment.snapshot->code.end(), pixelCode.begin(), pixelCode.end());
    ShaderRegistry registry;
    for (const auto* fixture : {&front, &back, &domain, &fragment}) registry.emplace(fixture->snapshot->codeAddress, fixture->snapshot);
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
        Require(expected.codeOffset == 64u && expected.binary.code.size() == (prepared.roles[index] == ProgramRole::Fragment ? pixelCode.size() : 1u) && expected.binary.code[0] == (prepared.roles[index] == ProgramRole::Fragment ? pixelCode[0] : 0xbf810000u), "graphics entry point did not trim the code prefix");
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
    }
    const auto stages = PrepareGraphicsStages(prepared, target);
    Require(stages.size() == (tessellation ? 4u : 2u), "graphics preparation compiled the wrong stages");
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
        Reject([&] { static_cast<void>(InvocationFor(*program.snapshot, program.codeOffset, request)); }, "artifact is missing");
        request.layout = {0, 0, 0, mesh ? MeshDrawPushOffsetBytes : 128u};
        if (pixel) request.context.pixel->interpolatorSettings[0] ^= 1u;
        else if (tessellation) ++request.graphics->tessellation->outputControlPoints;
        else if (mesh) ++request.graphics->mesh->maxVertices;
        else ++request.context.userDataBaseRegister;
        Reject([&] { static_cast<void>(InvocationFor(*program.snapshot, program.codeOffset, request)); }, "artifact is missing");
    }
    ShaderRegistry invalidRegistry;
    DrawDecode invalid{};
    invalid.state = prepared.state;
    Reject([&] { DecodeGraphicsPrograms(invalid, queue, invalidRegistry, true, true); }, "registered");
    const auto frontBase = tessellation ? 0x148u : 0xc8u;
    queue.shader[frontBase + 1u] |= 0x100u;
    Reject([&] { DecodeGraphicsPrograms(invalid, queue, registry, true, true); }, "reserved graphics program address");
}


void CheckPath(AgcDriver::Graphics::ShaderPath path) {
    auto& device = SharedVulkanTestDevice();
    const auto& arguments = Testing::Arguments();
    Require(arguments.size() <= 1, "invalid test arguments");
    const auto dump = arguments.empty() ? std::filesystem::path{} : std::filesystem::path(arguments.front());
    Check(device, path, dump);
}

const Testing::Case vertexPath{"PreparedGraphics_VertexPath_MatchesDrawAbiAndRejectsInvalidPrograms", [] {
    CheckPath(AgcDriver::Graphics::ShaderPath::Vertex);
}};

const Testing::Case geometryPath{"PreparedGraphics_MeshGeometryPath_MatchesDrawAbiAndRejectsInvalidPrograms", [] {
    CheckPath(AgcDriver::Graphics::ShaderPath::Geometry);
}};

const Testing::Case tessellationPath{"PreparedGraphics_TessellationPath_MatchesDrawAbiAndRejectsInvalidPrograms", [] {
    CheckPath(AgcDriver::Graphics::ShaderPath::Tessellation);
}};

} // namespace

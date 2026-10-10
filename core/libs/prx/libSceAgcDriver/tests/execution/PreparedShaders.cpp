#include "VulkanTestDevice.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <future>
#include <barrier>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <thread>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void ExpectFailure(TAction action, const char* expected) {
    try { action(); }
    catch (const std::exception& error) {
        Require(std::string(error.what()).find(expected) != std::string::npos, error.what());
        return;
    }
    throw std::runtime_error("expected preparation failure");
}

void RunUnregistered(AgcDriver::VulkanDevice& device, ShaderRecompiler::RecompileRequest request) {
    const auto code = request.shader.code;
    for (const bool sourceFirst : {false, true}) {
        AgcDriver::DriverDetail::ShaderSnapshot snapshot{request.shader.codeAddress, 0, 0, {code.begin(), code.end()}, {}};
        request.shader.code = snapshot.code;
        if (sourceFirst) {
            const auto handle = AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request);
            Require(snapshot.prepared->entries.size() == 1 && snapshot.prepared->entries.front().handle == handle, "unregistered compute source was not cached");
        }
        static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request));
        Require(snapshot.prepared->entries.size() == 1, "unregistered compute invocation did not cache exactly one artifact");
        const auto handle = snapshot.prepared->entries.front().handle;
        const auto& artifact = ShaderRecompiler::GetPreparedArtifact(*handle);
        for (const bool useCache : {false, true}) {
            request.useCache = useCache;
            Require(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request) == handle, "unregistered compute source was prepared again");
            const auto repeated = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
            ShaderRecompiler::SrtRuntime runtime{};
            runtime.userData = request.context.userData;
            const auto capture = repeated.Capture(runtime);
            const auto result = repeated.Materialize(*capture);
            Require(snapshot.prepared->entries.size() == 1 && result->variantId == artifact.variantId && result->spirv.data() == artifact.spirv.data(), "unregistered compute invocation replaced its cached artifact");
            device.Dispatch(*result, 1, 1, 1);
        }
        device.WaitIdle();
    }
}

void NullPixelAtDraw(AgcDriver::VulkanDevice& device) {
    std::vector<std::uint32_t> code(64, 0);
    code.front() = 0xbf810000u;
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{AgcDriver::DriverDetail::NullPixelProgramAddress(), 0, 1, code, {}};
    snapshot.header.resize(sizeof(Shader));
    std::array<std::uint32_t, 4> users{};
    const auto pixel = AgcDriver::Graphics::DecodePixelStageInfo({}, {}, true);
    const ShaderRecompiler::RecompileRequest registered{{ShaderRecompiler::ShaderStage::Fragment, snapshot.codeAddress, snapshot.code, 0, {}}, {64, 0, {}, {}, pixel, {}, {}}, device.Target(), {0, 0, 0, 128}};
    snapshot.prepared->entries.push_back({0, ShaderRecompiler::PrepareShader(registered)});
    ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Fragment, snapshot.codeAddress, snapshot.code, 0, {}}, {32, 0, users, {}, pixel, {}, {}}, device.Target(), {0, 0, 20, 108}};
    static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request));
    Require(snapshot.prepared->entries.size() == 2, "the null pixel program was not prepared at draw");
    static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request));
    Require(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request) == snapshot.prepared->entries.back().handle && snapshot.prepared->entries.size() == 2, "the null pixel program was prepared again for the same draw");
}

void Run(AgcDriver::VulkanDevice& device) {
    NullPixelAtDraw(device);
    alignas(256) std::array<std::uint32_t, 1> code{0xbf810000u};
    std::array<std::uint32_t, 4> users{};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{1, 1, 1}, 0, {false, false, false}, false, 1, {}};
    ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}}, {32, 0, users, compute, {}, {}, {}}, device.ComputeTarget(32), {0, 0, 0, 128}};
    RunUnregistered(device, request);
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{request.shader.codeAddress, 0, 0, {code.begin(), code.end()}, {}};
    snapshot.header.resize(sizeof(Shader));
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request)); }, "artifact is missing");
    const auto handle = ShaderRecompiler::PrepareShader(request);
    snapshot.prepared->entries.push_back({0, handle});
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 1, request)); }, "artifact is missing");
    const auto& artifact = ShaderRecompiler::GetPreparedArtifact(*handle);
    for (std::uint32_t value = 0; value < 8; ++value) {
        users.fill(value);
        request.useCache = value % 2 == 0;
        const auto ready = AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request);
        ShaderRecompiler::SrtRuntime runtime{};
        runtime.userData = users;
        const auto capture = ShaderRecompiler::CaptureResources(request, runtime, *ready);
        const auto result = ShaderRecompiler::MaterializeShader(request, *capture, *ready);
        Require(result->variantId == artifact.variantId && result->spirv.data() == artifact.spirv.data(), "invocation replaced the prepared artifact");
        device.Dispatch(*result, 1, 1, 1);
    }
    device.WaitIdle();
    auto registeredRequest = request;
    registeredRequest.shader.code = snapshot.code;
    registeredRequest.useCache = true;
    const auto invocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, registeredRequest);
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request)); }, "does not refer to registered code");
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 1, registeredRequest)); }, "outside the snapshot");
    registeredRequest.context.compute->numThreads[0] = 2;
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, registeredRequest)); }, "artifact is missing");
    ShaderRecompiler::SrtRuntime preparedRuntime{};
    preparedRuntime.userData = users;
    const auto preparedCapture = invocation.Capture(preparedRuntime);
    const auto firstResult = invocation.Materialize(*preparedCapture);
    const auto repeatedCapture = invocation.Capture(preparedRuntime);
    Require(invocation.Materialize(*repeatedCapture) == firstResult, "prepared invocation rebuilt an unchanged materialized result");
    users[0] += 1;
    const auto changedCapture = invocation.Capture(preparedRuntime);
    const auto changedResult = invocation.Materialize(*changedCapture);
    Require(changedResult != firstResult && changedResult->spirv.data() == firstResult->spirv.data(), "prepared invocation did not distinguish changed runtime data");
    users[0] -= 1;
    Require(invocation.Materialize(*preparedCapture) == firstResult, "prepared invocation evicted the previous runtime data");
    auto replacement = OpenVulkanTestDevice();
    Require(replacement != nullptr && replacement->Serial() != device.Serial(), "replacement device was not created");
    auto replacementRequest = request;
    replacementRequest.target = replacement->ComputeTarget(32);
    Require(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, replacementRequest) == handle, "device replacement discarded a compatible prepared artifact");
    replacementRequest.shader.code = snapshot.code;
    const auto replacementInvocation = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, replacementRequest);
    const auto replacementCapture = replacementInvocation.Capture(preparedRuntime);
    const auto replacementResult = replacementInvocation.Materialize(*replacementCapture);
    Require(replacementResult->spirv.data() == artifact.spirv.data(), "device replacement recompiled the prepared shader");
    replacement->Dispatch(*replacementResult, 1, 1, 1);
    replacement->WaitIdle();
    replacementRequest.target.nonConstantImageOffsets = !replacementRequest.target.nonConstantImageOffsets;
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, replacementRequest)); }, "artifact is missing");
    ShaderRecompiler::SrtRuntime runtime{};
    runtime.userData = users;
    const auto capture = ShaderRecompiler::CaptureResources(request, runtime, *handle);
    request.context.compute->numThreads[0] = 2;
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request)); }, "artifact is missing");
    ExpectFailure([&] { static_cast<void>(ShaderRecompiler::CaptureResources(request, runtime, *handle)); }, "does not match the static ABI");
    ExpectFailure([&] { static_cast<void>(ShaderRecompiler::MaterializeShader(request, *capture, *handle)); }, "does not match the static ABI");
    const auto otherHandle = ShaderRecompiler::PrepareShader(request);
    const auto otherCapture = ShaderRecompiler::CaptureResources(request, runtime, *otherHandle);
    ExpectFailure([&] { static_cast<void>(invocation.Materialize(*otherCapture)); }, "another prepared shader");
    ExpectFailure([&] { static_cast<void>(ShaderRecompiler::MaterializeShader(request, *capture, *otherHandle)); }, "another prepared shader");
    request.context.compute->numThreads[0] = 1;
    request.layout.pushConstantSizeBytes = 124;
    Require(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request) == handle, "compatible push constant capacity discarded the prepared artifact");
    request.layout.pushConstantSizeBytes = 126;
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request)); }, "artifact is missing");
    request.layout.pushConstantSizeBytes = 128;
    code[0] = 0xffffffffu;
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request)); }, "artifact is missing");
}

void FailureCapture(AgcDriver::VulkanDevice& device) {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 1> code{0xffffffffu};
    const std::vector<std::byte> header(2048, std::byte{0x5a});
    RecompileRequest request{{ShaderStage::Compute, 0x12345cafeull, code, 0x20000, header},
        {32, 0, {}, ShaderComputeStageInfo{{1, 1, 1}, 0, {}, false, 1, {}}, {}, {}, {}}, device.ComputeTarget(32), {0, 0, 0, 128}};
    const auto failure = [&](auto action) {
        try { action(); }
        catch (const std::exception& error) { return std::string(error.what()); }
        throw std::runtime_error("invalid shader preparation succeeded");
    };
    const auto expected = failure([&] { static_cast<void>(PrepareShader(request)); });
    const char* previous = std::getenv("APS5_DUMP_SHADERS");
    const std::string saved = previous != nullptr ? previous : "";
    const std::string path = "shader_12345cafe.req";
    std::remove(path.c_str());
#ifdef _WIN32
    Require(_putenv_s("APS5_DUMP_SHADERS", "") == 0, "cannot disable shader capture");
#else
    Require(unsetenv("APS5_DUMP_SHADERS") == 0, "cannot disable shader capture");
#endif
    Require(failure([&] { static_cast<void>(AgcDriver::DriverDetail::PrepareShaderWithDiagnostics(request)); }) == expected, "disabled shader capture changed the preparation failure");
    Require(!std::ifstream(path, std::ios::binary).is_open(), "disabled shader capture created a request");
#ifdef _WIN32
    Require(_putenv_s("APS5_DUMP_SHADERS", "1") == 0, "cannot enable shader capture");
#else
    Require(setenv("APS5_DUMP_SHADERS", "1", 1) == 0, "cannot enable shader capture");
#endif
    const auto actual = failure([&] { static_cast<void>(AgcDriver::DriverDetail::PrepareShaderWithDiagnostics(request)); });
#ifdef _WIN32
    Require(_putenv_s("APS5_DUMP_SHADERS", saved.c_str()) == 0, "cannot restore shader capture");
#else
    Require((previous != nullptr ? setenv("APS5_DUMP_SHADERS", saved.c_str(), 1) : unsetenv("APS5_DUMP_SHADERS")) == 0, "cannot restore shader capture");
#endif
    Require(actual == expected, "shader capture changed the preparation failure");
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    file.close();
    std::remove(path.c_str());
    const RequestSerializer serializer;
    Require(text.str().size() > 1024 && text.str() == serializer.Serialize(request), "shader failure capture was incomplete");
    const auto replay = serializer.Deserialize(text.str());
    Require(serializer.Serialize(replay.request) == text.str(), "shader failure capture changed on replay");
}

void PrepareMultisampledStorage(AgcDriver::VulkanDevice& device) {
    using namespace ShaderRecompiler;
    std::array<std::uint32_t, 13> code{0xd7460000u, 0x0401060cu, 0xd7460001u, 0x0405060du, 0x7e04020eu, 0x7e060280u, 0x7e080208u, 0x7e0a0209u, 0x7e0c020au, 0x7e0e020bu, 0xf0200f38u, 0x00000400u, 0xbf810000u};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x30000u, code, 0, {}};
    const std::array<std::uint32_t, 16> users{};
    request.context.userData = users;
    request.context.waveSize = 32;
    request.context.compute = ShaderComputeStageInfo{{8, 8, 1}, 0, {true, true, false}, false, 2, {}};
    request.target = device.ComputeTarget(32);
    std::vector<std::uint32_t> multisampleCapabilities(request.target.supportedCapabilities.begin(), request.target.supportedCapabilities.end());
    if (std::ranges::find(multisampleCapabilities, spv::CapabilityStorageImageMultisample) == multisampleCapabilities.end()) multisampleCapabilities.push_back(spv::CapabilityStorageImageMultisample);
    request.target.supportedCapabilities = multisampleCapabilities;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    for (std::uint32_t variant = 0; variant < 5; ++variant) {
        if (variant != 0u) {
            code[10] = variant % 2u != 0u ? 0xf0200f38u : 0xf0200f30u;
            if (variant >= 3u) code[10] = variant == 3u ? 0xf03c0138u : 0xf03c0130u;
            code[5] = 0x7e060283u;
            code[4] = 0x7e040282u;
        }
        const auto handle = PrepareShader(request);
        const auto& artifact = GetPreparedArtifact(*handle);
        std::vector<std::uint32_t> samples;
        for (std::size_t offset = 5; offset < artifact.spirv.size(); offset += artifact.spirv[offset] >> 16u) {
            const auto instruction = artifact.spirv[offset];
            if ((instruction & 0xffffu) == spv::OpImageWrite) {
                Require(variant < 3u && (instruction >> 16u) == 6u && artifact.spirv[offset + 4] == spv::ImageOperandsSampleMask, "MSAA storage write lost its sample operand");
                samples.push_back(artifact.spirv[offset + 5]);
            }
            if ((instruction & 0xffffu) == spv::OpImageTexelPointer) {
                Require(variant >= 3u && (instruction >> 16u) == 6u, "MSAA atomic lost its sample operand");
                samples.push_back(artifact.spirv[offset + 5]);
            }
        }
        Require(!samples.empty(), "MSAA storage shader has no image writes or atomics");
        const auto expectedSample = variant == 0u ? 0u : variant % 2u != 0u ? 3u : 2u;
        for (const auto sample : samples) {
            bool found = false;
            for (std::size_t offset = 5; offset < artifact.spirv.size(); offset += artifact.spirv[offset] >> 16u) {
                if ((artifact.spirv[offset] & 0xffffu) != spv::OpConstant || artifact.spirv[offset + 2] != sample) continue;
                Require(artifact.spirv[offset + 3] == expectedSample, "MSAA storage operation addresses the wrong sample");
                found = true;
            }
            Require(found, "MSAA sample constant is missing");
        }
    }
    std::vector<std::uint32_t> capabilities(request.target.supportedCapabilities.begin(), request.target.supportedCapabilities.end());
    std::erase(capabilities, spv::CapabilityStorageImageMultisample);
    request.target.supportedCapabilities = capabilities;
    ExpectFailure([&] { static_cast<void>(PrepareShader(request)); }, "storage image multisampling is unavailable on the target device");
}

void SettleLater(AgcDriver::DriverDetail::PreparedShaders& prepared, std::vector<AgcDriver::DriverDetail::PreparedShaders::Entry> entries, std::exception_ptr failure) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
        std::lock_guard lock(prepared.mutex);
        prepared.entries.insert(prepared.entries.end(), entries.begin(), entries.end());
        prepared.failure = failure;
        prepared.pending = false;
    }
    prepared.settled.notify_all();
}

std::exception_ptr InjectedFailure() {
    try {
        throw std::runtime_error("injected registration preparation failure");
    } catch (...) {
        return std::current_exception();
    }
}

void PendingRegistrationPreparation(AgcDriver::VulkanDevice& device) {
    alignas(256) const std::array<std::uint32_t, 1> code{0xbf810000u};
    std::array<std::uint32_t, 4> users{};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{1, 1, 1}, 0, {false, false, false}, false, 1, {}};
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    for (const bool source : {true, false}) {
        AgcDriver::DriverDetail::ShaderSnapshot snapshot{address, 0, 0, {code.begin(), code.end()}, {}};
        snapshot.header.resize(sizeof(Shader));
        ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Compute, address, snapshot.code, 0, {}}, {32, 0, users, compute, {}, {}, {}}, device.ComputeTarget(32), {0, 0, 0, 128}};
        const auto handle = ShaderRecompiler::PrepareShader(request);
        snapshot.prepared->pending = true;
        auto settle = std::async(std::launch::async, SettleLater, std::ref(*snapshot.prepared), std::vector<AgcDriver::DriverDetail::PreparedShaders::Entry>{{0, handle}}, nullptr);
        if (source) Require(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request) == handle, "first use did not wait for registration preparation");
        else static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request));
        settle.get();
        Require(snapshot.prepared->entries.size() == 1, "first use prepared a pending registration again");
    }
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{address, 0, 0, {code.begin(), code.end()}, {}};
    snapshot.header.resize(sizeof(Shader));
    snapshot.prepared->pending = true;
    auto settle = std::async(std::launch::async, SettleLater, std::ref(*snapshot.prepared), std::vector<AgcDriver::DriverDetail::PreparedShaders::Entry>{}, InjectedFailure());
    {
        AgcDriver::DriverDetail::ShaderPreparationTransaction transaction;
        ExpectFailure([&] { static_cast<void>(transaction.Read(snapshot)); }, "injected registration preparation failure");
    }
    settle.get();
    ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Compute, address, snapshot.code, 0, {}}, {32, 0, users, compute, {}, {}, {}}, device.ComputeTarget(32), {0, 0, 0, 128}};
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request)); }, "injected registration preparation failure");
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request)); }, "injected registration preparation failure");
}

void UnsupportedTypeRegistration() {
    alignas(256) static const std::array<std::uint32_t, 1> code{0xbf810000u};
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 8> registers{{{0, 0x1218}, {1, 0x40104004}, {2, 0xa0}, {5, 0}, {3, 6}, {4, 0xc}, {6, 0}, {7, 0}}};
    } header;
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = sizeof(code);
    header.shader.code = code.data();
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    header.shader.type = 8;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    AgcDriverRegisterShader_nid_postfix(&header.shader);
}

void RegistrationWithoutSpecials() {
    alignas(256) std::array<std::uint32_t, 1> code{0xbf810000u};
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 7> registers{};
    } header;
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = sizeof(code);
    header.shader.code = code.data();
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    header.registers = {{{0x20c, static_cast<std::uint32_t>(address >> 8u)}, {0x20d, static_cast<std::uint32_t>(address >> 40u)}, {0x207, 1}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 0}}};
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    std::vector<std::uint32_t> commands;
    for (const auto reg : header.registers) commands.insert(commands.end(), {0xc0017600u, reg.offset, reg.value});
    for (const std::uint32_t initiator : {0x8041u, 0x41u}) commands.insert(commands.end(), {0xc0031500u, 1, 1, 1, initiator});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    sceAgcDriverSubmitAcb(0x20, &packet);
    AgcDriverWaitIdle_nid_postfix();
}

void Registration(bool indirect) {
    UnsupportedTypeRegistration();
    alignas(256) std::array<std::uint32_t, 1> code{0xbf810000u};
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 9> registers{};
        std::array<ShaderRegister, 2> context{{{0x1b6, 0}, {0x1b6, 1}}};
        ShaderSpecialRegs specials{};
    } header;
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = sizeof(code);
    header.shader.code = code.data();
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    header.shader.cx_registers = header.context.data();
    header.shader.num_cx_registers = header.context.size();
    header.shader.specials = &header.specials;
    header.specials.dispatch_modifier = 0x8000;
    header.registers = {{{0x20c, static_cast<std::uint32_t>(address >> 8u)}, {0x20d, static_cast<std::uint32_t>(address >> 40u)}, {0x207, 2}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 0}, {0x207, 1}, {0x207, 1}}};
    const auto threadRegisterIndex = header.registers.size() - 1;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    std::barrier start(4);
    std::vector<std::future<void>> registrations;
    for (unsigned worker = 0; worker < 4; ++worker) {
        registrations.push_back(std::async(std::launch::async, [&] {
            start.arrive_and_wait();
            for (unsigned iteration = 0; iteration < 8; ++iteration) {
                AgcDriverRegisterShader_nid_postfix(&header.shader);
                AgcDriverResolveShaderAbi_nid_postfix(&header.shader, {}, {});
            }
        }));
    }
    for (auto& registration : registrations) registration.get();
    Shader copy = header.shader;
    copy.user_data = reinterpret_cast<ShaderUserData*>(&copy);
    AgcDriverResolveShaderAbi_nid_postfix(&copy, {}, {});
    alignas(Shader) std::array<std::byte, sizeof(Shader)> padded{};
    std::memcpy(padded.data(), &copy, sizeof(Shader));
    padded.back() = std::byte{0x7d};
    AgcDriverResolveShaderAbi_nid_postfix(reinterpret_cast<Shader*>(padded.data()), {}, {});
    copy.target ^= 1u;
    ExpectFailure([&] { AgcDriverResolveShaderAbi_nid_postfix(&copy, {}, {}); }, "replaced shader header");
    header.registers[1].value |= 0x100u;
    ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "invalid registered program address");
    header.registers[1].value &= 0xffu;
    ++header.registers[0].value;
    ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "entry point is outside shader code");
    --header.registers[0].value;
    header.shader.num_sh_registers = 255;
    ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "truncated shader metadata");
    header.shader.num_sh_registers = header.registers.size();
    header.registers[threadRegisterIndex].value = 0;
    ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "must be nonzero");
    header.registers[threadRegisterIndex].value = 1;
    code[0] = 0xffffffffu;
    ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "");
    std::vector<std::uint32_t> commands;
    for (const auto reg : header.registers) commands.insert(commands.end(), {0xc0017600u, reg.offset, reg.value});
    const std::array<std::uint32_t, 3> arguments{1, 1, 1};
    const auto argumentAddress = reinterpret_cast<std::uintptr_t>(arguments.data());
    if (indirect) commands.insert(commands.end(), {0xc0021600u, static_cast<std::uint32_t>(argumentAddress), static_cast<std::uint32_t>(argumentAddress >> 32u), 0x8041});
    else commands.insert(commands.end(), {0xc0031500u, 1, 1, 1, 0x8041});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    for (std::uint32_t iteration = 0; iteration < 3; ++iteration) {
        sceAgcDriverSubmitAcb(0x20, &packet);
        AgcDriverWaitIdle_nid_postfix();
    }
    std::uint32_t destination = 0;
    const auto destinationAddress = reinterpret_cast<std::uintptr_t>(&destination);
    commands[threadRegisterIndex * 3 + 2] = 2;
    commands.insert(commands.end(), {0xc0033700u, 0x00100200u, static_cast<std::uint32_t>(destinationAddress), static_cast<std::uint32_t>(destinationAddress >> 32u), 7});
    packet = Packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    sceAgcDriverSubmitAcb(0x20, &packet);
    ExpectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "artifact is missing");
    ExpectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "artifact is missing");
    ExpectFailure([] { AgcDriverShutdown_nid_postfix(); }, "artifact is missing");
    Require(destination == 0, "failed dispatch executed a subsequent memory write");
}

void DeferredRegistration() {
    alignas(256) std::array<std::uint32_t, 2> code{0xbe842104u, 0xbf810000u};
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 8> registers{};
        ShaderSpecialRegs specials{};
    } header;
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = sizeof(code);
    header.shader.code = code.data();
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    header.shader.specials = &header.specials;
    header.specials.dispatch_modifier = 0x8000;
    header.registers = {{{0x20c, static_cast<std::uint32_t>(address >> 8u)}, {0x20d, static_cast<std::uint32_t>(address >> 40u)}, {0x207, 1}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 12}, {0x207, 1}}};
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    std::vector<std::uint32_t> commands;
    for (const auto reg : header.registers) commands.insert(commands.end(), {0xc0017600u, reg.offset, reg.value});
    commands.insert(commands.end(), {0xc0031500u, 1, 1, 1, 0x8041});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    sceAgcDriverSubmitAcb(0x20, &packet);
    ExpectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "not statically resolvable");
    ExpectFailure([] { AgcDriverShutdown_nid_postfix(); }, "not statically resolvable");
}

void DeferredUndecodableRegistration() {
    alignas(256) std::array<std::uint32_t, 3> code{0xbe842104u, 0xf1949f05u, 0x00060018u};
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 8> registers{};
        ShaderSpecialRegs specials{};
    } header;
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = sizeof(code);
    header.shader.code = code.data();
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    header.shader.specials = &header.specials;
    header.specials.dispatch_modifier = 0x8000;
    header.registers = {{{0x20c, static_cast<std::uint32_t>(address >> 8u)}, {0x20d, static_cast<std::uint32_t>(address >> 40u)}, {0x207, 1}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 12}, {0x207, 1}}};
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    std::vector<std::uint32_t> commands;
    for (const auto reg : header.registers) commands.insert(commands.end(), {0xc0017600u, reg.offset, reg.value});
    commands.insert(commands.end(), {0xc0031500u, 1, 1, 1, 0x8041});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    sceAgcDriverSubmitAcb(0x20, &packet);
    ExpectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "unsupported MIMG opcode");
    ExpectFailure([] { AgcDriverShutdown_nid_postfix(); }, "unsupported MIMG opcode");
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 1 || (argc == 2 && (std::string_view(argv[1]) == "--indirect" || std::string_view(argv[1]) == "--fail-before-registration" || std::string_view(argv[1]) == "--deferred" || std::string_view(argv[1]) == "--deferred-undecodable")), "invalid test arguments");
        if (argc == 2 && std::string_view(argv[1]) == "--deferred") {
            if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
            DeferredRegistration();
            std::cout << "deferred shader preparation tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--deferred-undecodable") {
            if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
            DeferredUndecodableRegistration();
            std::cout << "deferred undecodable shader tests passed\n";
            return 0;
        }
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Run(*device);
        FailureCapture(*device);
        PendingRegistrationPreparation(*device);
        Require(argc != 2 || std::string_view(argv[1]) != "--fail-before-registration", "injected failure before registration");
        PrepareMultisampledStorage(*device);
        device.reset();
        RegistrationWithoutSpecials();
        Registration(argc == 2);
        std::cout << "prepared shader and transactional registration tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}

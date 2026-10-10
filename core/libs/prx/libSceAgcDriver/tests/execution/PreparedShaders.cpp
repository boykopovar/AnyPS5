#include "VulkanTestDevice.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <barrier>
#include <exception>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace {

const std::string& Mode() {
    static const std::string none;
    const auto& arguments = Testing::Arguments();
    if (arguments.empty()) return none;
    const auto& mode = arguments.front();
    Testing::Require(arguments.size() == 1 && (mode == "indirect" || mode == "fail-before-registration" || mode == "deferred"), "invalid test arguments");
    return mode;
}

void SkipInDeferredMode() {
    if (Mode() == "deferred") Testing::Skip("the --deferred run covers only deferred shader preparation");
}

void SkipBeforeRegistrationFailure() {
    SkipInDeferredMode();
    if (Mode() == "fail-before-registration") Testing::Skip("the --fail-before-registration run stops before this step");
}

template<typename TAction>
void RequireFailure(TAction action, const std::string& expected) {
    std::string failure;
    try {
        action();
    } catch (const std::exception& error) {
        failure = error.what();
    }
    Testing::Require(!failure.empty(), "expected preparation failure containing \"" + expected + "\"");
    Testing::Require(failure.find(expected) != std::string::npos, failure);
}

class DriverShutdownGuard {
public:
    DriverShutdownGuard() = default;
    DriverShutdownGuard(const DriverShutdownGuard&) = delete;
    DriverShutdownGuard& operator=(const DriverShutdownGuard&) = delete;

    ~DriverShutdownGuard() {
        if (dismissed) return;
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception&) {
        }
    }

    void Dismiss() {
        dismissed = true;
    }

private:
    bool dismissed = false;
};

struct PreparedCompute {
    explicit PreparedCompute(AgcDriver::VulkanDevice& device)
        : request{{ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
                  {32, 0, users, ShaderRecompiler::ShaderComputeStageInfo{{1, 1, 1}, 0, {false, false, false}, false, 1, {}}, {}, {}, {}},
                  device.ComputeTarget(32), {0, 0, 0, 128}},
          snapshot{request.shader.codeAddress, 0, 0, {code.begin(), code.end()}, {}} {
        snapshot.header.resize(sizeof(Shader));
    }

    PreparedCompute(const PreparedCompute&) = delete;
    PreparedCompute& operator=(const PreparedCompute&) = delete;

    void Prepare() {
        handle = ShaderRecompiler::PrepareShader(request);
        snapshot.prepared->entries.push_back({0, handle});
    }

    ShaderRecompiler::RecompileRequest RegisteredRequest() const {
        auto registered = request;
        registered.shader.code = snapshot.code;
        registered.useCache = true;
        return registered;
    }

    alignas(256) std::array<std::uint32_t, 1> code{0xbf810000u};
    std::array<std::uint32_t, 4> users{};
    ShaderRecompiler::RecompileRequest request;
    AgcDriver::DriverDetail::ShaderSnapshot snapshot;
    std::shared_ptr<const ShaderRecompiler::SourceHandle> handle;
};

const Testing::Case unregisteredCompute{"UnregisteredCompute_RepeatedInvocations_CacheExactlyOneArtifact", [] {
    SkipInDeferredMode();
    const auto device = RequireVulkanTestDevice();
    PreparedCompute fixture(*device);
    auto request = fixture.request;
    const auto code = request.shader.code;
    for (const bool sourceFirst : {false, true}) {
        AgcDriver::DriverDetail::ShaderSnapshot snapshot{request.shader.codeAddress, 0, 0, {code.begin(), code.end()}, {}};
        request.shader.code = snapshot.code;
        if (sourceFirst) {
            const auto handle = AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request);
            Testing::Require(snapshot.prepared->entries.size() == 1 && snapshot.prepared->entries.front().handle == handle, "unregistered compute source was not cached");
        }
        static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request));
        Testing::RequireEqual(snapshot.prepared->entries.size(), std::size_t{1}, "unregistered compute invocation did not cache exactly one artifact");
        const auto handle = snapshot.prepared->entries.front().handle;
        const auto& artifact = ShaderRecompiler::GetPreparedArtifact(*handle);
        for (const bool useCache : {false, true}) {
            request.useCache = useCache;
            Testing::Require(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request) == handle, "unregistered compute source was prepared again");
            const auto repeated = AgcDriver::DriverDetail::InvocationFor(snapshot, 0, request);
            ShaderRecompiler::SrtRuntime runtime{};
            runtime.userData = request.context.userData;
            const auto capture = repeated.Capture(runtime);
            const auto result = repeated.Materialize(*capture);
            Testing::Require(snapshot.prepared->entries.size() == 1 && result->variantId == artifact.variantId && result->spirv.data() == artifact.spirv.data(), "unregistered compute invocation replaced its cached artifact");
            device->Dispatch(*result, 1, 1, 1);
        }
        device->WaitIdle();
    }
}};

const Testing::Case missingArtifact{"SourceHandle_WithoutPreparedEntry_ReportsMissingArtifact", [] {
    SkipInDeferredMode();
    const auto device = RequireVulkanTestDevice();
    PreparedCompute fixture(*device);
    RequireFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(fixture.snapshot, 0, fixture.request)); }, "artifact is missing");
    fixture.Prepare();
    RequireFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(fixture.snapshot, 1, fixture.request)); }, "artifact is missing");
}};

const Testing::Case changingUserData{"PreparedSource_ChangingUserData_ReusesThePreparedArtifact", [] {
    SkipInDeferredMode();
    const auto device = RequireVulkanTestDevice();
    PreparedCompute fixture(*device);
    fixture.Prepare();
    auto& request = fixture.request;
    const auto& artifact = ShaderRecompiler::GetPreparedArtifact(*fixture.handle);
    for (std::uint32_t value = 0; value < 8; ++value) {
        fixture.users.fill(value);
        request.useCache = value % 2 == 0;
        const auto ready = AgcDriver::DriverDetail::SourceHandleFor(fixture.snapshot, 0, request);
        ShaderRecompiler::SrtRuntime runtime{};
        runtime.userData = fixture.users;
        const auto capture = ShaderRecompiler::CaptureResources(request, runtime, *ready);
        const auto result = ShaderRecompiler::MaterializeShader(request, *capture, *ready);
        Testing::Require(result->variantId == artifact.variantId && result->spirv.data() == artifact.spirv.data(), "invocation replaced the prepared artifact for user data " + std::to_string(value));
        device->Dispatch(*result, 1, 1, 1);
    }
    device->WaitIdle();
}};

const Testing::Case mismatchedInvocations{"Invocation_MismatchedRequests_AreRejected", [] {
    SkipInDeferredMode();
    const auto device = RequireVulkanTestDevice();
    PreparedCompute fixture(*device);
    fixture.Prepare();
    auto registeredRequest = fixture.RegisteredRequest();
    static_cast<void>(AgcDriver::DriverDetail::InvocationFor(fixture.snapshot, 0, registeredRequest));
    RequireFailure([&] { static_cast<void>(AgcDriver::DriverDetail::InvocationFor(fixture.snapshot, 0, fixture.request)); }, "does not refer to registered code");
    RequireFailure([&] { static_cast<void>(AgcDriver::DriverDetail::InvocationFor(fixture.snapshot, 1, registeredRequest)); }, "outside the snapshot");
    registeredRequest.context.compute->numThreads[0] = 2;
    RequireFailure([&] { static_cast<void>(AgcDriver::DriverDetail::InvocationFor(fixture.snapshot, 0, registeredRequest)); }, "artifact is missing");
}};

const Testing::Case materializeCache{"PreparedInvocation_Materialize_CachesOneResultPerRuntimeData", [] {
    SkipInDeferredMode();
    const auto device = RequireVulkanTestDevice();
    PreparedCompute fixture(*device);
    fixture.Prepare();
    const auto invocation = AgcDriver::DriverDetail::InvocationFor(fixture.snapshot, 0, fixture.RegisteredRequest());
    ShaderRecompiler::SrtRuntime preparedRuntime{};
    preparedRuntime.userData = fixture.users;
    const auto preparedCapture = invocation.Capture(preparedRuntime);
    const auto firstResult = invocation.Materialize(*preparedCapture);
    const auto repeatedCapture = invocation.Capture(preparedRuntime);
    Testing::Require(invocation.Materialize(*repeatedCapture) == firstResult, "prepared invocation rebuilt an unchanged materialized result");
    fixture.users[0] += 1;
    const auto changedCapture = invocation.Capture(preparedRuntime);
    const auto changedResult = invocation.Materialize(*changedCapture);
    Testing::Require(changedResult != firstResult && changedResult->spirv.data() == firstResult->spirv.data(), "prepared invocation did not distinguish changed runtime data");
    fixture.users[0] -= 1;
    Testing::Require(invocation.Materialize(*preparedCapture) == firstResult, "prepared invocation evicted the previous runtime data");
}};

const Testing::Case deviceReplacement{"DeviceReplacement_CompatibleTarget_ReusesThePreparedArtifact", [] {
    SkipInDeferredMode();
    const auto device = RequireVulkanTestDevice();
    PreparedCompute fixture(*device);
    fixture.Prepare();
    const auto& artifact = ShaderRecompiler::GetPreparedArtifact(*fixture.handle);
    ShaderRecompiler::SrtRuntime preparedRuntime{};
    preparedRuntime.userData = fixture.users;
    const auto replacement = std::make_unique<AgcDriver::VulkanDevice>();
    Testing::Require(replacement->Serial() != device->Serial(), "replacement device was not created");
    auto replacementRequest = fixture.request;
    replacementRequest.target = replacement->ComputeTarget(32);
    Testing::Require(AgcDriver::DriverDetail::SourceHandleFor(fixture.snapshot, 0, replacementRequest) == fixture.handle, "device replacement discarded a compatible prepared artifact");
    replacementRequest.shader.code = fixture.snapshot.code;
    const auto replacementInvocation = AgcDriver::DriverDetail::InvocationFor(fixture.snapshot, 0, replacementRequest);
    const auto replacementCapture = replacementInvocation.Capture(preparedRuntime);
    const auto replacementResult = replacementInvocation.Materialize(*replacementCapture);
    Testing::Require(replacementResult->spirv.data() == artifact.spirv.data(), "device replacement recompiled the prepared shader");
    replacement->Dispatch(*replacementResult, 1, 1, 1);
    replacement->WaitIdle();
    replacementRequest.target.nonConstantImageOffsets = !replacementRequest.target.nonConstantImageOffsets;
    RequireFailure([&] { static_cast<void>(AgcDriver::DriverDetail::InvocationFor(fixture.snapshot, 0, replacementRequest)); }, "artifact is missing");
}};

const Testing::Case staticAbiMismatch{"PreparedShader_StaticAbiMismatch_IsRejected", [] {
    SkipInDeferredMode();
    const auto device = RequireVulkanTestDevice();
    PreparedCompute fixture(*device);
    fixture.Prepare();
    auto& request = fixture.request;
    const auto& handle = fixture.handle;
    const auto invocation = AgcDriver::DriverDetail::InvocationFor(fixture.snapshot, 0, fixture.RegisteredRequest());
    ShaderRecompiler::SrtRuntime runtime{};
    runtime.userData = fixture.users;
    const auto capture = ShaderRecompiler::CaptureResources(request, runtime, *handle);
    request.context.compute->numThreads[0] = 2;
    RequireFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(fixture.snapshot, 0, request)); }, "artifact is missing");
    RequireFailure([&] { static_cast<void>(ShaderRecompiler::CaptureResources(request, runtime, *handle)); }, "does not match the static ABI");
    RequireFailure([&] { static_cast<void>(ShaderRecompiler::MaterializeShader(request, *capture, *handle)); }, "does not match the static ABI");
    const auto otherHandle = ShaderRecompiler::PrepareShader(request);
    const auto otherCapture = ShaderRecompiler::CaptureResources(request, runtime, *otherHandle);
    RequireFailure([&] { static_cast<void>(invocation.Materialize(*otherCapture)); }, "another prepared shader");
    RequireFailure([&] { static_cast<void>(ShaderRecompiler::MaterializeShader(request, *capture, *otherHandle)); }, "another prepared shader");
}};

const Testing::Case pushConstantsAndCode{"PreparedSource_PushConstantCapacityAndCode_SelectTheArtifact", [] {
    SkipInDeferredMode();
    const auto device = RequireVulkanTestDevice();
    PreparedCompute fixture(*device);
    fixture.Prepare();
    auto& request = fixture.request;
    request.layout.pushConstantSizeBytes = 124;
    Testing::Require(AgcDriver::DriverDetail::SourceHandleFor(fixture.snapshot, 0, request) == fixture.handle, "compatible push constant capacity discarded the prepared artifact");
    request.layout.pushConstantSizeBytes = 126;
    RequireFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(fixture.snapshot, 0, request)); }, "artifact is missing");
    request.layout.pushConstantSizeBytes = 128;
    fixture.code[0] = 0xffffffffu;
    RequireFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(fixture.snapshot, 0, request)); }, "artifact is missing");
}};

const Testing::Case injectedFailure{"InjectedFailure_BeforeRegistration_FailsTheRun", [] {
    if (Mode() != "fail-before-registration") Testing::Skip("the failure is injected only with --fail-before-registration");
    Testing::Fail("injected failure before registration");
}};

using MultisampledCode = std::array<std::uint32_t, 13>;

MultisampledCode MultisampledStorageCode(std::uint32_t variant) {
    MultisampledCode code{0xd7460000u, 0x0401060cu, 0xd7460001u, 0x0405060du, 0x7e04020eu, 0x7e060280u, 0x7e080208u, 0x7e0a0209u, 0x7e0c020au, 0x7e0e020bu, 0xf0200f38u, 0x00000400u, 0xbf810000u};
    if (variant != 0u) {
        code[10] = variant % 2u != 0u ? 0xf0200f38u : 0xf0200f30u;
        if (variant >= 3u) code[10] = variant == 3u ? 0xf03c0138u : 0xf03c0130u;
        code[5] = 0x7e060283u;
        code[4] = 0x7e040282u;
    }
    return code;
}

ShaderRecompiler::RecompileRequest MultisampledStorageRequest(AgcDriver::VulkanDevice& device, const MultisampledCode& code, std::span<const std::uint32_t> users) {
    using namespace ShaderRecompiler;
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x30000u, code, 0, {}};
    request.context.userData = users;
    request.context.waveSize = 32;
    request.context.compute = ShaderComputeStageInfo{{8, 8, 1}, 0, {true, true, false}, false, 2, {}};
    request.target = device.ComputeTarget(32);
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return request;
}

const Testing::Case multisampledStorage{"MultisampledStorage_WritesAndAtomics_AddressTheRequestedSample", [] {
    SkipBeforeRegistrationFailure();
    using namespace ShaderRecompiler;
    const auto device = RequireVulkanTestDevice();
    const std::array<std::uint32_t, 16> users{};
    for (std::uint32_t variant = 0; variant < 5; ++variant) {
        const auto code = MultisampledStorageCode(variant);
        const auto request = MultisampledStorageRequest(*device, code, users);
        const auto handle = PrepareShader(request);
        const auto& artifact = GetPreparedArtifact(*handle);
        const auto label = " (variant " + std::to_string(variant) + ")";
        std::vector<std::uint32_t> samples;
        for (std::size_t offset = 5; offset < artifact.spirv.size(); offset += artifact.spirv[offset] >> 16u) {
            const auto instruction = artifact.spirv[offset];
            if ((instruction & 0xffffu) == spv::OpImageWrite) {
                Testing::Require(variant < 3u && (instruction >> 16u) == 6u && artifact.spirv[offset + 4] == spv::ImageOperandsSampleMask, "MSAA storage write lost its sample operand" + label);
                samples.push_back(artifact.spirv[offset + 5]);
            }
            if ((instruction & 0xffffu) == spv::OpImageTexelPointer) {
                Testing::Require(variant >= 3u && (instruction >> 16u) == 6u, "MSAA atomic lost its sample operand" + label);
                samples.push_back(artifact.spirv[offset + 5]);
            }
        }
        Testing::Require(!samples.empty(), "MSAA storage shader has no image writes or atomics" + label);
        const auto expectedSample = variant == 0u ? 0u : variant % 2u != 0u ? 3u : 2u;
        for (const auto sample : samples) {
            bool found = false;
            for (std::size_t offset = 5; offset < artifact.spirv.size(); offset += artifact.spirv[offset] >> 16u) {
                if ((artifact.spirv[offset] & 0xffffu) != spv::OpConstant || artifact.spirv[offset + 2] != sample) continue;
                Testing::RequireEqual(artifact.spirv[offset + 3], expectedSample, "MSAA storage operation addresses the wrong sample" + label);
                found = true;
            }
            Testing::Require(found, "MSAA sample constant is missing" + label);
        }
    }
}};

const Testing::Case multisampledStorageUnavailable{"MultisampledStorage_WithoutTargetCapability_IsRefused", [] {
    SkipBeforeRegistrationFailure();
    const auto device = RequireVulkanTestDevice();
    const std::array<std::uint32_t, 16> users{};
    const auto code = MultisampledStorageCode(4);
    auto request = MultisampledStorageRequest(*device, code, users);
    std::vector<std::uint32_t> capabilities(request.target.supportedCapabilities.begin(), request.target.supportedCapabilities.end());
    std::erase(capabilities, spv::CapabilityStorageImageMultisample);
    request.target.supportedCapabilities = capabilities;
    RequireFailure([&] { static_cast<void>(ShaderRecompiler::PrepareShader(request)); }, "storage image multisampling is unavailable on the target device");
}};

void RegisterUnsupportedType() {
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

const Testing::Case registration{"Registration_ValidAndInvalidShaders_DispatchOrFailWithoutSideEffects", [] {
    SkipBeforeRegistrationFailure();
    static_cast<void>(RequireVulkanTestDevice());
    const bool indirect = Mode() == "indirect";
    DriverShutdownGuard shutdown;
    RegisterUnsupportedType();
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
    for (auto& pending : registrations) pending.get();
    header.registers[1].value |= 0x100u;
    RequireFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "invalid registered program address");
    header.registers[1].value &= 0xffu;
    ++header.registers[0].value;
    RequireFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "entry point is outside shader code");
    --header.registers[0].value;
    header.shader.num_sh_registers = 255;
    RequireFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "truncated shader metadata");
    header.shader.num_sh_registers = header.registers.size();
    header.registers[threadRegisterIndex].value = 0;
    RequireFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "must be nonzero");
    header.registers[threadRegisterIndex].value = 1;
    code[0] = 0xffffffffu;
    RequireFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "");
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
    RequireFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "artifact is missing");
    RequireFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "artifact is missing");
    shutdown.Dismiss();
    RequireFailure([] { AgcDriverShutdown_nid_postfix(); }, "artifact is missing");
    Testing::RequireEqual(destination, 0u, "failed dispatch executed a subsequent memory write");
}};

const Testing::Case deferredRegistration{"DeferredRegistration_UnresolvableShader_FailsWaitAndShutdown", [] {
    if (Mode() != "deferred") Testing::Skip("deferred shader preparation runs only with --deferred");
    static_cast<void>(RequireVulkanTestDevice());
    DriverShutdownGuard shutdown;
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
    RequireFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "not statically resolvable");
    shutdown.Dismiss();
    RequireFailure([] { AgcDriverShutdown_nid_postfix(); }, "not statically resolvable");
}};

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> arguments(argv, argv + argc);
    for (auto& argument : arguments) {
        if (argument == "--indirect" || argument == "--deferred" || argument == "--fail-before-registration") argument.erase(0, 2);
    }
    std::vector<char*> pointers;
    for (auto& argument : arguments) pointers.push_back(argument.data());
    pointers.push_back(nullptr);
    argv = pointers.data();
    const DriverShutdownGuard shutdown;
    return Testing::Run(argc, argv);
}

#include "VulkanTestDevice.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparationScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "../NarrowConstantStoreFixture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
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
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#define HOST_DUP _dup
#define HOST_DUP2 _dup2
#define HOST_CLOSE _close
#define HOST_FILENO _fileno
#else
#include <unistd.h>
#define HOST_DUP dup
#define HOST_DUP2 dup2
#define HOST_CLOSE close
#define HOST_FILENO fileno
#endif

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
    {
        AgcDriver::DriverDetail::ShaderSnapshot unpredicted{request.shader.codeAddress, 0, 0, {code.begin(), code.end()}, {}};
        unpredicted.header.resize(sizeof(Shader));
        const auto prepared = AgcDriver::DriverDetail::SourceHandleFor(unpredicted, 0, request);
        Require(unpredicted.prepared->entries.size() == 1 && unpredicted.prepared->entries.front().handle == prepared, "a registered shader with no matching artifact was not prepared");
        Require(AgcDriver::DriverDetail::SourceHandleFor(unpredicted, 0, request) == prepared && unpredicted.prepared->entries.size() == 1, "an artifact prepared at a dispatch was not reused");
    }
    AgcDriver::DriverDetail::ShaderSnapshot snapshot{request.shader.codeAddress, 0, 0, {code.begin(), code.end()}, {}};
    snapshot.header.resize(sizeof(Shader));
    const auto handle = ShaderRecompiler::PrepareShader(request);
    snapshot.prepared->entries.push_back({0, handle});
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 1, request)); }, "outside the snapshot");
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
    static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, registeredRequest));
    Require(snapshot.prepared->entries.size() == 2, "a dispatch state that registration did not predict was not prepared");
    static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, registeredRequest));
    Require(snapshot.prepared->entries.size() == 2, "an artifact prepared at a dispatch was not reused");
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
    static_cast<void>(AgcDriver::DriverDetail::InvocationFor(snapshot, 0, replacementRequest));
    Require(snapshot.prepared->entries.size() == 3, "a target that registration did not prepare for was not prepared");
    ShaderRecompiler::SrtRuntime runtime{};
    runtime.userData = users;
    const auto capture = ShaderRecompiler::CaptureResources(request, runtime, *handle);
    request.context.compute->numThreads[0] = 2;
    Require(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request) == snapshot.prepared->entries[1].handle, "a dispatch did not find the artifact prepared for its state");
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
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request)); }, "not dword-aligned");
    request.layout.pushConstantSizeBytes = 128;
    code[0] = 0xffffffffu;
    ExpectFailure([&] { static_cast<void>(AgcDriver::DriverDetail::SourceHandleFor(snapshot, 0, request)); }, "does not refer to registered code");
}

void DumpSelection(AgcDriver::VulkanDevice& device) {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 1> code{0xffffffffu};
    const std::vector<std::byte> header(2048, std::byte{0x5a});
    RecompileRequest request{{ShaderStage::Compute, 0x12345cafeull, code, 0x20000, header},
        {32, 0, {}, ShaderComputeStageInfo{{1, 1, 1}, 0, {}, false, 1, {}}, {}, {}, {}}, device.ComputeTarget(32), {0, 0, 0, 128}};
    const char* previous = std::getenv("APS5_DUMP_SHADERS");
    const std::string saved = previous != nullptr ? previous : "";
    const auto setCapture = [](const char* value) {
#ifdef _WIN32
        Require(_putenv_s("APS5_DUMP_SHADERS", value) == 0, "cannot set shader capture");
#else
        Require(setenv("APS5_DUMP_SHADERS", value, 1) == 0, "cannot set shader capture");
#endif
    };
    const auto written = [](const std::string& name) {
        const bool exists = std::ifstream(name, std::ios::binary).is_open();
        std::remove(name.c_str());
        return exists;
    };
    const std::string excludedPath = "shader_12345cafe_excluded.req";
    const std::string listedPath = "shader_12345cafe_listed.req";
    const std::string otherPath = "shader_feedface_unlisted.req";
    std::remove(excludedPath.c_str());
    std::remove(listedPath.c_str());
    std::remove(otherPath.c_str());
    setCapture("feedface");
    Require(AgcDriver::DriverDetail::Driver::dumpRequest(request.shader.codeAddress, request, "_excluded").empty(), "a program outside the APS5_DUMP_SHADERS list was named as dumped");
    Require(!written(excludedPath), "a program outside the APS5_DUMP_SHADERS list was dumped");
    setCapture("feedface,0x12345CAFE");
    Require(AgcDriver::DriverDetail::Driver::dumpRequest(request.shader.codeAddress, request, "_excluded") == excludedPath, "a program dropped by an earlier list was not dumped once listed");
    Require(written(excludedPath), "a listed program's request was not written after an earlier list dropped it");
    Require(AgcDriver::DriverDetail::Driver::dumpRequest(request.shader.codeAddress, request, "_listed") == listedPath, "a listed program was not dumped");
    Require(written(listedPath), "a listed program's request was not written");
    setCapture("12345cafe,not-an-address");
    Require(AgcDriver::DriverDetail::Driver::dumpRequest(0xfeedfaceull, request, "_unlisted") == otherPath, "a value that is not an address list did not dump every program");
    Require(written(otherPath), "a value that is not an address list did not write the request");
#ifdef _WIN32
    Require(_putenv_s("APS5_DUMP_SHADERS", saved.c_str()) == 0, "cannot restore shader capture");
#else
    Require((previous != nullptr ? setenv("APS5_DUMP_SHADERS", saved.c_str(), 1) : unsetenv("APS5_DUMP_SHADERS")) == 0, "cannot restore shader capture");
#endif
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
    const std::string path = "shader_12345cafe.prepare.req";
    const std::string capturePath = "shader_12345cafe.req";
    std::remove(path.c_str());
    std::remove(capturePath.c_str());
#ifdef _WIN32
    Require(_putenv_s("APS5_DUMP_SHADERS", "") == 0, "cannot disable shader capture");
#else
    Require(unsetenv("APS5_DUMP_SHADERS") == 0, "cannot disable shader capture");
#endif
    std::ostringstream prefix;
    prefix << "shader 0x" << std::hex << request.shader.codeAddress << " stage " << static_cast<unsigned>(request.shader.stage) << ": ";
    const auto diagnosed = failure([&] { static_cast<void>(AgcDriver::DriverDetail::PrepareShaderWithDiagnostics(request)); });
    Require(diagnosed.rfind(prefix.str(), 0) == 0, "diagnosed failure names no shader address and stage");
    Require(diagnosed.substr(prefix.str().size()) == expected, "diagnosed failure changed the reason");
    Require(!std::ifstream(path, std::ios::binary).is_open(), "disabled shader capture created a request");
#ifdef _WIN32
    Require(_putenv_s("APS5_DUMP_SHADERS", "1") == 0, "cannot enable shader capture");
#else
    Require(setenv("APS5_DUMP_SHADERS", "1", 1) == 0, "cannot enable shader capture");
#endif
    const auto actual = failure([&] { static_cast<void>(AgcDriver::DriverDetail::PrepareShaderWithDiagnostics(request)); });
    std::ostringstream dumpedPrefix;
    dumpedPrefix << "shader 0x" << std::hex << request.shader.codeAddress << " stage " << static_cast<unsigned>(request.shader.stage) << " (" << path << "): ";
    Require(actual == dumpedPrefix.str() + expected, "enabled shader capture changed the diagnosed failure");
    Require(!std::ifstream(capturePath, std::ios::binary).is_open(), "a preparation failure was dumped as a dispatch request");
    Require(AgcDriver::DriverDetail::Driver::dumpRequest(request.shader.codeAddress, request) == capturePath, "a dispatch request dump has the wrong name");
    std::ifstream capture(capturePath, std::ios::binary);
    std::ostringstream captureText;
    captureText << capture.rdbuf();
    capture.close();
    std::remove(capturePath.c_str());
    Require(captureText.str() == RequestSerializer{}.Serialize(request), "an earlier preparation failure dump suppressed the dispatch request dump");
    Require(AgcDriver::DriverDetail::Driver::dumpRequest(request.shader.codeAddress, request) == capturePath, "subsequent request dump has the wrong name");
    Require(!std::ifstream(capturePath, std::ios::binary).is_open(), "de-duplicated request dump was re-created on disk");
    const std::filesystem::path retryDirectory = "shader_12345cafe_retry";
    std::filesystem::remove_all(retryDirectory);
    const auto failedPath = AgcDriver::DriverDetail::Driver::dumpRequest(request.shader.codeAddress, request, "_retry/test");
    Require(failedPath.empty(), "uncreatable dump path reported a saved file");
    std::filesystem::create_directories(retryDirectory);
    const auto retriedPath = AgcDriver::DriverDetail::Driver::dumpRequest(request.shader.codeAddress, request, "_retry/test");
    Require(retriedPath == (retryDirectory / "test.req").generic_string(), "retried dump returned a different path");
    std::ifstream retriedFile(retriedPath, std::ios::binary);
    Require(retriedFile.is_open(), "retried dump file was not created");
    std::ostringstream retriedText;
    retriedText << retriedFile.rdbuf();
    retriedFile.close();
    std::filesystem::remove_all(retryDirectory);
    Require(retriedText.str() == RequestSerializer{}.Serialize(request), "retried dump file contents do not match serialized request");
    const char* previousSkipLimit = std::getenv("APS5_SKIP_REQUEST_DUMPS");
    const std::string savedSkipLimit = previousSkipLimit != nullptr ? previousSkipLimit : "";
#ifdef _WIN32
    Require(_putenv_s("APS5_SKIP_REQUEST_DUMPS", "-1") == 0, "cannot set uncapped skip dumps");
#else
    Require(setenv("APS5_SKIP_REQUEST_DUMPS", "-1", 1) == 0, "cannot set uncapped skip dumps");
#endif
    const auto skipLogPath = std::filesystem::temp_directory_path() / ("aps5_skip_report_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
    std::fflush(stderr);
    const int savedStderr = HOST_DUP(HOST_FILENO(stderr));
    Require(savedStderr >= 0, "stderr could not be saved");
#ifdef _WIN32
    Require(_wfreopen(skipLogPath.wstring().c_str(), L"wb", stderr) != nullptr, "stderr could not be redirected");
#else
    Require(std::freopen(skipLogPath.c_str(), "wb", stderr) != nullptr, "stderr could not be redirected");
#endif
    try {
        AgcDriver::DriverDetail::Driver::reportSkip("draw", "test plain skip");
        AgcDriver::DriverDetail::Driver::reportSkip("draw", "test plain skip");
        AgcDriver::DriverDetail::Driver::reportSkip("dispatch", "test request skip\nRecompileRequest:\nshader dump test data");
    } catch (...) {
        std::fflush(stderr);
        HOST_DUP2(savedStderr, HOST_FILENO(stderr));
        HOST_CLOSE(savedStderr);
        std::filesystem::remove(skipLogPath);
        throw;
    }
    std::fflush(stderr);
    HOST_DUP2(savedStderr, HOST_FILENO(stderr));
    HOST_CLOSE(savedStderr);
#ifdef _WIN32
    Require((previousSkipLimit != nullptr ? _putenv_s("APS5_SKIP_REQUEST_DUMPS", savedSkipLimit.c_str()) : _putenv_s("APS5_SKIP_REQUEST_DUMPS", "")) == 0, "cannot restore skip dump limit");
#else
    Require((previousSkipLimit != nullptr ? setenv("APS5_SKIP_REQUEST_DUMPS", savedSkipLimit.c_str(), 1) : unsetenv("APS5_SKIP_REQUEST_DUMPS")) == 0, "cannot restore skip dump limit");
#endif
    std::string capturedSkips;
    {
        std::ifstream skipFile(skipLogPath, std::ios::binary);
        std::ostringstream skipBuffer;
        skipBuffer << skipFile.rdbuf();
        capturedSkips = skipBuffer.str();
    }
    std::filesystem::remove(skipLogPath);
    Require(capturedSkips.find("RecompileRequest dumps uncapped (APS5_SKIP_REQUEST_DUMPS)") != std::string::npos, "uncapped skip dump announcement was not output to stderr");
    Require(capturedSkips.find("[gpu] skipped draw: test plain skip\n") != std::string::npos, "first plain skip was not output to stderr");
    Require(capturedSkips.find("[gpu] skipped dispatch: test request skip\nRecompileRequest:\nshader dump test data\n") != std::string::npos, "uncapped request skip was not output to stderr");
    Require(capturedSkips.find("[gpu] skipped draw: test plain skip\n") == capturedSkips.rfind("[gpu] skipped draw: test plain skip\n"), "duplicate plain skip was output multiple times");
#ifdef _WIN32
    Require(_putenv_s("APS5_DUMP_SHADERS", saved.c_str()) == 0, "cannot restore shader capture");
#else
    Require((previous != nullptr ? setenv("APS5_DUMP_SHADERS", saved.c_str(), 1) : unsetenv("APS5_DUMP_SHADERS")) == 0, "cannot restore shader capture");
#endif
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

void AwaitedRegistrationPreparation() {
    std::atomic<bool> ran{false};
    AgcDriver::DriverDetail::SubmitRegistrationPreparation([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ran.store(true, std::memory_order_release);
    });
    AgcDriver::DriverDetail::AwaitRegistrationPreparation();
    Require(ran.load(std::memory_order_acquire), "a submission did not wait for the registration preparation queued before it");
    std::atomic<bool> slow{false};
    std::atomic<bool> fast{false};
    AgcDriver::DriverDetail::SubmitRegistrationPreparation([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        slow.store(true, std::memory_order_release);
    });
    auto waiter = std::async(std::launch::async, [&] {
        AgcDriver::DriverDetail::AwaitRegistrationPreparation();
        return slow.load(std::memory_order_acquire);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    AgcDriver::DriverDetail::SubmitRegistrationPreparation([&] { fast.store(true, std::memory_order_release); });
    Require(waiter.get(), "a submission stopped waiting for an earlier preparation once a later one finished");
    AgcDriver::DriverDetail::AwaitRegistrationPreparation();
    Require(fast.load(std::memory_order_acquire), "the later preparation did not run");
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

void SharedCodeHeaders() {
    alignas(256) static const std::array<std::uint32_t, 11> code{0x7e0002ffu, 0, 0x7e0202ffu, 0, 0x7e0402ffu, 0, 0x7e0602ffu, 0x3f800000u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 7> registers{};
        ShaderSpecialRegs specials{};
        ShaderUserData users{};
        std::array<ShaderRegister, 1> context{{{0x2d5, 0x2000}}};
    } vertex, mesh;
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    for (auto* header : {&vertex, &mesh}) {
        header->shader.file_header = 0x34333231u;
        header->shader.version = 0x18;
        header->shader.header_size = sizeof(Header);
        header->shader.shader_size = sizeof(code);
        header->shader.code = code.data();
        header->shader.sh_registers = header->registers.data();
        header->shader.num_sh_registers = header->registers.size();
        header->shader.specials = &header->specials;
        header->shader.user_data = &header->users;
        header->specials.dispatch_modifier = 0x8000;
    }
    vertex.shader.cx_registers = vertex.context.data();
    vertex.shader.num_cx_registers = vertex.context.size();
    vertex.shader.type = 2;
    vertex.registers = {{{0xc8, static_cast<std::uint32_t>(address >> 8u)}, {0xc9, static_cast<std::uint32_t>(address >> 40u)}, {0x8b, 0}, {0x8a, 0}, {0xca, 0}, {0xcb, 0}, {0xcc, 0}}};
    mesh.shader.type = 4;
    mesh.registers = vertex.registers;
    const std::array<ShaderRegister, 1> primitive{{{0x242, 4}}};
    AgcDriverRegisterShader_nid_postfix(&vertex.shader);
    AgcDriverRegisterShader_nid_postfix(&mesh.shader);
    AgcDriverResolveShaderAbi_nid_postfix(&vertex.shader, {}, primitive);
    AgcDriverRegisterShader_nid_postfix(&mesh.shader);
    const auto snapshots = [&] {
        const auto registry = AgcDriver::DriverDetail::Driver::Get().RegisteredShaders();
        Require(registry != nullptr, "shared-code headers published no shader registry");
        auto pair = std::pair{AgcDriver::DriverDetail::RegisteredHeader(*registry, &vertex.shader), AgcDriver::DriverDetail::RegisteredHeader(*registry, &mesh.shader)};
        Require(pair.first != nullptr && pair.second != nullptr && pair.first != pair.second, "the shared-code headers were not registered apart");
        return pair;
    };
    const auto [vertexSnapshot, meshSnapshot] = snapshots();
    Require(vertexSnapshot->registeredState != meshSnapshot->registeredState, "a header with other registers shared another header's registered state");
    Shader copy = vertex.shader;
    copy.user_data = nullptr;
    AgcDriverResolveShaderAbi_nid_postfix(&copy, {}, primitive);
    auto moved = vertex.registers;
    copy.sh_registers = moved.data();
    AgcDriverResolveShaderAbi_nid_postfix(&copy, {}, primitive);
    moved[2].value ^= 2u;
    ExpectFailure([&] { AgcDriverResolveShaderAbi_nid_postfix(&copy, {}, primitive); }, "replaced shader header");
    copy.sh_registers = vertex.shader.sh_registers;
    copy.target ^= 1u;
    ExpectFailure([&] { AgcDriverResolveShaderAbi_nid_postfix(&copy, {}, primitive); }, "replaced shader header");
    std::vector<std::uint32_t> commands;
    for (const auto reg : vertex.registers) commands.insert(commands.end(), {0xc0017600u, reg.offset, reg.value});
    for (const auto reg : vertex.context) commands.insert(commands.end(), {0xc0016900u, reg.offset, reg.value});
    commands.insert(commands.end(), {0xc0017600u, 0x8, 0, 0xc0016900u, 0xd, 0x00100010u, 0xc0016900u, 0x204, 0x80000u, 0xc0016900u, 0x1c3, 4, 0xc0017900u, 0x242, 4, 0xc0012d00u, 3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    for (unsigned iteration = 0; iteration < 2; ++iteration) {
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "shared-code draw submission failed");
        AgcDriverWaitIdle_nid_postfix();
    }
    mesh.shader.type = 2;
    mesh.shader.cx_registers = mesh.context.data();
    mesh.shader.num_cx_registers = mesh.context.size();
    AgcDriverRegisterShader_nid_postfix(&mesh.shader);
    const auto [sameVertex, sameMesh] = snapshots();
    Require(sameVertex->registeredState == sameMesh->registeredState && sameMesh->registeredState->context.at(0x2d5) == 0x2000u, "two headers with the same code and registers decoded their registered state twice");
    AgcDriverResolveShaderAbi_nid_postfix(&mesh.shader, {}, primitive);
    AgcDriverResolveShaderAbi_nid_postfix(&vertex.shader, {}, primitive);
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "same-type registered header draw failed");
    AgcDriverWaitIdle_nid_postfix();
    AgcDriverRegisterShader_nid_postfix(&vertex.shader);
    AgcDriverResolveShaderAbi_nid_postfix(&vertex.shader, {}, primitive);
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "re-registered earlier header draw failed");
    AgcDriverWaitIdle_nid_postfix();
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
    alignas(Shader) std::array<std::byte, sizeof(Shader) + 4> padded{};
    auto* unaligned = padded.data() + 4;
    Require(reinterpret_cast<std::uintptr_t>(unaligned) % alignof(Shader) != 0, "shader ABI test header is unexpectedly aligned");
    std::memcpy(unaligned, &copy, sizeof(Shader));
    padded.back() = std::byte{0x7d};
    AgcDriverResolveShaderAbi_nid_postfix(reinterpret_cast<Shader*>(unaligned), {}, {});
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
    AgcDriverWaitIdle_nid_postfix();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::atomic_ref<std::uint32_t>(destination).load() != 7 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    Require(destination == 7, "a dispatch whose state registration did not predict did not run");
    destination = 0;
    alignas(256) static const std::array<std::uint32_t, 1> invalid{0xffffffffu};
    const auto invalidAddress = reinterpret_cast<std::uintptr_t>(invalid.data());
    commands[2] = static_cast<std::uint32_t>(invalidAddress >> 8u);
    commands[5] = static_cast<std::uint32_t>(invalidAddress >> 40u);
    packet = Packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    sceAgcDriverSubmitAcb(0x20, &packet);
    ExpectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "");
    ExpectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "");
    ExpectFailure([] { AgcDriverShutdown_nid_postfix(); }, "");
    Require(destination == 0, "failed dispatch executed a subsequent memory write");
}

void RegisteredVertexWorkgroup(const ShaderRecompiler::SpirvTarget& target, bool registrationOnly = false) {
    if (!registrationOnly && ((target.subgroupSize != 32u && target.subgroupSize != 64u) || std::find(target.supportedCapabilities.begin(), target.supportedCapabilities.end(), spv::CapabilityMeshShadingEXT) == target.supportedCapabilities.end())) {
        std::cout << "registered NGG wave64 preparation not run: requires mesh shading and a 32- or 64-lane host subgroup; subgroup=" << target.subgroupSize << '\n';
        return;
    }
    alignas(256) static constexpr std::array<std::uint32_t, 8> code{
        0xd8340000u, 0u, 0xd7600000u, 0x00017f05u, 0x7e020200u, 0xf80008cfu, 0x04030201u, 0xbf810000u
    };
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 4> registers{};
        std::array<ShaderRegister, 3> context{{{0x1ff, 64}, {0x2ab, 4}, {0x2ce, 0}}};
        ShaderSpecialRegs specials{{0x25b, 0x8040}, {0x2d5, 0x2000}, 0, {}, {}, {0x29b, 2}, {0x260, 0}};
        ShaderUserData userData{};
    } header;
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = sizeof(code);
    header.shader.code = code.data();
    header.shader.type = 2;
    header.shader.user_data = &header.userData;
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    header.shader.cx_registers = header.context.data();
    header.shader.num_cx_registers = header.context.size();
    header.shader.specials = &header.specials;
    header.registers = {{{0xc8, static_cast<std::uint32_t>(address >> 8u)}, {0xc9, static_cast<std::uint32_t>(address >> 40u)}, {0x8a, 0x622c0187}, {0x8b, 5u << 19u}}};
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    if (registrationOnly) {
        header.shader.user_data = nullptr;
        AgcDriverRegisterShader_nid_postfix(&header.shader);
        header.specials.vgt_shader_stages_en.value = 0;
        ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "missing AGC user-data header");
        header.shader.user_data = &header.userData;
        if (target.subgroupSize < 64u && std::getenv("APS5_SYNC_SHADER_PREPARE") != nullptr && std::getenv("APS5_LAZY_SHADER_PREPARE") == nullptr) {
            ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "v_readlane_b32 of lane 63 is outside");
        }
        std::cout << "registered NGG preparation deferral passed; subgroup=" << target.subgroupSize << '\n';
        return;
    }
    const std::array<ShaderRegister, 1> primitive{{{0x242, 4}}};
    AgcDriverResolveShaderAbi_nid_postfix(&header.shader, {}, primitive);
    AgcDriverResolveShaderAbi_nid_postfix(&header.shader, {}, primitive);
    std::cout << "registered NGG wave64 preparation passed; subgroup=" << target.subgroupSize << '\n';
}

void LazyRegistration() {
    using namespace NarrowConstantStoreFixture;
    const char* registrationBudget = std::getenv("APS5_REGISTRATION_PREPARE_COMMIT_GB");
    const bool budgetOnly = std::getenv("APS5_LAZY_SHADER_PREPARE") == nullptr;
    Require(!budgetOnly || (registrationBudget != nullptr && std::string_view(registrationBudget) == "0"), "lazy preparation is not enabled");
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 7> registers{};
        ShaderSpecialRegs specials{};
    } header;
    const auto address = reinterpret_cast<std::uintptr_t>(Code.data());
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = sizeof(Code);
    header.shader.code = Code.data();
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    header.shader.specials = &header.specials;
    header.specials.dispatch_modifier = 0x8000;
    header.registers = {{{0x20c, static_cast<std::uint32_t>(address >> 8u)}, {0x20d, static_cast<std::uint32_t>(address >> 40u)}, {0x207, Threads}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 4}}};
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    if (!budgetOnly) {
        ++header.registers[0].value;
        ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "entry point is outside shader code");
        --header.registers[0].value;
        header.registers[2].value = 0;
        ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "must be nonzero");
        header.registers[2].value = Threads;
        header.shader.num_sh_registers = 255;
        ExpectFailure([&] { AgcDriverRegisterShader_nid_postfix(&header.shader); }, "truncated shader metadata");
        header.shader.num_sh_registers = header.registers.size();
    }
    const auto registered = [&] {
        const auto registry = AgcDriver::DriverDetail::Driver::Get().RegisteredShaders();
        Require(registry != nullptr, "lazy registration published no shader registry");
        auto snapshot = AgcDriver::DriverDetail::RegisteredHeader(*registry, &header.shader);
        Require(snapshot != nullptr, "lazy registration published no snapshot");
        return snapshot;
    };
    const auto lazySnapshot = registered();
    {
        AgcDriver::DriverDetail::ShaderPreparationTransaction transaction;
        const auto& state = transaction.Read(*lazySnapshot);
        Require(state.deferred && state.entries.empty(), "lazy registration prepared the shader before its first dispatch");
    }
    constexpr std::size_t outputBytes = 65536;
#ifdef _WIN32
    auto* allocation = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, outputBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* allocation = static_cast<std::uint8_t*>(std::aligned_alloc(outputBytes, outputBytes));
#endif
    Require(allocation != nullptr, "cannot allocate lazy shader test output");
    const auto release = [](std::uint8_t* block) {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    };
    const std::unique_ptr<std::uint8_t, decltype(release)> ownedOutput(allocation, release);
    const std::span output(allocation, outputBytes);
    GuestAllocations::Mutation().Add(output.data(), output.size(), true, true, true);
    const auto outputAddress = reinterpret_cast<std::uintptr_t>(output.data());
    std::vector<std::uint32_t> commands;
    for (const auto reg : header.registers) commands.insert(commands.end(), {0xc0017600u, reg.offset, reg.value});
    commands.insert(commands.end(), {0xc0027600u, 0x240, static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>(outputAddress >> 32u), 0xc0031500u, 1, 1, 1, 0x8041});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    for (unsigned iteration = 0; iteration < 2; ++iteration) {
        std::ranges::fill(output, Fill);
        sceAgcDriverSubmitAcb(0x20, &packet);
        AgcDriverWaitIdle_nid_postfix();
        AgcDriverSuspendPoint_nid_postfix();
        AgcDriverWaitIdle_nid_postfix();
        for (std::size_t offset = 0; offset < output.size(); ++offset) {
            const auto expected = offset < Threads * LaneBytes ? ExpectedLane[offset % LaneBytes] : Fill;
            if (output[offset] != expected) throw std::runtime_error("lazy dispatch " + std::to_string(iteration) + " byte " + std::to_string(offset) + " is " + std::to_string(output[offset]) + ", expected " + std::to_string(expected));
        }
    }
    {
        AgcDriver::DriverDetail::ShaderPreparationTransaction transaction;
        Require(!transaction.Read(*lazySnapshot).entries.empty(), "the first dispatch did not prepare the lazily registered shader");
    }
    AgcDriverResolveShaderAbi_nid_postfix(&header.shader, {}, {});
    alignas(256) static const std::array<std::uint32_t, 1> invalid{0xbf9f0000u};
    const auto invalidAddress = reinterpret_cast<std::uintptr_t>(invalid.data());
    header.shader.code = invalid.data();
    header.shader.shader_size = sizeof(invalid);
    header.registers[0].value = static_cast<std::uint32_t>(invalidAddress >> 8u);
    header.registers[1].value = static_cast<std::uint32_t>(invalidAddress >> 40u);
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    AgcDriverResolveShaderAbi_nid_postfix(&header.shader, {}, {});
    {
        AgcDriver::DriverDetail::ShaderPreparationTransaction transaction;
        const auto& state = transaction.Read(*registered());
        Require(state.deferred && state.entries.empty(), "the pipeline ABI of a lazily registered shader it cannot prepare did not leave it to its dispatch");
    }
    commands[2] = header.registers[0].value;
    commands[5] = header.registers[1].value;
    sceAgcDriverSubmitAcb(0x20, &packet);
    ExpectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "s_code_end");
    ExpectFailure([] { AgcDriverShutdown_nid_postfix(); }, "s_code_end");
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

class StderrCapture {
public:
    StderrCapture() {
        AgcDriver::FlushAsyncLog_nid_no_patch();
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        _dup2(_fileno(file), _fileno(stderr));
#else
        saved = dup(fileno(stderr));
        dup2(fileno(file), fileno(stderr));
#endif
    }
    std::string Finish() {
        AgcDriver::FlushAsyncLog_nid_no_patch();
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t read = 0; (read = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, read);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

template<typename TAction>
std::string Logged(TAction action) {
    StderrCapture capture;
    try {
        action();
    } catch (...) {
        static_cast<void>(capture.Finish());
        throw;
    }
    return capture.Finish();
}

std::size_t Deferrals(const std::string& log) {
    constexpr std::string_view line = "could not be prepared when the title created its pipeline";
    std::size_t count = 0;
    for (auto found = log.find(line); found != std::string::npos; found = log.find(line, found + line.size())) ++count;
    return count;
}

struct AbiHeader {
    Shader shader{};
    std::array<ShaderRegister, 8> registers{};
    std::array<ShaderRegister, 8> context{};
    ShaderSpecialRegs specials{};
    ShaderUserData users{};
};

std::uint32_t ProgramLow(const void* code) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code) >> 8u);
}

std::uint32_t ProgramHigh(const void* code) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code) >> 40u);
}

template<std::size_t TCount>
void DescribeAbi(AbiHeader& header, const std::array<std::uint32_t, TCount>& code, std::uint8_t type, std::initializer_list<ShaderRegister> registers, std::initializer_list<ShaderRegister> context) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(AbiHeader);
    header.shader.shader_size = sizeof(code);
    header.shader.code = code.data();
    header.shader.type = type;
    header.shader.user_data = &header.users;
    std::copy(registers.begin(), registers.end(), header.registers.begin());
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = static_cast<std::uint8_t>(registers.size());
    std::copy(context.begin(), context.end(), header.context.begin());
    header.shader.cx_registers = header.context.data();
    header.shader.num_cx_registers = static_cast<std::uint8_t>(context.size());
}

alignas(256) constexpr std::array<std::uint32_t, 64> UserDataCaller{
    0x7e020280u, 0xbe940383u, 0xbe8e0300u, 0xbe8f0301u, 0xbe8e210eu, 0xbe8e0302u, 0xbe8f0303u, 0xbe8e210eu,
    0x80948114u, 0xbf078014u, 0xbf85fff7u, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u, 0xbf9f0000u,
    0x30306c73u, 0x00000040u, 0x00000008u};
alignas(256) constexpr std::array<std::uint32_t, 2> AddFiveCallee{0x4a020285u, 0xbe80200eu};
alignas(256) constexpr std::array<std::uint32_t, 2> AddSixtyFourCallee{0x4a0202c0u, 0xbe80200eu};
alignas(256) std::array<std::uint32_t, 64> CallerOutput{};
alignas(256) constexpr std::array<std::uint32_t, 64> WaterfallStoreCode{
    0x7e020280u, 0x7e040280u, 0x7e080280u, 0x7e0a0280u, 0x7e0c0280u, 0x7e0e0280u, 0xbe86047eu, 0xf40c0200u,
    0xfa000000u, 0xd4c10000u, 0x00020083u, 0xbf8cc07fu, 0xf0200f08u, 0x00020401u, 0x8afe007eu, 0xbf89fff7u,
    0xbefe0406u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 64> WaterfallPixelCode{
    0xc80a0002u, 0x7e000280u, 0x7e020280u, 0xbe86047eu, 0xf40c0200u, 0xfa000000u, 0xd4c10000u, 0x00020483u,
    0xbf8cc07fu, 0xf0000f08u, 0x00020400u, 0x8afe007eu, 0xbf89fff7u, 0xbefe0406u, 0xbf8c3f70u, 0xf800180fu,
    0x07060504u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 64> WaterfallVertexCode{
    0x7e020280u, 0x7e040280u, 0xbe86037eu, 0xf40c0404u, 0xfa000000u, 0xd4c10008u, 0x00020083u, 0xbf8cc07fu,
    0xf0000f08u, 0x00040401u, 0x8a7e087eu, 0xbf89fff7u, 0xbefe0306u, 0xbf8a0000u, 0xbf8c3f70u, 0xf80008cfu,
    0x07060504u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 64> OtherWaterfallVertexCode = WaterfallVertexCode;
alignas(256) constexpr std::array<std::uint32_t, 64> BarrierFreeWaterfallVertexCode{
    0x7e020280u, 0x7e040280u, 0xbe86037eu, 0xf40c0404u, 0xfa000000u, 0xd4c10008u, 0x00020083u, 0xbf8cc07fu,
    0xf0000f08u, 0x00040401u, 0x8a7e087eu, 0xbf89fff7u, 0xbefe0306u, 0xbf800000u, 0xbf8c3f70u, 0xf80008cfu,
    0x07060504u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 11> FrontCode{0x7e0002ffu, 0, 0x7e0202ffu, 0, 0x7e0402ffu, 0, 0x7e0602ffu, 0x3f800000u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 64> UndecodablePixelCode{0xf1949f05u, 0x00060018u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 64> PassthroughPixelCode{0xc8020002u, 0xf800180fu, 0x00000000u, 0xbf810000u};

void DescribeWaterfallPixel(AbiHeader& pixel) {
    DescribeAbi(pixel, WaterfallPixelCode, 1, {{0x8, ProgramLow(WaterfallPixelCode.data())}, {0x9, ProgramHigh(WaterfallPixelCode.data())}, {0xa, 0}, {0xb, 2u << 1u}}, {{0x1b6, 1}, {0x1b3, 2}, {0x1b4, 2}, {0x191, 0}, {0x1c5, 4}});
}

void DescribeFront(AbiHeader& vertex) {
    DescribeAbi(vertex, FrontCode, 2, {{0xc8, ProgramLow(FrontCode.data())}, {0xc9, ProgramHigh(FrontCode.data())}, {0x8b, 0}, {0x8a, 0}, {0xca, 0}, {0xcb, 0}, {0xcc, 0}}, {{0x2d5, 0x2000}});
    vertex.shader.specials = &vertex.specials;
    vertex.specials.dispatch_modifier = 0x8000;
}

void DescribeWaterfallVertex(AbiHeader& vertex, const std::array<std::uint32_t, 64>& code) {
    DescribeAbi(vertex, code, 2, {{0xc8, ProgramLow(code.data())}, {0xc9, ProgramHigh(code.data())}, {0x8a, 0}, {0x8b, 2u << 1u}}, {{0x1ff, 64}, {0x2ab, 4}, {0x2ce, 0}});
    vertex.shader.specials = &vertex.specials;
    vertex.specials = {{0x25b, 0x8040}, {0x2d5, 0x00402000}, 0, {}, {}, {0x29b, 2}, {0x260, 0}};
}

struct FetchingHeader {
    AbiHeader abi;
    std::array<std::uint16_t, 11> directOffsets{};
    ShaderSemantic semantic{};
};

void DescribeStaticFetch(FetchingHeader& vertex) {
    vertex.abi.shader.header_size = sizeof(FetchingHeader);
    for (auto& reg : vertex.abi.registers) {
        if (reg.offset == 0x8b) reg.value = 4u << 1u;
    }
    vertex.directOffsets.fill(0xffffu);
    vertex.directOffsets[8] = 0;
    vertex.directOffsets[10] = 2;
    vertex.abi.users.direct_resource_offset = vertex.directOffsets.data();
    vertex.abi.users.direct_resource_count = static_cast<std::uint16_t>(vertex.directOffsets.size());
    vertex.semantic.size_in_elements = 4;
    vertex.semantic.static_vb_index = 1;
    vertex.abi.shader.input_semantics = &vertex.semantic;
    vertex.abi.shader.num_input_semantics = 1;
}

void AbiDeferredCompute() {
    static AbiHeader caller;
    DescribeAbi(caller, UserDataCaller, 0, {{0x20c, ProgramLow(UserDataCaller.data())}, {0x20d, ProgramHigh(UserDataCaller.data())}, {0x207, 64}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 8u << 1u}}, {});
    caller.shader.specials = &caller.specials;
    caller.specials.dispatch_modifier = 0x8000u;
    AgcDriverRegisterShader_nid_postfix(&caller.shader);
    const bool lazy = std::getenv("APS5_LAZY_SHADER_PREPARE") != nullptr;
    const auto before = ShaderRecompiler::FrontEndBuilds();
    AgcDriverResolveShaderAbi_nid_postfix(&caller.shader, {}, {});
    const auto attempted = ShaderRecompiler::FrontEndBuilds();
    AgcDriverResolveShaderAbi_nid_postfix(&caller.shader, {}, {});
    if (lazy) Require(attempted == before && ShaderRecompiler::FrontEndBuilds() == attempted, "a compute static ABI resolved under lazy preparation built its front end before its dispatch");
    else Require(attempted > before && ShaderRecompiler::FrontEndBuilds() == attempted, "a compute static ABI deferred to its dispatch was prepared again for the same state");
    const auto first = reinterpret_cast<std::uintptr_t>(AddFiveCallee.data());
    const auto second = reinterpret_cast<std::uintptr_t>(AddSixtyFourCallee.data());
    const auto output = reinterpret_cast<std::uintptr_t>(CallerOutput.data());
    const std::array<std::uint32_t, 8> users{static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(first >> 32u), static_cast<std::uint32_t>(second), static_cast<std::uint32_t>(second >> 32u),
        static_cast<std::uint32_t>(output), static_cast<std::uint32_t>((output >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(CallerOutput)), 0x31016facu};
    std::vector<std::uint32_t> commands;
    for (std::size_t index = 0; index < caller.shader.num_sh_registers; ++index) commands.insert(commands.end(), {0xc0017600u, caller.registers[index].offset, caller.registers[index].value});
    for (std::uint32_t index = 0; index < users.size(); ++index) commands.insert(commands.end(), {0xc0017600u, 0x240u + index, users[index]});
    commands.insert(commands.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
    CallerOutput.fill(0xcafebabeu);
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    sceAgcDriverSubmitAcb(0x20, &packet);
    AgcDriverWaitIdle_nid_postfix();
    AgcDriver::GuestMemory::FlushGpuWrites(output, sizeof(CallerOutput));
    Require(std::ranges::all_of(CallerOutput, [](std::uint32_t lane) { return lane == 3u * (5u + 64u); }), "a compute shader its static ABI deferred did not run its user-data callees at its dispatch");
    AgcDriverShutdown_nid_postfix();
}

void AbiDeferredConcurrent() {
    using namespace NarrowConstantStoreFixture;
    Require(std::getenv("APS5_LAZY_SHADER_PREPARE") != nullptr, "lazy preparation is not enabled");
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 7> registers{};
        ShaderSpecialRegs specials{};
    };
    static Header header;
    const auto address = reinterpret_cast<std::uintptr_t>(Code.data());
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18;
    header.shader.header_size = sizeof(header);
    header.shader.shader_size = sizeof(Code);
    header.shader.code = Code.data();
    header.shader.sh_registers = header.registers.data();
    header.shader.num_sh_registers = header.registers.size();
    header.shader.specials = &header.specials;
    header.specials.dispatch_modifier = 0x8000;
    header.registers = {{{0x20c, static_cast<std::uint32_t>(address >> 8u)}, {0x20d, static_cast<std::uint32_t>(address >> 40u)}, {0x207, Threads}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 4}}};
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    AgcDriverResolveShaderAbi_nid_postfix(&header.shader, {}, {});
    constexpr std::size_t Queues = 4;
    constexpr std::size_t OutputBytes = 65536;
#ifdef _WIN32
    auto* allocation = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, Queues * OutputBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
    auto* allocation = static_cast<std::uint8_t*>(std::aligned_alloc(OutputBytes, Queues * OutputBytes));
#endif
    Require(allocation != nullptr, "cannot allocate the concurrent first use outputs");
    const auto release = [](std::uint8_t* block) {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    };
    const std::unique_ptr<std::uint8_t, decltype(release)> ownedOutputs(allocation, release);
    GuestAllocations::Mutation().Add(allocation, Queues * OutputBytes, true, true, true);
    std::fill_n(allocation, Queues * OutputBytes, Fill);
    std::vector<std::vector<std::uint32_t>> commands(Queues);
    std::vector<Packet> packets;
    for (std::size_t queue = 0; queue < Queues; ++queue) {
        const auto output = reinterpret_cast<std::uintptr_t>(allocation + queue * OutputBytes);
        for (const auto reg : header.registers) commands[queue].insert(commands[queue].end(), {0xc0017600u, reg.offset, reg.value});
        commands[queue].insert(commands[queue].end(), {0xc0027600u, 0x240, static_cast<std::uint32_t>(output), static_cast<std::uint32_t>(output >> 32u), 0xc0031500u, 1, 1, 1, 0x8041});
        packets.push_back({commands[queue].data(), static_cast<std::uint32_t>(commands[queue].size()), 0, {}});
    }
    const auto builds = ShaderRecompiler::FrontEndBuilds();
    const auto uses = AgcDriver::DriverDetail::Driver::DeferredAbiUses();
    std::barrier start(static_cast<std::ptrdiff_t>(Queues));
    std::vector<std::thread> threads;
    for (std::size_t queue = 0; queue < Queues; ++queue) {
        threads.emplace_back([&, queue] {
            start.arrive_and_wait();
            sceAgcDriverSubmitAcb(0x20u + static_cast<std::uint32_t>(queue), &packets[queue]);
        });
    }
    for (auto& thread : threads) thread.join();
    AgcDriverWaitIdle_nid_postfix();
    AgcDriverSuspendPoint_nid_postfix();
    AgcDriverWaitIdle_nid_postfix();
    for (std::size_t queue = 0; queue < Queues; ++queue) {
        const auto* output = allocation + queue * OutputBytes;
        for (std::size_t offset = 0; offset < OutputBytes; ++offset) {
            const auto expected = offset < Threads * LaneBytes ? ExpectedLane[offset % LaneBytes] : Fill;
            if (output[offset] != expected) throw std::runtime_error("concurrent first use: queue " + std::to_string(queue) + " byte " + std::to_string(offset) + " is " + std::to_string(output[offset]) + ", expected " + std::to_string(expected));
        }
    }
    const auto after = AgcDriver::DriverDetail::Driver::DeferredAbiUses();
    const auto built = ShaderRecompiler::FrontEndBuilds() - builds;
    const auto entries = AgcDriver::DriverDetail::Driver::Get().PreparedEntries(&header.shader);
    const auto matched = (after.entries - after.unmatchedEntries) - (uses.entries - uses.unmatchedEntries);
    const auto counts = std::to_string(built) + " front-end builds, " + std::to_string(entries) + " prepared entries, " + std::to_string(after.firstUses - uses.firstUses) + " first uses, " + std::to_string(after.found - uses.found) + " found, " + std::to_string(after.own - uses.own) + " own, " + std::to_string(matched) + " entries used";
    std::cout << "concurrent first use of a deferred ABI: " << counts << '\n';
    Require(after.firstUses == uses.firstUses + 1 && after.found == uses.found + 1 && after.own == uses.own, ("four queues making the first use of one deferred ABI at once did not prepare it once (" + counts + ")").c_str());
    Require(built == after.entries - uses.entries && entries == built, ("four queues making the first use of one deferred ABI at once prepared an artifact besides its preparation (" + counts + ")").c_str());
    Require(matched == 1, ("four queues making the first use of one deferred ABI at once did not all use the same entry of its preparation (" + counts + ")").c_str());
    AgcDriverShutdown_nid_postfix();
}

void AbiDeferredRegistrationFailure() {
    static AbiHeader store;
    DescribeAbi(store, WaterfallStoreCode, 0, {{0x20c, ProgramLow(WaterfallStoreCode.data())}, {0x20d, ProgramHigh(WaterfallStoreCode.data())}, {0x207, 64}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 2u << 1u}}, {});
    store.shader.specials = &store.specials;
    static AbiHeader front;
    DescribeFront(front);
    static AbiHeader pixel;
    DescribeWaterfallPixel(pixel);
    pixel.context[0] = {0x1b6, 0};
    static AbiHeader vertex;
    DescribeWaterfallVertex(vertex, BarrierFreeWaterfallVertexCode);
    static AbiHeader interpolated;
    DescribeWaterfallPixel(interpolated);
    for (auto* header : {&store, &front, &pixel, &vertex, &interpolated}) AgcDriverRegisterShader_nid_postfix(&header->shader);
    const std::array<ShaderRegister, 1> primitive{{{0x242, 4}}};
    const std::array<const Shader*, 2> frontStages{&front.shader, &pixel.shader};
    const std::array<const Shader*, 2> vertexStages{&vertex.shader, &interpolated.shader};
    const auto resolve = [&] {
        AgcDriver::ShaderPreparationScope scope({&front.shader, &pixel.shader, &vertex.shader});
        AgcDriverResolveShaderAbi_nid_postfix(&store.shader, {}, {});
        AgcDriverResolveShaderAbi_nid_postfix(&pixel.shader, {}, {});
        AgcDriverResolveGraphicsAbi_nid_postfix(&front.shader, &pixel.shader, 17);
        AgcDriverResolveGraphicsStagesAbi_nid_postfix(frontStages, {}, primitive);
        AgcDriverResolveGraphicsStagesAbi_nid_postfix(vertexStages, {}, primitive);
        AgcDriverResolveGraphicsAbi_nid_postfix(&vertex.shader, &interpolated.shader, 17);
        scope.Commit();
    };
    auto log = Logged(resolve);
    Require(Deferrals(log) == 3 && log.find("is not a valid runtime value") != std::string::npos, "stages whose registration preparation failed were not deferred once each, with the stored reason, when the title created their pipelines");
    log = Logged(resolve);
    Require(Deferrals(log) == 0, "stages whose registration preparation failed were logged again for the same pipelines");
    std::vector<std::uint32_t> commands;
    for (std::size_t index = 0; index < store.shader.num_sh_registers; ++index) commands.insert(commands.end(), {0xc0017600u, store.registers[index].offset, store.registers[index].value});
    commands.insert(commands.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    sceAgcDriverSubmitAcb(0x20, &packet);
    ExpectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "is not a valid runtime value");
    ExpectFailure([] { AgcDriverShutdown_nid_postfix(); }, "is not a valid runtime value");
}

void AbiDeferredPixel() {
    static AbiHeader pixel;
    DescribeWaterfallPixel(pixel);
    AgcDriverRegisterShader_nid_postfix(&pixel.shader);
    const auto before = ShaderRecompiler::FrontEndBuilds();
    auto log = Logged([&] { AgcDriverResolveShaderAbi_nid_postfix(&pixel.shader, {}, {}); });
    const auto attempted = ShaderRecompiler::FrontEndBuilds();
    log += Logged([&] { AgcDriverResolveShaderAbi_nid_postfix(&pixel.shader, {}, {}); });
    Require(attempted > before && ShaderRecompiler::FrontEndBuilds() == attempted, "a pixel static ABI deferred to its draw was prepared again for the same state");
    Require(Deferrals(log) == 1 && log.find("is not a valid runtime value") != std::string::npos, "a pixel shader its static ABI cannot prepare was not logged once with its reason");
    {
        AgcDriver::ShaderPreparationScope scope({&pixel.shader});
        const std::array<ShaderRegister, 1> interpolant{{{0x191, 0x20}}};
        log = Logged([&] { AgcDriverResolveShaderAbi_nid_postfix(&pixel.shader, interpolant, {}); });
        scope.Commit();
    }
    Require(ShaderRecompiler::FrontEndBuilds() > attempted && Deferrals(log) == 0, "a deferred pixel shader was not prepared once more for another state, or was logged again");
    static AbiHeader front;
    DescribeFront(front);
    AgcDriverRegisterShader_nid_postfix(&front.shader);
    std::vector<std::uint32_t> commands;
    for (const auto* header : {&front, &pixel}) {
        for (std::size_t index = 0; index < header->shader.num_sh_registers; ++index) commands.insert(commands.end(), {0xc0017600u, header->registers[index].offset, header->registers[index].value});
        for (std::size_t index = 0; index < header->shader.num_cx_registers; ++index) commands.insert(commands.end(), {0xc0016900u, header->context[index].offset, header->context[index].value});
    }
    commands.insert(commands.end(), {0xc0016900u, 0x8e, 0xfu, 0xc0016900u, 0x8f, 0xfu, 0xc0016900u, 0xd, 0x00100010u, 0xc0016900u, 0x204, 0x80000u, 0xc0016900u, 0x1c3, 4, 0xc0017900u, 0x242, 4, 0xc0012d00u, 3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw of a deferred pixel shader was not submitted");
    ExpectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "is not a valid runtime value");
    ExpectFailure([&] { AgcDriverResolveShaderAbi_nid_postfix(&pixel.shader, {}, {}); }, "is not a valid runtime value");
    ExpectFailure([] { AgcDriverShutdown_nid_postfix(); }, "is not a valid runtime value");
}

void AbiDeferredStages() {
    static AbiHeader nested;
    static AbiHeader direct;
    DescribeWaterfallVertex(nested, WaterfallVertexCode);
    DescribeWaterfallVertex(direct, OtherWaterfallVertexCode);
    AgcDriverRegisterShader_nid_postfix(&nested.shader);
    AgcDriverRegisterShader_nid_postfix(&direct.shader);
    const std::array<ShaderRegister, 1> primitive{{{0x242, 4}}};
    const auto before = ShaderRecompiler::FrontEndBuilds();
    std::string log;
    {
        AgcDriver::ShaderPreparationScope scope({&nested.shader});
        log = Logged([&] { AgcDriverResolveShaderAbi_nid_postfix(&nested.shader, {}, primitive); });
        scope.Commit();
    }
    const auto nestedAttempt = ShaderRecompiler::FrontEndBuilds();
    Require(nestedAttempt > before && Deferrals(log) == 1, "a vertex shader the static ABI resolves through its graphics stages was not deferred to its draw");
    log = Logged([&] { AgcDriverResolveShaderAbi_nid_postfix(&nested.shader, {}, primitive); });
    Require(ShaderRecompiler::FrontEndBuilds() == nestedAttempt && Deferrals(log) == 0, "a deferred vertex static ABI was prepared or logged again for the same state");
    const std::array<const Shader*, 1> stages{&direct.shader};
    log = Logged([&] { AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, {}, primitive); });
    const auto directAttempt = ShaderRecompiler::FrontEndBuilds();
    Require(directAttempt > nestedAttempt && Deferrals(log) == 1, "a graphics stage its ABI cannot prepare was not deferred to its draw");
    log = Logged([&] { AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, {}, primitive); });
    Require(ShaderRecompiler::FrontEndBuilds() == directAttempt && Deferrals(log) == 0, "a deferred graphics stage ABI was prepared or logged again for the same state");
    AgcDriverShutdown_nid_postfix();
}

void AbiDeferredRectangle() {
    static AbiHeader front;
    static AbiHeader undecodable;
    static AbiHeader pixel;
    DescribeFront(front);
    DescribeAbi(undecodable, UndecodablePixelCode, 1, {{0x8, ProgramLow(UndecodablePixelCode.data())}, {0x9, ProgramHigh(UndecodablePixelCode.data())}, {0xa, 0}, {0xb, 0}}, {});
    DescribeWaterfallPixel(pixel);
    for (auto* header : {&front, &undecodable, &pixel}) AgcDriverRegisterShader_nid_postfix(&header->shader);
    AgcDriverResolveGraphicsAbi_nid_postfix(&front.shader, &undecodable.shader, 17);
    AgcDriverResolveShaderAbi_nid_postfix(&pixel.shader, {}, {});
    {
        AgcDriver::ShaderPreparationScope scope({&front.shader, &pixel.shader});
        AgcDriverResolveGraphicsAbi_nid_postfix(&front.shader, &pixel.shader, 17);
        scope.Commit();
    }
    AgcDriverShutdown_nid_postfix();
}

void AbiDeferredUndecodableState() {
    static AbiHeader front;
    DescribeFront(front);
    front.context[0] = {0x2ce, 3};
    front.specials = {{0x25b, 0x8040}, {0x2d5, 0x2030}, 0x8000, {}, {}, {0x29b, 1}, {0x262, 0}};
    AgcDriverRegisterShader_nid_postfix(&front.shader);
    const std::array<const Shader*, 1> stages{&front.shader};
    const std::array<ShaderRegister, 2> context{front.specials.vgt_shader_stages_en, front.specials.vgt_gs_out_prim_type};
    const std::array<ShaderRegister, 3> primitive{front.specials.ge_cntl, front.specials.ge_user_vgpr_en, ShaderRegister{0x242, 4}};
    const auto before = ShaderRecompiler::FrontEndBuilds();
    auto log = Logged([&] { AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, context, primitive); });
    Require(Deferrals(log) == 1 && log.find("VGT_SHADER_STAGES_EN=0x2030: ") != std::string::npos, "a graphics stage whose pipeline state does not decode was not deferred with the decode's reason");
    log = Logged([&] { AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, context, primitive); });
    Require(ShaderRecompiler::FrontEndBuilds() == before && Deferrals(log) == 0, "a graphics stage deferred for a pipeline state that does not decode was prepared or logged again");
    std::vector<std::uint32_t> commands;
    for (std::size_t index = 0; index < front.shader.num_sh_registers; ++index) commands.insert(commands.end(), {0xc0017600u, front.registers[index].offset, front.registers[index].value});
    commands.insert(commands.end(), {0xc0016900u, 0x2d5, 0x2000u, 0xc0017600u, 0x8, 0, 0xc0016900u, 0xd, 0x00100010u, 0xc0016900u, 0x204, 0x80000u, 0xc0016900u, 0x1c3, 4, 0xc0017900u, 0x242, 4, 0xc0012d00u, 3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    log = Logged([&] {
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw of a graphics stage deferred at its pipeline ABI was not submitted");
        AgcDriverWaitIdle_nid_postfix();
    });
    Require(ShaderRecompiler::FrontEndBuilds() > before && log.find("has no artifact prepared at registration") == std::string::npos, "a graphics stage deferred at its pipeline ABI was not prepared at its first draw, or was reported there as missing a registration artifact");
    AgcDriverShutdown_nid_postfix();
}

void AbiDeferredVertexInputs() {
    static FetchingHeader fetching;
    DescribeWaterfallVertex(fetching.abi, WaterfallVertexCode);
    DescribeStaticFetch(fetching);
    AgcDriverRegisterShader_nid_postfix(&fetching.abi.shader);
    const std::array<const Shader*, 1> stages{&fetching.abi.shader};
    const std::array<ShaderRegister, 1> primitive{{{0x242, 4}}};
    const auto resolve = [&] {
        AgcDriver::ShaderPreparationScope scope({&fetching.abi.shader});
        AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, {}, primitive);
        scope.Commit();
    };
    const auto before = ShaderRecompiler::FrontEndBuilds();
    auto log = Logged(resolve);
    Require(Deferrals(log) == 1 && log.find("statically bound vertex attributes are not implemented") != std::string::npos, "a graphics stage whose vertex inputs its pipeline ABI cannot decode was not deferred with the decode's reason");
    log = Logged(resolve);
    Require(ShaderRecompiler::FrontEndBuilds() == before && Deferrals(log) == 0, "a graphics stage deferred for vertex inputs that do not decode was prepared or logged again");
    alignas(16) static std::array<std::uint32_t, 16> tables{};
    const auto address = reinterpret_cast<std::uintptr_t>(tables.data());
    std::vector<std::uint32_t> commands;
    for (std::size_t index = 0; index < fetching.abi.shader.num_sh_registers; ++index) commands.insert(commands.end(), {0xc0017600u, fetching.abi.registers[index].offset, fetching.abi.registers[index].value});
    for (std::uint32_t index = 0; index < 4; ++index) commands.insert(commands.end(), {0xc0017600u, 0x8cu + index, static_cast<std::uint32_t>(index % 2u == 0u ? address : address >> 32u)});
    for (std::size_t index = 0; index < fetching.abi.shader.num_cx_registers; ++index) commands.insert(commands.end(), {0xc0016900u, fetching.abi.context[index].offset, fetching.abi.context[index].value});
    for (const auto reg : {fetching.abi.specials.vgt_shader_stages_en, fetching.abi.specials.vgt_gs_out_prim_type}) commands.insert(commands.end(), {0xc0016900u, reg.offset, reg.value});
    for (const auto reg : {fetching.abi.specials.ge_cntl, fetching.abi.specials.ge_user_vgpr_en}) commands.insert(commands.end(), {0xc0017900u, reg.offset, reg.value});
    commands.insert(commands.end(), {0xc0017600u, 0x8, 0, 0xc0016900u, 0xd, 0x00100010u, 0xc0016900u, 0x204, 0x80000u, 0xc0016900u, 0x1c3, 4, 0xc0017900u, 0x242, 4, 0xc0012d00u, 3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw of a graphics stage deferred for its vertex inputs was not submitted");
    ExpectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "statically bound vertex attributes are not implemented");
    ExpectFailure([] { AgcDriverShutdown_nid_postfix(); }, "statically bound vertex attributes are not implemented");
}

void AbiDeferredRectanglePairs() {
    static AbiHeader front;
    DescribeFront(front);
    static AbiHeader pixel;
    DescribeAbi(pixel, PassthroughPixelCode, 1, {{0x8, ProgramLow(PassthroughPixelCode.data())}, {0x9, ProgramHigh(PassthroughPixelCode.data())}, {0xa, 0}, {0xb, 0}}, {{0x1b6, 1}, {0x1b3, 2}, {0x1b4, 2}, {0x191, 0x420}, {0x1c5, 4}});
    static AbiHeader unresolved;
    DescribeWaterfallPixel(unresolved);
    for (auto* header : {&front, &pixel, &unresolved}) AgcDriverRegisterShader_nid_postfix(&header->shader);
    const std::array<ShaderRegister, 1> passthrough{pixel.context[3]};
    const auto resolve = [&] {
        AgcDriver::ShaderPreparationScope scope({&front.shader, &pixel.shader});
        AgcDriverResolveGraphicsAbi_nid_postfix(&front.shader, nullptr, 17);
        AgcDriverResolveGraphicsAbi_nid_postfix(&front.shader, &pixel.shader, 17);
        AgcDriverResolveGraphicsAbi_nid_postfix(&front.shader, &unresolved.shader, 17);
        AgcDriverResolveShaderAbi_nid_postfix(&pixel.shader, passthrough, {});
        AgcDriverResolveGraphicsAbi_nid_postfix(&front.shader, &pixel.shader, 17);
        scope.Commit();
    };
    auto log = Logged(resolve);
    Require(Deferrals(log) == 1 && log.find("custom per-vertex interpolation is unsupported") != std::string::npos, "a rectangle pair its pipeline ABI cannot build was not deferred once with the builder's reason");
    log = Logged(resolve);
    Require(Deferrals(log) == 0, "a rectangle pair deferred at its pipeline ABI was logged again");
    const auto draw = [&](bool withPixel) {
        std::vector<std::uint32_t> commands;
        for (const auto* header : {&front, &pixel}) {
            if (header == &pixel && !withPixel) continue;
            for (std::size_t index = 0; index < header->shader.num_sh_registers; ++index) commands.insert(commands.end(), {0xc0017600u, header->registers[index].offset, header->registers[index].value});
            for (std::size_t index = 0; index < header->shader.num_cx_registers; ++index) commands.insert(commands.end(), {0xc0016900u, header->context[index].offset, header->context[index].value});
        }
        if (!withPixel) commands.insert(commands.end(), {0xc0017600u, 0x8, 0});
        commands.insert(commands.end(), {0xc0016900u, 0x8e, 0xfu, 0xc0016900u, 0x8f, 0xfu, 0xc0016900u, 0xd, 0x00100010u, 0xc0016900u, 0x204, 0x80000u, 0xc0016900u, 0x1c3, 4, 0xc0017900u, 0x242, 17, 0xc0012d00u, 3, 2});
        Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
        Require(sceAgcDriverSubmitDcb(&packet) == 0, "a rect-list draw was not submitted");
        AgcDriverWaitIdle_nid_postfix();
    };
    draw(false);
    ExpectFailure([&] { draw(true); }, "custom per-vertex interpolation is unsupported");
    ExpectFailure([] { AgcDriverShutdown_nid_postfix(); }, "custom per-vertex interpolation is unsupported");
}

void CommittedAbis() {
    static AbiHeader front;
    static AbiHeader pixel;
    DescribeFront(front);
    DescribeAbi(pixel, PassthroughPixelCode, 1, {{0x8, ProgramLow(PassthroughPixelCode.data())}, {0x9, ProgramHigh(PassthroughPixelCode.data())}, {0xa, 0}, {0xb, 0}}, {{0x1b6, 1}, {0x1b3, 2}, {0x1b4, 2}, {0x191, 0}, {0x1c5, 4}});
    for (auto* header : {&front, &pixel}) AgcDriverRegisterShader_nid_postfix(&header->shader);
    const std::array<const Shader*, 2> stages{&front.shader, &pixel.shader};
    const std::array<ShaderRegister, 1> primitive{{{0x242, 4}}};
    const std::array<ShaderRegister, 1> interpolant{{{0x191, 0x20}}};
    const auto resolve = [&] {
        AgcDriver::ShaderPreparationScope scope({&front.shader, &pixel.shader});
        AgcDriverResolveShaderAbi_nid_postfix(&pixel.shader, {}, {});
        AgcDriverResolveGraphicsStagesAbi_nid_postfix(stages, {}, primitive);
        AgcDriverResolveGraphicsAbi_nid_postfix(&front.shader, &pixel.shader, 0);
        scope.Commit();
    };
    const auto resolveOther = [&] {
        AgcDriver::ShaderPreparationScope scope({&pixel.shader});
        AgcDriverResolveShaderAbi_nid_postfix(&pixel.shader, interpolant, {});
        scope.Commit();
    };
    resolve();
    const auto registry = AgcDriver::DriverDetail::Driver::Get().RegisteredShaders();
    Require(registry != nullptr, "committed ABI resolution published no shader registry");
    const auto pixelSnapshot = AgcDriver::DriverDetail::RegisteredHeader(*registry, &pixel.shader);
    Require(pixelSnapshot != nullptr, "committed ABI resolution published no pixel snapshot");
    const auto committedAbis = [&] {
        std::lock_guard lock(pixelSnapshot->prepared->mutex);
        return pixelSnapshot->prepared->registeredAbis.size();
    };
    AgcDriver::DriverDetail::AwaitRegistrationPreparation();
    const auto before = ShaderRecompiler::FrontEndBuilds();
    const auto abisBefore = committedAbis();
    bool unblocked = false;
    bool excluded = false;
    std::uint64_t repeatedBuilds = 0;
    std::future<void> repeated;
    std::future<void> other;
    {
        AgcDriver::DriverDetail::ShaderSnapshot unrelated{};
        AgcDriver::DriverDetail::ShaderPreparationTransaction open;
        static_cast<void>(open.Read(unrelated));
        repeated = std::async(std::launch::async, resolve);
        unblocked = repeated.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
        repeatedBuilds = ShaderRecompiler::FrontEndBuilds() - before;
        other = std::async(std::launch::async, resolveOther);
        const auto started = std::chrono::steady_clock::now();
        while (ShaderRecompiler::FrontEndBuilds() == before + repeatedBuilds && std::chrono::steady_clock::now() - started < std::chrono::seconds(10)) std::this_thread::yield();
        excluded = other.wait_for(std::chrono::milliseconds(500)) == std::future_status::timeout && committedAbis() == abisBefore;
        open.Commit();
    }
    repeated.get();
    other.get();
    Require(unblocked && repeatedBuilds == 0, "resolving ABIs the shaders already had waited for another thread's shader preparation, or prepared them again");
    Require(excluded, "a new ABI was published beside a shader preparation that had read prepared state");
}

void RelocatedHeaders() {
    alignas(256) static const std::array<std::uint32_t, 11> code{0x7e0002ffu, 0, 0x7e0202ffu, 0, 0x7e0402ffu, 0, 0x7e0602ffu, 0x3f800000u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 7> registers{};
        ShaderSpecialRegs specials{};
        ShaderUserData users{};
        std::array<ShaderRegister, 1> context{{{0x2d5, 0x2000}}};
    };
    static Header registered;
    const auto address = reinterpret_cast<std::uintptr_t>(code.data());
    const auto fill = [&](Header& header) {
        header.shader.file_header = 0x34333231u;
        header.shader.version = 0x18;
        header.shader.header_size = sizeof(Header);
        header.shader.shader_size = sizeof(code);
        header.shader.code = code.data();
        header.shader.sh_registers = header.registers.data();
        header.shader.num_sh_registers = header.registers.size();
        header.shader.cx_registers = header.context.data();
        header.shader.num_cx_registers = header.context.size();
        header.shader.specials = &header.specials;
        header.shader.user_data = &header.users;
        header.specials.dispatch_modifier = 0x8000;
        header.shader.type = 2;
        header.registers = {{{0xc8, static_cast<std::uint32_t>(address >> 8u)}, {0xc9, static_cast<std::uint32_t>(address >> 40u)}, {0x8b, 0}, {0x8a, 0}, {0xca, 0}, {0xcb, 0}, {0xcc, 0}}};
    };
    fill(registered);
    const std::array<ShaderRegister, 1> primitive{{{0x242, 4}}};
    AgcDriverRegisterShader_nid_postfix(&registered.shader);
    AgcDriverResolveShaderAbi_nid_postfix(&registered.shader, {}, primitive);
    std::vector<std::uint32_t> commands;
    for (const auto reg : registered.registers) commands.insert(commands.end(), {0xc0017600u, reg.offset, reg.value});
    for (const auto reg : registered.context) commands.insert(commands.end(), {0xc0016900u, reg.offset, reg.value});
    commands.insert(commands.end(), {0xc0017600u, 0x8, 0, 0xc0016900u, 0xd, 0x00100010u, 0xc0016900u, 0x204, 0x80000u, 0xc0016900u, 0x1c3, 4, 0xc0017900u, 0x242, 4, 0xc0012d00u, 3, 2});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    // A title that relocates a registered header and its arrays into memory of its own resolves the ABI with the copy,
    // which was never registered. Every array it names holds the registered bytes, so the registered header decodes it.
    static Header relocated;
    fill(relocated);
    AgcDriverResolveShaderAbi_nid_postfix(&relocated.shader, {}, primitive);
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the relocated header's draw was not submitted");
    AgcDriverWaitIdle_nid_postfix();
    // A copy that moves only its user data pointer beside the registered arrays keeps resolving.
    static Header movedUserData;
    movedUserData = registered;
    movedUserData.shader.user_data = &movedUserData.users;
    AgcDriverResolveShaderAbi_nid_postfix(&movedUserData.shader, {}, primitive);
    // A copy whose registers, specials or semantics differ is still a replaced header.
    static Header patched;
    fill(patched);
    patched.registers[2].value ^= 2u;
    ExpectFailure([&] { AgcDriverResolveShaderAbi_nid_postfix(&patched.shader, {}, primitive); }, "replaced shader header");
    static Header otherSpecials;
    fill(otherSpecials);
    otherSpecials.specials.dispatch_modifier = 0x4000;
    ExpectFailure([&] { AgcDriverResolveShaderAbi_nid_postfix(&otherSpecials.shader, {}, primitive); }, "replaced shader header");
    static Header otherContext;
    fill(otherContext);
    otherContext.context[0].value = 0x2001;
    ExpectFailure([&] { AgcDriverResolveShaderAbi_nid_postfix(&otherContext.shader, {}, primitive); }, "replaced shader header");
}

void DispatchAheadPrewarm() {
    alignas(256) std::array<std::uint32_t, 1> pendingCode{0xbf810000u};
    alignas(256) std::array<std::uint32_t, 65> lazyCode{};
    lazyCode[0] = 0xbf810000u;
    lazyCode[64] = 0xbf810000u;
    struct Header {
        Shader shader{};
        std::array<ShaderRegister, 7> registers{};
        ShaderSpecialRegs specials{};
    } pendingHeader, lazyHeader;
    const auto fillHeader = [](Header& header, const void* code, std::size_t codeBytes) {
        const auto address = reinterpret_cast<std::uintptr_t>(code);
        header.shader.file_header = 0x34333231u;
        header.shader.version = 0x18;
        header.shader.header_size = sizeof(Header);
        header.shader.shader_size = static_cast<std::uint32_t>(codeBytes);
        header.shader.code = const_cast<void*>(code);
        header.shader.sh_registers = header.registers.data();
        header.shader.num_sh_registers = header.registers.size();
        header.shader.specials = &header.specials;
        header.specials.dispatch_modifier = 0x8000;
        header.registers = {{{0x20c, static_cast<std::uint32_t>(address >> 8u)}, {0x20d, static_cast<std::uint32_t>(address >> 40u)}, {0x207, 1}, {0x208, 1}, {0x209, 1}, {0x212, 0}, {0x213, 2}}};
    };
    fillHeader(pendingHeader, pendingCode.data(), sizeof(pendingCode));
    fillHeader(lazyHeader, lazyCode.data(), sizeof(lazyCode));
    AgcDriverRegisterShader_nid_postfix(&pendingHeader.shader);
    AgcDriverRegisterShader_nid_postfix(&lazyHeader.shader);
    AgcDriver::DriverDetail::AwaitRegistrationPreparation();
    const auto registry = AgcDriver::DriverDetail::Driver::Get().RegisteredShaders();
    Require(registry != nullptr, "dispatch ahead prewarm test published no shader registry");
    const auto pendingSnapshot = AgcDriver::DriverDetail::RegisteredHeader(*registry, &pendingHeader.shader);
    const auto lazySnapshot = AgcDriver::DriverDetail::RegisteredHeader(*registry, &lazyHeader.shader);
    Require(pendingSnapshot != nullptr && lazySnapshot != nullptr, "dispatch ahead prewarm test published no snapshots");
    {
        std::lock_guard lock(pendingSnapshot->prepared->mutex);
        pendingSnapshot->prepared->entries.clear();
        pendingSnapshot->prepared->demanded.store(false, std::memory_order_release);
        pendingSnapshot->prepared->pending = true;
    }
    {
        std::lock_guard lock(lazySnapshot->prepared->mutex);
        lazySnapshot->prepared->entries.clear();
        lazySnapshot->prepared->pending = false;
    }
    alignas(256) static const std::array<std::uint32_t, 1> invalidCode{0xbf9f0000u};
    const auto invalidAddress = reinterpret_cast<std::uintptr_t>(invalidCode.data());
    const auto lazyOffsetAddress = reinterpret_cast<std::uintptr_t>(lazyCode.data() + 64);
    alignas(16) const std::array<std::uint32_t, 6> indirectPairs{0x20cu, static_cast<std::uint32_t>(lazyOffsetAddress >> 8u), 0x20du, static_cast<std::uint32_t>(lazyOffsetAddress >> 40u), 0x240u, 7u};
    const auto pairsAddress = reinterpret_cast<std::uintptr_t>(indirectPairs.data());
    std::vector<std::uint32_t> commands;
    for (const auto reg : pendingHeader.registers) commands.insert(commands.end(), {0xc0017600u, reg.offset, reg.value});
    commands.insert(commands.end(), {0xc0017600u, 0x20cu, static_cast<std::uint32_t>(invalidAddress >> 8u), 0xc0017600u, 0x20du, static_cast<std::uint32_t>(invalidAddress >> 40u), 0xc0031500u, 1, 1, 1, 0x8041u, 0xffff1000u});
    commands.insert(commands.end(), {0xc0017600u, 0x20cu, pendingHeader.registers[0].value, 0xc0017600u, 0x20du, pendingHeader.registers[1].value, 0xc0031500u, 1, 1, 1, 0x8041u});
    commands.insert(commands.end(), {0xc0036300u, static_cast<std::uint32_t>(pairsAddress), static_cast<std::uint32_t>(pairsAddress >> 32u), 0x80000000u, 3u});
    commands.insert(commands.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "dispatch ahead prewarm submit failed");
    ExpectFailure([] { AgcDriverWaitIdle_nid_postfix(); }, "s_code_end");
    AgcDriver::DriverDetail::AwaitRegistrationPreparation();
    AgcDriver::DriverDetail::AwaitLibraryPrebuilds();
    Require(pendingSnapshot->prepared->demanded.load(std::memory_order_acquire), "dispatch ahead did not prioritize the pending shader");
    {
        std::lock_guard lock(lazySnapshot->prepared->mutex);
        Require(lazySnapshot->prepared->entries.size() == 1 && lazySnapshot->prepared->entries.front().codeOffset == 64 && lazySnapshot->prepared->entries.front().handle != nullptr, "dispatch ahead did not prewarm the lazy shader for its dispatched offset");
    }
    ExpectFailure([] { AgcDriverShutdown_nid_postfix(); }, "s_code_end");
}

}

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        HANDLE diagnosticError = nullptr;
        Require(DuplicateHandle(GetCurrentProcess(), GetStdHandle(STD_ERROR_HANDLE), GetCurrentProcess(), &diagnosticError, 0, FALSE, DUPLICATE_SAME_ACCESS) != 0, "cannot retain the native stderr handle");
        Require(SetStdHandle(STD_ERROR_HANDLE, diagnosticError) != 0, "cannot retain native diagnostics during stderr capture");
#endif
        const std::string_view abiMode = argc == 2 ? argv[1] : "";
        const bool abiDeferred = abiMode == "--abi-deferred-compute" || abiMode == "--abi-deferred-concurrent" || abiMode == "--abi-deferred-pixel" || abiMode == "--abi-deferred-stages" || abiMode == "--abi-deferred-rectangle" || abiMode == "--abi-deferred-undecodable-state" || abiMode == "--abi-deferred-registration-failure" || abiMode == "--abi-deferred-vertex-inputs" || abiMode == "--abi-deferred-rectangle-pairs";
        Require(argc == 1 || abiDeferred || (argc == 2 && (std::string_view(argv[1]) == "--indirect" || std::string_view(argv[1]) == "--fail-before-registration" || std::string_view(argv[1]) == "--deferred" || std::string_view(argv[1]) == "--deferred-undecodable" || std::string_view(argv[1]) == "--registered-workgroup" || std::string_view(argv[1]) == "--dispatch-ahead-prewarm" || std::string_view(argv[1]) == "--lazy")), "invalid test arguments");
        if (abiDeferred) {
            if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
            if (abiMode == "--abi-deferred-compute") AbiDeferredCompute();
            else if (abiMode == "--abi-deferred-concurrent") AbiDeferredConcurrent();
            else if (abiMode == "--abi-deferred-pixel") AbiDeferredPixel();
            else if (abiMode == "--abi-deferred-stages") AbiDeferredStages();
            else if (abiMode == "--abi-deferred-undecodable-state") AbiDeferredUndecodableState();
            else if (abiMode == "--abi-deferred-registration-failure") AbiDeferredRegistrationFailure();
            else if (abiMode == "--abi-deferred-vertex-inputs") AbiDeferredVertexInputs();
            else if (abiMode == "--abi-deferred-rectangle-pairs") AbiDeferredRectanglePairs();
            else AbiDeferredRectangle();
            std::cout << "shader ABI deferral tests passed: " << abiMode << '\n';
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--lazy") {
            if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
            LazyRegistration();
            std::cout << "lazy shader preparation tests passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--registered-workgroup") {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
            RegisteredVertexWorkgroup(device->Target(), true);
            AgcDriverShutdown_nid_postfix();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--dispatch-ahead-prewarm") {
            if (!OpenVulkanTestDevice()) return VulkanTestSkipped;
            DispatchAheadPrewarm();
            std::cout << "dispatch ahead prewarm tests passed\n";
            return 0;
        }
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
        RegisteredVertexWorkgroup(device->Target());
        FailureCapture(*device);
        DumpSelection(*device);
        PendingRegistrationPreparation(*device);
        AwaitedRegistrationPreparation();
        Require(argc != 2 || std::string_view(argv[1]) != "--fail-before-registration", "injected failure before registration");
        PrepareMultisampledStorage(*device);
        device.reset();
        SharedCodeHeaders();
        RelocatedHeaders();
        CommittedAbis();
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

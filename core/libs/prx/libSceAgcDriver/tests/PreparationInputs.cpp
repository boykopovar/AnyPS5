#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

std::string mode;
std::atomic<unsigned> entered{0};

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0, "cannot set test environment");
#else
    Require(value != nullptr ? setenv(name, value, 1) == 0 : unsetenv(name) == 0, "cannot set test environment");
#endif
}

std::vector<std::filesystem::path> Inputs(std::uint64_t address) {
    std::ostringstream prefix;
    prefix << "shader_" << std::hex << address << ".input-";
    std::vector<std::filesystem::path> result;
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        const auto name = entry.path().filename().string();
        if (entry.is_regular_file() && name.starts_with(prefix.str()) && name.ends_with(".prepare.req")) result.push_back(entry.path());
    }
    return result;
}

ShaderRecompiler::DeserializedRequest Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    Require(file.is_open(), "capture cannot be opened");
    const std::string text((std::istreambuf_iterator<char>(file)), {});
    return ShaderRecompiler::RequestSerializer{}.Deserialize(text);
}

bool Matches(const ShaderRecompiler::RecompileRequest& left, const ShaderRecompiler::RecompileRequest& right) {
    if (left.shader.codeAddress != right.shader.codeAddress || left.shader.stage != right.shader.stage ||
        !std::ranges::equal(left.shader.code, right.shader.code) ||
        !std::ranges::equal(left.context.userData, right.context.userData) ||
        left.context.memory.size() != right.context.memory.size()) return false;
    for (std::size_t i = 0; i < left.context.memory.size(); ++i) {
        if (left.context.memory[i].guestAddress != right.context.memory[i].guestAddress ||
            !std::ranges::equal(left.context.memory[i].bytes, right.context.memory[i].bytes)) return false;
    }
    return true;
}

void Invoke(unsigned index, std::uint64_t address = 0x12340000u) {
    const std::array<std::uint32_t, 1> code{0xbf810000u};
    const std::array<std::uint32_t, 2> users{index, index + 100u};
    const std::array<std::byte, 4> bytes{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{0x100000u, bytes}}};
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Compute, address, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userData = users;
    request.context.memory = memory;
    request.context.compute = ShaderRecompiler::ShaderComputeStageInfo{{1, 1, 1}, 0, {}, false, 0, {}};
    request.useCache = false;
    const auto handle = AgcDriver::DriverDetail::PrepareShaderWithDiagnostics(request);
    Require(handle == nullptr, "compiler return value was changed");
}

void WriteFailure() {
    const std::array<std::uint32_t, 1> code{0xbf810000u};
    ShaderRecompiler::RecompileRequest request{};
    request.shader = {ShaderRecompiler::ShaderStage::Compute, 0x12340000u, code, 0, {}};
    request.context.waveSize = 64;
    const std::string name = "shader_12340000.retry.req";
    Require(std::filesystem::create_directory(name), "cannot block capture path");
    Require(AgcDriver::DriverDetail::Driver::dumpRequest(request.shader.codeAddress, request, ".retry").empty(), "failed save reported a filename");
    Require(std::filesystem::remove(name), "cannot unblock capture path");
    Require(AgcDriver::DriverDetail::Driver::dumpRequest(request.shader.codeAddress, request, ".retry") == name, "failed save was not retried");
    Require(Matches(request, Read(name).request), "retried capture changed its input");
    Require(AgcDriver::DriverDetail::Driver::dumpRequest(request.shader.codeAddress, request, ".retry") == name, "successful capture was not retained");
}

struct Directory {
    std::filesystem::path previous = std::filesystem::current_path();
    std::filesystem::path path;
    Directory() {
#ifdef _WIN32
        const auto process = _getpid();
#else
        const auto process = getpid();
#endif
        path = std::filesystem::temp_directory_path() / ("anyps5-prepare-input-" + std::to_string(process) + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(path), "cannot create isolated capture directory");
        std::filesystem::current_path(path);
    }
    ~Directory() {
        std::error_code error;
        std::filesystem::current_path(previous, error);
        auto current = std::filesystem::directory_iterator(path, error);
        const std::filesystem::directory_iterator end;
        while (!error && current != end) {
            std::filesystem::remove(current->path(), error);
            if (!error) current.increment(error);
        }
        std::filesystem::remove(path, error);
    }
};

}

namespace ShaderRecompiler {

std::shared_ptr<const SourceHandle> PrepareShader(const RecompileRequest& request, ShaderPreparationContext*) {
    ++entered;
    const auto inputs = Inputs(request.shader.codeAddress);
    const bool expected = mode != "disabled" && (mode != "selected" || request.shader.codeAddress == 0x12340000u);
    bool found = false;
    for (const auto& path : inputs) {
        try {
            auto saved = Read(path);
            if (Matches(request, saved.request)) found = true;
        } catch (const std::exception&) {}
    }
    const bool blocked = mode == "capture-write-failure" && request.context.userData[0] == 2u;
    Require(found == (expected && !blocked), "capture state at compiler entry is wrong");
    if (mode == "failure") throw std::runtime_error("compiler sentinel");
    return {};
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2, "expected one test mode");
        mode = argv[1];
        Directory directory;
        SetEnvironment("APS5_DUMP_PREPARE_INPUTS", mode == "disabled" ? "" : "1");
        SetEnvironment("APS5_DUMP_SHADERS", mode == "selected" ? "12340000" : mode == "failure" ? "1" : "");
#ifndef _WIN32
        if (mode == "disabled") SetEnvironment("APS5_DUMP_PREPARE_INPUTS", nullptr);
        if (mode != "selected" && mode != "failure") SetEnvironment("APS5_DUMP_SHADERS", nullptr);
#endif
        unsigned expected = 0;
        if (mode == "write-failure") {
            WriteFailure();
        } else if (mode == "capture-write-failure") {
            Invoke(1);
            const auto first = Inputs(0x12340000u);
            Require(first.size() == 1, "first capture missing");
            const auto name = first.front().filename().string();
            const auto separator = name.rfind('-');
            const auto ordinal = std::stoull(name.substr(separator + 1));
            const auto blocked = name.substr(0, separator + 1) + std::to_string(ordinal + 1) + ".prepare.req";
            Require(std::filesystem::create_directory(blocked), "cannot block next capture");
            Invoke(2);
            Require(std::filesystem::remove(blocked), "cannot unblock next capture");
            Invoke(3);
            expected = 2;
        } else if (mode == "failure") {
            try {
                Invoke(1);
                throw std::runtime_error("compiler failure was swallowed");
            } catch (const std::exception& error) {
                Require(std::string(error.what()).find("compiler sentinel") != std::string::npos, "compiler error was changed");
                Require(std::string(error.what()).find("shader_12340000.prepare.req") != std::string::npos, "existing failure dump context was lost");
                Require(Read("shader_12340000.prepare.req").request.context.userData[0] == 1u, "existing failure dump changed");
            }
            expected = 1;
        } else if (mode == "concurrent") {
            std::vector<std::future<void>> futures;
            for (unsigned i = 1; i <= 8; ++i) futures.push_back(std::async(std::launch::async, [i] { Invoke(i); }));
            for (auto& future : futures) future.get();
            expected = 8;
        } else if (mode == "selected") {
            Invoke(1);
            Invoke(2, 0x12340001u);
            expected = 1;
        } else {
            Require(mode == "enabled" || mode == "disabled", "unknown test mode");
            Invoke(1);
            Invoke(2);
            expected = mode == "enabled" ? 2 : 0;
        }
        const auto inputs = Inputs(0x12340000u);
        Require(inputs.size() == expected, "capture count or naming changed");
        std::set<std::uint32_t> ids;
        for (const auto& path : inputs) {
            auto saved = Read(path);
            Require(saved.request.context.userData.size() == 2, "saved user data missing");
            Require(ids.insert(saved.request.context.userData[0]).second, "concurrent capture input was overwritten");
        }
        Require(entered == (mode == "write-failure" ? 0u : mode == "capture-write-failure" ? 3u : mode == "concurrent" ? 8u : mode == "failure" ? 1u : 2u), "compiler invocation count changed");
        std::printf("preparation input %s: passed\n", mode.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "preparation input: %s\n", error.what());
        return 1;
    }
}

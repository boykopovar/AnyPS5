#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void RemoveEnvironmentVariable(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

bool FileExists(const char* path) {
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

std::string ReadFile(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    Check(stream.is_open(), "the trace file was not created at the expected path");
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool WaitForRecord(const char* path, const std::string& record) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (FileExists(path) && ReadFile(path).find(record) != std::string::npos) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

const char* RequiredPathVariable() {
    const char* configured = std::getenv("APS5_CAPTURE_TRACE_PATH");
    Check(configured != nullptr && configured[0] != '\0',
          "this case requires a non-empty APS5_CAPTURE_TRACE_PATH");
    return configured;
}

void PathDefault() {
    RemoveEnvironmentVariable("APS5_CAPTURE_TRACE_PATH");
    Check(std::string(AgcDriver::CaptureTracePath()) == "capture-trace.log",
          "an absent APS5_CAPTURE_TRACE_PATH must select capture-trace.log");
}

void PathEmpty() {
    const char* configured = std::getenv("APS5_CAPTURE_TRACE_PATH");
    Check(configured != nullptr && configured[0] == '\0',
          "this case requires an empty APS5_CAPTURE_TRACE_PATH");
    Check(std::string(AgcDriver::CaptureTracePath()) == "capture-trace.log",
          "an empty APS5_CAPTURE_TRACE_PATH must select capture-trace.log");
}

void PathExplicit() {
    const char* configured = RequiredPathVariable();
    Check(std::string(AgcDriver::CaptureTracePath()) == std::string(configured),
          "a non-empty APS5_CAPTURE_TRACE_PATH must be selected verbatim");
}

void ExplicitWrites() {
    const char* configured = RequiredPathVariable();
    Check(AgcDriver::CaptureTrace::Enabled(), "APS5_CAPTURE_TRACE must enable capture tracing");
    AgcDriver::CaptureTrace::Log("capture-trace-explicit label=%d", 7);
    Check(WaitForRecord(configured, "capture-trace-explicit label=7"),
          "the configured trace path did not receive the capture record");
    std::remove(configured);
}

void DefaultWrites() {
    RemoveEnvironmentVariable("APS5_CAPTURE_TRACE_PATH");
    Check(AgcDriver::CaptureTrace::Enabled(), "APS5_CAPTURE_TRACE must enable capture tracing");
    Check(std::string(AgcDriver::CaptureTracePath()) == "capture-trace.log",
          "an absent APS5_CAPTURE_TRACE_PATH must keep the default path");
    AgcDriver::CaptureTrace::Log("capture-trace-default label=%d", 5);
    Check(WaitForRecord("capture-trace.log", "capture-trace-default label=5"),
          "the default trace path did not receive the capture record");
    std::remove("capture-trace.log");
}

void OpenFailure() {
    const char* configured = RequiredPathVariable();
    Check(!FileExists(configured), "the configured trace path must not already exist");
    Check(AgcDriver::CaptureTrace::Enabled(), "APS5_CAPTURE_TRACE must enable capture tracing");
    bool threw = false;
    try {
        AgcDriver::CaptureTrace::Log("capture-trace-failure label=%d", 9);
    } catch (const std::exception& error) {
        threw = true;
        std::cerr << "unexpected exception: " << error.what() << '\n';
    }
    Check(!threw, "an unopenable trace path must not throw from Log");
    Check(!FileExists(configured), "an unopenable trace path must not create the requested file");
    Check(!FileExists("capture-trace.log"),
          "an unopenable trace path must not fall back to capture-trace.log");
}

void BestEffortOverflow() {
    const char* configured = RequiredPathVariable();
    Check(AgcDriver::CaptureTrace::Enabled(), "APS5_CAPTURE_TRACE must enable capture tracing");
    const std::string oversized(4096, 'x');
    bool threw = false;
    try {
        AgcDriver::CaptureTrace::LogBestEffort("capture-trace-best-effort payload=%s", oversized.c_str());
    } catch (...) {
        threw = true;
    }
    Check(!threw, "best-effort capture logging must not propagate formatting failures");
    std::remove(configured);
}

void Disabled() {
    RemoveEnvironmentVariable("APS5_CAPTURE_TRACE");
    Check(!AgcDriver::CaptureTrace::Enabled(),
          "capture tracing must stay disabled when APS5_CAPTURE_TRACE is absent");
    AgcDriver::CaptureTrace::Log("capture-trace-disabled label=%d", 3);
    Check(!FileExists("capture-trace.log"),
          "disabled tracing must not initialize or write capture-trace.log");
}

}

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected exactly one test mode argument");
        const std::string mode = argv[1];
        if (mode == "path-default") PathDefault();
        else if (mode == "path-empty") PathEmpty();
        else if (mode == "path-explicit") PathExplicit();
        else if (mode == "explicit-writes") ExplicitWrites();
        else if (mode == "default-writes") DefaultWrites();
        else if (mode == "open-failure") OpenFailure();
        else if (mode == "best-effort-overflow") BestEffortOverflow();
        else if (mode == "disabled") Disabled();
        else throw std::runtime_error("unknown capture trace test mode: " + mode);
        std::cout << "capture trace tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

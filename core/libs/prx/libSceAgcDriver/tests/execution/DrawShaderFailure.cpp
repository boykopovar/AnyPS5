#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;

alignas(256) constexpr std::array<std::uint32_t, 2> FailingVertexCode{0xbe800200u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> ValidVertexCode{0x7e000208u, 0x7e020280u, 0x7e040280u, 0x7e0602f2u, 0xf80008cfu, 0x03020100u, 0xbf810000u, 0xbf810000u};
alignas(256) constexpr std::array<std::uint32_t, 8> PixelCode{0x7e000200u, 0x7e020280u, 0xf800180fu, 0x00010100u, 0xbf810000u, 0xbf810000u, 0xbf810000u, 0xbf810000u};
alignas(256) std::array<std::byte, 1024> ColorMemory{};

struct Header {
    Shader shader{};
    ShaderUserData userData{};
};

alignas(8) Header VertexHeader{};
alignas(8) Header PixelHeader{};

const Shader* Registered(Header& header, const std::uint32_t* code, std::size_t bytes, std::uint8_t type) {
    header.shader.file_header = 0x34333231u;
    header.shader.version = 0x18u;
    header.shader.header_size = sizeof(Header);
    header.shader.shader_size = static_cast<std::uint32_t>(bytes);
    header.shader.code = code;
    header.shader.type = type;
    header.shader.user_data = &header.userData;
    AgcDriverRegisterShader_nid_postfix(&header.shader);
    return &header.shader;
}

void Append(std::vector<std::uint32_t>& commands, std::uint32_t opcode, std::initializer_list<std::uint32_t> payload) {
    commands.push_back(0xc0000000u | (static_cast<std::uint32_t>(payload.size() - 1) << 16u) | (opcode << 8u));
    commands.insert(commands.end(), payload);
}

std::vector<std::uint32_t> DrawPacket(const std::uint32_t* vertex, const std::uint32_t* pixel) {
    const auto color = reinterpret_cast<std::uintptr_t>(ColorMemory.data());
    const auto vertexAddress = reinterpret_cast<std::uintptr_t>(vertex);
    const auto pixelAddress = reinterpret_cast<std::uintptr_t>(pixel);
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> context{
        {0x2d5, 0x2000},
        {0x1b6, 0}, {0x207, 0}, {0x200, 0}, {0x203, 0x800},
        {0x2dc, 0xaa00}, {0x2f8, 0}, {0x292, 2}, {0x293, 0},
        {0x80, 0}, {0x8d, 0}, {0x83, 0xffff}, {0x8c, 0xa},
        {0x2f9, 0x2d}, {0x313, 0x6000}, {0x30e, 0xffffffff}, {0x30f, 0xffffffff},
        {0x206, 0x43f}, {0x204, 0x80000}, {0x205, 0x240},
        {0x8e, 0xf}, {0x8f, 0xf}, {0x202, 0xcc0010},
        {0x1c4, 0}, {0x1c5, 9}, {0x1c3, 4}, {0x31c, 0x28028},
        {0x31b, 0}, {0x31d, 0}, {0x3b0, (63u << 14u) | 3u},
        {0x3b8, 0x9000000}, {0x1e0, 0},
        {0xc, 0}, {0xd, 0x40040},
        {0x81, 0x80000000}, {0x82, 0x40040},
        {0x90, 0x80000000}, {0x91, 0x40040},
        {0x94, 0x80000000}, {0x95, 0x40040},
        {0x318, static_cast<std::uint32_t>(color >> 8u)}, {0x390, static_cast<std::uint32_t>(color >> 40u)},
        {0x10f, std::bit_cast<std::uint32_t>(32.0f)}, {0x110, std::bit_cast<std::uint32_t>(32.0f)},
        {0x111, std::bit_cast<std::uint32_t>(-2.0f)}, {0x112, std::bit_cast<std::uint32_t>(2.0f)},
        {0x113, std::bit_cast<std::uint32_t>(1.0f)}, {0x114, 0},
        {0xb4, 0}, {0xb5, std::bit_cast<std::uint32_t>(1.0f)},
    };
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> shader{
        {0xc8, static_cast<std::uint32_t>(vertexAddress >> 8u)}, {0xc9, static_cast<std::uint32_t>(vertexAddress >> 40u)}, {0x8b, 1u << 1u}, {0x8c, 0},
        {0x008, static_cast<std::uint32_t>(pixelAddress >> 8u)}, {0x009, static_cast<std::uint32_t>(pixelAddress >> 40u)}, {0x00b, 1u << 1u}, {0x00c, std::bit_cast<std::uint32_t>(1.0f)},
    };
    std::vector<std::uint32_t> commands;
    Append(commands, 0x79, {0x242, 4});
    for (const auto& [offset, value] : context) Append(commands, 0x69, {offset, value});
    for (const auto& [offset, value] : shader) Append(commands, 0x76, {offset, value});
    Append(commands, 0x2d, {3, 2});
    return commands;
}

std::string ReadTraceFile(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) return {};
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool WaitForTraceRecord(const char* path, const std::string& record) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        if (ReadTraceFile(path).find(record) != std::string::npos) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

std::string TraceLine(const std::string& trace, const std::string& record) {
    const auto position = trace.find(record);
    if (position == std::string::npos) return {};
    const auto end = trace.find('\n', position);
    return trace.substr(position, end == std::string::npos ? std::string::npos : end - position);
}

std::string TraceField(const std::string& line, const char* name) {
    const auto field = line.find(std::string(name) + "=");
    if (field == std::string::npos) return {};
    const auto start = field + std::strlen(name) + 1;
    const auto end = line.find(' ', start);
    return line.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

std::string SubmissionSerialOf(const std::string& trace, const std::string& record) {
    const auto position = trace.find(record);
    if (position == std::string::npos) return {};
    const auto field = trace.find("submission=", position);
    if (field == std::string::npos) return {};
    const auto start = field + std::string("submission=").size();
    const auto end = trace.find(' ', start);
    return trace.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

void SubmitDraw(const std::uint32_t* vertex, const std::uint32_t* pixel) {
    auto commands = DrawPacket(vertex, pixel);
    Packet packet{commands.data(), static_cast<std::uint32_t>(commands.size()), 0, {}};
    Require(sceAgcDriverSubmitDcb(&packet) == 0, "the draw submission was not accepted");
}

void FailingShaderRequestRecordsTheDraw(const char* tracePath, bool correlationExpected = true) {
    Registered(VertexHeader, FailingVertexCode.data(), FailingVertexCode.size() * sizeof(std::uint32_t), 2);
    Registered(PixelHeader, PixelCode.data(), PixelCode.size() * sizeof(std::uint32_t), 1);
    SubmitDraw(FailingVertexCode.data(), PixelCode.data());
    std::string failure;
    try {
        AgcDriverWaitIdle_nid_postfix();
        Require(false, "the failing shader request did not fail the submission");
    } catch (const std::exception& error) {
        failure = error.what();
    }
    Require(failure.find("unsupported SOP1 opcode 2") != std::string::npos, "the submission failure lost the original shader-request error: " + failure);
    if (!correlationExpected) {
        const auto trace = ReadTraceFile(tracePath);
        Require(trace.find("draw-attempt") == std::string::npos, "correlation-disabled failure emitted draw-attempt");
        Require(trace.find("draw-shader-request-failed") == std::string::npos, "correlation-disabled failure emitted draw-shader-request-failed");
        Require(trace.find("draw-materialization-failed") == std::string::npos, "correlation-disabled failure emitted draw-materialization-failed");
        Require(trace.find("draw-submission-failed") == std::string::npos, "correlation-disabled failure emitted draw-submission-failed");
        Require(trace.find("draw-selected") == std::string::npos, "correlation-disabled failure emitted draw-selected");
        Require(trace.find("draw-recorded") == std::string::npos, "correlation-disabled failure emitted draw-recorded");
        return;
    }
    Require(WaitForTraceRecord(tracePath, "draw-shader-request-failed"), "the capture trace received no draw-shader-request-failed record");
    const auto trace = ReadTraceFile(tracePath);
    const auto selected = TraceLine(trace, "draw-selected");
    const auto request = TraceLine(trace, "draw-shader-request-failed");
    const auto attempt = TraceLine(trace, "draw-attempt");
    Require(!selected.empty(), "the failed draw emitted no draw-selected record");
    Require(!request.empty(), "the failed draw emitted no request-failure line");
    Require(!attempt.empty(), "the failed draw emitted no draw-attempt record");
    const auto token = TraceField(selected, "token");
    const auto snapshot = TraceField(selected, "snapshot");
    Require(!token.empty() && token != "0", "the selection record carries no draw token");
    Require(!snapshot.empty() && snapshot != "0x0", "the selection record carries no snapshot identity");
    Require(request.find("token=" + token) != std::string::npos, "the request failure lost the draw token");
    Require(attempt.find("token=" + token) != std::string::npos, "the draw-attempt record lost the draw token");
    Require(request.find("snapshot=" + snapshot) != std::string::npos, "the request failure lost the snapshot identity");
    Require(request.find("offset=" + TraceField(selected, "offset")) != std::string::npos, "the request failure lost the packet offset");
    Require(trace.find("draw-recorded token=" + token) == std::string::npos, "a failed draw emitted draw-recorded");
    const auto requestSerial = SubmissionSerialOf(trace, "draw-shader-request-failed");
    Require(!requestSerial.empty(), "the draw-shader-request-failed record carries no submission serial");
    const auto errorSerial = SubmissionSerialOf(trace, "draw-error");
    Require(requestSerial == errorSerial, "the draw-shader-request-failed record (submission " + requestSerial + ") does not correlate with the draw-error record (submission " + errorSerial + ")");
    Require(trace.find("draw-error submission=" + requestSerial + " offset=" + TraceField(selected, "offset")) != std::string::npos, "the draw-error record lost the selected packet offset");
    char codeAddress[19];
    std::snprintf(codeAddress, sizeof(codeAddress), "%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(FailingVertexCode.data())));
    Require(trace.find(std::string("code=") + codeAddress + " codeOffset=0 stage=1 type=2") != std::string::npos, "the draw-shader-request-failed record does not identify the failing vertex shader");
    Require(trace.find("unsupported SOP1 opcode 2") != std::string::npos, "the draw-shader-request-failed record lost the shader-request reason");
}

void SuccessfulDrawEmitsNoFailureRecord(const char* tracePath) {
    Registered(VertexHeader, ValidVertexCode.data(), ValidVertexCode.size() * sizeof(std::uint32_t), 2);
    Registered(PixelHeader, PixelCode.data(), PixelCode.size() * sizeof(std::uint32_t), 1);
    SubmitDraw(ValidVertexCode.data(), PixelCode.data());
    AgcDriverWaitIdle_nid_postfix();
    Require(WaitForTraceRecord(tracePath, "draw submission="), "the capture trace received no draw submission record");
    const auto trace = ReadTraceFile(tracePath);
    const auto selected = TraceLine(trace, "draw-selected");
    const auto attempt = TraceLine(trace, "draw-attempt");
    Require(!selected.empty(), "the successful draw emitted no draw-selected record");
    Require(!attempt.empty(), "the successful draw emitted no draw-attempt record");
    const auto token = TraceField(selected, "token");
    Require(!token.empty() && token != "0", "the successful selection record carries no draw token");
    Require(attempt.find("token=" + token) != std::string::npos, "the successful draw-attempt record lost the draw token");
    Require(WaitForTraceRecord(tracePath, "draw-recorded token=" + token), "the selected draw emitted no draw-recorded record");
    const auto recorded = TraceLine(ReadTraceFile(tracePath), "draw-recorded token=" + token);
    const auto batch = TraceField(recorded, "batch");
    Require(!batch.empty() && batch != "0", "the draw-recorded record carries no actual batch serial");
    Require(ReadTraceFile(tracePath).find("submit batch=" + batch + " ") != std::string::npos, "the recorded batch was not submitted");
    Require(ReadTraceFile(tracePath).find("submit batch=" + batch + " tokens=[" + token + "] ") != std::string::npos, "the submit record lost the draw token set");
    Require(ReadTraceFile(tracePath).find("submit-confirmed batch=" + batch + " tokens=[" + token + "]") != std::string::npos, "the confirmed submit record lost the draw token set");
    Require(trace.find("draw-shader-request-failed") == std::string::npos, "a successful draw emitted a draw-shader-request-failed record");
}

void DisabledCorrelationTraceLeavesDrawBehaviorUnchanged(const char* tracePath) {
    Registered(VertexHeader, ValidVertexCode.data(), ValidVertexCode.size() * sizeof(std::uint32_t), 2);
    Registered(PixelHeader, PixelCode.data(), PixelCode.size() * sizeof(std::uint32_t), 1);
    SubmitDraw(ValidVertexCode.data(), PixelCode.data());
    AgcDriverWaitIdle_nid_postfix();
    Require(WaitForTraceRecord(tracePath, "draw submission="), "the disabled-trace draw did not complete");
    const auto trace = ReadTraceFile(tracePath);
    Require(trace.find("draw-attempt") == std::string::npos, "the disabled trace emitted draw-attempt");
    Require(trace.find("draw-materialization-failed") == std::string::npos, "the disabled trace emitted draw-materialization-failed");
    Require(trace.find("draw-submission-failed") == std::string::npos, "the disabled trace emitted draw-submission-failed");
    Require(trace.find("draw-selected") == std::string::npos, "the disabled trace emitted draw-selected");
    Require(trace.find("draw-recorded") == std::string::npos, "the disabled trace emitted draw-recorded");
}

}

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "request-failure";
    const bool expectFailure = mode == "request-failure";
    const char* tracePath = std::getenv("APS5_CAPTURE_TRACE_PATH");
    try {
        if (!tracePath || tracePath[0] == '\0') throw std::runtime_error("this case requires a non-empty APS5_CAPTURE_TRACE_PATH");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (mode == "request-failure") FailingShaderRequestRecordsTheDraw(tracePath);
        else if (mode == "request-failure-correlation-disabled") FailingShaderRequestRecordsTheDraw(tracePath, false);
        else if (mode == "successful-draw") SuccessfulDrawEmitsNoFailureRecord(tracePath);
        else if (mode == "disabled-correlation") DisabledCorrelationTraceLeavesDrawBehaviorUnchanged(tracePath);
        else throw std::runtime_error("unknown draw shader failure test mode: " + mode);
        try {
            AgcDriverShutdown_nid_postfix();
            Require(!expectFailure, "the failed draw was not rethrown at driver shutdown");
        } catch (const std::exception&) {
            Require(expectFailure, "driver shutdown failed unexpectedly");
        }
        std::cout << "draw shader failure tests passed\n";
        std::_Exit(0);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (...) {
        }
        std::_Exit(1);
    }
}

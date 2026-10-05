#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <future>
#include <string>

extern "C" {
int APS5_VABI sceAmprCommandBufferConstructor(Apr::CommandBufferObject*);
int APS5_VABI sceAmprAprCommandBufferConstructor(Apr::CommandBufferObject*, std::uint64_t*, std::uint64_t*);
int APS5_VABI sceAmprCommandBufferSetBuffer(Apr::CommandBufferObject*, void*, std::uint32_t);
std::uint32_t APS5_VABI sceAmprCommandBufferGetCurrentOffset(const Apr::CommandBufferObject*);
std::uint32_t APS5_VABI sceAmprCommandBufferGetNumCommands(const Apr::CommandBufferObject*);
int APS5_VABI sceAmprCommandBufferWriteAddressOnCompletion(Apr::CommandBufferObject*, volatile std::uint64_t*, std::uint64_t);
int APS5_VABI sceAmprCommandBufferPushMarker(Apr::CommandBufferObject*, const char*);
int APS5_VABI sceAmprCommandBufferPushMarkerWithColor(Apr::CommandBufferObject*, const char*, std::uint32_t);
int APS5_VABI sceAmprCommandBufferPopMarker(Apr::CommandBufferObject*);
int APS5_VABI sceAmprCommandBufferSetMarker(Apr::CommandBufferObject*, const char*);
int APS5_VABI sceAmprCommandBufferSetMarkerWithColor(Apr::CommandBufferObject*, const char*, const std::uint32_t*);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizePushMarker(const char*);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizePushMarkerWithColor(const char*, std::uint32_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizePopMarker();
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeSetMarker(const char*);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeSetMarkerWithColor(const char*, std::uint32_t);
int APS5_VABI sceKernelAprSubmitCommandBuffer(const Apr::CommandBufferObject*, std::uint32_t);
int APS5_VABI sceAmprCommandBufferWaitOnAddress(Apr::CommandBufferObject*, volatile std::uint64_t*, std::uint64_t, std::uint8_t, std::uint8_t);
int APS5_VABI sceAmprCommandBufferWaitOnCounter(Apr::CommandBufferObject*, std::uint8_t, std::uint32_t, std::uint8_t, std::uint8_t);
int APS5_VABI sceAmprCommandBufferWriteCounterOnCompletion(Apr::CommandBufferObject*, std::uint8_t, std::uint32_t);
int APS5_VABI sceAmprCommandBufferWriteAddressFromTimeCounterOnCompletion(Apr::CommandBufferObject*, volatile std::uint64_t*);
int APS5_VABI sceAmprCommandBufferWriteAddressFromCounterOnCompletion(Apr::CommandBufferObject*, volatile std::uint64_t*, std::uint8_t);
int APS5_VABI sceAmprCommandBufferWriteAddressFromCounterPairOnCompletion(Apr::CommandBufferObject*, volatile std::uint64_t*, std::uint8_t);
int APS5_VABI sceAmprCommandBufferWriteKernelEventQueueOnCompletion(Apr::CommandBufferObject*, std::uint64_t, std::int32_t, std::uint64_t);
}

static void Require(bool value) { if (!value) std::abort(); }

namespace {

constexpr int invalidArgument = static_cast<int>(0x80020016);
constexpr int bufferFull = static_cast<int>(0x8002001C);
constexpr std::uint32_t color = 0xFF8040u;

struct Recorder {
    alignas(8) std::array<std::uint8_t, 4096> memory{};
    Apr::CommandBufferObject buffer{};
    std::uint64_t gatherState = 0;
    std::uint64_t scatterState = 0;

    explicit Recorder(std::uint32_t size = 4096) {
        Require(sceAmprCommandBufferConstructor(&buffer) == 0);
        Require(sceAmprAprCommandBufferConstructor(&buffer, &gatherState, &scatterState) == 0);
        Require(sceAmprCommandBufferSetBuffer(&buffer, memory.data(), size) == 0);
    }

    std::uint32_t Offset() const { return sceAmprCommandBufferGetCurrentOffset(&buffer); }
    std::uint32_t Commands() const { return sceAmprCommandBufferGetNumCommands(&buffer); }
};

void RequireAppended(const Recorder& recorder, std::uint32_t offset, std::uint32_t commands, Apr::Opcode opcode, std::uint64_t measured) {
    Require(recorder.Offset() == offset + measured);
    Require(recorder.Commands() == commands + 1);
    Apr::CommandHeader header;
    std::memcpy(&header, recorder.memory.data() + offset, sizeof(header));
    Require(header.opcode == opcode && header.bytes == measured);
}

void RequireRecorded(const Recorder& recorder, std::uint32_t offset, std::uint32_t commands, Apr::Opcode opcode, std::uint64_t measured, const std::string& text) {
    RequireAppended(recorder, offset, commands, opcode, measured);
    Require(measured >= sizeof(Apr::MarkerCommand) + text.size() + 1);
    Require(std::memcmp(recorder.memory.data() + offset + sizeof(Apr::MarkerCommand), text.c_str(), text.size() + 1) == 0);
}

void TestRecording(const std::string& text) {
    Recorder recorder;
    const char* marker = text.c_str();

    auto offset = recorder.Offset();
    auto commands = recorder.Commands();
    Require(sceAmprCommandBufferPushMarker(&recorder.buffer, marker) == 0);
    RequireRecorded(recorder, offset, commands, Apr::Opcode::PushMarker, sceAmprMeasureCommandSizePushMarker(marker), text);

    offset = recorder.Offset();
    commands = recorder.Commands();
    Require(sceAmprCommandBufferPushMarkerWithColor(&recorder.buffer, marker, color) == 0);
    RequireRecorded(recorder, offset, commands, Apr::Opcode::PushMarker, sceAmprMeasureCommandSizePushMarkerWithColor(marker, color), text);

    offset = recorder.Offset();
    commands = recorder.Commands();
    Require(sceAmprCommandBufferSetMarker(&recorder.buffer, marker) == 0);
    RequireRecorded(recorder, offset, commands, Apr::Opcode::SetMarker, sceAmprMeasureCommandSizeSetMarker(marker), text);

    offset = recorder.Offset();
    commands = recorder.Commands();
    Require(sceAmprCommandBufferSetMarkerWithColor(&recorder.buffer, marker, &color) == 0);
    RequireRecorded(recorder, offset, commands, Apr::Opcode::SetMarker, sceAmprMeasureCommandSizeSetMarkerWithColor(marker, color), text);

    offset = recorder.Offset();
    commands = recorder.Commands();
    Require(sceAmprCommandBufferPopMarker(&recorder.buffer) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::PopMarker, sceAmprMeasureCommandSizePopMarker());
}

void TestRejectedArguments() {
    Recorder recorder;
    Require(sceAmprCommandBufferPushMarker(&recorder.buffer, nullptr) == invalidArgument);
    Require(sceAmprCommandBufferPushMarkerWithColor(&recorder.buffer, nullptr, color) == invalidArgument);
    Require(sceAmprCommandBufferSetMarker(&recorder.buffer, nullptr) == invalidArgument);
    Require(sceAmprCommandBufferSetMarkerWithColor(&recorder.buffer, nullptr, &color) == invalidArgument);
    Require(sceAmprCommandBufferSetMarkerWithColor(&recorder.buffer, "frame", nullptr) == invalidArgument);
    Require(sceAmprCommandBufferPushMarker(nullptr, "frame") == invalidArgument);
    Require(sceAmprCommandBufferPushMarkerWithColor(nullptr, "frame", color) == invalidArgument);
    Require(sceAmprCommandBufferSetMarker(nullptr, "frame") == invalidArgument);
    Require(sceAmprCommandBufferSetMarkerWithColor(nullptr, "frame", &color) == invalidArgument);
    Require(sceAmprCommandBufferPopMarker(nullptr) == invalidArgument);
    Require(recorder.Offset() == 0 && recorder.Commands() == 0);

    const auto rejected = static_cast<std::uint64_t>(static_cast<std::uint32_t>(invalidArgument));
    Require(sceAmprMeasureCommandSizePushMarker(nullptr) == rejected);
    Require(sceAmprMeasureCommandSizePushMarkerWithColor(nullptr, color) == rejected);
    Require(sceAmprMeasureCommandSizeSetMarker(nullptr) == rejected);
    Require(sceAmprMeasureCommandSizeSetMarkerWithColor(nullptr, color) == rejected);
}

void TestFullBuffer() {
    const char* marker = "streaming";
    const auto measured = static_cast<std::uint32_t>(sceAmprMeasureCommandSizePushMarker(marker));
    Recorder exact(measured);
    Require(sceAmprCommandBufferPushMarker(&exact.buffer, marker) == 0);
    Require(exact.Offset() == measured && exact.Commands() == 1);
    Require(sceAmprCommandBufferPopMarker(&exact.buffer) == bufferFull);
    Require(sceAmprCommandBufferSetMarker(&exact.buffer, "") == bufferFull);
    Require(exact.Offset() == measured && exact.Commands() == 1);

    Recorder small(measured - 4);
    Require(sceAmprCommandBufferPushMarker(&small.buffer, marker) == bufferFull);
    Require(sceAmprCommandBufferSetMarkerWithColor(&small.buffer, marker, &color) == bufferFull);
    Require(small.Offset() == 0 && small.Commands() == 0);

    Apr::CommandBufferObject unbound{};
    Require(sceAmprCommandBufferConstructor(&unbound) == 0);
    Require(sceAmprCommandBufferPushMarker(&unbound, marker) == bufferFull);
    Require(sceAmprCommandBufferPopMarker(&unbound) == bufferFull);
}

void TestSubmission() {
    Recorder recorder;
    std::uint64_t first = 0;
    std::uint64_t second = 0;
    Require(sceAmprCommandBufferPushMarker(&recorder.buffer, "level") == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&recorder.buffer, &first, 0x1111) == 0);
    Require(sceAmprCommandBufferSetMarkerWithColor(&recorder.buffer, "textures", &color) == 0);
    Require(sceAmprCommandBufferPushMarkerWithColor(&recorder.buffer, std::string(200, 'm').c_str(), color) == 0);
    Require(sceAmprCommandBufferSetMarker(&recorder.buffer, "") == 0);
    Require(sceAmprCommandBufferPopMarker(&recorder.buffer) == 0);
    Require(sceAmprCommandBufferPopMarker(&recorder.buffer) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&recorder.buffer, &second, 0x2222) == 0);
    Require(recorder.Commands() == 8);
    Require(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0) == 0);
    Require(first == 0x1111 && second == 0x2222);
}

void TestWaits() {
    Recorder recorder;
    alignas(8) std::uint64_t value = 5;
    alignas(8) std::uint64_t done = 0;
    Require(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &value, 5, 0, 0) == 0);
    Require(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &value, 3, 1, 0) == 0);
    Require(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &value, 9, 2, 1) == 0);
    Require(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &value, 4, 3, 0) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&recorder.buffer, &done, 1) == 0);
    auto submitted = std::async(std::launch::async, [&]() { return sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0); });
    Require(submitted.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
    Require(submitted.get() == 0 && done == 1);
}

void TestCounters() {
    Recorder recorder;
    alignas(8) std::uint64_t single = 0;
    alignas(8) std::uint64_t pair = 0;
    Require(sceAmprCommandBufferWriteCounterOnCompletion(&recorder.buffer, 6, 7) == 0);
    Require(sceAmprCommandBufferWriteCounterOnCompletion(&recorder.buffer, 7, 9) == 0);
    Require(sceAmprCommandBufferWaitOnCounter(&recorder.buffer, 6, 7, 0, 0) == 0);
    Require(sceAmprCommandBufferWaitOnCounter(&recorder.buffer, 7, 8, 1, 1) == 0);
    Require(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, &single, 6) == 0);
    Require(sceAmprCommandBufferWriteAddressFromCounterPairOnCompletion(&recorder.buffer, &pair, 6) == 0);
    Require(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0) == 0);
    Require(single == 7 && pair == (7ull | (9ull << 32u)));
}

void TestRejectedWaitsAndCounters() {
    Recorder recorder;
    alignas(8) std::uint64_t words[2] = {};
    auto* misaligned = reinterpret_cast<volatile std::uint64_t*>(reinterpret_cast<std::uint8_t*>(words) + 4);
    Require(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &words[0], 0, 4, 0) == invalidArgument);
    Require(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &words[0], 0, 0, 2) == invalidArgument);
    Require(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, misaligned, 0, 0, 0) == invalidArgument);
    Require(sceAmprCommandBufferWaitOnCounter(&recorder.buffer, 128, 0, 0, 0) == invalidArgument);
    Require(sceAmprCommandBufferWaitOnCounter(&recorder.buffer, 0, 0, 4, 0) == invalidArgument);
    Require(sceAmprCommandBufferWaitOnCounter(&recorder.buffer, 0, 0, 0, 2) == invalidArgument);
    Require(sceAmprCommandBufferWriteCounterOnCompletion(&recorder.buffer, 128, 0) == invalidArgument);
    Require(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, &words[0], 128) == invalidArgument);
    Require(sceAmprCommandBufferWriteAddressFromCounterPairOnCompletion(&recorder.buffer, &words[0], 128) == invalidArgument);
    Require(sceAmprCommandBufferWriteAddressFromCounterPairOnCompletion(&recorder.buffer, &words[0], 7) == invalidArgument);
    Require(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, nullptr, 0) == invalidArgument);
    Require(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, misaligned, 0) == invalidArgument);
    Require(sceAmprCommandBufferWriteAddressFromTimeCounterOnCompletion(&recorder.buffer, nullptr) == invalidArgument);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&recorder.buffer, misaligned, 0) == invalidArgument);
    Require(sceAmprCommandBufferWriteKernelEventQueueOnCompletion(&recorder.buffer, 0, 1, 0) == invalidArgument);
    Require(recorder.Offset() == 0 && recorder.Commands() == 0);
}

}

int main() {
    TestRecording("frame");
    TestRecording("");
    TestRecording("1234567");
    TestRecording("12345678");
    TestRecording(std::string(300, 'a'));
    TestRejectedArguments();
    TestFullBuffer();
    TestSubmission();
    TestWaits();
    TestCounters();
    TestRejectedWaitsAndCounters();
    return 0;
}

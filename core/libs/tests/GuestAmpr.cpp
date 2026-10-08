#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <future>
#include <string>
#include <utility>

extern "C" {
int APS5_VABI sceAmprCommandBufferConstructor(Apr::CommandBufferObject*);
int APS5_VABI sceAmprAprCommandBufferConstructor(Apr::CommandBufferObject*, std::uint64_t*, std::uint64_t*);
int APS5_VABI sceAmprCommandBufferSetBuffer(Apr::CommandBufferObject*, void*, std::uint32_t);
int APS5_VABI sceAmprCommandBufferReset(Apr::CommandBufferObject*);
void* APS5_VABI sceAmprCommandBufferClearBuffer(Apr::CommandBufferObject*);
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
int APS5_VABI sceAmprCommandBufferNop(Apr::CommandBufferObject*, std::uint32_t);
int APS5_VABI sceAmprCommandBufferNopWithData(Apr::CommandBufferObject*, std::uint32_t, const std::uint32_t*);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeNop(std::uint32_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeNopWithData(std::uint32_t);
int APS5_VABI sceAmprCommandBufferWaitOnAddress_04_00(Apr::CommandBufferObject*, volatile std::uint64_t*, std::uint64_t, std::uint8_t, std::uint8_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWaitOnAddress_04_00(volatile std::uint64_t*, std::uint64_t, std::uint8_t, std::uint8_t);
int APS5_VABI sceAmprCommandBufferWriteAddressFromTimeCounter_04_00(Apr::CommandBufferObject*, volatile std::uint64_t*, std::uint64_t);
int APS5_VABI sceAmprCommandBufferWriteAddressFromCounter_04_00(Apr::CommandBufferObject*, volatile std::uint64_t*, std::uint8_t, std::uint64_t);
int APS5_VABI sceAmprCommandBufferWriteAddressFromCounterPair_04_00(Apr::CommandBufferObject*, volatile std::uint64_t*, std::uint8_t, std::uint64_t);
int APS5_VABI sceAmprCommandBufferWriteKernelEventQueue_04_00(Apr::CommandBufferObject*, std::uint64_t, std::int32_t, std::uint64_t, std::uint64_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromTimeCounter_04_00(volatile std::uint64_t*);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromCounter_04_00(volatile std::uint64_t*, std::uint8_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteAddressFromCounterPair_04_00(volatile std::uint64_t*, std::uint8_t);
int APS5_VABI sceAmprCommandBufferWaitOnCounter_04_00(Apr::CommandBufferObject*, std::uint8_t, std::uint8_t, std::uint64_t, std::uint8_t, std::uint8_t, std::uint64_t, std::uint8_t);
int APS5_VABI sceAmprCommandBufferWriteCounter_04_00(Apr::CommandBufferObject*, std::uint8_t, std::uint8_t, std::uint64_t, std::uint8_t, std::uint8_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWaitOnCounter_04_00(std::uint8_t, std::uint8_t, std::uint64_t, std::uint8_t, std::uint8_t, std::uint64_t, std::uint8_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeWriteCounter_04_00(std::uint8_t, std::uint8_t, std::uint64_t, std::uint8_t);
int APS5_VABI sceAmprCommandBufferConstructNop(Apr::CommandBufferObject*, std::int16_t, const void*, std::uint32_t, const std::uint32_t*);
int APS5_VABI sceAmprCommandBufferConstructMarker(Apr::CommandBufferObject*, std::uint32_t, const char*, const std::uint32_t*);
int APS5_VABI sceKernelAprResolveFilepathsToIds(const char**, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceKernelAprResolveFilepathsToIdsAndFileSizes(const char**, std::uint32_t, std::uint32_t*, std::uint64_t*, std::uint32_t*);
int APS5_VABI sceKernelAprResolveFilepathsToIdsAndFileSizesForEach(const char**, std::uint32_t, std::uint32_t*, std::uint64_t*, int*);
int* APS5_VABI __error_nid_postfix();
int APS5_VABI sceAmprAprCommandBufferReadFile(Apr::CommandBufferObject*, std::uint64_t*, std::uint64_t*, std::uint32_t, void*, std::uint64_t, std::uint64_t);
int APS5_VABI sceAmprAprCommandBufferReadFileGather(Apr::CommandBufferObject*, std::uint64_t*, std::uint64_t*, std::uint64_t, std::uint64_t);
int APS5_VABI sceAmprAprCommandBufferReadFileScatter(Apr::CommandBufferObject*, std::uint64_t*, std::uint64_t*, void*, std::uint64_t);
int APS5_VABI sceAmprAprCommandBufferReadFileGatherScatter(Apr::CommandBufferObject*, std::uint64_t*, std::uint64_t*, void*, std::uint64_t, std::uint64_t);
int APS5_VABI sceAmprAprCommandBufferResetGatherScatterState(Apr::CommandBufferObject*);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeReadFileGather(std::uint64_t, std::uint64_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeReadFileScatter(void*, std::uint64_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeReadFileGatherScatter(void*, std::uint64_t, std::uint64_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeResetGatherScatterState();
std::size_t APS5_VABI sceKernelGetDirectMemorySize();
int APS5_VABI sceAmprAmmCommandBufferConstructor(Apr::CommandBufferObject*);
int APS5_VABI sceAmprAmmCommandBufferMap(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t);
int APS5_VABI sceAmprAmmCommandBufferMapWithGpuMaskId(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t, std::uint8_t);
int APS5_VABI sceAmprAmmCommandBufferMapDirect(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t);
int APS5_VABI sceAmprAmmCommandBufferMapDirectWithGpuMaskId(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t, std::uint8_t);
int APS5_VABI sceAmprAmmCommandBufferUnmap(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMap(std::uint64_t, std::uint64_t, std::int32_t, std::int32_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMapWithGpuMaskId(std::uint64_t, std::uint64_t, std::int32_t, std::int32_t, std::uint8_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMapDirect(std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMapDirectWithGpuMaskId(std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t, std::uint8_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeUnmap(std::uint64_t, std::uint64_t);
int APS5_VABI sceAmprAmmGiveDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
int APS5_VABI sceAmprAmmGetVirtualAddressRanges(std::uint64_t*, std::uint64_t*, std::uint64_t*, std::uint64_t*);
int APS5_VABI sceAmprAmmSubmitCommandBuffer(void*, std::uint32_t, std::uint32_t);
int APS5_VABI sceAmprAmmSubmitCommandBuffer2(void*, std::uint32_t, std::uint32_t, std::uint64_t*, std::uint32_t*);
int APS5_VABI sceAmprAmmSubmitCommandBuffer3(void*, std::uint32_t, std::uint32_t, std::uint32_t*);
int APS5_VABI sceAmprAmmWaitCommandBufferCompletion(std::uint32_t);
int APS5_VABI sceAmprAprCommandBufferMapBegin(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t);
int APS5_VABI sceAmprAprCommandBufferMapDirectBegin(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t);
int APS5_VABI sceAmprAprCommandBufferMapEnd(Apr::CommandBufferObject*);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeMapBegin(std::uint64_t, std::uint64_t, std::uint32_t, std::uint32_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeMapDirectBegin(std::uint64_t, std::uint64_t, std::uint64_t, std::uint32_t, std::uint32_t);
std::uint64_t APS5_VABI sceAmprMeasureCommandSizeMapEnd();
int APS5_VABI sceKernelQueryMemoryProtection(void*, void**, void**, int*);
int APS5_VABI sceAmprAmmCommandBufferMapAsPrt(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t);
int APS5_VABI sceAmprAmmCommandBufferAllocatePaForPrt(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t);
int APS5_VABI sceAmprAmmCommandBufferRemapIntoPrt(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::uint32_t);
int APS5_VABI sceAmprAmmCommandBufferUnmapToPrt(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMapAsPrt(std::uint64_t, std::uint64_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeAllocatePaForPrt(std::uint64_t, std::uint64_t, std::int32_t, std::int32_t);
int APS5_VABI sceAmprAmmCommandBufferRemap(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t);
int APS5_VABI sceAmprAmmCommandBufferRemapWithGpuMaskId(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::uint8_t);
int APS5_VABI sceAmprAmmCommandBufferMultiMap(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t);
int APS5_VABI sceAmprAmmCommandBufferMultiMapWithGpuMaskId(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::uint8_t);
int APS5_VABI sceAmprAmmCommandBufferModifyProtect(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t);
int APS5_VABI sceAmprAmmCommandBufferModifyProtectWithGpuMaskId(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t, std::uint8_t);
int APS5_VABI sceAmprAmmCommandBufferModifyMtypeProtect(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t, std::int32_t);
int APS5_VABI sceAmprAmmCommandBufferModifyMtypeProtectWithGpuMaskId(Apr::CommandBufferObject*, std::uint64_t, std::uint64_t, std::int32_t, std::int32_t, std::int32_t, std::uint8_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeRemap(std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeRemapWithGpuMaskId(std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::uint8_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMultiMap(std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeMultiMapWithGpuMaskId(std::uint64_t, std::uint64_t, std::uint64_t, std::int32_t, std::uint8_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeModifyProtect(std::uint64_t, std::uint64_t, std::int32_t, std::int32_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeModifyProtectWithGpuMaskId(std::uint64_t, std::uint64_t, std::int32_t, std::int32_t, std::uint8_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtect(std::uint64_t, std::uint64_t, std::int32_t, std::int32_t, std::int32_t);
std::int64_t APS5_VABI sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtectWithGpuMaskId(std::uint64_t, std::uint64_t, std::int32_t, std::int32_t, std::int32_t, std::uint8_t);
int APS5_VABI sceKernelCreateEqueue(KernelEqueue*, const char*);
int APS5_VABI sceKernelDeleteEqueue(KernelEqueue);
int APS5_VABI sceKernelWaitEqueue(KernelEqueue, KernelEvent*, int, int*, const KernelUseconds*);
int APS5_VABI sceKernelAddAmprEvent(KernelEqueue, int, void*);
int APS5_VABI sceKernelDeleteAmprEvent(KernelEqueue, int);
int APS5_VABI sceKernelGetEventFilter(const KernelEvent*);
std::uintptr_t APS5_VABI sceKernelGetEventId(const KernelEvent*);
std::intptr_t APS5_VABI sceKernelGetEventData(const KernelEvent*);
void* APS5_VABI sceKernelGetEventUserData(const KernelEvent*);
}

static void RequireAt(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "GuestAmpr.cpp:%d: requirement failed\n", line);
    std::abort();
}

#define Require(value) RequireAt((value), __LINE__)

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

void TestKernelEventQueue() {
    KernelEqueue eq = 0;
    int userData = 0;
    Require(sceKernelCreateEqueue(&eq, "ampr") == 0);
    Require(sceKernelAddAmprEvent(eq, 7, &userData) == 0);
    Recorder recorder;
    Require(sceAmprCommandBufferWriteKernelEventQueue_04_00(&recorder.buffer, static_cast<std::uint64_t>(eq), 7, 0x1234, 0) == 0);
    Require(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0) == 0);
    KernelEvent event{};
    int count = 0;
    const KernelUseconds poll = 0;
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &poll) == 0 && count == 1);
    Require(sceKernelGetEventFilter(&event) == -25);
    Require(sceKernelGetEventId(&event) == 7 && sceKernelGetEventData(&event) == 0x1234 && sceKernelGetEventUserData(&event) == &userData);
    Require(sceKernelDeleteAmprEvent(eq, 7) == 0);
    Require(sceKernelDeleteEqueue(eq) == 0);
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

void TestNops() {
    Recorder recorder;
    const std::uint32_t data[3] = {0x11111111u, 0x22222222u, 0x33333333u};
    for (std::uint32_t dwords = 1; dwords <= 16; ++dwords) {
        const auto offset = recorder.Offset();
        const auto commands = recorder.Commands();
        Require(sceAmprCommandBufferNop(&recorder.buffer, dwords) == 0);
        RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNop(dwords));
    }
    const auto offset = recorder.Offset();
    const auto commands = recorder.Commands();
    Require(sceAmprCommandBufferNopWithData(&recorder.buffer, 3, data) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNopWithData(4));
    Require(std::memcmp(recorder.memory.data() + offset + sizeof(Apr::CommandHeader), data, sizeof(data)) == 0);
    Require(sceAmprCommandBufferNopWithData(&recorder.buffer, 0, nullptr) == 0);
    Require(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0) == 0);

    const auto rejected = static_cast<std::uint64_t>(static_cast<std::uint32_t>(invalidArgument));
    Require(sceAmprCommandBufferNop(&recorder.buffer, 0) == invalidArgument);
    Require(sceAmprCommandBufferNop(&recorder.buffer, 17) == invalidArgument);
    Require(sceAmprCommandBufferNopWithData(&recorder.buffer, 16, data) == invalidArgument);
    Require(sceAmprMeasureCommandSizeNop(0) == rejected && sceAmprMeasureCommandSizeNop(17) == rejected);
    Require(sceAmprMeasureCommandSizeNopWithData(0) == rejected && sceAmprMeasureCommandSizeNopWithData(17) == rejected);
}

void TestVersionedCommands() {
    Recorder recorder;
    alignas(8) std::uint64_t value = 0x8000000000000005ull;
    alignas(8) std::uint64_t single = 0;
    alignas(8) std::uint64_t pair = 0;
    alignas(8) std::uint64_t time = 0;
    Require(sceAmprCommandBufferWaitOnAddress_04_00(&recorder.buffer, &value, 0x8000000000000003ull, 4, 0) == 0);
    Require(sceAmprCommandBufferWaitOnAddress_04_00(&recorder.buffer, &value, 1, 6, 1) == 0);
    Require(sceAmprCommandBufferWaitOnAddress_04_00(&recorder.buffer, &value, 0x8000000000000000ull, 5, 0) == 0);
    Require(sceAmprCommandBufferWriteCounterOnCompletion(&recorder.buffer, 10, 3) == 0);
    Require(sceAmprCommandBufferWriteCounterOnCompletion(&recorder.buffer, 11, 4) == 0);
    Require(sceAmprCommandBufferWriteAddressFromCounter_04_00(&recorder.buffer, &single, 10, 1) == 0);
    Require(sceAmprCommandBufferWriteAddressFromCounterPair_04_00(&recorder.buffer, &pair, 10, 0) == 0);
    Require(sceAmprCommandBufferWriteAddressFromTimeCounter_04_00(&recorder.buffer, &time, 1) == 0);
    auto submitted = std::async(std::launch::async, [&]() { return sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0); });
    Require(submitted.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
    Require(submitted.get() == 0 && single == 3 && pair == (3ull | (4ull << 32u)) && time != 0);

    const auto rejected = static_cast<std::uint64_t>(static_cast<std::uint32_t>(invalidArgument));
    Recorder empty;
    Require(sceAmprCommandBufferWaitOnAddress_04_00(&empty.buffer, nullptr, 0, 0, 0) == invalidArgument);
    Require(sceAmprCommandBufferWaitOnAddress_04_00(&empty.buffer, &value, 0, 7, 0) == invalidArgument);
    Require(sceAmprCommandBufferWaitOnAddress_04_00(&empty.buffer, &value, 0, 0, 2) == invalidArgument);
    Require(sceAmprCommandBufferWriteAddressFromCounterPair_04_00(&empty.buffer, &pair, 11, 0) == invalidArgument);
    Require(sceAmprCommandBufferWriteAddressFromTimeCounter_04_00(&empty.buffer, nullptr, 0) == invalidArgument);
    Require(sceAmprCommandBufferWriteKernelEventQueue_04_00(&empty.buffer, 0, 1, 0, 0) == invalidArgument);
    Require(empty.Offset() == 0 && empty.Commands() == 0);
    Require(sceAmprMeasureCommandSizeWaitOnAddress_04_00(nullptr, 0, 0, 0) == rejected);
    Require(sceAmprMeasureCommandSizeWaitOnAddress_04_00(&value, 0, 7, 0) == rejected);
    Require(sceAmprMeasureCommandSizeWriteAddressFromTimeCounter_04_00(nullptr) == rejected);
    Require(sceAmprMeasureCommandSizeWriteAddressFromCounter_04_00(&single, 128) == sizeof(Apr::WriteAddressFromCounterCommand));
    Require(sceAmprMeasureCommandSizeWriteAddressFromCounter_04_00(nullptr, 0) == rejected);
    Require(sceAmprMeasureCommandSizeWriteAddressFromCounterPair_04_00(&pair, 128) == sizeof(Apr::WriteAddressFromCounterCommand));
    Require(sceAmprMeasureCommandSizeWriteAddressFromCounterPair_04_00(&pair, 11) == rejected);
    Require(sceAmprMeasureCommandSizeWaitOnAddress_04_00(&value, 0, 6, 1) == sizeof(Apr::WaitCommand));
    Require(sceAmprMeasureCommandSizeWriteAddressFromCounterPair_04_00(&pair, 10) == sizeof(Apr::WriteAddressFromCounterCommand));
}

void SubmitWithin10Seconds(const Recorder& recorder) {
    auto submitted = std::async(std::launch::async, [&]() { return sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0); });
    Require(submitted.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
    Require(submitted.get() == 0);
}

void TestVersionedCounters() {
    enum : std::uint8_t { size8, size4, size2Offset0, size2Offset1, size1Offset0, size1Offset1, size1Offset2, size1Offset3 };
    enum : std::uint8_t { store, atomicOr, atomicAndComplement, atomicXor, atomicAdd };
    Recorder recorder;
    alignas(8) std::uint64_t fields = 0;
    alignas(8) std::uint64_t wide = 0;
    alignas(8) std::uint64_t bits = 0;
    Require(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 20, size4, 0x11223344u, store, 0) == 0);
    Require(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 20, size1Offset2, 0x1AAu, store, 1) == 0);
    Require(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 20, size2Offset0, 0xFFFFu, atomicAdd, 0) == 0);
    Require(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 22, size8, 0x0000000500000001ull, store, 0) == 0);
    Require(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 22, size8, 0xFFFFFFFFu, atomicAdd, 0) == 0);
    Require(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 24, size4, 0xF0u, store, 0) == 0);
    Require(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 24, size4, 0x0Fu, atomicOr, 0) == 0);
    Require(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 24, size4, 0x3Cu, atomicAndComplement, 0) == 0);
    Require(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 24, size4, 0xFFu, atomicXor, 0) == 0);
    Require(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 20, size1Offset3, 0x11u, 0, 0, 0, 0) == 0);
    Require(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 20, size1Offset2, 1u, 6, 0, 0, 1) == 0);
    Require(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 20, size2Offset0, 0xF000u, 4, 0, 0, 0) == 0);
    Require(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 20, size2Offset1, 0x11ABu, 2, 0, 0, 0) == 0);
    Require(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 22, size8, 0x0000000600000000ull, 0, 0, 0, 0) == 0);
    Require(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 24, size4, 0xFCu, 0, 1, 0x0Fu, 0) == 0);
    Require(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 24, size1Offset0, 0x3Cu, 0, 0, 0x0Fu, 0) == 0);
    Require(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, &fields, 20) == 0);
    Require(sceAmprCommandBufferWriteAddressFromCounterPairOnCompletion(&recorder.buffer, &wide, 22) == 0);
    Require(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, &bits, 24) == 0);
    Require(recorder.Commands() == 19);
    SubmitWithin10Seconds(recorder);
    Require(fields == 0x11AA3343u && wide == 0x0000000600000000ull && bits == 0x3Cu);

    const auto rejected = static_cast<std::uint64_t>(static_cast<std::uint32_t>(invalidArgument));
    Recorder empty;
    Require(sceAmprCommandBufferWaitOnCounter_04_00(&empty.buffer, 0, 8, 0, 0, 0, 0, 0) == invalidArgument);
    Require(sceAmprCommandBufferWaitOnCounter_04_00(&empty.buffer, 0, size4, 0, 7, 0, 0, 0) == invalidArgument);
    Require(sceAmprCommandBufferWaitOnCounter_04_00(&empty.buffer, 0, size4, 0, 0, 2, 0, 0) == invalidArgument);
    Require(sceAmprCommandBufferWaitOnCounter_04_00(&empty.buffer, 0, size4, 0, 0, 0, 0, 2) == invalidArgument);
    Require(sceAmprCommandBufferWriteCounter_04_00(&empty.buffer, 128, size4, 0, store, 0) == invalidArgument);
    Require(sceAmprCommandBufferWriteCounter_04_00(&empty.buffer, 0, 8, 0, store, 0) == invalidArgument);
    Require(sceAmprCommandBufferWriteCounter_04_00(&empty.buffer, 0, size4, 0, 5, 0) == invalidArgument);
    Require(sceAmprCommandBufferWriteCounter_04_00(nullptr, 0, size4, 0, store, 0) == invalidArgument);
    Require(empty.Offset() == 0 && empty.Commands() == 0);
    Require(sceAmprMeasureCommandSizeWaitOnCounter_04_00(200, size1Offset3, 0, 6, 1, 0, 1) == sizeof(Apr::WaitCommand));
    Require(sceAmprMeasureCommandSizeWaitOnCounter_04_00(0, 8, 0, 0, 0, 0, 0) == rejected);
    Require(sceAmprMeasureCommandSizeWaitOnCounter_04_00(0, size4, 0, 7, 0, 0, 0) == rejected);
    Require(sceAmprMeasureCommandSizeWaitOnCounter_04_00(0, size4, 0, 0, 2, 0, 0) == rejected);
    Require(sceAmprMeasureCommandSizeWaitOnCounter_04_00(0, size4, 0, 0, 0, 0, 2) == rejected);
    Require(sceAmprMeasureCommandSizeWriteCounter_04_00(127, size8, 0, atomicAdd) == sizeof(Apr::WriteCounterCommand));
    Require(sceAmprMeasureCommandSizeWriteCounter_04_00(128, size4, 0, store) == rejected);
    Require(sceAmprMeasureCommandSizeWriteCounter_04_00(0, 8, 0, store) == rejected);
    Require(sceAmprMeasureCommandSizeWriteCounter_04_00(0, size4, 0, 5) == rejected);
}

void TestClearBuffer() {
    Recorder recorder;
    Require(sceAmprCommandBufferNop(&recorder.buffer, 1) == 0);
    Require(recorder.Offset() != 0 && recorder.Commands() == 1);
    Require(sceAmprCommandBufferClearBuffer(&recorder.buffer) == recorder.memory.data());
    Require(recorder.buffer.base == nullptr && recorder.buffer.size == 0 && recorder.Offset() == 0 && recorder.Commands() == 0);
    Require(sceAmprCommandBufferClearBuffer(&recorder.buffer) == nullptr);
}

void TestConstructed() {
    Recorder recorder;
    const std::uint8_t payload[5] = {1, 2, 3, 4, 5};
    const std::uint32_t word = 0xCAFEF00Du;
    auto offset = recorder.Offset();
    auto commands = recorder.Commands();
    Require(sceAmprCommandBufferConstructNop(&recorder.buffer, 7, payload, sizeof(payload), &word) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNopWithData(4));
    const std::uint8_t* data = recorder.memory.data() + offset + sizeof(Apr::CommandHeader);
    const std::uint8_t padding[3] = {};
    Require(std::memcmp(data, &word, sizeof(word)) == 0 && std::memcmp(data + 4, payload, sizeof(payload)) == 0 && std::memcmp(data + 9, padding, sizeof(padding)) == 0);

    const std::array<std::uint8_t, 60> large{};
    offset = recorder.Offset();
    commands = recorder.Commands();
    Require(sceAmprCommandBufferConstructNop(&recorder.buffer, 0, large.data(), 60, nullptr) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNopWithData(16));
    offset = recorder.Offset();
    commands = recorder.Commands();
    Require(sceAmprCommandBufferConstructNop(&recorder.buffer, 0, nullptr, 0, nullptr) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNopWithData(1));

    const std::pair<std::uint32_t, Apr::Opcode> markers[] = {{1, Apr::Opcode::SetMarker}, {2, Apr::Opcode::PushMarker}, {5, Apr::Opcode::SetMarker}, {6, Apr::Opcode::PushMarker}};
    for (const auto& [type, opcode] : markers) {
        offset = recorder.Offset();
        commands = recorder.Commands();
        Require(sceAmprCommandBufferConstructMarker(&recorder.buffer, type, "stream", &color) == 0);
        RequireRecorded(recorder, offset, commands, opcode, sceAmprMeasureCommandSizeSetMarker("stream"), "stream");
    }
    offset = recorder.Offset();
    commands = recorder.Commands();
    Require(sceAmprCommandBufferConstructMarker(&recorder.buffer, 3, nullptr, nullptr) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::PopMarker, sceAmprMeasureCommandSizePopMarker());
    Require(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0) == 0);

    Recorder empty;
    Require(sceAmprCommandBufferConstructNop(&empty.buffer, 0, large.data(), 61, nullptr) == invalidArgument);
    Require(sceAmprCommandBufferConstructNop(&empty.buffer, 0, large.data(), 57, &word) == invalidArgument);
    Require(sceAmprCommandBufferConstructNop(nullptr, 0, large.data(), 4, nullptr) == invalidArgument);
    Require(sceAmprCommandBufferConstructMarker(&empty.buffer, 5, "stream", nullptr) == invalidArgument);
    Require(sceAmprCommandBufferConstructMarker(&empty.buffer, 6, "stream", nullptr) == invalidArgument);
    Require(sceAmprCommandBufferConstructMarker(&empty.buffer, 1, nullptr, nullptr) == invalidArgument);
    Require(sceAmprCommandBufferConstructMarker(&empty.buffer, 0, "stream", &color) == invalidArgument);
    Require(sceAmprCommandBufferConstructMarker(&empty.buffer, 4, "stream", &color) == invalidArgument);
    Require(empty.Offset() == 0 && empty.Commands() == 0);
}

std::uint8_t FileByte(std::size_t offset) {
    return static_cast<std::uint8_t>(offset * 7u + 3u);
}

bool MatchesFile(const std::uint8_t* data, std::size_t offset, std::size_t bytes) {
    for (std::size_t index = 0; index < bytes; ++index) {
        if (data[index] != FileByte(offset + index)) return false;
    }
    return true;
}

void TestZeroFilledBuffer() {
    const char* path = "ampr_zero_filled.bin";
    {
        std::ofstream file(path, std::ios::binary);
        for (std::size_t offset = 0; offset < 256; ++offset) file.put(static_cast<char>(FileByte(offset)));
    }
    std::uint32_t fileId = 0;
    std::uint32_t failed = 0;
    Require(sceKernelAprResolveFilepathsToIds(&path, 1, &fileId, &failed) == 0);
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "ampr") == 0);
    Require(sceKernelAddAmprEvent(eq, 0, nullptr) == 0);

    alignas(8) std::array<std::uint8_t, 256> memory{};
    Apr::CommandBufferObject buffer{};
    std::uint64_t gatherState = 0;
    std::uint64_t scatterState = 0;
    Require(sceAmprCommandBufferSetBuffer(&buffer, memory.data(), static_cast<std::uint32_t>(memory.size())) == 0);
    Require(sceAmprCommandBufferReset(&buffer) == 0);
    std::array<std::uint8_t, 32> data{};
    std::uint64_t done = 0;
    Require(sceAmprAprCommandBufferReadFile(&buffer, &gatherState, &scatterState, fileId, data.data(), data.size(), 64) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 0x1234567) == 0);
    Require(sceAmprCommandBufferWriteKernelEventQueue_04_00(&buffer, static_cast<std::uint64_t>(eq), 0, 0, 0) == 0);
    Require(sceKernelAprSubmitCommandBuffer(&buffer, 1) == 0);
    Require(MatchesFile(data.data(), 64, data.size()) && done == 0x1234567);

    KernelEvent event{};
    int count = 0;
    const KernelUseconds poll = 0;
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &poll) == 0 && count == 1);
    Require(sceKernelGetEventFilter(&event) == -25 && sceKernelGetEventId(&event) == 0);
    Require(sceKernelDeleteAmprEvent(eq, 0) == 0);
    Require(sceKernelDeleteEqueue(eq) == 0);
    std::remove(path);
}

void TestResolveFailures() {
    const char* paths[]{"ampr_resolve_existing.bin", "ampr_resolve_missing.bin"};
    { std::ofstream file(paths[0], std::ios::binary); file << "data"; }
    std::uint32_t ids[]{123, 123};
    std::uint64_t sizes[]{123, 123};
    std::uint32_t failed = 99;
    *__error_nid_postfix() = 0;
    Require(sceKernelAprResolveFilepathsToIdsAndFileSizes(paths, 2, ids, sizes, &failed) == -1);
    Require(*__error_nid_postfix() == 2 && failed == 1 && ids[1] == 0xffffffffu && sizes[1] == 0);
    Require(sizes[0] == 4);
    int results[]{123, 123};
    Require(sceKernelAprResolveFilepathsToIdsAndFileSizesForEach(paths, 2, ids, sizes, results) == 1);
    Require(results[0] == 0 && results[1] == static_cast<int>(0x80020002));
    Require(ids[1] == 0xffffffffu && sizes[0] == 4 && sizes[1] == 0);
    std::remove(paths[0]);
}

void TestGatherScatter() {
    const char* path = "ampr_gather_scatter.bin";
    {
        std::ofstream file(path, std::ios::binary);
        for (std::size_t offset = 0; offset < 4096; ++offset) file.put(static_cast<char>(FileByte(offset)));
    }
    std::uint32_t fileId = 0;
    std::uint32_t failed = 0;
    Require(sceKernelAprResolveFilepathsToIds(&path, 1, &fileId, &failed) == 0);

    Recorder recorder;
    auto* buffer = &recorder.buffer;
    auto* map = &recorder.gatherState;
    auto* state = &recorder.scatterState;
    std::array<std::uint8_t, 64> first{};
    std::array<std::uint8_t, 64> second{};
    Require(sceAmprAprCommandBufferReadFileGather(buffer, map, state, 8, 0) == invalidArgument);
    Require(sceAmprAprCommandBufferReadFileScatter(buffer, map, state, second.data(), 8) == invalidArgument);
    Require(sceAmprAprCommandBufferReadFileGatherScatter(buffer, map, state, second.data(), 8, 0) == invalidArgument);
    Require(recorder.Commands() == 0);

    Require(sceAmprAprCommandBufferReadFile(buffer, map, state, fileId, first.data(), 16, 100) == 0);
    auto offset = recorder.Offset();
    auto commands = recorder.Commands();
    Require(sceAmprAprCommandBufferReadFileGather(buffer, map, state, 8, 500) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::ReadFileGather, sceAmprMeasureCommandSizeReadFileGather(8, 500));
    offset = recorder.Offset();
    commands = recorder.Commands();
    Require(sceAmprAprCommandBufferReadFileScatter(buffer, map, state, second.data(), 8) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::ReadFileScatter, sceAmprMeasureCommandSizeReadFileScatter(second.data(), 8));
    offset = recorder.Offset();
    commands = recorder.Commands();
    Require(sceAmprAprCommandBufferReadFileGatherScatter(buffer, map, state, second.data() + 32, 4, 1000) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::ReadFileGatherScatter, sceAmprMeasureCommandSizeReadFileGatherScatter(second.data() + 32, 4, 1000));
    Require(sceAmprAprCommandBufferReadFileGather(buffer, map, state, 4, 2000) == 0);
    Require(sceAmprAprCommandBufferReadFileScatter(buffer, map, state, first.data() + 40, 6) == 0);
    offset = recorder.Offset();
    commands = recorder.Commands();
    Require(sceAmprAprCommandBufferResetGatherScatterState(buffer) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::ResetGatherScatterState, sceAmprMeasureCommandSizeResetGatherScatterState());
    Require(sceAmprAprCommandBufferReadFileGather(buffer, map, state, 8, 0) == invalidArgument);
    Require(sceAmprAprCommandBufferReadFileScatter(buffer, map, state, second.data(), 8) == invalidArgument);
    Require(sceAmprAprCommandBufferReadFile(buffer, map, state, fileId, second.data() + 48, 4, 3000) == 0);
    Require(sceAmprAprCommandBufferReadFileScatter(buffer, map, state, second.data() + 56, 4) == 0);
    Require(recorder.Commands() == 9);
    SubmitWithin10Seconds(recorder);

    Require(MatchesFile(first.data(), 100, 16) && MatchesFile(first.data() + 16, 500, 8));
    Require(MatchesFile(second.data(), 508, 8));
    Require(MatchesFile(second.data() + 32, 1000, 4) && MatchesFile(second.data() + 36, 2000, 4));
    Require(MatchesFile(first.data() + 40, 2004, 6));
    Require(MatchesFile(second.data() + 48, 3000, 4) && MatchesFile(second.data() + 56, 3004, 4));
    Require(first[24] == 0 && first[39] == 0 && second[8] == 0 && second[31] == 0 && second[40] == 0);
    std::remove(path);

    const auto rejected = static_cast<std::uint64_t>(static_cast<std::uint32_t>(invalidArgument));
    auto* high = reinterpret_cast<void*>(std::uintptr_t{0xF00000000000ull});
    Recorder empty;
    Require(sceAmprAprCommandBufferReadFile(&empty.buffer, map, state, fileId, first.data(), 0, 0) == invalidArgument);
    Require(sceAmprAprCommandBufferReadFile(&empty.buffer, map, state, fileId, first.data(), 0x100000001ull, 0) == invalidArgument);
    Require(sceAmprAprCommandBufferReadFile(&empty.buffer, map, state, fileId, first.data(), 4, 0x10000000000ull) == invalidArgument);
    Require(sceAmprAprCommandBufferReadFile(&empty.buffer, map, state, fileId, high, 4, 0) == invalidArgument);
    Require(sceAmprAprCommandBufferReadFile(nullptr, map, state, fileId, first.data(), 4, 0) == invalidArgument);
    Require(empty.Offset() == 0 && empty.Commands() == 0);
    Require(sceAmprMeasureCommandSizeReadFileGather(0, 0) == rejected);
    Require(sceAmprMeasureCommandSizeReadFileGather(0x100000000ull, 0x10000000000ull - 1u) == sizeof(Apr::ReadFileCommand));
    Require(sceAmprMeasureCommandSizeReadFileGather(4, 0x10000000000ull) == rejected);
    Require(sceAmprMeasureCommandSizeReadFileScatter(first.data(), 0x100000001ull) == rejected);
    Require(sceAmprMeasureCommandSizeReadFileScatter(high, 4) == rejected);
    Require(sceAmprMeasureCommandSizeReadFileGatherScatter(first.data(), 4, 0x10000000000ull) == rejected);
    Require(sceAmprMeasureCommandSizeReadFileGatherScatter(nullptr, 4, 0) == sizeof(Apr::ReadFileCommand));
}

constexpr std::uint64_t page = 0x4000;
constexpr int permissionDenied = static_cast<int>(0x80020001);
constexpr int busy = static_cast<int>(0x80020010);
constexpr int noSuchSubmission = static_cast<int>(0x80020003);
constexpr std::int32_t cpuReadWrite = 0x03;
constexpr std::int32_t cpuGpuReadWrite = 0x33;
constexpr std::int32_t amprReadWrite = 0xC0;

volatile std::uint64_t& At(std::uint64_t address) {
    return *reinterpret_cast<volatile std::uint64_t*>(address);
}

void TestAmm() {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
    Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0);
    Require(start != 0 && start % page == 0 && start < end && end <= multimapStart && multimapStart < multimapEnd);

    const auto directMemory = static_cast<std::int64_t>(sceKernelGetDirectMemorySize());
    std::int64_t pool = -1;
    std::int64_t direct = -1;
    Require(sceAmprAmmGiveDirectMemory(0, directMemory, 7 * page, page, 1, &pool) == 0 && pool >= 0);
    Require(sceAmprAmmGiveDirectMemory(0, directMemory, 2 * page, page, 0, &direct) == 0 && direct >= 0);
    Require(sceAmprAmmGiveDirectMemory(0, directMemory, page, page, 2, &direct) == invalidArgument);
    Require(sceAmprAmmGiveDirectMemory(0, directMemory, page, page, 1, nullptr) == invalidArgument);

    const std::uint64_t automatic = start;
    const std::uint64_t first = start + 0x10000;
    const std::uint64_t alias = start + 0x20000;
    const std::uint64_t reused = start + 0x30000;
    const std::uint64_t region = start + 0x60000;
    const std::uint64_t directRegion = start + 0x70000;

    Recorder maps;
    Require(sceAmprAmmCommandBufferConstructor(&maps.buffer) == 0);
    auto offset = maps.Offset();
    auto commands = maps.Commands();
    Require(sceAmprAmmCommandBufferMap(&maps.buffer, automatic, 2 * page, 0, cpuReadWrite) == 0);
    RequireAppended(maps, offset, commands, Apr::Opcode::AmmMap, sceAmprAmmMeasureAmmCommandSizeMap(automatic, 2 * page, 0, cpuReadWrite));
    offset = maps.Offset();
    commands = maps.Commands();
    Require(sceAmprAmmCommandBufferMapDirect(&maps.buffer, first, direct, 2 * page, 0, cpuGpuReadWrite) == 0);
    RequireAppended(maps, offset, commands, Apr::Opcode::AmmMapDirect, sceAmprAmmMeasureAmmCommandSizeMapDirect(first, direct, 2 * page, 0, cpuGpuReadWrite));
    Require(sceAmprAmmCommandBufferMapDirectWithGpuMaskId(&maps.buffer, alias, direct, 2 * page, 0, amprReadWrite, 3) == 0);
    std::uint32_t id = 0;
    Require(sceAmprAmmSubmitCommandBuffer3(maps.memory.data(), maps.Offset(), 0, &id) == 0 && id != 0);
    Require(sceAmprAmmWaitCommandBufferCompletion(id) == 0);
    Require(sceAmprAmmWaitCommandBufferCompletion(id + 1000) == noSuchSubmission);
    At(automatic) = 0x1111;
    At(automatic + 2 * page - 8) = 0x2222;
    Require(At(automatic) == 0x1111 && At(automatic + 2 * page - 8) == 0x2222);
    At(first + page) = 0x3333;
    Require(At(alias + page) == 0x3333);
    At(alias) = 0x4444;
    Require(At(first) == 0x4444);

    Recorder remaps;
    Require(sceAmprAmmCommandBufferConstructor(&remaps.buffer) == 0);
    offset = remaps.Offset();
    commands = remaps.Commands();
    Require(sceAmprAmmCommandBufferUnmap(&remaps.buffer, automatic, 2 * page) == 0);
    RequireAppended(remaps, offset, commands, Apr::Opcode::AmmUnmap, sceAmprAmmMeasureAmmCommandSizeUnmap(automatic, 2 * page));
    Require(sceAmprAmmCommandBufferMapWithGpuMaskId(&remaps.buffer, reused, 6 * page, 0, cpuReadWrite, 1) == 0);
    std::uint64_t result = ~0ull;
    Require(sceAmprAmmSubmitCommandBuffer2(remaps.memory.data(), remaps.Offset(), 0, &result, &id) == 0 && result == 0);
    Require(sceAmprAmmWaitCommandBufferCompletion(id) == 0);
    At(reused + 6 * page - 8) = 0x5555;
    Require(At(reused + 6 * page - 8) == 0x5555);

    Recorder apr;
    alignas(8) std::uint64_t done = 0;
    offset = apr.Offset();
    commands = apr.Commands();
    Require(sceAmprAprCommandBufferMapBegin(&apr.buffer, region, page, 0, amprReadWrite) == 0);
    RequireAppended(apr, offset, commands, Apr::Opcode::AmmMap, sceAmprMeasureCommandSizeMapBegin(region, page, 0, amprReadWrite));
    Require(sceAmprAprCommandBufferMapBegin(&apr.buffer, region, page, 0, amprReadWrite) == permissionDenied);
    Require(sceAmprAprCommandBufferMapDirectBegin(&apr.buffer, directRegion, direct, page, 0, amprReadWrite) == permissionDenied);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&apr.buffer, &done, 1) == permissionDenied);
    Require(sceAmprCommandBufferWriteCounterOnCompletion(&apr.buffer, 30, 1) == permissionDenied);
    Require(sceAmprCommandBufferWriteKernelEventQueueOnCompletion(&apr.buffer, 1, 1, 0) == permissionDenied);
    Require(sceAmprCommandBufferWriteAddressFromCounter_04_00(&apr.buffer, &done, 30, 0) == permissionDenied);
    Require(sceAmprCommandBufferWriteCounter_04_00(&apr.buffer, 30, 1, 5, 0, 0) == permissionDenied);
    Require(sceAmprCommandBufferWriteCounter_04_00(&apr.buffer, 30, 1, 5, 0, 1) == 0);
    offset = apr.Offset();
    commands = apr.Commands();
    Require(sceAmprAprCommandBufferMapEnd(&apr.buffer) == 0);
    RequireAppended(apr, offset, commands, Apr::Opcode::MapEnd, sceAmprMeasureCommandSizeMapEnd());
    Require(sceAmprAprCommandBufferMapEnd(&apr.buffer) == permissionDenied);
    Require(sceAmprAprCommandBufferMapDirectBegin(&apr.buffer, directRegion, direct, page, 0, cpuReadWrite) == 0);
    Require(sceAmprAprCommandBufferMapEnd(&apr.buffer) == 0);
    Require(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&apr.buffer, &done, 30) == 0);
    SubmitWithin10Seconds(apr);
    Require(done == 5);
    At(region + page - 8) = 0x6666;
    Require(At(region + page - 8) == 0x6666);
    Require(At(directRegion) == 0x4444);

    const auto rejected = static_cast<std::uint64_t>(static_cast<std::uint32_t>(invalidArgument));
    const std::int64_t ammRejected = invalidArgument;
    Recorder empty;
    Require(sceAmprAmmCommandBufferMap(&empty.buffer, start, page, 0, 0x04) == invalidArgument);
    Require(sceAmprAmmCommandBufferMap(&empty.buffer, start, page, 0, 0x400) == invalidArgument);
    Require(sceAmprAmmCommandBufferMap(&empty.buffer, start + 0x1000, page, 0, cpuReadWrite) == invalidArgument);
    Require(sceAmprAmmCommandBufferMap(&empty.buffer, start, page + 0x1000, 0, cpuReadWrite) == invalidArgument);
    Require(sceAmprAmmCommandBufferMap(&empty.buffer, start, 0, 0, cpuReadWrite) == invalidArgument);
    Require(sceAmprAmmCommandBufferMap(&empty.buffer, 0, page, 0, cpuReadWrite) == invalidArgument);
    Require(sceAmprAmmCommandBufferMap(&empty.buffer, ~(page - 1), 2 * page, 0, cpuReadWrite) == invalidArgument);
    Require(sceAmprAmmCommandBufferMapDirect(&empty.buffer, start, direct + 0x1000, page, 0, cpuReadWrite) == invalidArgument);
    Require(sceAmprAmmCommandBufferUnmap(&empty.buffer, start + 8, page) == invalidArgument);
    Require(sceAmprAmmCommandBufferMap(nullptr, start, page, 0, cpuReadWrite) == invalidArgument);
    Require(sceAmprAprCommandBufferMapBegin(&empty.buffer, start, page, 0, 0x04) == invalidArgument);
    Require(sceAmprAprCommandBufferMapEnd(nullptr) == invalidArgument);
    Require(empty.Offset() == 0 && empty.Commands() == 0);

    Apr::CommandBufferObject unbound{};
    Require(sceAmprCommandBufferConstructor(&unbound) == 0);
    Require(sceAmprAmmCommandBufferMap(&unbound, start, page, 0, cpuReadWrite) == permissionDenied);
    Require(sceAmprAmmCommandBufferUnmap(&unbound, start, page) == permissionDenied);
    const auto measured = static_cast<std::uint32_t>(sceAmprAmmMeasureAmmCommandSizeUnmap(start, page));
    Recorder exact(measured);
    Require(sceAmprAmmCommandBufferUnmap(&exact.buffer, start, page) == 0);
    Require(sceAmprAmmCommandBufferUnmap(&exact.buffer, start, page) == busy);
    Require(sceAmprAmmCommandBufferMap(&exact.buffer, start, page, 0, cpuReadWrite) == busy);
    Require(sceAmprAmmSubmitCommandBuffer(nullptr, 0, 0) == permissionDenied);
    Require(sceAmprAmmSubmitCommandBuffer(exact.memory.data(), 0, 3) == invalidArgument);
    Require(sceAmprAmmSubmitCommandBuffer3(nullptr, 0, 3, nullptr) == invalidArgument);
    Require(sceAmprAmmSubmitCommandBuffer(exact.memory.data(), 0, 2) == 0);

    Require(sceAmprAmmMeasureAmmCommandSizeMap(start, page, 0, 0x04) == ammRejected);
    Require(sceAmprAmmMeasureAmmCommandSizeMapWithGpuMaskId(start + 8, page, 0, cpuReadWrite, 0) == ammRejected);
    Require(sceAmprAmmMeasureAmmCommandSizeMapWithGpuMaskId(start, page, 0, cpuReadWrite, 0) == std::int64_t{sizeof(Apr::AmmMapCommand)});
    Require(sceAmprAmmMeasureAmmCommandSizeMapDirect(start, direct + 8, page, 0, cpuReadWrite) == ammRejected);
    Require(sceAmprAmmMeasureAmmCommandSizeMapDirectWithGpuMaskId(start, direct, page, 0, cpuReadWrite, 0) == std::int64_t{sizeof(Apr::AmmMapCommand)});
    Require(sceAmprAmmMeasureAmmCommandSizeUnmap(start, 0) == ammRejected);
    Require(sceAmprMeasureCommandSizeMapBegin(start, page, 0, 0x04) == rejected);
    Require(sceAmprMeasureCommandSizeMapDirectBegin(start, direct + 8, page, 0, cpuReadWrite) == rejected);
    Require(sceAmprMeasureCommandSizeMapDirectBegin(start, direct, page, 0, cpuReadWrite) == sizeof(Apr::AmmMapCommand));
}

int Protection(std::uint64_t address) {
    int protection = -1;
    Require(sceKernelQueryMemoryProtection(reinterpret_cast<void*>(address), nullptr, nullptr, &protection) == 0);
    return protection;
}

void SubmitAmm(Recorder& recorder) {
    std::uint32_t id = 0;
    Require(sceAmprAmmSubmitCommandBuffer3(recorder.memory.data(), recorder.Offset(), 0, &id) == 0);
    Require(sceAmprAmmWaitCommandBufferCompletion(id) == 0);
}

void TestAmmRemapAndProtect() {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
    Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0);
    const auto directMemory = static_cast<std::int64_t>(sceKernelGetDirectMemorySize());
    std::int64_t pool = -1;
    std::int64_t direct = -1;
    Require(sceAmprAmmGiveDirectMemory(0, directMemory, 4 * page, page, 1, &pool) == 0);
    Require(sceAmprAmmGiveDirectMemory(0, directMemory, page, page, 0, &direct) == 0);

    const std::uint64_t source = start + 0x100000;
    const std::uint64_t directSource = start + 0x140000;
    const std::uint64_t moved = start + 0x180000;
    const std::uint64_t alias = multimapStart + 0x100000;
    const std::uint64_t directAlias = multimapStart + 0x140000;
    const std::uint64_t fresh = start + 0x1C0000;
    const std::uint64_t returned = start + 0x200000;

    Recorder maps;
    Require(sceAmprAmmCommandBufferMap(&maps.buffer, source, 2 * page, 0, cpuReadWrite) == 0);
    Require(sceAmprAmmCommandBufferMapDirect(&maps.buffer, directSource, direct, page, 0, cpuReadWrite) == 0);
    SubmitAmm(maps);
    At(source) = 0xA1;
    At(source + page) = 0xA2;
    At(directSource) = 0xD1;

    Recorder moves;
    auto offset = moves.Offset();
    auto commands = moves.Commands();
    Require(sceAmprAmmCommandBufferRemap(&moves.buffer, moved, source, 2 * page, cpuReadWrite) == 0);
    RequireAppended(moves, offset, commands, Apr::Opcode::AmmRemap, sceAmprAmmMeasureAmmCommandSizeRemap(moved, source, 2 * page, cpuReadWrite));
    offset = moves.Offset();
    commands = moves.Commands();
    Require(sceAmprAmmCommandBufferMultiMap(&moves.buffer, alias, moved, 2 * page, cpuReadWrite) == 0);
    RequireAppended(moves, offset, commands, Apr::Opcode::AmmMultiMap, sceAmprAmmMeasureAmmCommandSizeMultiMap(alias, moved, 2 * page, cpuReadWrite));
    Require(sceAmprAmmCommandBufferMultiMapWithGpuMaskId(&moves.buffer, directAlias, directSource, page, amprReadWrite, 2) == 0);
    SubmitAmm(moves);
    Require(At(moved) == 0xA1 && At(moved + page) == 0xA2 && At(alias + page) == 0xA2);
    At(alias) = 0xB1;
    Require(At(moved) == 0xB1);
    Require(At(directAlias) == 0xD1);

    Recorder shared;
    Require(sceAmprAmmCommandBufferUnmap(&shared.buffer, moved, 2 * page) == 0);
    Require(sceAmprAmmCommandBufferMap(&shared.buffer, fresh, 2 * page, 0, cpuReadWrite) == 0);
    SubmitAmm(shared);
    At(fresh) = 0xC1;
    At(fresh + page) = 0xC2;
    Require(At(alias) == 0xB1 && At(alias + page) == 0xA2);

    Recorder released;
    Require(sceAmprAmmCommandBufferUnmap(&released.buffer, alias, 2 * page) == 0);
    Require(sceAmprAmmCommandBufferRemapWithGpuMaskId(&released.buffer, returned, fresh, 2 * page, cpuReadWrite, 1) == 0);
    Require(sceAmprAmmCommandBufferMap(&released.buffer, fresh, 2 * page, 0, cpuReadWrite) == 0);
    SubmitAmm(released);
    Require(At(returned) == 0xC1 && At(returned + page) == 0xC2);
    At(fresh) = 0xE1;
    Require(At(returned) == 0xC1);

    Recorder protects;
    offset = protects.Offset();
    commands = protects.Commands();
    Require(sceAmprAmmCommandBufferModifyProtect(&protects.buffer, returned, 2 * page, 0x01, 0x02) == 0);
    RequireAppended(protects, offset, commands, Apr::Opcode::AmmModifyProtect, sceAmprAmmMeasureAmmCommandSizeModifyProtect(returned, 2 * page, 0x01, 0x02));
    offset = protects.Offset();
    commands = protects.Commands();
    Require(sceAmprAmmCommandBufferModifyMtypeProtect(&protects.buffer, returned, page, 3, 0x22, 0x22) == 0);
    RequireAppended(protects, offset, commands, Apr::Opcode::AmmModifyMtypeProtect, sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtect(returned, page, 3, 0x22, 0x22));
    Require(sceAmprAmmCommandBufferModifyProtectWithGpuMaskId(&protects.buffer, directAlias, page, 0x00, 0x80, 0) == 0);
    Require(sceAmprAmmCommandBufferModifyMtypeProtectWithGpuMaskId(&protects.buffer, fresh, page, 1, 0x10, 0x13, 0) == 0);
    SubmitAmm(protects);
    Require(Protection(returned) == 0x23 && Protection(returned + page) == 0x01);
    Require(Protection(directAlias) == 0x01);
    Require(Protection(fresh) == 0x10 && Protection(fresh + page) == 0x03);
    At(returned) = 0xF1;
    Require(At(returned) == 0xF1 && At(returned + page) == 0xC2);

    const std::int64_t ammRejected = invalidArgument;
    Recorder empty;
    Require(sceAmprAmmCommandBufferRemap(&empty.buffer, moved, source + 8, page, cpuReadWrite) == invalidArgument);
    Require(sceAmprAmmCommandBufferRemap(&empty.buffer, moved + 8, source, page, cpuReadWrite) == invalidArgument);
    Require(sceAmprAmmCommandBufferRemap(&empty.buffer, moved, source, 0, cpuReadWrite) == invalidArgument);
    Require(sceAmprAmmCommandBufferMultiMap(&empty.buffer, alias, moved, page, 0x04) == invalidArgument);
    Require(sceAmprAmmCommandBufferModifyProtect(&empty.buffer, returned, page, 0x04, 0x03) == invalidArgument);
    Require(sceAmprAmmCommandBufferModifyProtect(&empty.buffer, returned, page, 0x03, 0x04) == invalidArgument);
    Require(sceAmprAmmCommandBufferModifyMtypeProtect(&empty.buffer, returned + 8, page, 0, 0x03, 0x03) == invalidArgument);
    Require(sceAmprAmmCommandBufferRemap(nullptr, moved, source, page, cpuReadWrite) == invalidArgument);
    Require(empty.Offset() == 0 && empty.Commands() == 0);
    Apr::CommandBufferObject unbound{};
    Require(sceAmprCommandBufferConstructor(&unbound) == 0);
    Require(sceAmprAmmCommandBufferMultiMap(&unbound, alias, moved, page, cpuReadWrite) == permissionDenied);
    Require(sceAmprAmmCommandBufferModifyProtect(&unbound, returned, page, 0x03, 0x03) == permissionDenied);
    const auto measured = static_cast<std::uint32_t>(sceAmprAmmMeasureAmmCommandSizeRemap(moved, source, page, cpuReadWrite));
    Recorder exact(measured);
    Require(sceAmprAmmCommandBufferRemap(&exact.buffer, moved, source, page, cpuReadWrite) == 0);
    Require(sceAmprAmmCommandBufferModifyProtect(&exact.buffer, returned, page, 0x03, 0x03) == busy);

    Require(sceAmprAmmMeasureAmmCommandSizeRemapWithGpuMaskId(moved, source + 8, page, cpuReadWrite, 0) == ammRejected);
    Require(sceAmprAmmMeasureAmmCommandSizeRemapWithGpuMaskId(moved, source, page, cpuReadWrite, 0) == std::int64_t{sizeof(Apr::AmmRemapCommand)});
    Require(sceAmprAmmMeasureAmmCommandSizeMultiMapWithGpuMaskId(alias, moved, page, 0x400, 0) == ammRejected);
    Require(sceAmprAmmMeasureAmmCommandSizeMultiMapWithGpuMaskId(alias, moved, page, cpuReadWrite, 0) == std::int64_t{sizeof(Apr::AmmRemapCommand)});
    Require(sceAmprAmmMeasureAmmCommandSizeModifyProtectWithGpuMaskId(returned, page, 0x03, 0x04, 0) == ammRejected);
    Require(sceAmprAmmMeasureAmmCommandSizeModifyProtectWithGpuMaskId(returned, page, 0x03, 0x03, 0) == std::int64_t{sizeof(Apr::AmmProtectCommand)});
    Require(sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtectWithGpuMaskId(returned, 0, 0, 0x03, 0x03, 0) == ammRejected);
    Require(sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtectWithGpuMaskId(returned, page, 0, 0x03, 0x03, 0) == std::int64_t{sizeof(Apr::AmmProtectCommand)});
    Require(sceAmprAmmMeasureAmmCommandSizeMultiMap(alias, moved, 0, cpuReadWrite) == ammRejected);
    Require(sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtect(returned, page, 0, 0x08, 0x03) == ammRejected);
}

void TestAmmPrt() {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
    Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0);
    std::int64_t pool = -1;
    Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), 4 * page, page, 1, &pool) == 0);

    const std::uint64_t prt = start + 0x300000;
    const std::uint64_t source = start + 0x380000;

    Recorder reserve;
    auto offset = reserve.Offset();
    auto commands = reserve.Commands();
    Require(sceAmprAmmCommandBufferMapAsPrt(&reserve.buffer, prt, 4 * page) == 0);
    RequireAppended(reserve, offset, commands, Apr::Opcode::AmmMapAsPrt, sceAmprAmmMeasureAmmCommandSizeMapAsPrt(prt, 4 * page));
    Require(sceAmprAmmCommandBufferMap(&reserve.buffer, source, page, 0, cpuReadWrite) == 0);
    SubmitAmm(reserve);
    Require(Protection(prt) == 0x10 && Protection(prt + 3 * page) == 0x10);
    Require(At(prt) == 0 && At(prt + 4 * page - 8) == 0);
    At(source) = 0x88;

    Recorder back;
    offset = back.Offset();
    commands = back.Commands();
    Require(sceAmprAmmCommandBufferAllocatePaForPrt(&back.buffer, prt + page, 2 * page, 0, cpuReadWrite) == 0);
    RequireAppended(back, offset, commands, Apr::Opcode::AmmAllocatePaForPrt, sceAmprAmmMeasureAmmCommandSizeAllocatePaForPrt(prt + page, 2 * page, 0, cpuReadWrite));
    Require(sceAmprAmmCommandBufferAllocatePaForPrt(&back.buffer, prt + 2 * page, page, 2, cpuGpuReadWrite) == 0);
    Require(sceAmprAmmCommandBufferRemapIntoPrt(&back.buffer, prt + 3 * page, source, page, cpuReadWrite, 0) == 0);
    SubmitAmm(back);
    Require(Protection(prt) == 0x10 && Protection(prt + page) == 0x03 && Protection(prt + 2 * page) == 0x33 && Protection(prt + 3 * page) == 0x03);
    At(prt + page) = 0x71;
    At(prt + 2 * page) = 0x72;
    Require(At(prt + page) == 0x71 && At(prt + 2 * page) == 0x72 && At(prt + 3 * page) == 0x88 && At(prt) == 0);

    Recorder release;
    offset = release.Offset();
    commands = release.Commands();
    Require(sceAmprAmmCommandBufferUnmapToPrt(&release.buffer, prt + page, 2 * page) == 0);
    RequireAppended(release, offset, commands, Apr::Opcode::AmmUnmapToPrt, sceAmprAmmMeasureAmmCommandSizeUnmap(prt + page, 2 * page));
    SubmitAmm(release);
    Require(Protection(prt + page) == 0x10 && At(prt + page) == 0 && At(prt + 2 * page) == 0 && At(prt + 3 * page) == 0x88);

    Recorder rebuild;
    Require(sceAmprAmmCommandBufferAllocatePaForPrt(&rebuild.buffer, prt, 3 * page, 0, cpuReadWrite) == 0);
    SubmitAmm(rebuild);
    for (std::uint64_t index = 0; index < 3; ++index) {
        Require(Protection(prt + index * page) == 0x03);
        At(prt + index * page) = 0x90 + index;
    }
    Require(At(prt) == 0x90 && At(prt + page) == 0x91 && At(prt + 2 * page) == 0x92 && At(prt + 3 * page) == 0x88);

    const std::int64_t ammRejected = invalidArgument;
    Recorder empty;
    Require(sceAmprAmmCommandBufferMapAsPrt(&empty.buffer, prt + 8, page) == invalidArgument);
    Require(sceAmprAmmCommandBufferMapAsPrt(nullptr, prt, page) == invalidArgument);
    Require(sceAmprAmmCommandBufferAllocatePaForPrt(&empty.buffer, prt, page, 0, 0x04) == invalidArgument);
    Require(sceAmprAmmCommandBufferAllocatePaForPrt(nullptr, prt, page, 0, 0x404) == invalidArgument);
    Require(sceAmprAmmCommandBufferAllocatePaForPrt(&empty.buffer, prt, 0, 0, cpuReadWrite) == invalidArgument);
    Require(sceAmprAmmCommandBufferRemapIntoPrt(&empty.buffer, prt, source + 8, page, cpuReadWrite, 0) == invalidArgument);
    Require(sceAmprAmmCommandBufferRemapIntoPrt(&empty.buffer, prt, source, page, 0x04, 0) == invalidArgument);
    Require(sceAmprAmmCommandBufferUnmapToPrt(&empty.buffer, prt, page + 8) == invalidArgument);
    Require(empty.Offset() == 0 && empty.Commands() == 0);
    Apr::CommandBufferObject unbound{};
    Require(sceAmprCommandBufferConstructor(&unbound) == 0);
    Require(sceAmprAmmCommandBufferMapAsPrt(&unbound, prt, page) == busy);
    Require(sceAmprAmmCommandBufferAllocatePaForPrt(&unbound, prt, page, 0, cpuReadWrite) == busy);
    Require(sceAmprAmmCommandBufferRemapIntoPrt(&unbound, prt, source, page, cpuReadWrite, 0) == permissionDenied);
    Require(sceAmprAmmCommandBufferUnmapToPrt(&unbound, prt, page) == permissionDenied);
    std::array<std::uint8_t, 64> loose{};
    Apr::CommandBufferObject sized{Apr::BufferType::Generic, 0, 0, 0, static_cast<std::uint32_t>(loose.size()), nullptr};
    Require(sceAmprAmmCommandBufferMapAsPrt(&sized, prt, page) == permissionDenied);
    Require(sceAmprAmmMeasureAmmCommandSizeMapAsPrt(prt, 0) == ammRejected);
    Require(sceAmprAmmMeasureAmmCommandSizeAllocatePaForPrt(prt, page, 0, 0x08) == ammRejected);
    Require(sceAmprAmmMeasureAmmCommandSizeAllocatePaForPrt(prt + 8, page, 0, cpuReadWrite) == ammRejected);
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
    TestKernelEventQueue();
    TestWaits();
    TestCounters();
    TestRejectedWaitsAndCounters();
    TestNops();
    TestVersionedCommands();
    TestVersionedCounters();
    TestClearBuffer();
    TestConstructed();
    TestZeroFilledBuffer();
    TestResolveFailures();
    TestGatherScatter();
    TestAmm();
    TestAmmRemapAndProtect();
    TestAmmPrt();
    return 0;
}

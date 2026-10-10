#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include <string_view>
#include <system_error>
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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int invalidArgument = static_cast<int>(0x80020016);
constexpr int bufferFull = static_cast<int>(0x8002001C);
constexpr int permissionDenied = static_cast<int>(0x80020001);
constexpr int busy = static_cast<int>(0x80020010);
constexpr int noSuchSubmission = static_cast<int>(0x80020003);
constexpr std::uint64_t measureRejected = static_cast<std::uint32_t>(invalidArgument);
constexpr std::int64_t ammRejected = invalidArgument;
constexpr std::uint32_t color = 0xFF8040u;
constexpr int amprEventFilter = -25;
constexpr std::uint64_t page = 0x4000;
constexpr std::int32_t cpuReadWrite = 0x03;
constexpr std::int32_t cpuGpuReadWrite = 0x33;
constexpr std::int32_t amprReadWrite = 0xC0;

struct Recorder {
    alignas(8) std::array<std::uint8_t, 4096> memory{};
    Apr::CommandBufferObject buffer{};
    std::uint64_t gatherState = 0;
    std::uint64_t scatterState = 0;

    explicit Recorder(std::uint32_t size = 4096) {
        RequireEqual(sceAmprCommandBufferConstructor(&buffer), 0, "construct the command buffer");
        RequireEqual(sceAmprAprCommandBufferConstructor(&buffer, &gatherState, &scatterState), 0, "construct the APR command buffer");
        RequireEqual(sceAmprCommandBufferSetBuffer(&buffer, memory.data(), size), 0, "bind the command buffer memory");
    }

    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    std::uint32_t Offset() const { return sceAmprCommandBufferGetCurrentOffset(&buffer); }
    std::uint32_t Commands() const { return sceAmprCommandBufferGetNumCommands(&buffer); }
};

class AmprEventQueue {
public:
    AmprEventQueue(int ident, void* userData) : ident(ident) {
        RequireEqual(sceKernelCreateEqueue(&queue, "ampr"), 0, "create the event queue");
        const int added = sceKernelAddAmprEvent(queue, ident, userData);
        if (added != 0) sceKernelDeleteEqueue(queue);
        RequireEqual(added, 0, "add the AMPR event");
        eventAdded = true;
        created = true;
    }

    ~AmprEventQueue() {
        if (eventAdded) sceKernelDeleteAmprEvent(queue, ident);
        if (created) sceKernelDeleteEqueue(queue);
    }

    AmprEventQueue(const AmprEventQueue&) = delete;
    AmprEventQueue& operator=(const AmprEventQueue&) = delete;

    KernelEqueue Handle() const { return queue; }

    int DeleteEvent() {
        eventAdded = false;
        return sceKernelDeleteAmprEvent(queue, ident);
    }

    int Delete() {
        created = false;
        return sceKernelDeleteEqueue(queue);
    }

private:
    KernelEqueue queue = 0;
    int ident;
    bool eventAdded = false;
    bool created = false;
};

std::uint8_t FileByte(std::size_t offset) {
    return static_cast<std::uint8_t>(offset * 7u + 3u);
}

bool MatchesFile(const std::uint8_t* data, std::size_t offset, std::size_t bytes) {
    for (std::size_t index = 0; index < bytes; ++index) {
        if (data[index] != FileByte(offset + index)) return false;
    }
    return true;
}

class GuestFile {
public:
    GuestFile(std::string name, std::size_t bytes)
        : name(std::move(name)), path(std::filesystem::current_path() / this->name) {
        std::ofstream file(path, std::ios::binary);
        for (std::size_t offset = 0; offset < bytes; ++offset) file.put(static_cast<char>(FileByte(offset)));
    }

    ~GuestFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }

    GuestFile(const GuestFile&) = delete;
    GuestFile& operator=(const GuestFile&) = delete;

    std::uint32_t ResolveId() const {
        const char* guestPath = name.c_str();
        std::uint32_t fileId = 0;
        std::uint32_t failed = 0;
        RequireEqual(sceKernelAprResolveFilepathsToIds(&guestPath, 1, &fileId, &failed), 0, "resolve " + name);
        return fileId;
    }

private:
    const std::string name;
    const std::filesystem::path path;
};

void RequireAppended(const Recorder& recorder, std::uint32_t offset, std::uint32_t commands, Apr::Opcode opcode, std::uint64_t measured, const std::string& what) {
    RequireEqual(recorder.Offset(), offset + measured, what + ": offset advances by the measured size");
    RequireEqual(recorder.Commands(), commands + 1, what + ": command count");
    Apr::CommandHeader header;
    std::memcpy(&header, recorder.memory.data() + offset, sizeof(header));
    RequireEqual(header.opcode, opcode, what + ": header opcode");
    RequireEqual(header.bytes, measured, what + ": header size");
}

void RequireRecorded(const Recorder& recorder, std::uint32_t offset, std::uint32_t commands, Apr::Opcode opcode, std::uint64_t measured, const std::string& text, const std::string& what) {
    RequireAppended(recorder, offset, commands, opcode, measured, what);
    Require(measured >= sizeof(Apr::MarkerCommand) + text.size() + 1, what + ": measured size holds the text");
    Require(std::memcmp(recorder.memory.data() + offset + sizeof(Apr::MarkerCommand), text.c_str(), text.size() + 1) == 0, what + ": recorded text");
}

void RequireEmpty(const Recorder& recorder, std::string_view what) {
    RequireEqual(recorder.Offset(), 0u, std::string(what) + ": offset");
    RequireEqual(recorder.Commands(), 0u, std::string(what) + ": command count");
}

void SubmitWithin10Seconds(const Recorder& recorder) {
    auto submitted = std::async(std::launch::async, [&]() { return sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0); });
    Require(submitted.wait_for(std::chrono::seconds(10)) == std::future_status::ready, "submission completes within 10 seconds");
    RequireEqual(submitted.get(), 0, "submit the command buffer");
}

std::string TextLabel(const std::string& text) {
    return "text of length " + std::to_string(text.size());
}

const std::string markerTexts[] = {"frame", "", "1234567", "12345678", std::string(300, 'a')};

const Case pushMarker{"PushMarker_Text_RecordsTextAndMeasuredSize", [] {
    Recorder recorder;
    for (const auto& text : markerTexts) {
        const auto offset = recorder.Offset();
        const auto commands = recorder.Commands();
        RequireEqual(sceAmprCommandBufferPushMarker(&recorder.buffer, text.c_str()), 0, "push marker, " + TextLabel(text));
        RequireRecorded(recorder, offset, commands, Apr::Opcode::PushMarker, sceAmprMeasureCommandSizePushMarker(text.c_str()), text, "push marker, " + TextLabel(text));
    }
}};

const Case pushMarkerWithColor{"PushMarkerWithColor_Text_RecordsTextAndMeasuredSize", [] {
    Recorder recorder;
    for (const auto& text : markerTexts) {
        const auto offset = recorder.Offset();
        const auto commands = recorder.Commands();
        RequireEqual(sceAmprCommandBufferPushMarkerWithColor(&recorder.buffer, text.c_str(), color), 0, "push colored marker, " + TextLabel(text));
        RequireRecorded(recorder, offset, commands, Apr::Opcode::PushMarker, sceAmprMeasureCommandSizePushMarkerWithColor(text.c_str(), color), text, "push colored marker, " + TextLabel(text));
    }
}};

const Case setMarker{"SetMarker_Text_RecordsTextAndMeasuredSize", [] {
    Recorder recorder;
    for (const auto& text : markerTexts) {
        const auto offset = recorder.Offset();
        const auto commands = recorder.Commands();
        RequireEqual(sceAmprCommandBufferSetMarker(&recorder.buffer, text.c_str()), 0, "set marker, " + TextLabel(text));
        RequireRecorded(recorder, offset, commands, Apr::Opcode::SetMarker, sceAmprMeasureCommandSizeSetMarker(text.c_str()), text, "set marker, " + TextLabel(text));
    }
}};

const Case setMarkerWithColor{"SetMarkerWithColor_Text_RecordsTextAndMeasuredSize", [] {
    Recorder recorder;
    for (const auto& text : markerTexts) {
        const auto offset = recorder.Offset();
        const auto commands = recorder.Commands();
        RequireEqual(sceAmprCommandBufferSetMarkerWithColor(&recorder.buffer, text.c_str(), &color), 0, "set colored marker, " + TextLabel(text));
        RequireRecorded(recorder, offset, commands, Apr::Opcode::SetMarker, sceAmprMeasureCommandSizeSetMarkerWithColor(text.c_str(), color), text, "set colored marker, " + TextLabel(text));
    }
}};

const Case popMarker{"PopMarker_AfterPushedMarker_RecordsMeasuredSize", [] {
    Recorder recorder;
    for (const auto& text : markerTexts) {
        RequireEqual(sceAmprCommandBufferPushMarker(&recorder.buffer, text.c_str()), 0, "push marker, " + TextLabel(text));
        const auto offset = recorder.Offset();
        const auto commands = recorder.Commands();
        RequireEqual(sceAmprCommandBufferPopMarker(&recorder.buffer), 0, "pop marker after " + TextLabel(text));
        RequireAppended(recorder, offset, commands, Apr::Opcode::PopMarker, sceAmprMeasureCommandSizePopMarker(), "pop marker after " + TextLabel(text));
    }
}};

const Case markerNullArguments{"MarkerCommands_NullTextOrColor_FailWithEinvalAndRecordNothing", [] {
    Recorder recorder;
    RequireEqual(sceAmprCommandBufferPushMarker(&recorder.buffer, nullptr), invalidArgument, "push null text");
    RequireEqual(sceAmprCommandBufferPushMarkerWithColor(&recorder.buffer, nullptr, color), invalidArgument, "push colored null text");
    RequireEqual(sceAmprCommandBufferSetMarker(&recorder.buffer, nullptr), invalidArgument, "set null text");
    RequireEqual(sceAmprCommandBufferSetMarkerWithColor(&recorder.buffer, nullptr, &color), invalidArgument, "set colored null text");
    RequireEqual(sceAmprCommandBufferSetMarkerWithColor(&recorder.buffer, "frame", nullptr), invalidArgument, "set marker with null color");
    RequireEmpty(recorder, "after rejected markers");
}};

const Case markerNullBuffer{"MarkerCommands_NullBuffer_FailWithEinval", [] {
    RequireEqual(sceAmprCommandBufferPushMarker(nullptr, "frame"), invalidArgument, "push marker");
    RequireEqual(sceAmprCommandBufferPushMarkerWithColor(nullptr, "frame", color), invalidArgument, "push colored marker");
    RequireEqual(sceAmprCommandBufferSetMarker(nullptr, "frame"), invalidArgument, "set marker");
    RequireEqual(sceAmprCommandBufferSetMarkerWithColor(nullptr, "frame", &color), invalidArgument, "set colored marker");
    RequireEqual(sceAmprCommandBufferPopMarker(nullptr), invalidArgument, "pop marker");
}};

const Case measureMarkerNullText{"MeasureMarkerSize_NullText_ReturnsEinval", [] {
    RequireEqual(sceAmprMeasureCommandSizePushMarker(nullptr), measureRejected, "push marker");
    RequireEqual(sceAmprMeasureCommandSizePushMarkerWithColor(nullptr, color), measureRejected, "push colored marker");
    RequireEqual(sceAmprMeasureCommandSizeSetMarker(nullptr), measureRejected, "set marker");
    RequireEqual(sceAmprMeasureCommandSizeSetMarkerWithColor(nullptr, color), measureRejected, "set colored marker");
}};

const Case exactBuffer{"PushMarker_BufferFilledExactly_LaterCommandsFailWithBufferFull", [] {
    const char* marker = "streaming";
    const auto measured = static_cast<std::uint32_t>(sceAmprMeasureCommandSizePushMarker(marker));
    Recorder exact(measured);
    RequireEqual(sceAmprCommandBufferPushMarker(&exact.buffer, marker), 0, "push marker filling the buffer");
    RequireEqual(exact.Offset(), measured, "offset after filling");
    RequireEqual(exact.Commands(), 1u, "commands after filling");
    RequireEqual(sceAmprCommandBufferPopMarker(&exact.buffer), bufferFull, "pop marker into a full buffer");
    RequireEqual(sceAmprCommandBufferSetMarker(&exact.buffer, ""), bufferFull, "set marker into a full buffer");
    RequireEqual(exact.Offset(), measured, "offset after rejected commands");
    RequireEqual(exact.Commands(), 1u, "commands after rejected commands");
}};

const Case smallBuffer{"MarkerCommands_BufferTooSmall_FailWithBufferFullAndRecordNothing", [] {
    const char* marker = "streaming";
    const auto measured = static_cast<std::uint32_t>(sceAmprMeasureCommandSizePushMarker(marker));
    Recorder small(measured - 4);
    RequireEqual(sceAmprCommandBufferPushMarker(&small.buffer, marker), bufferFull, "push marker");
    RequireEqual(sceAmprCommandBufferSetMarkerWithColor(&small.buffer, marker, &color), bufferFull, "set colored marker");
    RequireEmpty(small, "after rejected markers");
}};

const Case unboundBuffer{"MarkerCommands_UnboundBuffer_FailWithBufferFull", [] {
    Apr::CommandBufferObject unbound{};
    RequireEqual(sceAmprCommandBufferConstructor(&unbound), 0, "construct the command buffer");
    RequireEqual(sceAmprCommandBufferPushMarker(&unbound, "streaming"), bufferFull, "push marker");
    RequireEqual(sceAmprCommandBufferPopMarker(&unbound), bufferFull, "pop marker");
}};

const Case submission{"AprSubmit_MarkersAroundAddressWrites_WritesCompletionValues", [] {
    Recorder recorder;
    std::uint64_t first = 0;
    std::uint64_t second = 0;
    RequireEqual(sceAmprCommandBufferPushMarker(&recorder.buffer, "level"), 0, "push level marker");
    RequireEqual(sceAmprCommandBufferWriteAddressOnCompletion(&recorder.buffer, &first, 0x1111), 0, "write first address");
    RequireEqual(sceAmprCommandBufferSetMarkerWithColor(&recorder.buffer, "textures", &color), 0, "set textures marker");
    RequireEqual(sceAmprCommandBufferPushMarkerWithColor(&recorder.buffer, std::string(200, 'm').c_str(), color), 0, "push 200 character marker");
    RequireEqual(sceAmprCommandBufferSetMarker(&recorder.buffer, ""), 0, "set empty marker");
    RequireEqual(sceAmprCommandBufferPopMarker(&recorder.buffer), 0, "pop first marker");
    RequireEqual(sceAmprCommandBufferPopMarker(&recorder.buffer), 0, "pop second marker");
    RequireEqual(sceAmprCommandBufferWriteAddressOnCompletion(&recorder.buffer, &second, 0x2222), 0, "write second address");
    RequireEqual(recorder.Commands(), 8u, "recorded commands");
    RequireEqual(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0), 0, "submit");
    RequireEqual(first, std::uint64_t{0x1111}, "first address");
    RequireEqual(second, std::uint64_t{0x2222}, "second address");
}};

const Case kernelEventQueue{"WriteKernelEventQueue0400_Submitted_TriggersAmprEvent", [] {
    int userData = 0;
    AmprEventQueue queue(7, &userData);
    Recorder recorder;
    RequireEqual(sceAmprCommandBufferWriteKernelEventQueue_04_00(&recorder.buffer, static_cast<std::uint64_t>(queue.Handle()), 7, 0x1234, 0), 0, "record the event write");
    RequireEqual(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0), 0, "submit");
    KernelEvent event{};
    int count = 0;
    const KernelUseconds poll = 0;
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), &event, 1, &count, &poll), 0, "poll the event queue");
    RequireEqual(count, 1, "triggered events");
    RequireEqual(sceKernelGetEventFilter(&event), amprEventFilter, "event filter");
    RequireEqual(sceKernelGetEventId(&event), std::uintptr_t{7}, "event id");
    RequireEqual(sceKernelGetEventData(&event), std::intptr_t{0x1234}, "event data");
    RequireEqual(sceKernelGetEventUserData(&event), static_cast<void*>(&userData), "event user data");
    RequireEqual(queue.DeleteEvent(), 0, "delete the AMPR event");
    RequireEqual(queue.Delete(), 0, "delete the event queue");
}};

const Case waits{"WaitOnAddress_SatisfiedComparisons_CompletesWithinTenSeconds", [] {
    Recorder recorder;
    alignas(8) std::uint64_t value = 5;
    alignas(8) std::uint64_t done = 0;
    RequireEqual(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &value, 5, 0, 0), 0, "wait compare 0 reference 5");
    RequireEqual(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &value, 3, 1, 0), 0, "wait compare 1 reference 3");
    RequireEqual(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &value, 9, 2, 1), 0, "wait compare 2 reference 9");
    RequireEqual(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &value, 4, 3, 0), 0, "wait compare 3 reference 4");
    RequireEqual(sceAmprCommandBufferWriteAddressOnCompletion(&recorder.buffer, &done, 1), 0, "write completion");
    SubmitWithin10Seconds(recorder);
    RequireEqual(done, std::uint64_t{1}, "completion value");
}};

const Case counters{"WriteCounterOnCompletion_ThenWriteAddressFromCounter_WritesCounterValues", [] {
    Recorder recorder;
    alignas(8) std::uint64_t single = 0;
    alignas(8) std::uint64_t pair = 0;
    RequireEqual(sceAmprCommandBufferWriteCounterOnCompletion(&recorder.buffer, 6, 7), 0, "write counter 6");
    RequireEqual(sceAmprCommandBufferWriteCounterOnCompletion(&recorder.buffer, 7, 9), 0, "write counter 7");
    RequireEqual(sceAmprCommandBufferWaitOnCounter(&recorder.buffer, 6, 7, 0, 0), 0, "wait on counter 6");
    RequireEqual(sceAmprCommandBufferWaitOnCounter(&recorder.buffer, 7, 8, 1, 1), 0, "wait on counter 7");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, &single, 6), 0, "write address from counter 6");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterPairOnCompletion(&recorder.buffer, &pair, 6), 0, "write address from counter pair 6");
    RequireEqual(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0), 0, "submit");
    RequireEqual(single, std::uint64_t{7}, "single counter value");
    RequireEqual(pair, 7ull | (9ull << 32u), "counter pair value");
}};

const Case rejectedWaitsAndCounters{"WaitAndCounterCommands_InvalidArguments_FailWithEinvalAndRecordNothing", [] {
    Recorder recorder;
    alignas(8) std::uint64_t words[2] = {};
    auto* misaligned = reinterpret_cast<volatile std::uint64_t*>(reinterpret_cast<std::uint8_t*>(words) + 4);
    RequireEqual(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &words[0], 0, 4, 0), invalidArgument, "wait on address compare 4");
    RequireEqual(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, &words[0], 0, 0, 2), invalidArgument, "wait on address flush 2");
    RequireEqual(sceAmprCommandBufferWaitOnAddress(&recorder.buffer, misaligned, 0, 0, 0), invalidArgument, "wait on misaligned address");
    RequireEqual(sceAmprCommandBufferWaitOnCounter(&recorder.buffer, 128, 0, 0, 0), invalidArgument, "wait on counter 128");
    RequireEqual(sceAmprCommandBufferWaitOnCounter(&recorder.buffer, 0, 0, 4, 0), invalidArgument, "wait on counter compare 4");
    RequireEqual(sceAmprCommandBufferWaitOnCounter(&recorder.buffer, 0, 0, 0, 2), invalidArgument, "wait on counter flush 2");
    RequireEqual(sceAmprCommandBufferWriteCounterOnCompletion(&recorder.buffer, 128, 0), invalidArgument, "write counter 128");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, &words[0], 128), invalidArgument, "write address from counter 128");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterPairOnCompletion(&recorder.buffer, &words[0], 128), invalidArgument, "write address from counter pair 128");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterPairOnCompletion(&recorder.buffer, &words[0], 7), invalidArgument, "write address from odd counter pair 7");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, nullptr, 0), invalidArgument, "write counter to null address");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, misaligned, 0), invalidArgument, "write counter to misaligned address");
    RequireEqual(sceAmprCommandBufferWriteAddressFromTimeCounterOnCompletion(&recorder.buffer, nullptr), invalidArgument, "write time counter to null address");
    RequireEqual(sceAmprCommandBufferWriteAddressOnCompletion(&recorder.buffer, misaligned, 0), invalidArgument, "write misaligned address");
    RequireEqual(sceAmprCommandBufferWriteKernelEventQueueOnCompletion(&recorder.buffer, 0, 1, 0), invalidArgument, "write kernel event queue 0");
    RequireEmpty(recorder, "after rejected commands");
}};

const Case nops{"Nop_OneToSixteenDwords_RecordsMeasuredSize", [] {
    Recorder recorder;
    for (std::uint32_t dwords = 1; dwords <= 16; ++dwords) {
        const auto offset = recorder.Offset();
        const auto commands = recorder.Commands();
        RequireEqual(sceAmprCommandBufferNop(&recorder.buffer, dwords), 0, "nop of " + std::to_string(dwords) + " dwords");
        RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNop(dwords), "nop of " + std::to_string(dwords) + " dwords");
    }
}};

const Case nopWithData{"NopWithData_ThreeDwords_RecordsPayloadAfterHeader", [] {
    Recorder recorder;
    const std::uint32_t data[3] = {0x11111111u, 0x22222222u, 0x33333333u};
    RequireEqual(sceAmprCommandBufferNop(&recorder.buffer, 1), 0, "leading nop");
    const auto offset = recorder.Offset();
    const auto commands = recorder.Commands();
    RequireEqual(sceAmprCommandBufferNopWithData(&recorder.buffer, 3, data), 0, "nop with 3 data dwords");
    RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNopWithData(4), "nop with 3 data dwords");
    Require(std::memcmp(recorder.memory.data() + offset + sizeof(Apr::CommandHeader), data, sizeof(data)) == 0, "nop payload follows the header");
}};

const Case nopSubmission{"Nop_RecordedNops_SubmitSucceeds", [] {
    Recorder recorder;
    const std::uint32_t data[3] = {0x11111111u, 0x22222222u, 0x33333333u};
    for (std::uint32_t dwords = 1; dwords <= 16; ++dwords) RequireEqual(sceAmprCommandBufferNop(&recorder.buffer, dwords), 0, "nop of " + std::to_string(dwords) + " dwords");
    RequireEqual(sceAmprCommandBufferNopWithData(&recorder.buffer, 3, data), 0, "nop with 3 data dwords");
    RequireEqual(sceAmprCommandBufferNopWithData(&recorder.buffer, 0, nullptr), 0, "nop with no data");
    RequireEqual(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0), 0, "submit");
}};

const Case rejectedNops{"Nop_OutOfRangeDwords_FailsWithEinval", [] {
    Recorder recorder;
    const std::uint32_t data[3] = {0x11111111u, 0x22222222u, 0x33333333u};
    RequireEqual(sceAmprCommandBufferNop(&recorder.buffer, 0), invalidArgument, "nop of 0 dwords");
    RequireEqual(sceAmprCommandBufferNop(&recorder.buffer, 17), invalidArgument, "nop of 17 dwords");
    RequireEqual(sceAmprCommandBufferNopWithData(&recorder.buffer, 16, data), invalidArgument, "nop with 16 data dwords");
}};

const Case rejectedNopMeasures{"MeasureNop_OutOfRangeDwords_ReturnsEinval", [] {
    RequireEqual(sceAmprMeasureCommandSizeNop(0), measureRejected, "nop of 0 dwords");
    RequireEqual(sceAmprMeasureCommandSizeNop(17), measureRejected, "nop of 17 dwords");
    RequireEqual(sceAmprMeasureCommandSizeNopWithData(0), measureRejected, "nop with data of 0 dwords");
    RequireEqual(sceAmprMeasureCommandSizeNopWithData(17), measureRejected, "nop with data of 17 dwords");
}};

const Case versionedCommands{"VersionedCommands0400_Submitted_WriteCountersAndTime", [] {
    Recorder recorder;
    alignas(8) std::uint64_t value = 0x8000000000000005ull;
    alignas(8) std::uint64_t single = 0;
    alignas(8) std::uint64_t pair = 0;
    alignas(8) std::uint64_t time = 0;
    RequireEqual(sceAmprCommandBufferWaitOnAddress_04_00(&recorder.buffer, &value, 0x8000000000000003ull, 4, 0), 0, "wait compare 4");
    RequireEqual(sceAmprCommandBufferWaitOnAddress_04_00(&recorder.buffer, &value, 1, 6, 1), 0, "wait compare 6");
    RequireEqual(sceAmprCommandBufferWaitOnAddress_04_00(&recorder.buffer, &value, 0x8000000000000000ull, 5, 0), 0, "wait compare 5");
    RequireEqual(sceAmprCommandBufferWriteCounterOnCompletion(&recorder.buffer, 10, 3), 0, "write counter 10");
    RequireEqual(sceAmprCommandBufferWriteCounterOnCompletion(&recorder.buffer, 11, 4), 0, "write counter 11");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounter_04_00(&recorder.buffer, &single, 10, 1), 0, "write address from counter 10");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterPair_04_00(&recorder.buffer, &pair, 10, 0), 0, "write address from counter pair 10");
    RequireEqual(sceAmprCommandBufferWriteAddressFromTimeCounter_04_00(&recorder.buffer, &time, 1), 0, "write address from time counter");
    SubmitWithin10Seconds(recorder);
    RequireEqual(single, std::uint64_t{3}, "single counter value");
    RequireEqual(pair, 3ull | (4ull << 32u), "counter pair value");
    Require(time != 0, "time counter value is written");
}};

const Case rejectedVersionedCommands{"VersionedCommands0400_InvalidArguments_FailWithEinvalAndRecordNothing", [] {
    Recorder empty;
    alignas(8) std::uint64_t value = 0x8000000000000005ull;
    alignas(8) std::uint64_t pair = 0;
    RequireEqual(sceAmprCommandBufferWaitOnAddress_04_00(&empty.buffer, nullptr, 0, 0, 0), invalidArgument, "wait on null address");
    RequireEqual(sceAmprCommandBufferWaitOnAddress_04_00(&empty.buffer, &value, 0, 7, 0), invalidArgument, "wait compare 7");
    RequireEqual(sceAmprCommandBufferWaitOnAddress_04_00(&empty.buffer, &value, 0, 0, 2), invalidArgument, "wait flush 2");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterPair_04_00(&empty.buffer, &pair, 11, 0), invalidArgument, "write address from odd counter pair 11");
    RequireEqual(sceAmprCommandBufferWriteAddressFromTimeCounter_04_00(&empty.buffer, nullptr, 0), invalidArgument, "write time counter to null address");
    RequireEqual(sceAmprCommandBufferWriteKernelEventQueue_04_00(&empty.buffer, 0, 1, 0, 0), invalidArgument, "write kernel event queue 0");
    RequireEmpty(empty, "after rejected commands");
}};

const Case measureVersionedCommands{"MeasureVersionedCommands0400_ValidAndInvalidArguments_ReturnSizeOrEinval", [] {
    alignas(8) std::uint64_t value = 0x8000000000000005ull;
    alignas(8) std::uint64_t single = 0;
    alignas(8) std::uint64_t pair = 0;
    RequireEqual(sceAmprMeasureCommandSizeWaitOnAddress_04_00(nullptr, 0, 0, 0), measureRejected, "wait on null address");
    RequireEqual(sceAmprMeasureCommandSizeWaitOnAddress_04_00(&value, 0, 7, 0), measureRejected, "wait compare 7");
    RequireEqual(sceAmprMeasureCommandSizeWriteAddressFromTimeCounter_04_00(nullptr), measureRejected, "time counter to null address");
    RequireEqual(sceAmprMeasureCommandSizeWriteAddressFromCounter_04_00(&single, 128), sizeof(Apr::WriteAddressFromCounterCommand), "address from counter 128");
    RequireEqual(sceAmprMeasureCommandSizeWriteAddressFromCounter_04_00(nullptr, 0), measureRejected, "counter to null address");
    RequireEqual(sceAmprMeasureCommandSizeWriteAddressFromCounterPair_04_00(&pair, 128), sizeof(Apr::WriteAddressFromCounterCommand), "address from counter pair 128");
    RequireEqual(sceAmprMeasureCommandSizeWriteAddressFromCounterPair_04_00(&pair, 11), measureRejected, "address from odd counter pair 11");
    RequireEqual(sceAmprMeasureCommandSizeWaitOnAddress_04_00(&value, 0, 6, 1), sizeof(Apr::WaitCommand), "wait compare 6 flush 1");
    RequireEqual(sceAmprMeasureCommandSizeWriteAddressFromCounterPair_04_00(&pair, 10), sizeof(Apr::WriteAddressFromCounterCommand), "address from counter pair 10");
}};

enum : std::uint8_t { size8, size4, size2Offset0, size2Offset1, size1Offset0, size1Offset1, size1Offset2, size1Offset3 };
enum : std::uint8_t { store, atomicOr, atomicAndComplement, atomicXor, atomicAdd };

const Case versionedCounters{"WriteCounter0400_FieldOperations_ProduceExpectedCounters", [] {
    Recorder recorder;
    alignas(8) std::uint64_t fields = 0;
    alignas(8) std::uint64_t wide = 0;
    alignas(8) std::uint64_t bits = 0;
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 20, size4, 0x11223344u, store, 0), 0, "store 4 bytes into counter 20");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 20, size1Offset2, 0x1AAu, store, 1), 0, "store byte 2 of counter 20");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 20, size2Offset0, 0xFFFFu, atomicAdd, 0), 0, "add to low half of counter 20");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 22, size8, 0x0000000500000001ull, store, 0), 0, "store 8 bytes into counter 22");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 22, size8, 0xFFFFFFFFu, atomicAdd, 0), 0, "add 8 bytes to counter 22");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 24, size4, 0xF0u, store, 0), 0, "store into counter 24");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 24, size4, 0x0Fu, atomicOr, 0), 0, "or into counter 24");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 24, size4, 0x3Cu, atomicAndComplement, 0), 0, "and-complement into counter 24");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&recorder.buffer, 24, size4, 0xFFu, atomicXor, 0), 0, "xor into counter 24");
    RequireEqual(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 20, size1Offset3, 0x11u, 0, 0, 0, 0), 0, "wait on byte 3 of counter 20");
    RequireEqual(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 20, size1Offset2, 1u, 6, 0, 0, 1), 0, "wait on byte 2 of counter 20");
    RequireEqual(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 20, size2Offset0, 0xF000u, 4, 0, 0, 0), 0, "wait on low half of counter 20");
    RequireEqual(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 20, size2Offset1, 0x11ABu, 2, 0, 0, 0), 0, "wait on high half of counter 20");
    RequireEqual(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 22, size8, 0x0000000600000000ull, 0, 0, 0, 0), 0, "wait on counter 22");
    RequireEqual(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 24, size4, 0xFCu, 0, 1, 0x0Fu, 0), 0, "wait on masked counter 24");
    RequireEqual(sceAmprCommandBufferWaitOnCounter_04_00(&recorder.buffer, 24, size1Offset0, 0x3Cu, 0, 0, 0x0Fu, 0), 0, "wait on byte 0 of counter 24");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, &fields, 20), 0, "write counter 20");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterPairOnCompletion(&recorder.buffer, &wide, 22), 0, "write counter pair 22");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&recorder.buffer, &bits, 24), 0, "write counter 24");
    RequireEqual(recorder.Commands(), 19u, "recorded commands");
    SubmitWithin10Seconds(recorder);
    RequireEqual(fields, std::uint64_t{0x11AA3343u}, "counter 20");
    RequireEqual(wide, std::uint64_t{0x0000000600000000ull}, "counter pair 22");
    RequireEqual(bits, std::uint64_t{0x3Cu}, "counter 24");
}};

const Case rejectedVersionedCounters{"CounterCommands0400_InvalidArguments_FailWithEinvalAndRecordNothing", [] {
    Recorder empty;
    RequireEqual(sceAmprCommandBufferWaitOnCounter_04_00(&empty.buffer, 0, 8, 0, 0, 0, 0, 0), invalidArgument, "wait access 8");
    RequireEqual(sceAmprCommandBufferWaitOnCounter_04_00(&empty.buffer, 0, size4, 0, 7, 0, 0, 0), invalidArgument, "wait compare 7");
    RequireEqual(sceAmprCommandBufferWaitOnCounter_04_00(&empty.buffer, 0, size4, 0, 0, 2, 0, 0), invalidArgument, "wait mask operation 2");
    RequireEqual(sceAmprCommandBufferWaitOnCounter_04_00(&empty.buffer, 0, size4, 0, 0, 0, 0, 2), invalidArgument, "wait flush 2");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&empty.buffer, 128, size4, 0, store, 0), invalidArgument, "write counter 128");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&empty.buffer, 0, 8, 0, store, 0), invalidArgument, "write access 8");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&empty.buffer, 0, size4, 0, 5, 0), invalidArgument, "write operation 5");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(nullptr, 0, size4, 0, store, 0), invalidArgument, "write into null buffer");
    RequireEmpty(empty, "after rejected commands");
}};

const Case measureVersionedCounters{"MeasureCounterCommands0400_ValidAndInvalidArguments_ReturnSizeOrEinval", [] {
    RequireEqual(sceAmprMeasureCommandSizeWaitOnCounter_04_00(200, size1Offset3, 0, 6, 1, 0, 1), sizeof(Apr::WaitCommand), "wait on counter 200");
    RequireEqual(sceAmprMeasureCommandSizeWaitOnCounter_04_00(0, 8, 0, 0, 0, 0, 0), measureRejected, "wait access 8");
    RequireEqual(sceAmprMeasureCommandSizeWaitOnCounter_04_00(0, size4, 0, 7, 0, 0, 0), measureRejected, "wait compare 7");
    RequireEqual(sceAmprMeasureCommandSizeWaitOnCounter_04_00(0, size4, 0, 0, 2, 0, 0), measureRejected, "wait mask operation 2");
    RequireEqual(sceAmprMeasureCommandSizeWaitOnCounter_04_00(0, size4, 0, 0, 0, 0, 2), measureRejected, "wait flush 2");
    RequireEqual(sceAmprMeasureCommandSizeWriteCounter_04_00(127, size8, 0, atomicAdd), sizeof(Apr::WriteCounterCommand), "write counter 127");
    RequireEqual(sceAmprMeasureCommandSizeWriteCounter_04_00(128, size4, 0, store), measureRejected, "write counter 128");
    RequireEqual(sceAmprMeasureCommandSizeWriteCounter_04_00(0, 8, 0, store), measureRejected, "write access 8");
    RequireEqual(sceAmprMeasureCommandSizeWriteCounter_04_00(0, size4, 0, 5), measureRejected, "write operation 5");
}};

const Case clearBuffer{"ClearBuffer_BoundBuffer_ReturnsMemoryAndResetsBuffer", [] {
    Recorder recorder;
    RequireEqual(sceAmprCommandBufferNop(&recorder.buffer, 1), 0, "record a nop");
    Require(recorder.Offset() != 0, "offset advanced by the nop");
    RequireEqual(recorder.Commands(), 1u, "commands before clearing");
    const void* cleared = sceAmprCommandBufferClearBuffer(&recorder.buffer);
    RequireEqual(cleared, static_cast<const void*>(recorder.memory.data()), "cleared memory");
    RequireEqual(static_cast<const void*>(recorder.buffer.base), static_cast<const void*>(nullptr), "base after clearing");
    RequireEqual(recorder.buffer.size, 0u, "size after clearing");
    RequireEmpty(recorder, "after clearing");
}};

const Case clearClearedBuffer{"ClearBuffer_AlreadyCleared_ReturnsNull", [] {
    Recorder recorder;
    RequireEqual(sceAmprCommandBufferNop(&recorder.buffer, 1), 0, "record a nop");
    sceAmprCommandBufferClearBuffer(&recorder.buffer);
    RequireEqual(sceAmprCommandBufferClearBuffer(&recorder.buffer), static_cast<void*>(nullptr), "second clear");
}};

const Case constructNop{"ConstructNop_WordAndPayload_RecordsWordPayloadAndPadding", [] {
    Recorder recorder;
    const std::uint8_t payload[5] = {1, 2, 3, 4, 5};
    const std::uint32_t word = 0xCAFEF00Du;
    const auto offset = recorder.Offset();
    const auto commands = recorder.Commands();
    RequireEqual(sceAmprCommandBufferConstructNop(&recorder.buffer, 7, payload, sizeof(payload), &word), 0, "construct nop");
    RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNopWithData(4), "constructed nop");
    const std::uint8_t* data = recorder.memory.data() + offset + sizeof(Apr::CommandHeader);
    const std::uint8_t padding[3] = {};
    Require(std::memcmp(data, &word, sizeof(word)) == 0, "word follows the header");
    Require(std::memcmp(data + 4, payload, sizeof(payload)) == 0, "payload follows the word");
    Require(std::memcmp(data + 9, padding, sizeof(padding)) == 0, "payload is zero padded");
}};

const Case constructLargeNop{"ConstructNop_SixtyBytePayload_RecordsSixteenDwords", [] {
    Recorder recorder;
    const std::array<std::uint8_t, 60> large{};
    RequireEqual(sceAmprCommandBufferNop(&recorder.buffer, 1), 0, "leading nop");
    const auto offset = recorder.Offset();
    const auto commands = recorder.Commands();
    RequireEqual(sceAmprCommandBufferConstructNop(&recorder.buffer, 0, large.data(), 60, nullptr), 0, "construct 60 byte nop");
    RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNopWithData(16), "60 byte nop");
}};

const Case constructEmptyNop{"ConstructNop_EmptyPayload_RecordsOneDword", [] {
    Recorder recorder;
    RequireEqual(sceAmprCommandBufferNop(&recorder.buffer, 1), 0, "leading nop");
    const auto offset = recorder.Offset();
    const auto commands = recorder.Commands();
    RequireEqual(sceAmprCommandBufferConstructNop(&recorder.buffer, 0, nullptr, 0, nullptr), 0, "construct empty nop");
    RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNopWithData(1), "empty nop");
}};

const Case constructMarkers{"ConstructMarker_MarkerTypes_RecordMatchingOpcode", [] {
    Recorder recorder;
    const std::pair<std::uint32_t, Apr::Opcode> markers[] = {{1, Apr::Opcode::SetMarker}, {2, Apr::Opcode::PushMarker}, {5, Apr::Opcode::SetMarker}, {6, Apr::Opcode::PushMarker}};
    for (const auto& [type, opcode] : markers) {
        const auto offset = recorder.Offset();
        const auto commands = recorder.Commands();
        RequireEqual(sceAmprCommandBufferConstructMarker(&recorder.buffer, type, "stream", &color), 0, "construct marker type " + std::to_string(type));
        RequireRecorded(recorder, offset, commands, opcode, sceAmprMeasureCommandSizeSetMarker("stream"), "stream", "marker type " + std::to_string(type));
    }
}};

const Case constructPopMarker{"ConstructMarker_PopType_RecordsPopMarker", [] {
    Recorder recorder;
    RequireEqual(sceAmprCommandBufferConstructMarker(&recorder.buffer, 2, "stream", &color), 0, "construct push marker");
    const auto offset = recorder.Offset();
    const auto commands = recorder.Commands();
    RequireEqual(sceAmprCommandBufferConstructMarker(&recorder.buffer, 3, nullptr, nullptr), 0, "construct pop marker");
    RequireAppended(recorder, offset, commands, Apr::Opcode::PopMarker, sceAmprMeasureCommandSizePopMarker(), "pop marker");
}};

const Case constructedSubmission{"ConstructedCommands_Recorded_SubmitSucceeds", [] {
    Recorder recorder;
    const std::uint8_t payload[5] = {1, 2, 3, 4, 5};
    const std::uint32_t word = 0xCAFEF00Du;
    const std::array<std::uint8_t, 60> large{};
    RequireEqual(sceAmprCommandBufferConstructNop(&recorder.buffer, 7, payload, sizeof(payload), &word), 0, "construct nop with word");
    RequireEqual(sceAmprCommandBufferConstructNop(&recorder.buffer, 0, large.data(), 60, nullptr), 0, "construct 60 byte nop");
    RequireEqual(sceAmprCommandBufferConstructNop(&recorder.buffer, 0, nullptr, 0, nullptr), 0, "construct empty nop");
    for (const std::uint32_t type : {1u, 2u, 5u, 6u}) RequireEqual(sceAmprCommandBufferConstructMarker(&recorder.buffer, type, "stream", &color), 0, "construct marker type " + std::to_string(type));
    RequireEqual(sceAmprCommandBufferConstructMarker(&recorder.buffer, 3, nullptr, nullptr), 0, "construct pop marker");
    RequireEqual(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0), 0, "submit");
}};

const Case rejectedConstructs{"ConstructCommands_InvalidArguments_FailWithEinvalAndRecordNothing", [] {
    Recorder empty;
    const std::array<std::uint8_t, 60> large{};
    const std::uint32_t word = 0xCAFEF00Du;
    RequireEqual(sceAmprCommandBufferConstructNop(&empty.buffer, 0, large.data(), 61, nullptr), invalidArgument, "61 byte payload");
    RequireEqual(sceAmprCommandBufferConstructNop(&empty.buffer, 0, large.data(), 57, &word), invalidArgument, "57 byte payload with word");
    RequireEqual(sceAmprCommandBufferConstructNop(nullptr, 0, large.data(), 4, nullptr), invalidArgument, "nop into null buffer");
    RequireEqual(sceAmprCommandBufferConstructMarker(&empty.buffer, 5, "stream", nullptr), invalidArgument, "marker type 5 without color");
    RequireEqual(sceAmprCommandBufferConstructMarker(&empty.buffer, 6, "stream", nullptr), invalidArgument, "marker type 6 without color");
    RequireEqual(sceAmprCommandBufferConstructMarker(&empty.buffer, 1, nullptr, nullptr), invalidArgument, "marker type 1 without text");
    RequireEqual(sceAmprCommandBufferConstructMarker(&empty.buffer, 0, "stream", &color), invalidArgument, "marker type 0");
    RequireEqual(sceAmprCommandBufferConstructMarker(&empty.buffer, 4, "stream", &color), invalidArgument, "marker type 4");
    RequireEmpty(empty, "after rejected commands");
}};

const Case zeroFilledBuffer{"AprSubmit_ReadFileIntoZeroFilledBuffer_ReadsDataAndSignalsCompletion", [] {
    const GuestFile file("ampr_zero_filled.bin", 256);
    const auto fileId = file.ResolveId();
    AmprEventQueue queue(0, nullptr);

    alignas(8) std::array<std::uint8_t, 256> memory{};
    Apr::CommandBufferObject buffer{};
    std::uint64_t gatherState = 0;
    std::uint64_t scatterState = 0;
    RequireEqual(sceAmprCommandBufferSetBuffer(&buffer, memory.data(), static_cast<std::uint32_t>(memory.size())), 0, "bind the zero filled buffer");
    RequireEqual(sceAmprCommandBufferReset(&buffer), 0, "reset the buffer");
    std::array<std::uint8_t, 32> data{};
    std::uint64_t done = 0;
    RequireEqual(sceAmprAprCommandBufferReadFile(&buffer, &gatherState, &scatterState, fileId, data.data(), data.size(), 64), 0, "record the file read");
    RequireEqual(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 0x1234567), 0, "record the completion write");
    RequireEqual(sceAmprCommandBufferWriteKernelEventQueue_04_00(&buffer, static_cast<std::uint64_t>(queue.Handle()), 0, 0, 0), 0, "record the event write");
    RequireEqual(sceKernelAprSubmitCommandBuffer(&buffer, 1), 0, "submit");
    Require(MatchesFile(data.data(), 64, data.size()), "read 32 bytes at file offset 64");
    RequireEqual(done, std::uint64_t{0x1234567}, "completion value");

    KernelEvent event{};
    int count = 0;
    const KernelUseconds poll = 0;
    RequireEqual(sceKernelWaitEqueue(queue.Handle(), &event, 1, &count, &poll), 0, "poll the event queue");
    RequireEqual(count, 1, "triggered events");
    RequireEqual(sceKernelGetEventFilter(&event), amprEventFilter, "event filter");
    RequireEqual(sceKernelGetEventId(&event), std::uintptr_t{0}, "event id");
    RequireEqual(queue.DeleteEvent(), 0, "delete the AMPR event");
    RequireEqual(queue.Delete(), 0, "delete the event queue");
}};

const Case gatherWithoutRead{"ReadFileGatherAndScatter_WithoutPrecedingRead_FailWithEinval", [] {
    Recorder recorder;
    std::array<std::uint8_t, 64> second{};
    RequireEqual(sceAmprAprCommandBufferReadFileGather(&recorder.buffer, &recorder.gatherState, &recorder.scatterState, 8, 0), invalidArgument, "gather");
    RequireEqual(sceAmprAprCommandBufferReadFileScatter(&recorder.buffer, &recorder.gatherState, &recorder.scatterState, second.data(), 8), invalidArgument, "scatter");
    RequireEqual(sceAmprAprCommandBufferReadFileGatherScatter(&recorder.buffer, &recorder.gatherState, &recorder.scatterState, second.data(), 8, 0), invalidArgument, "gather scatter");
    RequireEqual(recorder.Commands(), 0u, "recorded commands");
}};

const Case gatherAfterReset{"ReadFileGatherAndScatter_AfterResetGatherScatterState_FailWithEinval", [] {
    Recorder recorder;
    std::array<std::uint8_t, 64> first{};
    std::array<std::uint8_t, 64> second{};
    RequireEqual(sceAmprAprCommandBufferReadFile(&recorder.buffer, &recorder.gatherState, &recorder.scatterState, 0, first.data(), 16, 100), 0, "read file");
    RequireEqual(sceAmprAprCommandBufferResetGatherScatterState(&recorder.buffer), 0, "reset gather scatter state");
    RequireEqual(sceAmprAprCommandBufferReadFileGather(&recorder.buffer, &recorder.gatherState, &recorder.scatterState, 8, 0), invalidArgument, "gather after reset");
    RequireEqual(sceAmprAprCommandBufferReadFileScatter(&recorder.buffer, &recorder.gatherState, &recorder.scatterState, second.data(), 8), invalidArgument, "scatter after reset");
}};

const Case gatherScatter{"ReadFileGatherScatter_Submitted_ContinuesFileAndDestinationRanges", [] {
    const GuestFile file("ampr_gather_scatter.bin", 4096);
    const auto fileId = file.ResolveId();

    Recorder recorder;
    auto* buffer = &recorder.buffer;
    auto* map = &recorder.gatherState;
    auto* state = &recorder.scatterState;
    std::array<std::uint8_t, 64> first{};
    std::array<std::uint8_t, 64> second{};
    RequireEqual(sceAmprAprCommandBufferReadFile(buffer, map, state, fileId, first.data(), 16, 100), 0, "read 16 bytes at 100");
    auto offset = recorder.Offset();
    auto commands = recorder.Commands();
    RequireEqual(sceAmprAprCommandBufferReadFileGather(buffer, map, state, 8, 500), 0, "gather 8 bytes at 500");
    RequireAppended(recorder, offset, commands, Apr::Opcode::ReadFileGather, sceAmprMeasureCommandSizeReadFileGather(8, 500), "gather");
    offset = recorder.Offset();
    commands = recorder.Commands();
    RequireEqual(sceAmprAprCommandBufferReadFileScatter(buffer, map, state, second.data(), 8), 0, "scatter 8 bytes");
    RequireAppended(recorder, offset, commands, Apr::Opcode::ReadFileScatter, sceAmprMeasureCommandSizeReadFileScatter(second.data(), 8), "scatter");
    offset = recorder.Offset();
    commands = recorder.Commands();
    RequireEqual(sceAmprAprCommandBufferReadFileGatherScatter(buffer, map, state, second.data() + 32, 4, 1000), 0, "gather scatter 4 bytes at 1000");
    RequireAppended(recorder, offset, commands, Apr::Opcode::ReadFileGatherScatter, sceAmprMeasureCommandSizeReadFileGatherScatter(second.data() + 32, 4, 1000), "gather scatter");
    RequireEqual(sceAmprAprCommandBufferReadFileGather(buffer, map, state, 4, 2000), 0, "gather 4 bytes at 2000");
    RequireEqual(sceAmprAprCommandBufferReadFileScatter(buffer, map, state, first.data() + 40, 6), 0, "scatter 6 bytes");
    offset = recorder.Offset();
    commands = recorder.Commands();
    RequireEqual(sceAmprAprCommandBufferResetGatherScatterState(buffer), 0, "reset gather scatter state");
    RequireAppended(recorder, offset, commands, Apr::Opcode::ResetGatherScatterState, sceAmprMeasureCommandSizeResetGatherScatterState(), "reset gather scatter state");
    RequireEqual(sceAmprAprCommandBufferReadFile(buffer, map, state, fileId, second.data() + 48, 4, 3000), 0, "read 4 bytes at 3000");
    RequireEqual(sceAmprAprCommandBufferReadFileScatter(buffer, map, state, second.data() + 56, 4), 0, "scatter 4 bytes");
    RequireEqual(recorder.Commands(), 9u, "recorded commands");
    SubmitWithin10Seconds(recorder);

    Require(MatchesFile(first.data(), 100, 16), "first[0..16) holds file bytes 100..116");
    Require(MatchesFile(first.data() + 16, 500, 8), "first[16..24) holds file bytes 500..508");
    Require(MatchesFile(second.data(), 508, 8), "second[0..8) holds file bytes 508..516");
    Require(MatchesFile(second.data() + 32, 1000, 4), "second[32..36) holds file bytes 1000..1004");
    Require(MatchesFile(second.data() + 36, 2000, 4), "second[36..40) holds file bytes 2000..2004");
    Require(MatchesFile(first.data() + 40, 2004, 6), "first[40..46) holds file bytes 2004..2010");
    Require(MatchesFile(second.data() + 48, 3000, 4), "second[48..52) holds file bytes 3000..3004");
    Require(MatchesFile(second.data() + 56, 3004, 4), "second[56..60) holds file bytes 3004..3008");
    RequireEqual(first[24], std::uint8_t{0}, "first[24] untouched");
    RequireEqual(first[39], std::uint8_t{0}, "first[39] untouched");
    RequireEqual(second[8], std::uint8_t{0}, "second[8] untouched");
    RequireEqual(second[31], std::uint8_t{0}, "second[31] untouched");
    RequireEqual(second[40], std::uint8_t{0}, "second[40] untouched");
}};

void* const highAddress = reinterpret_cast<void*>(std::uintptr_t{0xF00000000000ull});

const Case rejectedReads{"ReadFile_InvalidArguments_FailsWithEinvalAndRecordsNothing", [] {
    Recorder empty;
    std::array<std::uint8_t, 64> first{};
    auto* map = &empty.gatherState;
    auto* state = &empty.scatterState;
    const std::uint32_t fileId = 0;
    RequireEqual(sceAmprAprCommandBufferReadFile(&empty.buffer, map, state, fileId, first.data(), 0, 0), invalidArgument, "zero length");
    RequireEqual(sceAmprAprCommandBufferReadFile(&empty.buffer, map, state, fileId, first.data(), 0x100000001ull, 0), invalidArgument, "length above 4 GiB");
    RequireEqual(sceAmprAprCommandBufferReadFile(&empty.buffer, map, state, fileId, first.data(), 4, 0x10000000000ull), invalidArgument, "offset at 1 TiB");
    RequireEqual(sceAmprAprCommandBufferReadFile(&empty.buffer, map, state, fileId, highAddress, 4, 0), invalidArgument, "destination above the user limit");
    RequireEqual(sceAmprAprCommandBufferReadFile(nullptr, map, state, fileId, first.data(), 4, 0), invalidArgument, "null buffer");
    RequireEmpty(empty, "after rejected reads");
}};

const Case measureReads{"MeasureReadFileCommands_ValidAndInvalidArguments_ReturnSizeOrEinval", [] {
    std::array<std::uint8_t, 64> first{};
    RequireEqual(sceAmprMeasureCommandSizeReadFileGather(0, 0), measureRejected, "gather zero length");
    RequireEqual(sceAmprMeasureCommandSizeReadFileGather(0x100000000ull, 0x10000000000ull - 1u), sizeof(Apr::ReadFileCommand), "gather 4 GiB below 1 TiB");
    RequireEqual(sceAmprMeasureCommandSizeReadFileGather(4, 0x10000000000ull), measureRejected, "gather offset at 1 TiB");
    RequireEqual(sceAmprMeasureCommandSizeReadFileScatter(first.data(), 0x100000001ull), measureRejected, "scatter length above 4 GiB");
    RequireEqual(sceAmprMeasureCommandSizeReadFileScatter(highAddress, 4), measureRejected, "scatter above the user limit");
    RequireEqual(sceAmprMeasureCommandSizeReadFileGatherScatter(first.data(), 4, 0x10000000000ull), measureRejected, "gather scatter offset at 1 TiB");
    RequireEqual(sceAmprMeasureCommandSizeReadFileGatherScatter(nullptr, 4, 0), sizeof(Apr::ReadFileCommand), "gather scatter null destination");
}};

volatile std::uint64_t& At(std::uint64_t address) {
    return *reinterpret_cast<volatile std::uint64_t*>(address);
}

std::uint64_t Load(std::uint64_t address) {
    return At(address);
}

int Protection(std::uint64_t address) {
    int protection = -1;
    RequireEqual(sceKernelQueryMemoryProtection(reinterpret_cast<void*>(address), nullptr, nullptr, &protection), 0, "query memory protection");
    return protection;
}

void SubmitAmm(Recorder& recorder) {
    std::uint32_t id = 0;
    RequireEqual(sceAmprAmmSubmitCommandBuffer3(recorder.memory.data(), recorder.Offset(), 0, &id), 0, "submit the AMM command buffer");
    RequireEqual(sceAmprAmmWaitCommandBufferCompletion(id), 0, "wait for the AMM submission");
}

struct AmmRanges {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
};

AmmRanges QueryRanges() {
    AmmRanges ranges;
    RequireEqual(sceAmprAmmGetVirtualAddressRanges(&ranges.start, &ranges.end, &ranges.multimapStart, &ranges.multimapEnd), 0, "query AMM ranges");
    return ranges;
}

std::int64_t DirectMemorySize() {
    return static_cast<std::int64_t>(sceKernelGetDirectMemorySize());
}

std::int64_t GiveDirectMemory(std::uint64_t bytes, int usage) {
    std::int64_t offset = -1;
    RequireEqual(sceAmprAmmGiveDirectMemory(0, DirectMemorySize(), bytes, page, usage, &offset), 0, "give " + std::to_string(bytes) + " bytes of direct memory with usage " + std::to_string(usage));
    Require(offset >= 0, "direct memory offset is valid");
    return offset;
}

const Case ammRanges{"AmmGetVirtualAddressRanges_Query_ReturnsOrderedPageAlignedRanges", [] {
    const auto ranges = QueryRanges();
    Require(ranges.start != 0, "start is not null");
    RequireEqual(ranges.start % page, std::uint64_t{0}, "start is page aligned");
    Require(ranges.start < ranges.end, "start precedes end");
    Require(ranges.end <= ranges.multimapStart, "multimap range follows the map range");
    Require(ranges.multimapStart < ranges.multimapEnd, "multimap start precedes multimap end");
}};

const Case ammGiveInvalid{"AmmGiveDirectMemory_InvalidUsageOrNullOffset_FailsWithEinval", [] {
    std::int64_t direct = -1;
    RequireEqual(sceAmprAmmGiveDirectMemory(0, DirectMemorySize(), page, page, 2, &direct), invalidArgument, "usage 2");
    RequireEqual(sceAmprAmmGiveDirectMemory(0, DirectMemorySize(), page, page, 1, nullptr), invalidArgument, "null offset");
}};

const Case ammMapping{"AmmSubmit_MapUnmapAndAprMapSequence_MapsAccessibleSharedMemory", [] {
    const auto start = QueryRanges().start;
    GiveDirectMemory(7 * page, 1);
    const auto direct = GiveDirectMemory(2 * page, 0);

    const std::uint64_t automatic = start;
    const std::uint64_t first = start + 0x10000;
    const std::uint64_t alias = start + 0x20000;
    const std::uint64_t reused = start + 0x30000;
    const std::uint64_t region = start + 0x60000;
    const std::uint64_t directRegion = start + 0x70000;

    Recorder maps;
    RequireEqual(sceAmprAmmCommandBufferConstructor(&maps.buffer), 0, "construct the AMM command buffer");
    auto offset = maps.Offset();
    auto commands = maps.Commands();
    RequireEqual(sceAmprAmmCommandBufferMap(&maps.buffer, automatic, 2 * page, 0, cpuReadWrite), 0, "map automatic");
    RequireAppended(maps, offset, commands, Apr::Opcode::AmmMap, sceAmprAmmMeasureAmmCommandSizeMap(automatic, 2 * page, 0, cpuReadWrite), "map");
    offset = maps.Offset();
    commands = maps.Commands();
    RequireEqual(sceAmprAmmCommandBufferMapDirect(&maps.buffer, first, direct, 2 * page, 0, cpuGpuReadWrite), 0, "map direct");
    RequireAppended(maps, offset, commands, Apr::Opcode::AmmMapDirect, sceAmprAmmMeasureAmmCommandSizeMapDirect(first, direct, 2 * page, 0, cpuGpuReadWrite), "map direct");
    RequireEqual(sceAmprAmmCommandBufferMapDirectWithGpuMaskId(&maps.buffer, alias, direct, 2 * page, 0, amprReadWrite, 3), 0, "map direct alias");
    std::uint32_t id = 0;
    RequireEqual(sceAmprAmmSubmitCommandBuffer3(maps.memory.data(), maps.Offset(), 0, &id), 0, "submit maps");
    Require(id != 0, "submission id is not zero");
    RequireEqual(sceAmprAmmWaitCommandBufferCompletion(id), 0, "wait for maps");
    RequireEqual(sceAmprAmmWaitCommandBufferCompletion(id + 1000), noSuchSubmission, "wait for an unknown submission");
    At(automatic) = 0x1111;
    At(automatic + 2 * page - 8) = 0x2222;
    RequireEqual(Load(automatic), std::uint64_t{0x1111}, "automatic first word");
    RequireEqual(Load(automatic + 2 * page - 8), std::uint64_t{0x2222}, "automatic last word");
    At(first + page) = 0x3333;
    RequireEqual(Load(alias + page), std::uint64_t{0x3333}, "alias sees the direct write");
    At(alias) = 0x4444;
    RequireEqual(Load(first), std::uint64_t{0x4444}, "direct mapping sees the alias write");

    Recorder remaps;
    RequireEqual(sceAmprAmmCommandBufferConstructor(&remaps.buffer), 0, "construct the AMM command buffer");
    offset = remaps.Offset();
    commands = remaps.Commands();
    RequireEqual(sceAmprAmmCommandBufferUnmap(&remaps.buffer, automatic, 2 * page), 0, "unmap automatic");
    RequireAppended(remaps, offset, commands, Apr::Opcode::AmmUnmap, sceAmprAmmMeasureAmmCommandSizeUnmap(automatic, 2 * page), "unmap");
    RequireEqual(sceAmprAmmCommandBufferMapWithGpuMaskId(&remaps.buffer, reused, 6 * page, 0, cpuReadWrite, 1), 0, "map reused pool pages");
    std::uint64_t result = ~0ull;
    RequireEqual(sceAmprAmmSubmitCommandBuffer2(remaps.memory.data(), remaps.Offset(), 0, &result, &id), 0, "submit remaps");
    RequireEqual(result, std::uint64_t{0}, "submission result");
    RequireEqual(sceAmprAmmWaitCommandBufferCompletion(id), 0, "wait for remaps");
    At(reused + 6 * page - 8) = 0x5555;
    RequireEqual(Load(reused + 6 * page - 8), std::uint64_t{0x5555}, "reused last word");

    Recorder apr;
    alignas(8) std::uint64_t done = 0;
    offset = apr.Offset();
    commands = apr.Commands();
    RequireEqual(sceAmprAprCommandBufferMapBegin(&apr.buffer, region, page, 0, amprReadWrite), 0, "begin map");
    RequireAppended(apr, offset, commands, Apr::Opcode::AmmMap, sceAmprMeasureCommandSizeMapBegin(region, page, 0, amprReadWrite), "begin map");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&apr.buffer, 30, 1, 5, 0, 1), 0, "write counter at start inside a map");
    offset = apr.Offset();
    commands = apr.Commands();
    RequireEqual(sceAmprAprCommandBufferMapEnd(&apr.buffer), 0, "end map");
    RequireAppended(apr, offset, commands, Apr::Opcode::MapEnd, sceAmprMeasureCommandSizeMapEnd(), "end map");
    RequireEqual(sceAmprAprCommandBufferMapDirectBegin(&apr.buffer, directRegion, direct, page, 0, cpuReadWrite), 0, "begin direct map");
    RequireEqual(sceAmprAprCommandBufferMapEnd(&apr.buffer), 0, "end direct map");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounterOnCompletion(&apr.buffer, &done, 30), 0, "write counter 30 on completion");
    SubmitWithin10Seconds(apr);
    RequireEqual(done, std::uint64_t{5}, "counter 30");
    At(region + page - 8) = 0x6666;
    RequireEqual(Load(region + page - 8), std::uint64_t{0x6666}, "APR mapped region last word");
    RequireEqual(Load(directRegion), std::uint64_t{0x4444}, "APR direct map shares the direct memory");
}};

const Case ammRemapAndProtect{"AmmSubmit_RemapMultiMapAndProtectSequence_MovesSharesAndProtectsPages", [] {
    const auto ranges = QueryRanges();
    const auto start = ranges.start;
    GiveDirectMemory(4 * page, 1);
    const auto direct = GiveDirectMemory(page, 0);

    const std::uint64_t source = start + 0x100000;
    const std::uint64_t directSource = start + 0x140000;
    const std::uint64_t moved = start + 0x180000;
    const std::uint64_t alias = ranges.multimapStart + 0x100000;
    const std::uint64_t directAlias = ranges.multimapStart + 0x140000;
    const std::uint64_t fresh = start + 0x1C0000;
    const std::uint64_t returned = start + 0x200000;

    Recorder maps;
    RequireEqual(sceAmprAmmCommandBufferMap(&maps.buffer, source, 2 * page, 0, cpuReadWrite), 0, "map source");
    RequireEqual(sceAmprAmmCommandBufferMapDirect(&maps.buffer, directSource, direct, page, 0, cpuReadWrite), 0, "map direct source");
    SubmitAmm(maps);
    At(source) = 0xA1;
    At(source + page) = 0xA2;
    At(directSource) = 0xD1;

    Recorder moves;
    auto offset = moves.Offset();
    auto commands = moves.Commands();
    RequireEqual(sceAmprAmmCommandBufferRemap(&moves.buffer, moved, source, 2 * page, cpuReadWrite), 0, "remap source");
    RequireAppended(moves, offset, commands, Apr::Opcode::AmmRemap, sceAmprAmmMeasureAmmCommandSizeRemap(moved, source, 2 * page, cpuReadWrite), "remap");
    offset = moves.Offset();
    commands = moves.Commands();
    RequireEqual(sceAmprAmmCommandBufferMultiMap(&moves.buffer, alias, moved, 2 * page, cpuReadWrite), 0, "multimap moved");
    RequireAppended(moves, offset, commands, Apr::Opcode::AmmMultiMap, sceAmprAmmMeasureAmmCommandSizeMultiMap(alias, moved, 2 * page, cpuReadWrite), "multimap");
    RequireEqual(sceAmprAmmCommandBufferMultiMapWithGpuMaskId(&moves.buffer, directAlias, directSource, page, amprReadWrite, 2), 0, "multimap direct source");
    SubmitAmm(moves);
    RequireEqual(Load(moved), std::uint64_t{0xA1}, "moved first page");
    RequireEqual(Load(moved + page), std::uint64_t{0xA2}, "moved second page");
    RequireEqual(Load(alias + page), std::uint64_t{0xA2}, "alias second page");
    At(alias) = 0xB1;
    RequireEqual(Load(moved), std::uint64_t{0xB1}, "moved sees the alias write");
    RequireEqual(Load(directAlias), std::uint64_t{0xD1}, "direct alias");

    Recorder shared;
    RequireEqual(sceAmprAmmCommandBufferUnmap(&shared.buffer, moved, 2 * page), 0, "unmap moved");
    RequireEqual(sceAmprAmmCommandBufferMap(&shared.buffer, fresh, 2 * page, 0, cpuReadWrite), 0, "map fresh");
    SubmitAmm(shared);
    At(fresh) = 0xC1;
    At(fresh + page) = 0xC2;
    RequireEqual(Load(alias), std::uint64_t{0xB1}, "alias keeps its first page");
    RequireEqual(Load(alias + page), std::uint64_t{0xA2}, "alias keeps its second page");

    Recorder released;
    RequireEqual(sceAmprAmmCommandBufferUnmap(&released.buffer, alias, 2 * page), 0, "unmap alias");
    RequireEqual(sceAmprAmmCommandBufferRemapWithGpuMaskId(&released.buffer, returned, fresh, 2 * page, cpuReadWrite, 1), 0, "remap fresh");
    RequireEqual(sceAmprAmmCommandBufferMap(&released.buffer, fresh, 2 * page, 0, cpuReadWrite), 0, "map fresh again");
    SubmitAmm(released);
    RequireEqual(Load(returned), std::uint64_t{0xC1}, "returned first page");
    RequireEqual(Load(returned + page), std::uint64_t{0xC2}, "returned second page");
    At(fresh) = 0xE1;
    RequireEqual(Load(returned), std::uint64_t{0xC1}, "returned is independent of the new fresh mapping");

    Recorder protects;
    offset = protects.Offset();
    commands = protects.Commands();
    RequireEqual(sceAmprAmmCommandBufferModifyProtect(&protects.buffer, returned, 2 * page, 0x01, 0x02), 0, "modify protect");
    RequireAppended(protects, offset, commands, Apr::Opcode::AmmModifyProtect, sceAmprAmmMeasureAmmCommandSizeModifyProtect(returned, 2 * page, 0x01, 0x02), "modify protect");
    offset = protects.Offset();
    commands = protects.Commands();
    RequireEqual(sceAmprAmmCommandBufferModifyMtypeProtect(&protects.buffer, returned, page, 3, 0x22, 0x22), 0, "modify mtype protect");
    RequireAppended(protects, offset, commands, Apr::Opcode::AmmModifyMtypeProtect, sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtect(returned, page, 3, 0x22, 0x22), "modify mtype protect");
    RequireEqual(sceAmprAmmCommandBufferModifyProtectWithGpuMaskId(&protects.buffer, directAlias, page, 0x00, 0x80, 0), 0, "modify direct alias protect");
    RequireEqual(sceAmprAmmCommandBufferModifyMtypeProtectWithGpuMaskId(&protects.buffer, fresh, page, 1, 0x10, 0x13, 0), 0, "modify fresh mtype protect");
    SubmitAmm(protects);
    RequireEqual(Protection(returned), 0x23, "returned first page protection");
    RequireEqual(Protection(returned + page), 0x01, "returned second page protection");
    RequireEqual(Protection(directAlias), 0x01, "direct alias protection");
    RequireEqual(Protection(fresh), 0x10, "fresh first page protection");
    RequireEqual(Protection(fresh + page), 0x03, "fresh second page protection");
    At(returned) = 0xF1;
    RequireEqual(Load(returned), std::uint64_t{0xF1}, "returned first page after protect");
    RequireEqual(Load(returned + page), std::uint64_t{0xC2}, "returned second page after protect");
}};

const Case ammPrt{"AmmSubmit_PrtReserveBackReleaseAndRebuild_TracksResidentPages", [] {
    const auto start = QueryRanges().start;
    GiveDirectMemory(4 * page, 1);

    const std::uint64_t prt = start + 0x300000;
    const std::uint64_t source = start + 0x380000;

    Recorder reserve;
    auto offset = reserve.Offset();
    auto commands = reserve.Commands();
    RequireEqual(sceAmprAmmCommandBufferMapAsPrt(&reserve.buffer, prt, 4 * page), 0, "map as PRT");
    RequireAppended(reserve, offset, commands, Apr::Opcode::AmmMapAsPrt, sceAmprAmmMeasureAmmCommandSizeMapAsPrt(prt, 4 * page), "map as PRT");
    RequireEqual(sceAmprAmmCommandBufferMap(&reserve.buffer, source, page, 0, cpuReadWrite), 0, "map source");
    SubmitAmm(reserve);
    RequireEqual(Protection(prt), 0x10, "PRT first page protection");
    RequireEqual(Protection(prt + 3 * page), 0x10, "PRT last page protection");
    RequireEqual(Load(prt), std::uint64_t{0}, "PRT first word");
    RequireEqual(Load(prt + 4 * page - 8), std::uint64_t{0}, "PRT last word");
    At(source) = 0x88;

    Recorder back;
    offset = back.Offset();
    commands = back.Commands();
    RequireEqual(sceAmprAmmCommandBufferAllocatePaForPrt(&back.buffer, prt + page, 2 * page, 0, cpuReadWrite), 0, "allocate PRT pages 1 and 2");
    RequireAppended(back, offset, commands, Apr::Opcode::AmmAllocatePaForPrt, sceAmprAmmMeasureAmmCommandSizeAllocatePaForPrt(prt + page, 2 * page, 0, cpuReadWrite), "allocate PRT");
    RequireEqual(sceAmprAmmCommandBufferAllocatePaForPrt(&back.buffer, prt + 2 * page, page, 2, cpuGpuReadWrite), 0, "allocate PRT page 2 again");
    RequireEqual(sceAmprAmmCommandBufferRemapIntoPrt(&back.buffer, prt + 3 * page, source, page, cpuReadWrite, 0), 0, "remap source into PRT page 3");
    SubmitAmm(back);
    RequireEqual(Protection(prt), 0x10, "PRT page 0 protection");
    RequireEqual(Protection(prt + page), 0x03, "PRT page 1 protection");
    RequireEqual(Protection(prt + 2 * page), 0x33, "PRT page 2 protection");
    RequireEqual(Protection(prt + 3 * page), 0x03, "PRT page 3 protection");
    At(prt + page) = 0x71;
    At(prt + 2 * page) = 0x72;
    RequireEqual(Load(prt + page), std::uint64_t{0x71}, "PRT page 1");
    RequireEqual(Load(prt + 2 * page), std::uint64_t{0x72}, "PRT page 2");
    RequireEqual(Load(prt + 3 * page), std::uint64_t{0x88}, "PRT page 3 holds the source");
    RequireEqual(Load(prt), std::uint64_t{0}, "PRT page 0");

    Recorder release;
    offset = release.Offset();
    commands = release.Commands();
    RequireEqual(sceAmprAmmCommandBufferUnmapToPrt(&release.buffer, prt + page, 2 * page), 0, "unmap PRT pages 1 and 2");
    RequireAppended(release, offset, commands, Apr::Opcode::AmmUnmapToPrt, sceAmprAmmMeasureAmmCommandSizeUnmap(prt + page, 2 * page), "unmap to PRT");
    SubmitAmm(release);
    RequireEqual(Protection(prt + page), 0x10, "released page protection");
    RequireEqual(Load(prt + page), std::uint64_t{0}, "released page 1");
    RequireEqual(Load(prt + 2 * page), std::uint64_t{0}, "released page 2");
    RequireEqual(Load(prt + 3 * page), std::uint64_t{0x88}, "page 3 keeps the source");

    Recorder rebuild;
    RequireEqual(sceAmprAmmCommandBufferAllocatePaForPrt(&rebuild.buffer, prt, 3 * page, 0, cpuReadWrite), 0, "allocate PRT pages 0 to 2");
    SubmitAmm(rebuild);
    for (std::uint64_t index = 0; index < 3; ++index) {
        RequireEqual(Protection(prt + index * page), 0x03, "rebuilt page " + std::to_string(index) + " protection");
        At(prt + index * page) = 0x90 + index;
    }
    RequireEqual(Load(prt), std::uint64_t{0x90}, "rebuilt page 0");
    RequireEqual(Load(prt + page), std::uint64_t{0x91}, "rebuilt page 1");
    RequireEqual(Load(prt + 2 * page), std::uint64_t{0x92}, "rebuilt page 2");
    RequireEqual(Load(prt + 3 * page), std::uint64_t{0x88}, "page 3 keeps the source");
}};

const Case ammRejectedMaps{"AmmMapCommands_InvalidArguments_FailWithEinvalAndRecordNothing", [] {
    const auto start = QueryRanges().start;
    const auto direct = GiveDirectMemory(page, 0);
    Recorder empty;
    RequireEqual(sceAmprAmmCommandBufferMap(&empty.buffer, start, page, 0, 0x04), invalidArgument, "map protection 0x04");
    RequireEqual(sceAmprAmmCommandBufferMap(&empty.buffer, start, page, 0, 0x400), invalidArgument, "map protection 0x400");
    RequireEqual(sceAmprAmmCommandBufferMap(&empty.buffer, start + 0x1000, page, 0, cpuReadWrite), invalidArgument, "map misaligned address");
    RequireEqual(sceAmprAmmCommandBufferMap(&empty.buffer, start, page + 0x1000, 0, cpuReadWrite), invalidArgument, "map misaligned size");
    RequireEqual(sceAmprAmmCommandBufferMap(&empty.buffer, start, 0, 0, cpuReadWrite), invalidArgument, "map zero size");
    RequireEqual(sceAmprAmmCommandBufferMap(&empty.buffer, 0, page, 0, cpuReadWrite), invalidArgument, "map null address");
    RequireEqual(sceAmprAmmCommandBufferMap(&empty.buffer, ~(page - 1), 2 * page, 0, cpuReadWrite), invalidArgument, "map wrapping range");
    RequireEqual(sceAmprAmmCommandBufferMapDirect(&empty.buffer, start, direct + 0x1000, page, 0, cpuReadWrite), invalidArgument, "map misaligned direct offset");
    RequireEqual(sceAmprAmmCommandBufferUnmap(&empty.buffer, start + 8, page), invalidArgument, "unmap misaligned address");
    RequireEqual(sceAmprAmmCommandBufferMap(nullptr, start, page, 0, cpuReadWrite), invalidArgument, "map into null buffer");
    RequireEqual(sceAmprAprCommandBufferMapBegin(&empty.buffer, start, page, 0, 0x04), invalidArgument, "begin map protection 0x04");
    RequireEqual(sceAmprAprCommandBufferMapEnd(nullptr), invalidArgument, "end map on null buffer");
    RequireEmpty(empty, "after rejected maps");
}};

const Case ammUnboundOrFullMaps{"AmmMapCommands_UnboundOrFullBuffer_FailWithEpermOrEbusy", [] {
    const auto start = QueryRanges().start;
    Apr::CommandBufferObject unbound{};
    RequireEqual(sceAmprCommandBufferConstructor(&unbound), 0, "construct the command buffer");
    RequireEqual(sceAmprAmmCommandBufferMap(&unbound, start, page, 0, cpuReadWrite), permissionDenied, "map into unbound buffer");
    RequireEqual(sceAmprAmmCommandBufferUnmap(&unbound, start, page), permissionDenied, "unmap into unbound buffer");
    const auto measured = static_cast<std::uint32_t>(sceAmprAmmMeasureAmmCommandSizeUnmap(start, page));
    Recorder exact(measured);
    RequireEqual(sceAmprAmmCommandBufferUnmap(&exact.buffer, start, page), 0, "unmap filling the buffer");
    RequireEqual(sceAmprAmmCommandBufferUnmap(&exact.buffer, start, page), busy, "unmap into full buffer");
    RequireEqual(sceAmprAmmCommandBufferMap(&exact.buffer, start, page, 0, cpuReadWrite), busy, "map into full buffer");
}};

const Case ammSubmitInvalid{"AmmSubmitCommandBuffer_NullBaseOrPriorityAboveTwo_Fails", [] {
    Recorder recorder;
    RequireEqual(sceAmprAmmSubmitCommandBuffer(nullptr, 0, 0), permissionDenied, "null base");
    RequireEqual(sceAmprAmmSubmitCommandBuffer(recorder.memory.data(), 0, 3), invalidArgument, "priority 3");
    RequireEqual(sceAmprAmmSubmitCommandBuffer3(nullptr, 0, 3, nullptr), invalidArgument, "priority 3 with null base");
}};

const Case ammSubmitEmpty{"AmmSubmitCommandBuffer_EmptyBufferPriorityTwo_Succeeds", [] {
    Recorder recorder;
    RequireEqual(sceAmprAmmSubmitCommandBuffer(recorder.memory.data(), 0, 2), 0, "empty submission");
}};

const Case ammMeasureMaps{"MeasureAmmMapCommands_ValidAndInvalidArguments_ReturnSizeOrEinval", [] {
    const auto start = QueryRanges().start;
    const auto direct = GiveDirectMemory(page, 0);
    const auto directOffset = static_cast<std::uint64_t>(direct);
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeMap(start, page, 0, 0x04), ammRejected, "map protection 0x04");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeMapWithGpuMaskId(start + 8, page, 0, cpuReadWrite, 0), ammRejected, "map misaligned address");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeMapWithGpuMaskId(start, page, 0, cpuReadWrite, 0), std::int64_t{sizeof(Apr::AmmMapCommand)}, "map");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeMapDirect(start, directOffset + 8, page, 0, cpuReadWrite), ammRejected, "map misaligned direct offset");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeMapDirectWithGpuMaskId(start, directOffset, page, 0, cpuReadWrite, 0), std::int64_t{sizeof(Apr::AmmMapCommand)}, "map direct");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeUnmap(start, 0), ammRejected, "unmap zero size");
    RequireEqual(sceAmprMeasureCommandSizeMapBegin(start, page, 0, 0x04), measureRejected, "begin map protection 0x04");
    RequireEqual(sceAmprMeasureCommandSizeMapDirectBegin(start, directOffset + 8, page, 0, cpuReadWrite), measureRejected, "begin map misaligned direct offset");
    RequireEqual(sceAmprMeasureCommandSizeMapDirectBegin(start, directOffset, page, 0, cpuReadWrite), sizeof(Apr::AmmMapCommand), "begin direct map");
}};

const Case aprMapBeginInsideMap{"AprCommandBufferMapBegin_InsideOpenMap_RejectsNestedMapsAndCompletionCommands", [] {
    const auto start = QueryRanges().start;
    const auto direct = GiveDirectMemory(page, 0);
    const std::uint64_t region = start + 0x60000;
    const std::uint64_t directRegion = start + 0x70000;
    Recorder apr;
    alignas(8) std::uint64_t done = 0;
    RequireEqual(sceAmprAprCommandBufferMapBegin(&apr.buffer, region, page, 0, amprReadWrite), 0, "begin map");
    RequireEqual(sceAmprAprCommandBufferMapBegin(&apr.buffer, region, page, 0, amprReadWrite), permissionDenied, "nested begin map");
    RequireEqual(sceAmprAprCommandBufferMapDirectBegin(&apr.buffer, directRegion, direct, page, 0, amprReadWrite), permissionDenied, "nested begin direct map");
    RequireEqual(sceAmprCommandBufferWriteAddressOnCompletion(&apr.buffer, &done, 1), permissionDenied, "write address on completion");
    RequireEqual(sceAmprCommandBufferWriteCounterOnCompletion(&apr.buffer, 30, 1), permissionDenied, "write counter on completion");
    RequireEqual(sceAmprCommandBufferWriteKernelEventQueueOnCompletion(&apr.buffer, 1, 1, 0), permissionDenied, "write kernel event queue on completion");
    RequireEqual(sceAmprCommandBufferWriteAddressFromCounter_04_00(&apr.buffer, &done, 30, 0), permissionDenied, "write address from counter on completion");
    RequireEqual(sceAmprCommandBufferWriteCounter_04_00(&apr.buffer, 30, 1, 5, 0, 0), permissionDenied, "write counter on completion");
    RequireEqual(apr.Commands(), 1u, "only the begin map is recorded");
}};

const Case aprMapEndWithoutMap{"AprCommandBufferMapEnd_WithoutOpenMap_FailsWithEperm", [] {
    const std::uint64_t region = QueryRanges().start + 0x60000;
    Recorder apr;
    RequireEqual(sceAmprAprCommandBufferMapBegin(&apr.buffer, region, page, 0, amprReadWrite), 0, "begin map");
    RequireEqual(sceAmprAprCommandBufferMapEnd(&apr.buffer), 0, "end map");
    RequireEqual(sceAmprAprCommandBufferMapEnd(&apr.buffer), permissionDenied, "second end map");
}};

const Case ammRejectedRemaps{"AmmRemapAndProtectCommands_InvalidArguments_FailWithEinvalAndRecordNothing", [] {
    const auto ranges = QueryRanges();
    const std::uint64_t source = ranges.start + 0x100000;
    const std::uint64_t moved = ranges.start + 0x180000;
    const std::uint64_t alias = ranges.multimapStart + 0x100000;
    const std::uint64_t returned = ranges.start + 0x200000;
    Recorder empty;
    RequireEqual(sceAmprAmmCommandBufferRemap(&empty.buffer, moved, source + 8, page, cpuReadWrite), invalidArgument, "remap misaligned source");
    RequireEqual(sceAmprAmmCommandBufferRemap(&empty.buffer, moved + 8, source, page, cpuReadWrite), invalidArgument, "remap misaligned address");
    RequireEqual(sceAmprAmmCommandBufferRemap(&empty.buffer, moved, source, 0, cpuReadWrite), invalidArgument, "remap zero size");
    RequireEqual(sceAmprAmmCommandBufferMultiMap(&empty.buffer, alias, moved, page, 0x04), invalidArgument, "multimap protection 0x04");
    RequireEqual(sceAmprAmmCommandBufferModifyProtect(&empty.buffer, returned, page, 0x04, 0x03), invalidArgument, "modify protection 0x04");
    RequireEqual(sceAmprAmmCommandBufferModifyProtect(&empty.buffer, returned, page, 0x03, 0x04), invalidArgument, "modify protect mask 0x04");
    RequireEqual(sceAmprAmmCommandBufferModifyMtypeProtect(&empty.buffer, returned + 8, page, 0, 0x03, 0x03), invalidArgument, "modify mtype misaligned address");
    RequireEqual(sceAmprAmmCommandBufferRemap(nullptr, moved, source, page, cpuReadWrite), invalidArgument, "remap into null buffer");
    RequireEmpty(empty, "after rejected commands");
}};

const Case ammUnboundOrFullRemaps{"AmmRemapAndProtectCommands_UnboundOrFullBuffer_FailWithEpermOrEbusy", [] {
    const auto ranges = QueryRanges();
    const std::uint64_t source = ranges.start + 0x100000;
    const std::uint64_t moved = ranges.start + 0x180000;
    const std::uint64_t alias = ranges.multimapStart + 0x100000;
    const std::uint64_t returned = ranges.start + 0x200000;
    Apr::CommandBufferObject unbound{};
    RequireEqual(sceAmprCommandBufferConstructor(&unbound), 0, "construct the command buffer");
    RequireEqual(sceAmprAmmCommandBufferMultiMap(&unbound, alias, moved, page, cpuReadWrite), permissionDenied, "multimap into unbound buffer");
    RequireEqual(sceAmprAmmCommandBufferModifyProtect(&unbound, returned, page, 0x03, 0x03), permissionDenied, "modify protect into unbound buffer");
    const auto measured = static_cast<std::uint32_t>(sceAmprAmmMeasureAmmCommandSizeRemap(moved, source, page, cpuReadWrite));
    Recorder exact(measured);
    RequireEqual(sceAmprAmmCommandBufferRemap(&exact.buffer, moved, source, page, cpuReadWrite), 0, "remap filling the buffer");
    RequireEqual(sceAmprAmmCommandBufferModifyProtect(&exact.buffer, returned, page, 0x03, 0x03), busy, "modify protect into full buffer");
}};

const Case ammMeasureRemaps{"MeasureAmmRemapAndProtectCommands_ValidAndInvalidArguments_ReturnSizeOrEinval", [] {
    const auto ranges = QueryRanges();
    const std::uint64_t source = ranges.start + 0x100000;
    const std::uint64_t moved = ranges.start + 0x180000;
    const std::uint64_t alias = ranges.multimapStart + 0x100000;
    const std::uint64_t returned = ranges.start + 0x200000;
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeRemapWithGpuMaskId(moved, source + 8, page, cpuReadWrite, 0), ammRejected, "remap misaligned source");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeRemapWithGpuMaskId(moved, source, page, cpuReadWrite, 0), std::int64_t{sizeof(Apr::AmmRemapCommand)}, "remap");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeMultiMapWithGpuMaskId(alias, moved, page, 0x400, 0), ammRejected, "multimap protection 0x400");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeMultiMapWithGpuMaskId(alias, moved, page, cpuReadWrite, 0), std::int64_t{sizeof(Apr::AmmRemapCommand)}, "multimap");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeModifyProtectWithGpuMaskId(returned, page, 0x03, 0x04, 0), ammRejected, "modify protect mask 0x04");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeModifyProtectWithGpuMaskId(returned, page, 0x03, 0x03, 0), std::int64_t{sizeof(Apr::AmmProtectCommand)}, "modify protect");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtectWithGpuMaskId(returned, 0, 0, 0x03, 0x03, 0), ammRejected, "modify mtype zero size");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtectWithGpuMaskId(returned, page, 0, 0x03, 0x03, 0), std::int64_t{sizeof(Apr::AmmProtectCommand)}, "modify mtype protect");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeMultiMap(alias, moved, 0, cpuReadWrite), ammRejected, "multimap zero size");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeModifyMtypeProtect(returned, page, 0, 0x08, 0x03), ammRejected, "modify mtype protection 0x08");
}};

const Case ammRejectedPrt{"AmmPrtCommands_InvalidArguments_FailWithEinvalAndRecordNothing", [] {
    const auto start = QueryRanges().start;
    const std::uint64_t prt = start + 0x300000;
    const std::uint64_t source = start + 0x380000;
    Recorder empty;
    RequireEqual(sceAmprAmmCommandBufferMapAsPrt(&empty.buffer, prt + 8, page), invalidArgument, "map as PRT misaligned address");
    RequireEqual(sceAmprAmmCommandBufferMapAsPrt(nullptr, prt, page), invalidArgument, "map as PRT into null buffer");
    RequireEqual(sceAmprAmmCommandBufferAllocatePaForPrt(&empty.buffer, prt, page, 0, 0x04), invalidArgument, "allocate PRT protection 0x04");
    RequireEqual(sceAmprAmmCommandBufferAllocatePaForPrt(nullptr, prt, page, 0, 0x404), invalidArgument, "allocate PRT protection 0x404 into null buffer");
    RequireEqual(sceAmprAmmCommandBufferAllocatePaForPrt(&empty.buffer, prt, 0, 0, cpuReadWrite), invalidArgument, "allocate PRT zero size");
    RequireEqual(sceAmprAmmCommandBufferRemapIntoPrt(&empty.buffer, prt, source + 8, page, cpuReadWrite, 0), invalidArgument, "remap into PRT misaligned source");
    RequireEqual(sceAmprAmmCommandBufferRemapIntoPrt(&empty.buffer, prt, source, page, 0x04, 0), invalidArgument, "remap into PRT protection 0x04");
    RequireEqual(sceAmprAmmCommandBufferUnmapToPrt(&empty.buffer, prt, page + 8), invalidArgument, "unmap to PRT misaligned size");
    RequireEmpty(empty, "after rejected commands");
}};

const Case ammUnboundPrt{"AmmPrtCommands_UnboundBuffer_FailWithEbusyOrEperm", [] {
    const auto start = QueryRanges().start;
    const std::uint64_t prt = start + 0x300000;
    const std::uint64_t source = start + 0x380000;
    Apr::CommandBufferObject unbound{};
    RequireEqual(sceAmprCommandBufferConstructor(&unbound), 0, "construct the command buffer");
    RequireEqual(sceAmprAmmCommandBufferMapAsPrt(&unbound, prt, page), busy, "map as PRT into unbound buffer");
    RequireEqual(sceAmprAmmCommandBufferAllocatePaForPrt(&unbound, prt, page, 0, cpuReadWrite), busy, "allocate PRT into unbound buffer");
    RequireEqual(sceAmprAmmCommandBufferRemapIntoPrt(&unbound, prt, source, page, cpuReadWrite, 0), permissionDenied, "remap into PRT into unbound buffer");
    RequireEqual(sceAmprAmmCommandBufferUnmapToPrt(&unbound, prt, page), permissionDenied, "unmap to PRT into unbound buffer");
}};

const Case ammSizedWithoutBasePrt{"AmmCommandBufferMapAsPrt_SizedBufferWithoutBase_FailsWithEperm", [] {
    const std::uint64_t prt = QueryRanges().start + 0x300000;
    std::array<std::uint8_t, 64> loose{};
    Apr::CommandBufferObject sized{nullptr, static_cast<std::uint32_t>(loose.size()), 0, 0, Apr::BufferType::Generic, 0};
    RequireEqual(sceAmprAmmCommandBufferMapAsPrt(&sized, prt, page), permissionDenied, "map as PRT");
}};

const Case ammMeasurePrt{"MeasureAmmPrtCommands_InvalidArguments_ReturnEinval", [] {
    const std::uint64_t prt = QueryRanges().start + 0x300000;
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeMapAsPrt(prt, 0), ammRejected, "map as PRT zero size");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeAllocatePaForPrt(prt, page, 0, 0x08), ammRejected, "allocate PRT protection 0x08");
    RequireEqual(sceAmprAmmMeasureAmmCommandSizeAllocatePaForPrt(prt + 8, page, 0, cpuReadWrite), ammRejected, "allocate PRT misaligned address");
}};

} // namespace

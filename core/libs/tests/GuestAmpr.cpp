#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceAmpr/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include "prx/libkernel/Apr/include/AprCommandBuffer.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#endif

static void RequireAt(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "GuestAmpr.cpp:%d: requirement failed\n", line);
    std::abort();
}

#define Require(value) RequireAt((value), __LINE__)

namespace {

constexpr int invalidArgument = static_cast<int>(0x80020016);
constexpr int permissionDenied = static_cast<int>(0x80020001);
constexpr int busy = static_cast<int>(0x80020010);
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

std::uint32_t HeaderAt(const Recorder& recorder, std::uint32_t offset) {
    std::uint32_t first;
    std::memcpy(&first, (recorder.buffer.base ? recorder.buffer.base : recorder.memory.data()) + offset, sizeof(first));
    return first;
}

void ReleaseRecorderBuffer(Recorder& recorder) {
    void* memory = sceAmprCommandBufferClearBuffer(&recorder.buffer);
    if (memory != recorder.memory.data()) GuestHeap::GuestHeapFree_nid_postfix(memory);
}

void RequireAppended(const Recorder& recorder, std::uint32_t offset, std::uint32_t commands, Apr::Opcode opcode, std::uint64_t measured) {
    Require(recorder.Offset() == offset + measured);
    Require(recorder.Commands() == commands + 1);
    if (opcode == Apr::Opcode::Nop) {
        const std::uint32_t first = HeaderAt(recorder, offset);
        Require(Apr::IsNopHeader(first) && Apr::NopDwords(first) * 4u == measured);
        return;
    }
    Apr::CommandHeader header;
    std::memcpy(&header, recorder.memory.data() + offset, sizeof(header));
    if (opcode == Apr::Opcode::CompactReadFileGather) {
        Require((static_cast<std::uint32_t>(header.opcode) & 0xffu) == static_cast<std::uint32_t>(opcode));
        Require(measured == sizeof(Apr::CompactReadFileGatherCommand));
    } else Require(header.opcode == opcode && header.bytes == measured);
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

    std::uint64_t gatherState = 0;
    std::uint64_t scatterState = 0;
    std::array<std::uint8_t, 64> memory{};
    Require(sceAmprCommandBufferConstructor(nullptr) == invalidArgument);
    Require(sceAmprAprCommandBufferConstructor(nullptr, &gatherState, &scatterState) == invalidArgument);
    Require(sceAmprAprCommandBufferConstructor(&recorder.buffer, nullptr, &scatterState) == invalidArgument);
    Require(sceAmprAprCommandBufferConstructor(&recorder.buffer, &gatherState, nullptr) == invalidArgument);
    Require(sceAmprCommandBufferReset(nullptr) == invalidArgument);
    Require(sceAmprCommandBufferSetBuffer(nullptr, memory.data(), memory.size()) == invalidArgument);
}

void TestFullBuffer() {
    const char* marker = "streaming";
    const auto measured = static_cast<std::uint32_t>(sceAmprMeasureCommandSizePushMarker(marker));
    Recorder exact(measured);
    Require(sceAmprCommandBufferPushMarker(&exact.buffer, marker) == 0);
    Require(exact.Offset() == measured && exact.Commands() == 1);
    Require(sceAmprCommandBufferPopMarker(&exact.buffer) == 0);
    Require(sceAmprCommandBufferSetMarker(&exact.buffer, "") == 0);
    Require(exact.Offset() > measured && exact.Commands() == 3);
    Require(exact.buffer.base != exact.memory.data() && exact.buffer.size > measured);

    Recorder small(measured - 4);
    Require(sceAmprCommandBufferPushMarker(&small.buffer, marker) == 0);
    Require(sceAmprCommandBufferSetMarkerWithColor(&small.buffer, marker, &color) == 0);
    Require(small.Offset() > measured && small.Commands() == 2);
    Require(small.buffer.base != small.memory.data() && small.buffer.size > measured - 4);

    std::array<std::uint8_t, 16> destination{};
    volatile std::uint64_t done = 0;
    const auto read = sceAmprMeasureCommandSizeReadFile();
    Recorder reads(read);
    Require(sceAmprAprCommandBufferReadFile(&reads.buffer, &reads.gatherState, &reads.scatterState, 1, destination.data(), destination.size(), 0) == 0);
    Require(sceAmprAprCommandBufferReadFile(&reads.buffer, &reads.gatherState, &reads.scatterState, 1, destination.data(), destination.size(), 0) == 0);
    Require(sceAmprCommandBufferNop(&reads.buffer, 1) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&reads.buffer, &done, 1) == 0);
    Require(reads.Offset() > read && reads.Commands() == 4);
    Require(reads.buffer.base != reads.memory.data() && reads.buffer.size > read);

    Apr::CommandBufferObject unbound{};
    std::uint64_t gatherState = 0;
    std::uint64_t scatterState = 0;
    Require(sceAmprCommandBufferConstructor(&unbound) == 0);
    Require(sceAmprAprCommandBufferConstructor(&unbound, &gatherState, &scatterState) == 0);
    Require(sceAmprCommandBufferPushMarker(&unbound, marker) == permissionDenied);
    Require(sceAmprCommandBufferPopMarker(&unbound) == permissionDenied);
    Require(sceAmprAprCommandBufferReadFile(&unbound, &gatherState, &scatterState, 1, destination.data(), destination.size(), 0) == permissionDenied);
    Require(sceAmprCommandBufferNop(&unbound, 1) == permissionDenied);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&unbound, &done, 1) == permissionDenied);
    Require(unbound.offset == 0 && unbound.numCommands == 0);
    ReleaseRecorderBuffer(exact);
    ReleaseRecorderBuffer(small);
    ReleaseRecorderBuffer(reads);
}

void TestOffsetPastBuffer() {
    constexpr std::array<std::uint32_t, 3> invalidOffsets{33u, 64u, 0xffffffffu};
    for (const auto offset : invalidOffsets) {
        Recorder recorder(32);
        recorder.memory.fill(0xA5);
        recorder.buffer.offset = offset;
        recorder.buffer.numCommands = 7;
        const auto before = recorder.buffer;
        const auto memory = recorder.memory;
        const auto rejected = [&](const auto& record) {
            bool threw = false;
            try {
                record();
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            Require(threw);
            Require(recorder.memory == memory);
            Require(recorder.buffer.base == before.base && recorder.buffer.size == before.size);
            Require(recorder.buffer.offset == before.offset && recorder.buffer.numCommands == before.numCommands);
            Require(recorder.buffer.type == before.type && recorder.buffer.recording == before.recording);
        };
        rejected([&] { sceAmprCommandBufferPopMarker(&recorder.buffer); });
        rejected([&] { sceAmprCommandBufferNop(&recorder.buffer, 1); });
        rejected([&] { sceAmprCommandBufferPushMarker(&recorder.buffer, "bounds"); });
        rejected([&] { sceAmprAmmCommandBufferMap(&recorder.buffer, 0x4000, 0x4000, 0, 3); });
        rejected([&] { sceAmprAmmCommandBufferMapAsPrt(&recorder.buffer, 0x4000, 0x4000); });
    }
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
    RequireAppended(recorder, 0, 0, Apr::Opcode::WriteKernelEventQueue, sceAmprMeasureCommandSizeWriteKernelEventQueue_04_00(static_cast<std::uint64_t>(eq), 7, 0x1234));
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

void TestWaitSatisfiedAfterSubmission() {
    Recorder first;
    Recorder second;
    alignas(8) std::uint64_t flag = 0;
    alignas(8) std::uint64_t done = 0;
    alignas(8) std::uint64_t later = 0;
    Require(sceAmprCommandBufferWaitOnAddress(&first.buffer, &flag, 1, 0, 0) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&first.buffer, &done, 1) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&second.buffer, &later, 2) == 0);
    std::uint32_t firstId = 0;
    auto submitted = std::async(std::launch::async, [&]() { return sceKernelAprSubmitCommandBufferAndGetId(&first.buffer, 0, &firstId); });
    const bool returned = submitted.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    if (!returned) std::atomic_ref<std::uint64_t>(flag).store(1, std::memory_order_release);
    Require(returned);
    Require(submitted.get() == 0 && firstId != 0);
    std::uint32_t secondId = 0;
    Require(sceKernelAprSubmitCommandBufferAndGetId(&second.buffer, 0, &secondId) == 0 && secondId > firstId);
    Require(std::atomic_ref<std::uint64_t>(done).load(std::memory_order_acquire) == 0);
    Require(std::atomic_ref<std::uint64_t>(later).load(std::memory_order_acquire) == 0);
    std::atomic_ref<std::uint64_t>(flag).store(1, std::memory_order_release);
    Require(sceKernelAprWaitCommandBuffer(secondId) == 0);
    Require(done == 1 && later == 2);
    Require(sceKernelAprWaitCommandBuffer(firstId) == 0);
}

void TestAmmNotBehindAprWait() {
    Recorder reader;
    Recorder maps;
    alignas(8) std::uint64_t flag = 0;
    alignas(8) std::uint64_t done = 0;
    Require(sceAmprCommandBufferWaitOnAddress(&reader.buffer, &flag, 1, 0, 0) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&reader.buffer, &done, 1) == 0);
    Require(sceAmprAmmCommandBufferConstructor(&maps.buffer) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&maps.buffer, &flag, 1) == 0);
    std::uint32_t readerId = 0;
    Require(sceKernelAprSubmitCommandBufferAndGetId(&reader.buffer, 0, &readerId) == 0 && readerId != 0);
    std::uint32_t mapsId = 0;
    Require(sceAmprAmmSubmitCommandBuffer3(maps.memory.data(), maps.Offset(), 0, &mapsId) == 0 && mapsId != 0);
    auto completed = std::async(std::launch::async, [&]() { return sceAmprAmmWaitCommandBufferCompletion(mapsId); });
    const bool returned = completed.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    if (!returned) std::atomic_ref<std::uint64_t>(flag).store(1, std::memory_order_release);
    Require(returned);
    Require(completed.get() == 0);
    Require(sceKernelAprWaitCommandBuffer(readerId) == 0);
    Require(done == 1);
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
        std::memset(recorder.memory.data() + offset, 0x5A, dwords * 4u);
        Require(sceAmprCommandBufferNop(&recorder.buffer, dwords) == 0);
        RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNop(dwords));
        Require(HeaderAt(recorder, offset) == (0x5452000Fu | (dwords - 1u) << 8u));
        for (std::size_t index = Apr::NopHeaderBytes; index < dwords * 4u; ++index) Require(recorder.memory[offset + index] == 0x5A);
    }
    const auto offset = recorder.Offset();
    const auto commands = recorder.Commands();
    Require(sceAmprCommandBufferNopWithData(&recorder.buffer, 3, data) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNopWithData(4));
    Require(HeaderAt(recorder, offset) == 0x5452030Fu);
    Require(std::memcmp(recorder.memory.data() + offset + Apr::NopHeaderBytes, data, sizeof(data)) == 0);
    Require(sceAmprCommandBufferNopWithData(&recorder.buffer, 0, nullptr) == 0);
    Require(HeaderAt(recorder, offset + 16u) == 0x5452000Fu);
    const auto emptyPayloadOffset = recorder.Offset();
    std::memset(recorder.memory.data() + emptyPayloadOffset, 0x5A, sceAmprMeasureCommandSizeNopWithData(4));
    Require(sceAmprCommandBufferNopWithData(&recorder.buffer, 3, nullptr) == 0);
    for (std::size_t index = 0; index < 12; ++index) Require(recorder.memory[emptyPayloadOffset + Apr::NopHeaderBytes + index] == 0x5A);
    Require(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0) == 0);

    const auto rejected = static_cast<std::uint64_t>(static_cast<std::uint32_t>(invalidArgument));
    Require(sceAmprCommandBufferNop(&recorder.buffer, 0) == invalidArgument);
    Require(sceAmprCommandBufferNop(&recorder.buffer, 17) == invalidArgument);
    Require(sceAmprCommandBufferNopWithData(&recorder.buffer, 16, data) == invalidArgument);
    Require(sceAmprMeasureCommandSizeNop(0) == rejected && sceAmprMeasureCommandSizeNop(17) == rejected);
    Require(sceAmprMeasureCommandSizeNopWithData(0) == rejected && sceAmprMeasureCommandSizeNopWithData(17) == rejected);

    Apr::CommandBufferObject unbound{};
    Require(sceAmprCommandBufferConstructor(&unbound) == 0);
    Require(sceAmprCommandBufferNop(&unbound, 0) == permissionDenied);
    Require(sceAmprCommandBufferNop(&unbound, 17) == permissionDenied);
    Require(sceAmprCommandBufferNopWithData(&unbound, 16, data) == permissionDenied);
    Require(sceAmprCommandBufferConstructNop(&unbound, 0, data, 61, nullptr) == permissionDenied);
    Require(unbound.offset == 0 && unbound.numCommands == 0);
}

void TestNopPadding() {
    Require(sceAmprMeasureCommandSizeNop(1) == 4 && sceAmprMeasureCommandSizeNop(16) == 64);
    Require(sceAmprMeasureCommandSizeNopWithData(1) == 4 && sceAmprMeasureCommandSizeNopWithData(16) == 64);

    Recorder recorder(64);
    alignas(8) std::uint64_t done = 0;
    Require(sceAmprCommandBufferPushMarker(&recorder.buffer, "pad") == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&recorder.buffer, &done, 1) == 0);
    const std::uint32_t remaining = 64 - recorder.Offset();
    Require(remaining % 4 == 0 && remaining / 4 >= 2);
    Require(sceAmprCommandBufferNop(&recorder.buffer, remaining / 4) == 0);
    Require(recorder.Offset() == 64 && recorder.Commands() == 3);
    Require(recorder.buffer.base == recorder.memory.data());
    Require(sceAmprCommandBufferNop(&recorder.buffer, 1) == 0);
    Require(recorder.buffer.base != recorder.memory.data() && recorder.buffer.size > 64);
    Require(recorder.Offset() == 68 && recorder.Commands() == 4);
    Require(sceAmprCommandBufferNopWithData(&recorder.buffer, 0, nullptr) == 0);
    Require(recorder.Offset() == 72 && recorder.Commands() == 5);
    Require(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0) == 0);
    Require(done == 1);
    ReleaseRecorderBuffer(recorder);

    Recorder odd;
    done = 0;
    Require(sceAmprCommandBufferNop(&odd.buffer, 1) == 0);
    Require(odd.Offset() == 4);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&odd.buffer, &done, 2) == 0);
    Require(sceAmprCommandBufferNopWithData(&odd.buffer, 2, nullptr) == 0);
    Require(sceAmprCommandBufferNop(&odd.buffer, 1) == 0);
    Require(odd.Offset() == 4 + sizeof(Apr::WriteAddressCommand) + 12 + 4);
    Require(sceKernelAprSubmitCommandBuffer(&odd.buffer, 0) == 0);
    Require(done == 2);
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
    Require(sceAmprMeasureCommandSizeWriteAddressFromTimeCounterOnCompletion(&time) == sizeof(Apr::WriteAddressFromCounterCommand));
    Require(sceAmprMeasureCommandSizeWriteAddressFromTimeCounterOnCompletion(nullptr) == rejected);
    Require(sceAmprMeasureCommandSizeWriteAddressFromCounterOnCompletion(&single, 128) == sizeof(Apr::WriteAddressFromCounterCommand));
    Require(sceAmprMeasureCommandSizeWriteAddressFromCounterOnCompletion(nullptr, 0) == rejected);
    Require(sceAmprMeasureCommandSizeWriteAddressFromCounterPairOnCompletion(&pair, 10) == sizeof(Apr::WriteAddressFromCounterCommand));
    Require(sceAmprMeasureCommandSizeWriteAddressFromCounterPairOnCompletion(&pair, 11) == rejected);
    Require(sceAmprMeasureCommandSizeWriteAddressFromCounterPairOnCompletion(nullptr, 10) == rejected);
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
    Require(sceAmprMeasureCommandSizeWriteAddressOnCompletion() == sizeof(Apr::WriteAddressCommand));
    Require(sceAmprMeasureCommandSizeWriteCounterOnCompletion() == sizeof(Apr::WriteCounterCommand));
    Require(sceAmprMeasureCommandSizeWriteKernelEventQueueOnCompletion() == sizeof(Apr::WriteKernelEventQueueCommand));
    Require(sceAmprCommandBufferGetBufferBaseAddress(&recorder.buffer) == recorder.buffer.base);
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
    std::memset(recorder.memory.data() + offset, 0x5A, 20);
    Require(sceAmprCommandBufferConstructNop(&recorder.buffer, 7, payload, sizeof(payload), &word) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::Nop, sceAmprMeasureCommandSizeNopWithData(4));
    Require(HeaderAt(recorder, offset) == 0x5452730Fu);
    const std::uint8_t* data = recorder.memory.data() + offset + Apr::NopHeaderBytes;
    const std::uint8_t padding[3] = {};
    Require(std::memcmp(data, &word, sizeof(word)) == 0 && std::memcmp(data + 4, payload, sizeof(payload)) == 0 && std::memcmp(data + 9, padding, sizeof(padding)) == 0);
    Require(recorder.memory[offset + 16] == 0x5A);
    offset = recorder.Offset();
    Require(sceAmprCommandBufferConstructNop(&recorder.buffer, 0x15, payload, 4, nullptr) == 0);
    Require(HeaderAt(recorder, offset) == 0x5452510Fu);
    offset = recorder.Offset();
    Require(sceAmprCommandBufferConstructNop(&recorder.buffer, -1, payload, 4, nullptr) == 0);
    Require(HeaderAt(recorder, offset) == 0x5452F10Fu);

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
    Require(sceAmprCommandBufferSetBuffer(&buffer, memory.data(), sizeof(Apr::ReadFileCommand)) == 0);
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
    void* commandMemory = sceAmprCommandBufferClearBuffer(&buffer);
    if (commandMemory != memory.data()) GuestHeap::GuestHeapFree_nid_postfix(commandMemory);
    std::remove(path);
}

void TestReadPastEndOfFileZeroFills() {
    const std::filesystem::path directory = "ampr_read_past_end";
    std::filesystem::remove_all(directory);
    Require(std::filesystem::create_directory(directory));
    const auto file = directory / "settings.ini";
    {
        std::ofstream stream(file, std::ios::binary);
        for (std::size_t offset = 0; offset < 64; ++offset) stream.put(static_cast<char>(FileByte(offset)));
    }
    const std::string filePath = file.generic_string();
    const char* path = filePath.c_str();
    std::uint32_t fileId = 0;
    std::uint64_t size = 0;
    std::uint32_t failed = 0;
    Require(sceKernelAprResolveFilepathsToIdsAndFileSizes(&path, 1, &fileId, &size, &failed) == 0 && size == 64);
    {
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        for (std::size_t offset = 1; offset < size; ++offset) stream.put(static_cast<char>(FileByte(offset)));
    }

    std::array<std::uint8_t, 64> whole;
    std::array<std::uint8_t, 24> continued;
    std::array<std::uint8_t, 16> atEnd;
    std::array<std::uint8_t, 16> pastEnd;
    whole.fill(0xA5);
    continued.fill(0xA5);
    atEnd.fill(0xA5);
    pastEnd.fill(0xA5);
    Recorder recorder;
    Require(sceAmprAprCommandBufferReadFile(&recorder.buffer, &recorder.gatherState, &recorder.scatterState, fileId, whole.data(), size, 0) == 0);
    Require(sceAmprAprCommandBufferReadFileScatter(&recorder.buffer, &recorder.gatherState, &recorder.scatterState, continued.data(), 16) == 0);
    Require(sceAmprAprCommandBufferReadFileGather(&recorder.buffer, &recorder.gatherState, &recorder.scatterState, 8, 0) == 0);
    Require(sceAmprAprCommandBufferReadFile(&recorder.buffer, &recorder.gatherState, &recorder.scatterState, fileId, atEnd.data(), atEnd.size(), size - 1) == 0);
    Require(sceAmprAprCommandBufferReadFile(&recorder.buffer, &recorder.gatherState, &recorder.scatterState, fileId, pastEnd.data(), pastEnd.size(), 4096) == 0);
    Require(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0) == 0);
    Require(MatchesFile(whole.data(), 1, size - 1) && whole[size - 1] == 0);
    for (std::size_t index = 0; index < 16; ++index) Require(continued[index] == 0);
    Require(MatchesFile(continued.data() + 16, 1, 8));
    for (const auto byte : atEnd) Require(byte == 0);
    for (const auto byte : pastEnd) Require(byte == 0);
    std::filesystem::remove_all(directory);
}

void TestReadAtUnseekableOffsetThrows() {
    const char* path = "ampr_unseekable_offset.bin";
    { std::ofstream file(path, std::ios::binary); file << "data"; }
    std::uint32_t fileId = 0;
    std::uint32_t failed = 0;
    Require(sceKernelAprResolveFilepathsToIds(&path, 1, &fileId, &failed) == 0);
    std::array<std::uint8_t, 4> data{};
    const Apr::ReadFileCommand command{{Apr::Opcode::ReadFile, sizeof(Apr::ReadFileCommand)}, fileId, 0, reinterpret_cast<std::uint64_t>(data.data()), data.size(), std::uint64_t{1} << 63u};
    alignas(8) std::array<std::uint8_t, sizeof(command)> memory{};
    std::memcpy(memory.data(), &command, sizeof(command));
    Apr::CommandBufferObject buffer{Apr::BufferType::Generic, 0, sizeof(memory), 1, sizeof(memory), memory.data()};
    bool rejected = false;
    try {
        sceKernelAprSubmitCommandBuffer(&buffer, 0);

    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("returned 0 of 4 bytes") != std::string::npos;
    }
    Require(rejected);
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
    for (const char* invalid : {"ampr_resolve_missing.bin/../ampr_resolve_existing.bin", "ampr_resolve_existing.bin/../ampr_resolve_existing.bin"}) {
        Require(sceKernelAprResolveFilepathsToIds(&invalid, 1, ids, &failed) == -1);
        Require(*__error_nid_postfix() == 2 && failed == 0 && ids[0] == 0xffffffffu);
    }
    std::remove(paths[0]);
}

void TestResolveForEachVariants() {
    const char* paths[]{"ampr_foreach_existing.bin", "ampr_foreach_missing.bin"};
    const char* names[]{"foreach_existing.bin", "foreach_missing.bin"};
    { std::ofstream file(paths[0], std::ios::binary); file << "data"; }
    std::uint32_t expected = 0;
    std::uint32_t failed = 99;
    Require(sceKernelAprResolveFilepathsToIds(paths, 1, &expected, &failed) == 0);
    std::uint32_t ids[]{123, 123};
    int results[]{123, 123};
    Require(sceKernelAprResolveFilepathsToIdsForEach(paths, 2, ids, results) == 1);
    Require(ids[0] == expected && ids[1] == 0xffffffffu && results[0] == 0 && results[1] == static_cast<int>(0x80020002));
    ids[0] = ids[1] = 123;
    results[0] = results[1] = 123;
    Require(sceKernelAprResolveFilepathsWithPrefixToIdsForEach("ampr_", names, 2, ids, results) == 1);
    Require(ids[0] == expected && ids[1] == 0xffffffffu && results[0] == 0 && results[1] == static_cast<int>(0x80020002));
    std::uint64_t sizes[]{123, 123};
    ids[0] = ids[1] = 123;
    results[0] = results[1] = 123;
    Require(sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizesForEach("ampr_", names, 2, ids, sizes, results) == 1);
    Require(ids[0] == expected && ids[1] == 0xffffffffu && sizes[0] == 4 && sizes[1] == 0 && results[0] == 0 && results[1] == static_cast<int>(0x80020002));
    std::remove(paths[0]);
}

void TestPrefixedResolveWithoutSizes() {
    std::uint32_t probe = 0;
    const std::string prefix = "ampr_ids_" + std::to_string(reinterpret_cast<std::uintptr_t>(&probe)) + "_";
    { std::ofstream file(prefix + "first.bin", std::ios::binary); file << "aaaa"; }
    { std::ofstream file(prefix + "second.bin", std::ios::binary); file << "bb"; }
    const char* names[]{"first.bin", "second.bin"};
    std::uint32_t ids[]{0x123456, 0x123456};
    std::uint32_t failed = 99;
    Require(sceKernelAprResolveFilepathsWithPrefixToIds(prefix.c_str(), names, 2, ids, &failed) == 0);
    Require(ids[0] != 0xffffffffu && ids[0] != 0x123456 && ids[1] != 0xffffffffu && ids[1] != 0x123456 && ids[0] != ids[1]);
    Require(failed == 99);
    std::uint32_t sizedIds[]{0x123456, 0x123456};
    std::uint64_t sizes[]{0, 0};
    Require(sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes(prefix.c_str(), names, 2, sizedIds, sizes, &failed) == 0);
    Require(sizedIds[0] == ids[0] && sizedIds[1] == ids[1] && sizes[0] == 4 && sizes[1] == 2);
    const char* mixed[]{"first.bin", "missing.bin", "second.bin"};
    std::uint32_t mid[]{0x123456, 0x123456, 0x123456};
    failed = 99;
    *__error_nid_postfix() = 0;
    Require(sceKernelAprResolveFilepathsWithPrefixToIds(prefix.c_str(), mixed, 3, mid, &failed) == -1);
    Require(*__error_nid_postfix() == 2 && failed == 1 && mid[0] == ids[0] && mid[1] == 0xffffffffu && mid[2] == 0x123456);
    Require(sceKernelAprResolveFilepathsWithPrefixToIds(prefix.c_str(), mixed, 3, mid, nullptr) == -1);
    Require(*__error_nid_postfix() == 2);
    const char* nulled[]{"first.bin", nullptr};
    std::uint32_t dropped[]{0x123456, 0x123456};
    failed = 99;
    Require(sceKernelAprResolveFilepathsWithPrefixToIds(prefix.c_str(), nulled, 2, dropped, &failed) == -1);
    Require(*__error_nid_postfix() == 2 && failed == 1 && dropped[0] == ids[0] && dropped[1] == 0xffffffffu);
    *__error_nid_postfix() = 0;
    Require(sceKernelAprResolveFilepathsWithPrefixToIds(nullptr, names, 1, ids, &failed) == -1 && *__error_nid_postfix() == 22);
    Require(sceKernelAprResolveFilepathsWithPrefixToIds(prefix.c_str(), nullptr, 1, ids, &failed) == -1 && *__error_nid_postfix() == 22);
    Require(sceKernelAprResolveFilepathsWithPrefixToIds(prefix.c_str(), names, 1, nullptr, &failed) == -1 && *__error_nid_postfix() == 22);
    std::uint32_t untouched[]{0x123456};
    Require(sceKernelAprResolveFilepathsWithPrefixToIds(prefix.c_str(), names, 0, untouched, &failed) == 0);
    Require(untouched[0] == 0x123456 && failed == 1);
    std::remove((prefix + "first.bin").c_str());
    std::remove((prefix + "second.bin").c_str());
}

void TestMalformedCommandSizes() {
    const auto requireFailure = [](const void* record, std::size_t recordBytes, std::uint32_t bufferBytes, const char* expected) {
        std::array<std::uint8_t, 16> memory{};
        std::memcpy(memory.data(), record, recordBytes);
        Apr::CommandBufferObject buffer{Apr::BufferType::Generic, 0, bufferBytes, 1, bufferBytes, memory.data()};
        bool rejected = false;
        try {
            sceKernelAprSubmitCommandBuffer(&buffer, 0);

        } catch (const std::runtime_error& error) {
            rejected = std::string(error.what()).starts_with(expected);
        }
        Require(rejected);
    };
    const auto requireHeaderFailure = [&](Apr::Opcode opcode, std::uint32_t recordBytes, std::uint32_t bufferBytes, const char* expected) {
        const Apr::CommandHeader header{opcode, recordBytes};
        requireFailure(&header, sizeof(header), bufferBytes, expected);
    };

    requireHeaderFailure(Apr::Opcode::ReadFile, sizeof(Apr::CommandHeader), sizeof(Apr::CommandHeader), "APR: truncated command payload");
    requireHeaderFailure(Apr::Opcode::WriteAddress, sizeof(Apr::CommandHeader), sizeof(Apr::CommandHeader), "APR: truncated command payload");
    requireHeaderFailure(Apr::Opcode::PopMarker, std::numeric_limits<std::uint32_t>::max(), 2u * sizeof(Apr::CommandHeader), "APR: malformed command");

    const std::uint32_t overflowingNop = Apr::NopHeader(15);
    requireFailure(&overflowingNop, sizeof(overflowingNop), sizeof(overflowingNop), "APR: malformed command");
    const Apr::CommandHeader eightByteNop{Apr::Opcode::Nop, sizeof(Apr::CommandHeader)};
    requireFailure(&eightByteNop, sizeof(eightByteNop), sizeof(eightByteNop), "APR: unknown opcode 0");
    const std::uint32_t nops[2] = {Apr::NopHeader(0), Apr::NopHeader(0)};
    requireFailure(nops, sizeof(nops), 7u, "APR: truncated command buffer");

    std::array<std::uint8_t, 2 * sizeof(Apr::CommandHeader)> memory{};
    const Apr::CommandHeader first{Apr::Opcode::PopMarker, sizeof(Apr::CommandHeader)};
    const Apr::CommandHeader overflowing{Apr::Opcode::PopMarker, std::numeric_limits<std::uint32_t>::max()};
    std::memcpy(memory.data(), &first, sizeof(first));
    std::memcpy(memory.data() + sizeof(first), &overflowing, sizeof(overflowing));
    Apr::CommandBufferObject buffer{Apr::BufferType::Generic, 0, sizeof(memory), 2, sizeof(memory), memory.data()};
    bool rejected = false;
    try {
        sceKernelAprSubmitCommandBuffer(&buffer, 0);

    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).starts_with("APR: malformed command");
    }
    Require(rejected);
}

void TestResolveSymbolicLink() {
    const std::filesystem::path directory = "ampr_resolve_links";
    std::filesystem::remove_all(directory);
    Require(std::filesystem::create_directory(directory));
    { std::ofstream file(directory / "target.bin", std::ios::binary); file << "linked data"; }
    const auto link = directory / "link.bin";
#ifdef _WIN32
    if (CreateSymbolicLinkW(link.c_str(), L"target.bin", SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) == 0) {
        Require(GetLastError() == ERROR_PRIVILEGE_NOT_HELD);
        std::fprintf(stderr, "APR symbolic link check skipped: creating symbolic links needs Developer Mode or administrator rights\n");
        std::filesystem::remove_all(directory);
        return;
    }
#else
    std::filesystem::create_symlink("target.bin", link);
#endif
    const std::string linkPath = link.generic_string();
    const char* path = linkPath.c_str();
    std::uint32_t id = 0;
    std::uint64_t size = 0;
    std::uint32_t failed = 0;
    Require(sceKernelAprResolveFilepathsToIdsAndFileSizes(&path, 1, &id, &size, &failed) == 0 && size == 11);
    std::filesystem::remove_all(directory);
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

    Recorder compact(64);
    std::array<std::uint8_t, 24> compactData{};
    Require(sceAmprAprCommandBufferReadFile(&compact.buffer, &compact.gatherState, &compact.scatterState, fileId, compactData.data(), 8, 100) == 0);
    Require(sceAmprAprCommandBufferReadFileGather(&compact.buffer, &compact.gatherState, &compact.scatterState, 8, 108) == 0);
    Require(sceAmprAprCommandBufferReadFileGather(&compact.buffer, &compact.gatherState, &compact.scatterState, 8, 116) == 0);
    Require(compact.Offset() == 64);
    Require(sceKernelAprSubmitCommandBuffer(&compact.buffer, 0) == 0);
    Require(MatchesFile(compactData.data(), 100, compactData.size()));

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
    RequireAppended(recorder, offset, commands, Apr::Opcode::CompactReadFileGather, sceAmprMeasureCommandSizeReadFileGather(8, 500));
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
    Require(sceAmprMeasureCommandSizeReadFileGather(0x100000000ull, 0x10000000000ull - 1u) == sizeof(Apr::CompactReadFileGatherCommand));
    Require(sceAmprMeasureCommandSizeReadFileGather(4, 0x10000000000ull) == rejected);
    Require(sceAmprMeasureCommandSizeReadFileScatter(first.data(), 0x100000001ull) == rejected);
    Require(sceAmprMeasureCommandSizeReadFileScatter(high, 4) == rejected);
    Require(sceAmprMeasureCommandSizeReadFileGatherScatter(first.data(), 4, 0x10000000000ull) == rejected);
    Require(sceAmprMeasureCommandSizeReadFileGatherScatter(nullptr, 4, 0) == sizeof(Apr::ReadFileCommand));
}

constexpr std::uint64_t page = 0x4000;
constexpr int noSuchSubmission = static_cast<int>(0x80020003);
constexpr std::int32_t cpuReadWrite = 0x03;
constexpr std::int32_t cpuGpuReadWrite = 0x33;
constexpr std::int32_t amprReadWrite = 0xC0;

volatile std::uint64_t& At(std::uint64_t address) {
    return *reinterpret_cast<volatile std::uint64_t*>(address);
}

void TestAmmRangesAboveHintedMaps() {
    constexpr std::uint64_t step = 0x400000;
    void* span = nullptr;
    Require(sceKernelReserveVirtualRange(&span, 4 * step, 0, 0) == 0);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(span), 4 * step) == 0);
    const auto heap = reinterpret_cast<std::uint64_t>(span);
    void* placed = span;
    Require(sceKernelMapFlexibleMemory(&placed, page, 3, 0) == 0 && placed == span);

    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
    Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0);
    std::uintptr_t arenaBase = 0;
    std::size_t arenaBytes = 0;
    GuestArena::GuestArenaRange_nid_postfix(&arenaBase, &arenaBytes);
    Require(start >= arenaBase && multimapEnd <= arenaBase + arenaBytes && heap + 4 * step <= start);

    for (std::uint64_t hint = heap + step; hint < heap + 4 * step; hint += step) {
        void* mapped = reinterpret_cast<void*>(hint);
        Require(sceKernelMapFlexibleMemory(&mapped, page, 3, 0) == 0);
        Require(reinterpret_cast<std::uint64_t>(mapped) == hint);
        Require(hint + page <= start || hint >= multimapEnd);
        Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapped), page) == 0);
    }
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(span), page) == 0);
}

void TestAmmUsageStats() {
    std::array<std::uint64_t, 4> stats{};
    Require(sceAmprAmmGetUsageStatsData(nullptr) == static_cast<int>(0x80020016));
    stats[0] = 0x19;
    Require(sceAmprAmmGetUsageStatsData(reinterpret_cast<AmmUsageStatsData*>(stats.data())) == static_cast<int>(0x80020016));
    stats = {0x18, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef};
    Require(sceAmprAmmGetUsageStatsData(reinterpret_cast<AmmUsageStatsData*>(stats.data())) == 0);
    Require(stats[0] == 0x18);
    Require((stats[1] & 0xffffu) == 512u && (stats[1] >> 16u) == 0u);
    Require((stats[2] & 0xffffffffu) == 7u && (stats[2] >> 32u) == 0u);
    stats = {0x8, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef};
    Require(sceAmprAmmGetUsageStatsData(reinterpret_cast<AmmUsageStatsData*>(stats.data())) == 0);
    Require(stats[0] == 0x8 && stats[1] == 0xdeadbeef && stats[2] == 0xdeadbeef);
    stats = {0x0, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef};
    Require(sceAmprAmmGetUsageStatsData(reinterpret_cast<AmmUsageStatsData*>(stats.data())) == 0);
    Require(stats[0] == 0 && stats[1] == 0xdeadbeef);
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
    Require(sceAmprAmmSubmitCommandBuffer2(remaps.memory.data(), remaps.Offset(), 0, reinterpret_cast<AmmSubmitResult*>(&result), &id) == 0 && result == 0);
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
    Require(sceAmprAmmCommandBufferUnmap(&exact.buffer, start, page) == 0);
    Require(sceAmprAmmCommandBufferMap(&exact.buffer, start, page, 0, cpuReadWrite) == 0);
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
    ReleaseRecorderBuffer(exact);
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

void TestAmmRemapKeepsPoolReferences() {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
    Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0);
    std::int64_t pool = -1;
    Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), page, page, 1, &pool) == 0);

    const auto source = start + 256 * page;
    const auto alias = multimapStart + 256 * page;
    const auto fresh = start + 257 * page;
    const auto lastAlias = multimapStart + 257 * page;
    const auto overlappingAlias = multimapStart + 258 * page;
    Recorder maps;
    Require(sceAmprAmmCommandBufferMap(&maps.buffer, source, page, 0, cpuReadWrite) == 0);
    Require(sceAmprAmmCommandBufferMultiMap(&maps.buffer, alias, source, page, cpuReadWrite) == 0);
    SubmitAmm(maps);
    At(source) = 0xA1;
    Require(At(alias) == 0xA1);

    Recorder move;
    Require(sceAmprAmmCommandBufferRemap(&move.buffer, alias, source, page, cpuReadWrite) == 0);
    SubmitAmm(move);
    Require(Protection(source) == 0 && At(alias) == 0xA1);

    Recorder exhausted;
    Require(sceAmprAmmCommandBufferMap(&exhausted.buffer, fresh, page, 0, cpuReadWrite) == 0);
    const auto requireExhausted = [&] {
        bool rejected = false;
        try {
            SubmitAmm(exhausted);
        } catch (const std::runtime_error& error) {
            rejected = std::string(error.what()).starts_with("AMM: no free run of ");
        }
        Require(rejected);
    };
    requireExhausted();
    At(alias) = 0xB1;
    Require(At(alias) == 0xB1);

    Recorder retain;
    Require(sceAmprAmmCommandBufferRemap(&retain.buffer, alias, alias, page, cpuReadWrite) == 0);
    Require(sceAmprAmmCommandBufferMultiMap(&retain.buffer, lastAlias, alias, page, cpuReadWrite) == 0);
    Require(sceAmprAmmCommandBufferRemap(&retain.buffer, lastAlias, alias, 2 * page, cpuReadWrite) == 0);
    Require(sceAmprAmmCommandBufferUnmap(&retain.buffer, lastAlias, page) == 0);
    SubmitAmm(retain);
    Require(At(overlappingAlias) == 0xB1);
    requireExhausted();

    Recorder release;
    Require(sceAmprAmmCommandBufferUnmap(&release.buffer, overlappingAlias, page) == 0);
    Require(sceAmprAmmCommandBufferMap(&release.buffer, fresh, page, 0, cpuReadWrite) == 0);
    SubmitAmm(release);
    Require(At(fresh) == 0xB1);
    At(fresh) = 0xC1;
    Require(At(fresh) == 0xC1);
}

void TestAmmMapsScatteredPoolPages() {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
    Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0);
    const auto exhausts = [](Recorder& recorder) {
        try {
            SubmitAmm(recorder);
        } catch (const std::runtime_error& error) {
            return std::string(error.what()).starts_with("AMM: no free run of ");
        }
        return false;
    };
    const auto drained = start + 1024 * page;
    for (std::uint64_t index = 0;; ++index) {
        Require(index < 4096);
        Recorder drain;
        Require(sceAmprAmmCommandBufferMap(&drain.buffer, drained + index * page, page, 0, cpuReadWrite) == 0);
        if (exhausts(drain)) break;
    }

    std::int64_t pool = -1;
    Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), 5 * page, page, 1, &pool) == 0);
    const auto singles = start + 6144 * page;
    Recorder maps;
    for (std::uint64_t index = 0; index < 5; ++index) Require(sceAmprAmmCommandBufferMap(&maps.buffer, singles + index * page, page, 0, cpuReadWrite) == 0);
    SubmitAmm(maps);
    for (std::uint64_t index = 0; index < 5; ++index) At(singles + index * page) = 0x100 + index;

    Recorder release;
    for (const std::uint64_t index : {0, 2, 4}) Require(sceAmprAmmCommandBufferUnmap(&release.buffer, singles + index * page, page) == 0);
    SubmitAmm(release);

    const auto scattered = start + 6400 * page;
    Recorder gather;
    Require(sceAmprAmmCommandBufferMap(&gather.buffer, scattered, 3 * page, 0, cpuReadWrite) == 0);
    SubmitAmm(gather);
    for (std::uint64_t index = 0; index < 3; ++index) At(scattered + index * page) = 0x200 + index;
    for (std::uint64_t index = 0; index < 3; ++index) Require(At(scattered + index * page) == 0x200 + index);
    Require(At(singles + page) == 0x101 && At(singles + 3 * page) == 0x103);

    Recorder empty;
    Require(sceAmprAmmCommandBufferMap(&empty.buffer, singles, page, 0, cpuReadWrite) == 0);
    Require(exhausts(empty));
}

void TestAmmPrtAllocationFailureKeepsZeros() {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
    Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0);
    const auto exhausts = [](Recorder& recorder) {
        try {
            SubmitAmm(recorder);
        } catch (const std::runtime_error& error) {
            return std::string(error.what()).starts_with("AMM: no free run of ");
        }
        return false;
    };
    const auto drained = start + 24576 * page;
    for (std::uint64_t index = 0;; ++index) {
        Require(index < 4096);
        Recorder drain;
        Require(sceAmprAmmCommandBufferMap(&drain.buffer, drained + index * page, page, 0, cpuReadWrite) == 0);
        if (exhausts(drain)) break;
    }
    std::int64_t pool = -1;
    Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), page, page, 1, &pool) == 0);

    const std::uint64_t prt = start + 28672 * page;
    Recorder reserve;
    Require(sceAmprAmmCommandBufferMapAsPrt(&reserve.buffer, prt, 4 * page) == 0);
    SubmitAmm(reserve);
    Require(At(prt) == 0 && At(prt + 3 * page) == 0);

    Recorder tooMany;
    Require(sceAmprAmmCommandBufferAllocatePaForPrt(&tooMany.buffer, prt + page, 2 * page, 0, cpuReadWrite) == 0);
    Require(exhausts(tooMany));
    Require(Protection(prt + page) == 0x10 && At(prt) == 0 && At(prt + page) == 0 && At(prt + 2 * page) == 0 && At(prt + 3 * page) == 0);

    Recorder one;
    Require(sceAmprAmmCommandBufferAllocatePaForPrt(&one.buffer, prt + page, page, 0, cpuReadWrite) == 0);
    SubmitAmm(one);
    Require(Protection(prt + page) == 0x03);
    At(prt + page) = 0x61;
    Require(At(prt + page) == 0x61 && At(prt) == 0 && At(prt + 2 * page) == 0);

    Recorder none;
    Require(sceAmprAmmCommandBufferAllocatePaForPrt(&none.buffer, prt + 2 * page, page, 0, cpuReadWrite) == 0);
    Require(exhausts(none));
    Require(At(prt + 2 * page) == 0 && At(prt + page) == 0x61);
}

void TestAmmMapFailureKeepsPoolOwnership() {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
    Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0);
    std::int64_t pool = -1;
    Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), page, page, 1, &pool) == 0);
    const auto original = start + 512 * page;
    const auto fresh = start + 516 * page;

    Recorder mapping;
    Require(sceAmprAmmCommandBufferMap(&mapping.buffer, original, page, 0, cpuReadWrite) == 0);
    SubmitAmm(mapping);
    At(original) = 0xA1;
    SubmitAmm(mapping);
    Require(At(original) == 0xA1 && Protection(original) == cpuReadWrite);

    Recorder oversized;
    Require(sceAmprAmmCommandBufferMap(&oversized.buffer, original, 2 * page, 0, cpuReadWrite) == 0);
    bool rejected = false;
    try {
        SubmitAmm(oversized);
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).starts_with("AMM: no free run of ");
    }
    Require(rejected);
    Require(Protection(original) == 0 && Protection(original + page) == 0);

    Recorder reuse;
    Require(sceAmprAmmCommandBufferMap(&reuse.buffer, fresh, page, 0, cpuReadWrite) == 0);
    SubmitAmm(reuse);
    Require(At(fresh) == 0xA1);
    At(fresh) = 0xB1;
    Require(At(fresh) == 0xB1 && Protection(original) == 0);
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
    Require(sceAmprAmmCommandBufferModifyProtect(&exact.buffer, returned, page, 0x03, 0x03) == 0);

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
    ReleaseRecorderBuffer(exact);
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

void TestWriteAddressCommand() {
    Recorder recorder;
    alignas(8) std::uint64_t target = 0;
    const auto offset = recorder.Offset();
    const auto commands = recorder.Commands();
    Require(sceAmprCommandBufferWriteAddress_04_00(&recorder.buffer, &target, 0x1122334455667788ull, 0) == 0);
    RequireAppended(recorder, offset, commands, Apr::Opcode::WriteAddress, sceAmprMeasureCommandSizeWriteAddress_04_00());
    Apr::WriteAddressCommand command{};
    std::memcpy(&command, recorder.memory.data() + offset, sizeof(command));
    Require(command.address == reinterpret_cast<std::uint64_t>(&target) && command.value == 0x1122334455667788ull && command.flags == 0);
    Require(sceAmprMeasureCommandSizeWriteAddress_04_00() == sizeof(Apr::WriteAddressCommand));
    Require(sceAmprMeasureCommandSizeWriteKernelEventQueue_04_00(0, 0, 0) == sizeof(Apr::WriteKernelEventQueueCommand));
    Require(sceAmprCommandBufferGetType(&recorder.buffer) == static_cast<std::uint32_t>(Apr::BufferType::Apr));
    Require(sceKernelAprSubmitCommandBuffer(&recorder.buffer, 0) == 0);
    Require(target == 0x1122334455667788ull);
    Apr::CommandBufferObject generic{};
    Require(sceAmprCommandBufferConstructor(&generic) == 0);
    Require(sceAmprCommandBufferGetType(&generic) == static_cast<std::uint32_t>(Apr::BufferType::Generic));
}

void TestKernelWriteCommands() {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
    Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0);
    std::int64_t pool = -1;
    Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), 4 * page, page, 1, &pool) == 0);

    const std::uint64_t target = start + 0x400000;
    const std::uint64_t source = start + 0x440000;
    const std::uint64_t prt = start + 0x480000;

    Recorder setup;
    Require(sceAmprAmmCommandBufferMap(&setup.buffer, target, 2 * page, 0, cpuReadWrite) == 0);
    Require(sceAmprAmmCommandBufferMap(&setup.buffer, source, page, 0, cpuReadWrite) == 0);
    Require(sceAmprAmmCommandBufferMapAsPrt(&setup.buffer, prt, 2 * page) == 0);
    SubmitAmm(setup);
    At(source) = 0x9C;

    std::uint64_t measured = 0;
    Require(sceKernelWriteModifyMtypeProtectCommand(nullptr, target, 2 * page, 5, 0x13, 0x10, &measured) == 0);
    Require(measured == sizeof(Apr::AmmProtectCommand));
    std::array<std::uint8_t, 128> written{};
    std::uint64_t reported = 0;
    Require(sceKernelWriteModifyMtypeProtectCommand(written.data(), target, 2 * page, 5, 0x13, 0x10, &reported) == 0);
    Require(reported == sizeof(Apr::AmmProtectCommand));
    Apr::CommandHeader header;
    std::memcpy(&header, written.data(), sizeof(header));
    Require(header.opcode == Apr::Opcode::AmmModifyMtypeProtect && header.bytes == sizeof(Apr::AmmProtectCommand));
    Recorder sibling;
    Require(sceAmprAmmCommandBufferModifyMtypeProtect(&sibling.buffer, target, 2 * page, 5, 0x13, 0x10) == 0);
    Require(std::memcmp(written.data(), sibling.memory.data(), sizeof(Apr::AmmProtectCommand)) == 0);
    Require(sceKernelWriteModifyMtypeProtectCommand(written.data(), target, 2 * page, 5, 0x13, 0x10, nullptr) == 0);
    std::uint32_t id = 0;
    Require(sceAmprAmmSubmitCommandBuffer3(written.data(), static_cast<std::uint32_t>(reported), 0, &id) == 0);
    Require(sceAmprAmmWaitCommandBufferCompletion(id) == 0);
    Require(Protection(target) == 0x13 && Protection(target + page) == 0x13);
    At(target) = 0x9D;
    Require(At(target) == 0x9D);

    std::uint64_t prtMeasured = 0;
    Require(sceKernelWriteRemapIntoPrtCommand(nullptr, prt + page, source, page, cpuReadWrite, 0, &prtMeasured) == 0);
    Require(prtMeasured == sizeof(Apr::AmmRemapCommand));
    std::array<std::uint8_t, 128> prtWritten{};
    Require(sceKernelWriteRemapIntoPrtCommand(reinterpret_cast<std::uint32_t*>(prtWritten.data()), prt + page, source, page, cpuReadWrite, 0, &prtMeasured) == 0);
    Apr::AmmRemapCommand remap;
    std::memcpy(&remap, prtWritten.data(), sizeof(remap));
    Require(remap.header.opcode == Apr::Opcode::AmmRemapIntoPrt && remap.header.bytes == sizeof(remap));
    Require(remap.address == prt + page && remap.source == source && remap.size == page && remap.protection == cpuReadWrite && remap.reserved == 1011);
    Require(sceKernelWriteRemapIntoPrtCommand(reinterpret_cast<std::uint32_t*>(prtWritten.data()), prt + page, source, page, cpuReadWrite, 42, &prtMeasured) == 0);
    std::memcpy(&remap, prtWritten.data(), sizeof(remap));
    Require(remap.reserved == 42);
    Recorder prtSibling;
    Require(sceAmprAmmCommandBufferRemapIntoPrt(&prtSibling.buffer, prt + page, source, page, cpuReadWrite, 0) == 0);
    std::array<std::uint8_t, 128> prtSubmit{};
    Require(sceKernelWriteRemapIntoPrtCommand(reinterpret_cast<std::uint32_t*>(prtSubmit.data()), prt + page, source, page, cpuReadWrite, 0, nullptr) == 0);
    Require(std::memcmp(prtSubmit.data(), prtSibling.memory.data(), sizeof(Apr::AmmRemapCommand)) == 0);
    Require(sceAmprAmmSubmitCommandBuffer3(prtSubmit.data(), static_cast<std::uint32_t>(sizeof(Apr::AmmRemapCommand)), 0, &id) == 0);
    Require(sceAmprAmmWaitCommandBufferCompletion(id) == 0);
    Require(At(prt + page) == 0x9C);

    std::array<std::uint8_t, 128> untouched{};
    untouched.fill(0xCC);
    std::uint64_t sentinel = 0xDDDDDDDDDDDDDDDDULL;
    Require(sceKernelWriteModifyMtypeProtectCommand(untouched.data(), target + 8, page, 0, 0x03, 0x03, &sentinel) == invalidArgument);
    Require(sceKernelWriteModifyMtypeProtectCommand(untouched.data(), target, 0, 0, 0x03, 0x03, &sentinel) == invalidArgument);
    Require(sceKernelWriteModifyMtypeProtectCommand(untouched.data(), 0, page, 0, 0x03, 0x03, &sentinel) == invalidArgument);
    Require(sceKernelWriteModifyMtypeProtectCommand(untouched.data(), target, page, 0, 0x04, 0x03, &sentinel) == invalidArgument);
    Require(sceKernelWriteModifyMtypeProtectCommand(untouched.data(), target, page, 0, 0x03, 0x04, &sentinel) == invalidArgument);
    Require(sceKernelWriteModifyMtypeProtectCommand(untouched.data(), ~std::uint64_t(0), page, 0, 0x03, 0x03, &sentinel) == invalidArgument);
    Require(sceKernelWriteModifyMtypeProtectCommand(untouched.data(), target, page, 0, 0x100000000ULL, 0x03, &sentinel) == invalidArgument);
    Require(sceKernelWriteRemapIntoPrtCommand(reinterpret_cast<std::uint32_t*>(untouched.data()), prt, source + 8, page, cpuReadWrite, 0, &sentinel) == invalidArgument);
    Require(sceKernelWriteRemapIntoPrtCommand(reinterpret_cast<std::uint32_t*>(untouched.data()), prt, source, 0, cpuReadWrite, 0, &sentinel) == invalidArgument);
    Require(sceKernelWriteRemapIntoPrtCommand(reinterpret_cast<std::uint32_t*>(untouched.data()), prt, source, page, 0x04, 0, &sentinel) == invalidArgument);
    Require(sentinel == 0xDDDDDDDDDDDDDDDDULL);
    for (const auto byte : untouched) Require(byte == 0xCC);
    Require(sceKernelWriteModifyMtypeProtectCommand(nullptr, target, page, 0, 0x03, 0x03, nullptr) == 0);
    Require(sceKernelWriteRemapIntoPrtCommand(nullptr, prt, source, page, cpuReadWrite, 0, nullptr) == 0);
}

}


void TestAmmSubmitAfterGrowth() {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint64_t multimapStart = 0;
    std::uint64_t multimapEnd = 0;
    Require(sceAmprAmmGetVirtualAddressRanges(&start, &end, &multimapStart, &multimapEnd) == 0);
    std::int64_t pool = -1;
    Require(sceAmprAmmGiveDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), 2 * page, page, 1, &pool) == 0);

    const auto target = start + 0x500000;
    const auto measured = static_cast<std::uint32_t>(sceAmprAmmMeasureAmmCommandSizeMap(target, page, 0, cpuReadWrite));
    Recorder exact(measured);
    Require(sceAmprAmmCommandBufferConstructor(&exact.buffer) == 0);
    Require(sceAmprCommandBufferSetBuffer(&exact.buffer, exact.memory.data(), measured) == 0);
    Require(sceAmprAmmCommandBufferMap(&exact.buffer, target, page, 0, cpuReadWrite) == 0);
    Require(exact.buffer.base == exact.memory.data());
    Require(sceAmprAmmCommandBufferMap(&exact.buffer, target + page, page, 0, cpuReadWrite) == 0);
    Require(exact.buffer.base != exact.memory.data());

    std::uint32_t id = 0;
    std::uint64_t result = ~0ull;
    Require(sceAmprAmmSubmitCommandBuffer(exact.memory.data(), exact.Offset(), 0) == 0);
    Require(sceAmprAmmSubmitCommandBuffer2(exact.memory.data(), exact.Offset(), 0, reinterpret_cast<AmmSubmitResult*>(&result), &id) == 0 && result == 0);
    Require(sceAmprAmmWaitCommandBufferCompletion(id) == 0);
    Require(sceAmprAmmSubmitCommandBuffer3(exact.memory.data(), exact.Offset(), 0, &id) == 0);
    Require(sceAmprAmmWaitCommandBufferCompletion(id) == 0);
    At(target) = 0x1111;
    At(target + page) = 0x2222;
    Require(At(target) == 0x1111 && At(target + page) == 0x2222);

    Require(sceAmprCommandBufferReset(&exact.buffer) == 0);
    Require(sceAmprAmmCommandBufferUnmap(&exact.buffer, target, 2 * page) == 0);
    Require(sceAmprAmmSubmitCommandBuffer(exact.memory.data(), exact.Offset(), 0) == 0);
    ReleaseRecorderBuffer(exact);
}

void TestExpandedBufferLifecycle() {
    std::array<std::uint8_t, 32> memory1{};
    std::array<std::uint8_t, 32> memory2{};
    Apr::CommandBufferObject buffer{};

    Require(sceAmprCommandBufferConstructor(&buffer) == 0);
    Require(sceAmprCommandBufferSetBuffer(&buffer, memory1.data(), static_cast<std::uint32_t>(memory1.size())) == 0);

    alignas(8) std::uint64_t done = 0;
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 1) == 0);
    Require(buffer.base == memory1.data());

    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 2) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 3) == 0);
    Require(buffer.base != memory1.data());
    Require(buffer.offset == 3 * sizeof(Apr::WriteAddressCommand));

    Require(sceAmprCommandBufferReset(&buffer) == 0);
    Require(buffer.offset == 0 && buffer.numCommands == 0);
    Require(buffer.base != memory1.data());

    Require(sceAmprCommandBufferSetBuffer(&buffer, memory2.data(), static_cast<std::uint32_t>(memory2.size())) == 0);
    Require(buffer.base == memory2.data() && buffer.size == memory2.size());
    Require(buffer.offset == 0);

    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 4) == 0);
    Require(buffer.base == memory2.data());
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 5) == 0);
    Require(buffer.base != memory2.data());
    void* cleared = sceAmprCommandBufferClearBuffer(&buffer);
    Require(cleared == memory2.data());
    Require(buffer.base == nullptr && buffer.size == 0 && buffer.offset == 0);

    Require(sceAmprCommandBufferSetBuffer(&buffer, memory1.data(), static_cast<std::uint32_t>(memory1.size())) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 6) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 7) == 0);
    Require(buffer.base != memory1.data());
    Require(sceAmprCommandBufferConstructor(&buffer) == 0);
    Require(buffer.base == nullptr && buffer.size == 0);

    std::uint64_t gather = 0;
    std::uint64_t scatter = 0;
    Require(sceAmprAprCommandBufferConstructor(&buffer, &gather, &scatter) == 0);
    Require(sceAmprCommandBufferSetBuffer(&buffer, memory1.data(), static_cast<std::uint32_t>(memory1.size())) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 8) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 9) == 0);
    Require(buffer.base != memory1.data());
    Require(sceAmprAprCommandBufferDestructor(&buffer, &gather, &scatter) == 0);

    Require(sceAmprAmmCommandBufferConstructor(&buffer) == 0);
    Require(sceAmprCommandBufferSetBuffer(&buffer, memory1.data(), static_cast<std::uint32_t>(memory1.size())) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 10) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 11) == 0);
    Require(buffer.base != memory1.data());
    Require(sceAmprAmmCommandBufferDestructor(&buffer) == 0);

    Require(sceAmprCommandBufferConstructor(&buffer) == 0);
    Require(sceAmprCommandBufferSetBuffer(&buffer, memory1.data(), static_cast<std::uint32_t>(memory1.size())) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 12) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 13) == 0);
    Require(buffer.base != memory1.data());
    Require(sceAmprCommandBufferDestructor(&buffer) == 0);

    std::array<std::uint8_t, 32> sharedMem{};
    Apr::CommandBufferObject buf1{};
    Apr::CommandBufferObject buf2{};
    Require(sceAmprAmmCommandBufferConstructor(&buf1) == 0);
    Require(sceAmprAmmCommandBufferConstructor(&buf2) == 0);
    Require(sceAmprCommandBufferSetBuffer(&buf1, sharedMem.data(), static_cast<std::uint32_t>(sharedMem.size())) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buf1, &done, 14) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buf1, &done, 15) == 0);
    Require(buf1.base != sharedMem.data());
    Require(sceAmprCommandBufferSetBuffer(&buf2, sharedMem.data(), static_cast<std::uint32_t>(sharedMem.size())) == 0);
    Require(buf2.base == sharedMem.data());
    std::uint32_t submitId = 0;
    Require(sceAmprAmmSubmitCommandBuffer3(sharedMem.data(), 0, 0, &submitId) == 0 && submitId != 0);
    Require(sceAmprAmmWaitCommandBufferCompletion(submitId) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buf2, &done, 16) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buf2, &done, 17) == 0);
    Require(buf2.base != sharedMem.data());
    Require(sceAmprAmmSubmitCommandBuffer3(sharedMem.data(), buf2.offset, 0, &submitId) == 0 && submitId != 0);
    Require(sceAmprAmmWaitCommandBufferCompletion(submitId) == 0);
    Require(sceAmprAmmCommandBufferDestructor(&buf1) == 0);
    Require(sceAmprAmmCommandBufferDestructor(&buf2) == 0);

    Require(sceAmprCommandBufferConstructor(&buffer) == 0);
    Require(sceAmprCommandBufferSetBuffer(&buffer, memory1.data(), static_cast<std::uint32_t>(memory1.size())) == 0);
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 18) == 0);
    Require(buffer.base == memory1.data());
    Require(buffer.offset == sizeof(Apr::WriteAddressCommand));
    Require(buffer.numCommands == 1);
    constexpr std::uint32_t maxCap = 32u * 1024u * 1024u;
    buffer.offset = maxCap - 4u;
    buffer.size = maxCap;
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 19) == busy);
    Require(buffer.offset == maxCap - 4u);
    Require(buffer.size == maxCap);
    Require(buffer.numCommands == 1);
    constexpr std::uint32_t aboveCap = 34u * 1024u * 1024u;
    buffer.offset = 0;
    buffer.size = aboveCap;
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 20) == 0);
    Require(buffer.base == memory1.data());
    Require(buffer.offset == sizeof(Apr::WriteAddressCommand));
    Require(buffer.numCommands == 2);
    buffer.offset = aboveCap - 4u;
    Require(sceAmprCommandBufferWriteAddressOnCompletion(&buffer, &done, 21) == busy);
    Require(sceAmprCommandBufferDestructor(&buffer) == 0);
}

int main() {
    TestRecording("frame");
    TestRecording("");
    TestRecording("1234567");
    TestRecording("12345678");
    TestRecording(std::string(300, 'a'));
    TestRejectedArguments();
    TestFullBuffer();
    TestOffsetPastBuffer();
    TestSubmission();
    TestWriteAddressCommand();
    TestKernelEventQueue();
    TestWaits();
    TestWaitSatisfiedAfterSubmission();
    TestAmmNotBehindAprWait();
    TestCounters();
    TestRejectedWaitsAndCounters();
    TestNops();
    TestNopPadding();
    TestVersionedCommands();
    TestVersionedCounters();
    TestClearBuffer();
    TestConstructed();
    TestZeroFilledBuffer();
    TestReadPastEndOfFileZeroFills();
    TestReadAtUnseekableOffsetThrows();
    TestResolveFailures();
    TestResolveForEachVariants();
    TestPrefixedResolveWithoutSizes();
    TestMalformedCommandSizes();
    TestResolveSymbolicLink();
    TestGatherScatter();
    TestAmmRangesAboveHintedMaps();
    TestAmmUsageStats();
    TestAmmRemapKeepsPoolReferences();
    TestAmmMapFailureKeepsPoolOwnership();
    TestAmmMapsScatteredPoolPages();
    TestAmm();
    TestAmmRemapAndProtect();
    TestAmmPrt();
    TestAmmPrtAllocationFailureKeepsZeros();
    TestKernelWriteCommands();
    TestAmmSubmitAfterGrowth();
    TestExpandedBufferLifecycle();
    return 0;
}

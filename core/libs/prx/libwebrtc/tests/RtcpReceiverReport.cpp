#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void* lastFreed = nullptr;
int allocations = 0;
int frees = 0;

void* APS5_VABI TestAllocate(std::size_t size) { ++allocations; return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { lastFreed = pointer; ++frees; std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { ++allocations; return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { ++allocations; return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    ++allocations;
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtcp::ReceiverReport check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);
using DestructorFunction = void (APS5_VABI*)(WebRtcRtcpReceiverReport* report);
using BlockLengthFunction = std::size_t (APS5_VABI*)(const WebRtcRtcpReceiverReport* report);
using CreateFunction = bool (APS5_VABI*)(const WebRtcRtcpReceiverReport* report, std::uint8_t* buffer, std::size_t* index, std::size_t maxLength, void* function, PacketReady call);

template<typename TFunction> TFunction Slot(const WebRtcRtcpReceiverReport& report, int slot) {
    return reinterpret_cast<TFunction>(static_cast<void* const*>(report.vtable)[slot]);
}

std::size_t BlockLength(const WebRtcRtcpReceiverReport& report) { return Slot<BlockLengthFunction>(report, 2)(&report); }

bool Create(const WebRtcRtcpReceiverReport& report, std::uint8_t* buffer, std::size_t* index, std::size_t maxLength, void* function, PacketReady call) {
    return Slot<CreateFunction>(report, 3)(&report, buffer, index, maxLength, function, call);
}

void* ConsoleAllocate(std::size_t bytes) {
    if (bytes < 0x1000) return malloc_nid_postfix(bytes);
    auto* raw = static_cast<std::uint8_t*>(malloc_nid_postfix(bytes + 0x27));
    const auto aligned = (reinterpret_cast<std::uintptr_t>(raw) + 0x27) & ~std::uintptr_t {0x1f};
    reinterpret_cast<void**>(aligned)[-1] = raw;
    return reinterpret_cast<void*>(aligned);
}

WebRtcRtcpReportBlock Block(std::uint32_t seed, std::int32_t cumulativeLost) {
    return WebRtcRtcpReportBlock {seed, static_cast<std::uint8_t>(seed + 1), cumulativeLost, seed + 2, seed + 3, seed + 4, seed + 5};
}

void SetBlocks(WebRtcRtcpReceiverReport& report, const std::vector<WebRtcRtcpReportBlock>& blocks, std::size_t capacity) {
    auto* storage = static_cast<WebRtcRtcpReportBlock*>(ConsoleAllocate(capacity * sizeof(WebRtcRtcpReportBlock)));
    for (std::size_t index = 0; index < blocks.size(); ++index) storage[index] = blocks[index];
    report.reportBlocksBegin = storage;
    report.reportBlocksEnd = storage + blocks.size();
    report.reportBlocksCapacity = storage + capacity;
}

void Destroy(WebRtcRtcpReceiverReport& report) { Slot<DestructorFunction>(report, 0)(&report); }

struct Sent {
    std::vector<std::vector<std::uint8_t>> packets;
};

void APS5_VABI Collect(void* function, const std::uint8_t* data, std::size_t size) {
    static_cast<Sent*>(function)->packets.emplace_back(data, data + size);
}

void TestConstructor() {
    WebRtcRtcpReceiverReport report;
    report.senderSsrc = 0xffffffffu;
    report.reportBlocksBegin = reinterpret_cast<WebRtcRtcpReportBlock*>(std::uintptr_t {8});
    report.reportBlocksEnd = report.reportBlocksBegin;
    report.reportBlocksCapacity = report.reportBlocksBegin;
    const int before = allocations;
    _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(&report);
    Require(allocations == before && report.vtable != nullptr && report.senderSsrc == 0);
    Require(report.reportBlocksBegin == nullptr && report.reportBlocksEnd == nullptr && report.reportBlocksCapacity == nullptr);
    Require(BlockLength(report) == 8);
    const int freesBefore = frees;
    Destroy(report);
    Require(frees == freesBefore);
}

void TestCreateWithoutBlocks() {
    WebRtcRtcpReceiverReport report;
    _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(&report);
    report.senderSsrc = 0x12345678u;
    std::array<std::uint8_t, 8> buffer {};
    std::size_t index = 0;
    Require(Create(report, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require(index == 8 && buffer == std::array<std::uint8_t, 8> {0x80, 201, 0x00, 0x01, 0x12, 0x34, 0x56, 0x78});
    Destroy(report);
}

void TestCreateWritesEveryReportBlock() {
    WebRtcRtcpReceiverReport report;
    _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(&report);
    report.senderSsrc = 0x01020304u;
    SetBlocks(report, {Block(0x11223344u, -2), Block(0xa0b0c0d0u, 0x7fffff)}, 3);
    Require(BlockLength(report) == 56);
    std::vector<std::uint8_t> buffer(60, 0xee);
    std::size_t index = 2;
    Require(Create(report, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    const std::vector<std::uint8_t> expected {
        0xee, 0xee,
        0x82, 201, 0x00, 0x0d, 0x01, 0x02, 0x03, 0x04,
        0x11, 0x22, 0x33, 0x44, 0x45, 0xff, 0xff, 0xfe, 0x11, 0x22, 0x33, 0x46,
        0x11, 0x22, 0x33, 0x47, 0x11, 0x22, 0x33, 0x48, 0x11, 0x22, 0x33, 0x49,
        0xa0, 0xb0, 0xc0, 0xd0, 0xd1, 0x7f, 0xff, 0xff, 0xa0, 0xb0, 0xc0, 0xd2,
        0xa0, 0xb0, 0xc0, 0xd3, 0xa0, 0xb0, 0xc0, 0xd4, 0xa0, 0xb0, 0xc0, 0xd5,
        0xee, 0xee};
    Require(index == 58 && buffer == expected);
    WebRtcRtcpReportBlock* storage = report.reportBlocksBegin;
    Destroy(report);
    Require(lastFreed == storage && report.reportBlocksBegin == nullptr && report.reportBlocksEnd == nullptr && report.reportBlocksCapacity == nullptr);
}

void TestFullBufferIsSentFirst() {
    WebRtcRtcpReceiverReport report;
    _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(&report);
    report.senderSsrc = 7;
    std::array<std::uint8_t, 12> buffer {1, 2, 3, 4, 5, 6};
    std::size_t index = 6;
    Sent sent;
    Require(Create(report, buffer.data(), &index, buffer.size(), &sent, &Collect));
    Require(sent.packets.size() == 1 && sent.packets[0] == std::vector<std::uint8_t> {1, 2, 3, 4, 5, 6});
    Require(index == 8 && buffer[0] == 0x80 && buffer[1] == 201 && buffer[7] == 7);
    index = 6;
    RequireInvalidArgument([&] { Create(report, buffer.data(), &index, buffer.size(), nullptr, nullptr); });
    index = 0;
    Require(!Create(report, buffer.data(), &index, 4, &sent, &Collect) && index == 0 && sent.packets.size() == 1);
    Destroy(report);
}

void TestMoreThan31BlocksThrow() {
    WebRtcRtcpReceiverReport report;
    _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(&report);
    SetBlocks(report, std::vector<WebRtcRtcpReportBlock>(31, Block(1, 0)), 32);
    std::vector<std::uint8_t> buffer(8 + 32 * 24);
    std::size_t index = 0;
    Require(Create(report, buffer.data(), &index, buffer.size(), nullptr, nullptr) && index == 8 + 31 * 24 && buffer[0] == 0x9f);
    report.reportBlocksEnd = report.reportBlocksCapacity;
    index = 0;
    RequireInvalidArgument([&] { Create(report, buffer.data(), &index, buffer.size(), nullptr, nullptr); });
    Destroy(report);
}

void TestStorageOf4096BytesKeepsTheRawPointer() {
    WebRtcRtcpReceiverReport report;
    _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(&report);
    SetBlocks(report, {Block(1, 0)}, 146);
    WebRtcRtcpReportBlock* small = report.reportBlocksBegin;
    Destroy(report);
    Require(lastFreed == small);
    SetBlocks(report, {Block(1, 0)}, 147);
    void* raw = reinterpret_cast<void**>(report.reportBlocksBegin)[-1];
    Destroy(report);
    Require(lastFreed == raw);
}

void TestDeletingDestructorFreesTheObject() {
    auto* report = static_cast<WebRtcRtcpReceiverReport*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpReceiverReport)));
    _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(report);
    SetBlocks(*report, {Block(1, 0)}, 1);
    const int before = frees;
    Slot<DestructorFunction>(*report, 1)(report);
    Require(frees == before + 2 && lastFreed == report);
}

void TestNullArgumentsThrow() {
    RequireInvalidArgument([] { _ZN6webrtc4rtcp14ReceiverReportC1Ev_nid_postfix(nullptr); });
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestConstructor();
    TestCreateWithoutBlocks();
    TestCreateWritesEveryReportBlock();
    TestFullBufferIsSentFirst();
    TestMoreThan31BlocksThrow();
    TestStorageOf4096BytesKeepsTheRawPointer();
    TestDeletingDestructorFreesTheObject();
    TestNullArgumentsThrow();
    return 0;
}

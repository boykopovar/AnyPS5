#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    std::fprintf(stderr, "libwebrtc rtcp::App check failed at line %u\n", location.line());
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

const void* const Vtable = reinterpret_cast<const void*>(std::uintptr_t {0x1234});

WebRtcRtcpApp Constructed() {
    WebRtcRtcpApp app;
    std::memset(&app, 0, sizeof(app));
    app.vtable = Vtable;
    return app;
}

std::vector<std::uint8_t> Packet(std::uint8_t subType, std::uint32_t name, std::size_t dataSize) {
    const std::size_t payload = 8 + dataSize;
    std::vector<std::uint8_t> packet {static_cast<std::uint8_t>(0x80 | subType), 204, 0, static_cast<std::uint8_t>(payload / 4),
                                      0x12, 0x34, 0x56, 0x78,
                                      static_cast<std::uint8_t>(name >> 24), static_cast<std::uint8_t>(name >> 16),
                                      static_cast<std::uint8_t>(name >> 8), static_cast<std::uint8_t>(name)};
    for (std::size_t index = 0; index < dataSize; ++index) packet.push_back(static_cast<std::uint8_t>(index + 1));
    return packet;
}

bool ParsePacket(WebRtcRtcpApp& app, const std::vector<std::uint8_t>& packet) {
    WebRtcRtcpCommonHeader header;
    Require(_ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(&header, packet.data(), packet.size()));
    return _ZN6webrtc4rtcp3App5ParseERKNS0_12CommonHeaderE_nid_postfix(&app, &header);
}

bool HoldsData(const WebRtcRtcpApp& app, std::size_t size) {
    if (app.data.size != size) return false;
    for (std::size_t index = 0; index < size; ++index)
        if (app.data.data[index] != index + 1) return false;
    return true;
}

void TestParse() {
    WebRtcRtcpApp app = Constructed();
    Require(ParsePacket(app, Packet(30, 0x54455354u, 8)));
    Require(app.vtable == Vtable);
    Require(app.subType == 30 && app.senderSsrc == 0x12345678u && app.name == 0x54455354u);
    Require(app.data.capacity == 8 && HoldsData(app, 8));
    _ZN6webrtc4rtcp3AppD1Ev_nid_postfix(&app);
}

void TestDataGrowsByHalfItsCapacity() {
    WebRtcRtcpApp app = Constructed();
    Require(ParsePacket(app, Packet(1, 1, 8)));
    std::uint8_t* first = app.data.data;
    Require(ParsePacket(app, Packet(1, 1, 12)));
    Require(lastFreed == first && app.data.capacity == 12 && HoldsData(app, 12));
    first = app.data.data;
    Require(ParsePacket(app, Packet(1, 1, 16)));
    Require(lastFreed == first && app.data.capacity == 18 && HoldsData(app, 16));
    first = app.data.data;
    const int before = allocations;
    Require(ParsePacket(app, Packet(2, 7, 4)));
    Require(allocations == before && app.data.data == first && app.data.capacity == 18 && HoldsData(app, 4));
    Require(app.subType == 2 && app.name == 7);
    _ZN6webrtc4rtcp3AppD1Ev_nid_postfix(&app);
    Require(lastFreed == first);
}

void TestEmptyDataKeepsTheBuffer() {
    WebRtcRtcpApp app = Constructed();
    Require(ParsePacket(app, Packet(0, 0, 0)));
    Require(app.data.data == nullptr && app.data.size == 0 && app.data.capacity == 0);
    Require(ParsePacket(app, Packet(3, 9, 8)));
    std::uint8_t* data = app.data.data;
    const int before = allocations;
    Require(ParsePacket(app, Packet(4, 5, 0)));
    Require(allocations == before && app.data.data == data && app.data.size == 0 && app.data.capacity == 8);
    Require(app.subType == 4 && app.name == 5);
    _ZN6webrtc4rtcp3AppD1Ev_nid_postfix(&app);
}

void TestInvalidPayloadLeavesThePacketUntouched() {
    std::vector<std::uint8_t> payload(16, 0x11);
    WebRtcRtcpApp app = Constructed();
    app.subType = 9;
    for (std::uint32_t size : {0u, 4u, 7u, 10u, 13u}) {
        WebRtcRtcpCommonHeader header {204, 5, 0, size, payload.data()};
        Require(!_ZN6webrtc4rtcp3App5ParseERKNS0_12CommonHeaderE_nid_postfix(&app, &header));
    }
    WebRtcRtcpCommonHeader header {204, 5, 0, 4, nullptr};
    Require(!_ZN6webrtc4rtcp3App5ParseERKNS0_12CommonHeaderE_nid_postfix(&app, &header));
    Require(app.subType == 9 && app.senderSsrc == 0 && app.name == 0 && app.data.data == nullptr && app.data.size == 0);
}

void TestDestructorWithoutDataFreesNothing() {
    WebRtcRtcpApp app = Constructed();
    const int before = frees;
    _ZN6webrtc4rtcp3AppD1Ev_nid_postfix(&app);
    Require(frees == before);
}

void TestNullArgumentsThrow() {
    std::vector<std::uint8_t> payload(8, 0x11);
    WebRtcRtcpCommonHeader header {204, 1, 0, 8, payload.data()};
    WebRtcRtcpApp app = Constructed();
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3App5ParseERKNS0_12CommonHeaderE_nid_postfix(nullptr, &header); });
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3App5ParseERKNS0_12CommonHeaderE_nid_postfix(&app, nullptr); });
    header.payload = nullptr;
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3App5ParseERKNS0_12CommonHeaderE_nid_postfix(&app, &header); });
    Require(app.subType == 0 && app.senderSsrc == 0 && app.data.data == nullptr);
    RequireInvalidArgument([] { _ZN6webrtc4rtcp3AppD1Ev_nid_postfix(nullptr); });
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestParse();
    TestDataGrowsByHalfItsCapacity();
    TestEmptyDataKeepsTheBuffer();
    TestInvalidPayloadLeavesThePacketUntouched();
    TestDestructorWithoutDataFreesNothing();
    TestNullArgumentsThrow();
    return 0;
}

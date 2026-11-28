#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtcp::Pli check failed at line %u\n", location.line());
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

WebRtcRtcpPli Filled() {
    WebRtcRtcpPli pli;
    std::memset(&pli, 0xA5, sizeof(pli));
    return pli;
}

bool Unchanged(const WebRtcRtcpPli& pli) {
    WebRtcRtcpPli filled = Filled();
    return std::memcmp(&pli, &filled, sizeof(pli)) == 0;
}

bool ParsePacket(WebRtcRtcpPli& pli, const std::vector<std::uint8_t>& packet) {
    WebRtcRtcpCommonHeader header;
    Require(_ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(&header, packet.data(), packet.size()));
    return _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, &header);
}

void TestPli() {
    std::vector<std::uint8_t> packet {0x81, 206, 0x00, 0x02, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    WebRtcRtcpPli pli = Filled();
    WebRtcRtcpPli filled = Filled();
    Require(ParsePacket(pli, packet));
    Require(pli.senderSsrc == 0x12345678u);
    Require(pli.mediaSsrc == 0x9ABCDEF0u);
    Require(pli.vtable == filled.vtable);
}

void TestLongerPayloadReadsTheCommonFeedbackOnly() {
    std::vector<std::uint8_t> packet {0x81, 206, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFE, 0x55, 0x55, 0x55, 0x55};
    WebRtcRtcpPli pli = Filled();
    Require(ParsePacket(pli, packet));
    Require(pli.senderSsrc == 1);
    Require(pli.mediaSsrc == 0xFFFFFFFEu);
}

void TestShortPayloadLeavesThePacketUntouched() {
    std::vector<std::uint8_t> empty {0x81, 206, 0x00, 0x00};
    WebRtcRtcpPli pli = Filled();
    Require(!ParsePacket(pli, empty));
    Require(Unchanged(pli));

    std::vector<std::uint8_t> padded {0xA1, 206, 0x00, 0x02, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0x01};
    Require(!ParsePacket(pli, padded));
    Require(Unchanged(pli));

    WebRtcRtcpCommonHeader header {206, 1, 0, 7, nullptr};
    Require(!_ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, &header));
    Require(Unchanged(pli));
}

void TestNullArgumentsThrow() {
    std::vector<std::uint8_t> payload(8, 0x11);
    WebRtcRtcpCommonHeader header {206, 1, 0, 8, payload.data()};
    WebRtcRtcpPli pli = Filled();
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(nullptr, &header); });
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, nullptr); });
    header.payload = nullptr;
    RequireInvalidArgument([&] { _ZN6webrtc4rtcp3Pli5ParseERKNS0_12CommonHeaderE_nid_postfix(&pli, &header); });
    Require(Unchanged(pli));
}

}

using BlockLength = std::size_t (APS5_VABI*)(const WebRtcRtcpPli*);
using Create = bool (APS5_VABI*)(const WebRtcRtcpPli*, std::uint8_t*, std::size_t*, std::size_t, WebRtcRtcpPacketReadyCallback);
using Destroy = void (APS5_VABI*)(WebRtcRtcpPli*);

template<class TFunction>
TFunction Slot(const WebRtcRtcpPli& pli, int slot) {
    return reinterpret_cast<TFunction>(const_cast<void*>(static_cast<const void* const*>(pli.vtable)[slot]));
}

struct Flushed {
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    int calls = 0;
};

void APS5_VABI Flush(void* function, const std::uint8_t* data, std::size_t size) {
    auto* flushed = static_cast<Flushed*>(function);
    flushed->data = data;
    flushed->size = size;
    ++flushed->calls;
}

void TestConstructor() {
    WebRtcRtcpPli pli = Filled();
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(&pli);
    Require(pli.vtable != nullptr);
    Require(pli.senderSsrc == 0 && pli.mediaSsrc == 0);
    Require(Slot<BlockLength>(pli, 2)(&pli) == 12);
}

void TestCreateWritesHeaderAndSsrcs() {
    WebRtcRtcpPli pli;
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(&pli);
    pli.senderSsrc = 0x01020304;
    pli.mediaSsrc = 0x0A0B0C0D;
    std::uint8_t packet[12] = {};
    std::size_t index = 0;
    Require(Slot<Create>(pli, 3)(&pli, packet, &index, sizeof(packet), {}));
    Require(index == 12);
    const std::uint8_t expected[12] = {0x81, 0xCE, 0x00, 0x02, 0x01, 0x02, 0x03, 0x04, 0x0A, 0x0B, 0x0C, 0x0D};
    Require(std::memcmp(packet, expected, sizeof(expected)) == 0);

    index = 0;
    Require(!Slot<Create>(pli, 3)(&pli, packet, &index, 11, {}));
    Require(index == 0);
}

void TestCreateFlushesAFullBuffer() {
    WebRtcRtcpPli pli;
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(&pli);
    pli.senderSsrc = 7;
    std::uint8_t packet[12] = {1, 2, 3, 4};
    std::size_t index = 4;
    Flushed flushed;
    Require(Slot<Create>(pli, 3)(&pli, packet, &index, sizeof(packet), {&flushed, &Flush}));
    Require(flushed.calls == 1 && flushed.data == packet && flushed.size == 4);
    Require(index == 12);
    Require(packet[0] == 0x81 && packet[7] == 7);
}

std::size_t APS5_VABI LongerBlock(const WebRtcRtcpPli*) {
    return 16;
}

void TestCreateUsesTheVirtualBlockLength() {
    WebRtcRtcpPli pli;
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(&pli);
    const void* derived[4];
    std::memcpy(derived, pli.vtable, sizeof(derived));
    derived[2] = reinterpret_cast<const void*>(&LongerBlock);
    pli.vtable = derived;
    std::uint8_t packet[16] = {};
    std::size_t index = 0;
    Require(!Slot<Create>(pli, 3)(&pli, packet, &index, 12, {}));
    Require(Slot<Create>(pli, 3)(&pli, packet, &index, sizeof(packet), {}));
    Require(packet[3] == 3 && index == 12);
}

void TestDeletingDestructor() {
    auto* pli = static_cast<WebRtcRtcpPli*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpPli)));
    _ZN6webrtc4rtcp3PliC1Ev_nid_postfix(pli);
    Slot<Destroy>(*pli, 1)(pli);
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    TestPli();
    TestLongerPayloadReadsTheCommonFeedbackOnly();
    TestShortPayloadLeavesThePacketUntouched();
    TestNullArgumentsThrow();
    TestConstructor();
    TestCreateWritesHeaderAndSsrcs();
    TestCreateFlushesAFullBuffer();
    TestCreateUsesTheVirtualBlockLength();
    TestDeletingDestructor();
    return 0;
}

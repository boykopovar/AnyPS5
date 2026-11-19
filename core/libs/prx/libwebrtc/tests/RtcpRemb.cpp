#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {

int allocations = 0;
int frees = 0;
void* lastFreed = nullptr;
bool failAllocation = false;
void Require(bool condition) {
    if (!condition) throw std::runtime_error("Remb assertion failed");
}
void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (failAllocation) throw std::bad_alloc();
    ++allocations;
    return std::malloc(size);
}
void APS5_VABI Free_nid_no_patch(void* pointer) { if (pointer) { ++frees; lastFreed = pointer; } std::free(pointer); }
void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}


std::vector<std::uint8_t> Packet(std::uint8_t count, std::uint8_t exponent = 0, std::uint32_t mantissa = 0) {
    std::vector<std::uint8_t> bytes {0x8f, 206, static_cast<std::uint8_t>((4 + count) >> 8), static_cast<std::uint8_t>(4 + count),
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
        'R', 'E', 'M', 'B', count, static_cast<std::uint8_t>((exponent << 2) | (mantissa >> 16)),
        static_cast<std::uint8_t>(mantissa >> 8), static_cast<std::uint8_t>(mantissa)};
    for (unsigned i = 0; i < count; ++i) {
        bytes.insert(bytes.end(), {0x11, 0x22, 0x33, static_cast<std::uint8_t>(i)});
    }
    return bytes;
}

bool Parse(WebRtcRtcpRemb& remb, const std::vector<std::uint8_t>& bytes) {
    WebRtcRtcpCommonHeader header {};
    Require(_ZN6webrtc4rtcp12CommonHeader5ParseEPKhm_nid_postfix(&header, bytes.data(), bytes.size()));
    return _ZN6webrtc4rtcp4Remb5ParseERKNS0_12CommonHeaderE_nid_postfix(&remb, &header);
}

void TestValidAndReplacement() {
    const int before = allocations;
    WebRtcRtcpRemb remb {};
    remb.vtable = &remb;
    Require(Parse(remb, Packet(0)));
    Require(remb.vtable == &remb && remb.senderSsrc == 0x12345678 && remb.mediaSsrc == 0x9abcdef0);
    Require(remb.bitrateBps == 0 && remb.ssrcBegin == nullptr && allocations == before);
    Require(Parse(remb, Packet(3, 5, 0x3ffff)));
    Require(remb.bitrateBps == (0x3ffffLL << 5));
    Require(remb.ssrcEnd - remb.ssrcBegin == 3 && remb.ssrcCapacityEnd - remb.ssrcBegin == 3);
    for (int i = 0; i < 3; ++i) Require(remb.ssrcBegin[i] == 0x11223300u + i);
    auto* storage = remb.ssrcBegin;
    Require(Parse(remb, Packet(1, 63, 0)));
    Require(remb.ssrcBegin == storage && remb.ssrcEnd == storage + 1 && remb.bitrateBps == 0);
    Require(Parse(remb, Packet(0)));
    Require(remb.ssrcEnd == storage && remb.ssrcCapacityEnd == storage + 3);
    Require(Parse(remb, Packet(255, 45, 0x3ffff)));
    Require(remb.ssrcEnd - remb.ssrcBegin == 255 && remb.ssrcBegin[254] == 0x112233fe);
    Require(remb.bitrateBps == (0x3ffffLL << 45));
    _ZdlPv_nid_postfix(remb.ssrcBegin);
}

void TestFailures() {
    WebRtcRtcpRemb remb {};
    Require(Parse(remb, Packet(2)));
    auto* storage = remb.ssrcBegin;
    remb.senderSsrc = 17; remb.mediaSsrc = 18; remb.bitrateBps = 19;
    auto unchanged = [&] {
        Require(remb.senderSsrc == 17 && remb.mediaSsrc == 18 && remb.bitrateBps == 19);
        Require(remb.ssrcBegin == storage && remb.ssrcEnd == storage + 2 && storage[1] == 0x11223301);
    };
    auto wrong = Packet(0); wrong[12] = 'X';
    Require(!Parse(remb, wrong)); unchanged();
    auto shortPacket = Packet(0); shortPacket.resize(16); shortPacket[3] = 3;
    Require(!Parse(remb, shortPacket)); unchanged();
    auto mismatch = Packet(0); mismatch[16] = 1;
    Require(!Parse(remb, mismatch)); unchanged();
    mismatch = Packet(1); mismatch[16] = 0;
    Require(!Parse(remb, mismatch)); unchanged();
    for (auto pair : {std::pair<unsigned, unsigned>{63, 1}, {63, 2}, {47, 0x3ffff}}) {
        Require(!Parse(remb, Packet(0, pair.first, pair.second)));
        Require(remb.senderSsrc == 0x12345678 && remb.mediaSsrc == 0x9abcdef0);
        Require(remb.bitrateBps == std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(pair.second) << pair.first));
        Require(remb.ssrcBegin == storage && remb.ssrcEnd == storage + 2 && storage[1] == 0x11223301);
    }
    failAllocation = true;
    bool threw = false;
    try { Parse(remb, Packet(3)); } catch (const std::bad_alloc&) { threw = true; }
    failAllocation = false;
    Require(threw && remb.ssrcBegin == storage && remb.ssrcEnd == storage && remb.ssrcCapacityEnd == storage + 2);
    _ZdlPv_nid_postfix(storage);
}

void TestReleaseAndNulls() {
    WebRtcRtcpRemb remb {};
    auto bytes = Packet(0);
    bytes[0] = 0x81; bytes[1] = 200;
    Require(Parse(remb, bytes));
    WebRtcRtcpCommonHeader header {};
    auto invalid = [&](WebRtcRtcpRemb* object, const WebRtcRtcpCommonHeader* common) {
        bool threw = false;
        try { _ZN6webrtc4rtcp4Remb5ParseERKNS0_12CommonHeaderE_nid_postfix(object, common); }
        catch (const std::invalid_argument&) { threw = true; }
        Require(threw);
    };
    invalid(nullptr, &header);
    invalid(&remb, nullptr);
    Require(!_ZN6webrtc4rtcp4Remb5ParseERKNS0_12CommonHeaderE_nid_postfix(&remb, &header));
    header.payloadSize = 16;
    invalid(&remb, &header);
}

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);
using BlockLength = std::size_t (APS5_VABI*)(const WebRtcRtcpRemb*);
using Create = bool (APS5_VABI*)(const WebRtcRtcpRemb*, std::uint8_t*, std::size_t*, std::size_t, void*, PacketReady);
using Destroy = void (APS5_VABI*)(WebRtcRtcpRemb*);

template<typename TFunction> TFunction Slot(const WebRtcRtcpRemb& remb, int slot) {
    return reinterpret_cast<TFunction>(const_cast<void*>(static_cast<const void* const*>(remb.vtable)[slot]));
}

template<typename TFunction> void RequireInvalidArgument(TFunction function) {
    bool threw = false;
    try { function(); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

struct Sent {
    std::vector<std::uint8_t> bytes;
    int calls = 0;
};

void APS5_VABI Send(void* function, const std::uint8_t* data, std::size_t size) {
    auto* sent = static_cast<Sent*>(function);
    sent->bytes.assign(data, data + size);
    ++sent->calls;
}

WebRtcRtcpRemb Default() {
    WebRtcRtcpRemb remb;
    std::memset(&remb, 0xA5, sizeof(remb));
    _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(&remb);
    return remb;
}

std::vector<std::uint8_t> Serialize(const WebRtcRtcpRemb& remb) {
    std::vector<std::uint8_t> buffer(Slot<BlockLength>(remb, 2)(&remb) + 4, 0xee);
    std::size_t index = 0;
    Require(Slot<Create>(remb, 3)(&remb, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require(index + 4 == buffer.size() && buffer[index] == 0xee);
    buffer.resize(index);
    return buffer;
}

void TestConstructor() {
    WebRtcRtcpRemb remb = Default();
    Require(remb.vtable != nullptr && remb.senderSsrc == 0 && remb.mediaSsrc == 0 && remb.bitrateBps == 0);
    Require(remb.ssrcBegin == nullptr && remb.ssrcEnd == nullptr && remb.ssrcCapacityEnd == nullptr);
    Require(Slot<BlockLength>(remb, 2)(&remb) == 20);
    const std::vector<std::uint8_t> expected {0x8f, 206, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 'R', 'E', 'M', 'B', 0, 0, 0, 0};
    Require(Serialize(remb) == expected);
    const int before = frees;
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
    Require(frees == before);
}

void TestCreateRoundTripsParsedPackets() {
    for (const auto& packet : {Packet(0), Packet(3, 5, 0x3ffff), Packet(1, 0, 0x2345), Packet(255, 45, 0x3ffff)}) {
        WebRtcRtcpRemb remb = Default();
        Require(Parse(remb, packet));
        Require(Slot<BlockLength>(remb, 2)(&remb) == packet.size());
        Require(Serialize(remb) == packet);
        _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
    }
}

void TestCreateNormalizesTheBitrate() {
    WebRtcRtcpRemb remb = Default();
    Require(Parse(remb, Packet(2, 2, 0x100)));
    Require(remb.bitrateBps == 0x400);
    Require(Serialize(remb) == Packet(2, 0, 0x400));
    remb.bitrateBps = 0x40000;
    Require(Serialize(remb) == Packet(2, 1, 0x20000));
    remb.bitrateBps = 0x7fffffffffffffffLL;
    Require(Serialize(remb) == Packet(2, 45, 0x3ffff));
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
}

void TestCreateSendsAFullBuffer() {
    WebRtcRtcpRemb remb = Default();
    remb.senderSsrc = 0x01020304;
    std::vector<std::uint8_t> buffer(24, 0);
    std::size_t index = 5;
    Sent sent;
    Require(Slot<Create>(remb, 3)(&remb, buffer.data(), &index, buffer.size(), &sent, &Send));
    Require(sent.calls == 1 && sent.bytes.size() == 5 && index == 20);
    Require(buffer[0] == 0x8f && buffer[1] == 206 && buffer[3] == 4 && buffer[4] == 1 && buffer[7] == 4);
    index = 0;
    Require(!Slot<Create>(remb, 3)(&remb, buffer.data(), &index, 19, &sent, &Send));
    Require(index == 0 && sent.calls == 1);
    index = 4;
    RequireInvalidArgument([&] { Slot<Create>(remb, 3)(&remb, buffer.data(), &index, 20, nullptr, nullptr); });
    Require(index == 4);
}

void TestDestructors() {
    WebRtcRtcpRemb remb = Default();
    Require(Parse(remb, Packet(3)));
    auto* storage = remb.ssrcBegin;
    int before = frees;
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
    Require(frees == before + 1 && lastFreed == storage);
    auto* heap = static_cast<WebRtcRtcpRemb*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpRemb)));
    _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(heap);
    Require(Parse(*heap, Packet(2)));
    before = frees;
    Slot<Destroy>(*heap, 1)(heap);
    Require(frees == before + 2 && lastFreed == heap);
    auto* empty = static_cast<WebRtcRtcpRemb*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpRemb)));
    _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(empty);
    before = frees;
    Slot<Destroy>(*empty, 0)(empty);
    Require(frees == before);
    Slot<Destroy>(*empty, 1)(empty);
    Require(frees == before + 1 && lastFreed == empty);
}

void TestNullObjectsThrow() {
    RequireInvalidArgument([] { _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(nullptr); });
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch),
        reinterpret_cast<void*>(&PosixAlign_nid_no_patch), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    try {
        TestValidAndReplacement();
        TestFailures();
        TestReleaseAndNulls();
        TestConstructor();
        TestCreateRoundTripsParsedPackets();
        TestCreateNormalizesTheBitrate();
        TestCreateSendsAFullBuffer();
        TestDestructors();
        TestNullObjectsThrow();
        Require(allocations == frees);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

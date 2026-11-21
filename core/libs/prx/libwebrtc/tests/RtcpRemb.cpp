#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

int allocations = 0;
int frees = 0;
bool failAllocation = false;
void Require(bool condition) {
    if (!condition) throw std::runtime_error("Remb assertion failed");
}
void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (failAllocation) throw std::bad_alloc();
    ++allocations;
    return std::malloc(size);
}
void APS5_VABI Free_nid_no_patch(void* pointer) { if (pointer) ++frees; std::free(pointer); }
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

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);
using BlockLength = std::size_t (APS5_VABI*)(const WebRtcRtcpRemb*);
using Create = bool (APS5_VABI*)(const WebRtcRtcpRemb*, std::uint8_t*, std::size_t*, std::size_t, void*, PacketReady);
using DeletingDestructor = void (APS5_VABI*)(WebRtcRtcpRemb*);

template<typename TFunction> TFunction Slot(const WebRtcRtcpRemb& remb, int slot) {
    return reinterpret_cast<TFunction>(const_cast<void*>(static_cast<const void* const*>(remb.vtable)[slot]));
}

struct Sent {
    std::vector<std::vector<std::uint8_t>> packets;
};

void APS5_VABI Send(void* function, const std::uint8_t* data, std::size_t size) {
    static_cast<Sent*>(function)->packets.emplace_back(data, data + size);
}

template<typename TAction> bool ThrowsInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

WebRtcRtcpRemb Constructed() {
    WebRtcRtcpRemb remb;
    std::memset(&remb, 0xA5, sizeof(remb));
    _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(&remb);
    return remb;
}

void TestConstructor() {
    const int allocated = allocations;
    const int freed = frees;
    WebRtcRtcpRemb remb = Constructed();
    Require(remb.vtable != nullptr && remb.senderSsrc == 0 && remb.mediaSsrc == 0 && remb.bitrateBps == 0);
    Require(remb.ssrcBegin == nullptr && remb.ssrcEnd == nullptr && remb.ssrcCapacityEnd == nullptr);
    Require(Slot<BlockLength>(remb, 2)(&remb) == 20);
    std::vector<std::uint8_t> buffer(22, 0xee);
    std::size_t index = 1;
    Require(Slot<Create>(remb, 3)(&remb, buffer.data(), &index, 21, nullptr, nullptr));
    const std::vector<std::uint8_t> expected {0xee, 0x8f, 206, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 'R', 'E', 'M', 'B', 0, 0, 0, 0, 0xee};
    Require(index == 21 && buffer == expected);
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
    Require(allocations == allocated && frees == freed);
}

void TestCreateRoundTrip() {
    for (const auto& bytes : {Packet(0), Packet(3, 5, 0x3ffff), Packet(1, 0, 1), Packet(2, 45, 0x20000), Packet(255, 45, 0x3ffff)}) {
        WebRtcRtcpRemb remb = Constructed();
        Require(Parse(remb, bytes));
        Require(Slot<BlockLength>(remb, 2)(&remb) == bytes.size());
        std::vector<std::uint8_t> buffer(bytes.size(), 0xee);
        std::size_t index = 0;
        Require(Slot<Create>(remb, 3)(&remb, buffer.data(), &index, buffer.size(), nullptr, nullptr));
        Require(index == bytes.size() && buffer == bytes);
        _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
    }
}

void TestCreateBitrates() {
    struct Case {
        std::int64_t bitrate;
        std::uint8_t exponentAndHigh;
        std::uint8_t middle;
        std::uint8_t low;
    };
    for (const Case& test : {Case {0, 0, 0, 0}, Case {1, 0, 0, 1}, Case {0x3ffff, 3, 0xff, 0xff}, Case {0x40000, (1 << 2) | 2, 0, 0},
                             Case {0x7ffff, (1 << 2) | 3, 0xff, 0xff}, Case {1000000, (2 << 2) | 3, 0xd0, 0x90},
                             Case {0x7fffffffffffffffLL, static_cast<std::uint8_t>((45 << 2) | 3), 0xff, 0xff},
                             Case {-1, static_cast<std::uint8_t>((46 << 2) | 3), 0xff, 0xff}}) {
        WebRtcRtcpRemb remb = Constructed();
        remb.senderSsrc = 0x01020304;
        remb.mediaSsrc = 0x05060708;
        remb.bitrateBps = test.bitrate;
        std::array<std::uint8_t, 20> buffer {};
        std::size_t index = 0;
        Require(Slot<Create>(remb, 3)(&remb, buffer.data(), &index, buffer.size(), nullptr, nullptr));
        const std::array<std::uint8_t, 20> expected {0x8f, 206, 0, 4, 1, 2, 3, 4, 5, 6, 7, 8, 'R', 'E', 'M', 'B', 0, test.exponentAndHigh, test.middle, test.low};
        Require(index == 20 && buffer == expected);
        _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
    }
}

void TestCreateFullBuffer() {
    WebRtcRtcpRemb remb = Constructed();
    Require(Parse(remb, Packet(1, 0, 7)));
    std::vector<std::uint8_t> buffer(30, 0xee);
    std::size_t index = 0;
    Require(!Slot<Create>(remb, 3)(&remb, buffer.data(), &index, 23, nullptr, nullptr));
    Require(index == 0 && buffer[0] == 0xee);
    Sent sent;
    index = 7;
    Require(Slot<Create>(remb, 3)(&remb, buffer.data(), &index, 30, &sent, &Send));
    Require(sent.packets.size() == 1 && sent.packets[0] == std::vector<std::uint8_t>(7, 0xee));
    Require(index == 24 && std::vector<std::uint8_t>(buffer.begin(), buffer.begin() + 24) == Packet(1, 0, 7));
    sent.packets.clear();
    index = 7;
    Require(!Slot<Create>(remb, 3)(&remb, buffer.data(), &index, 23, &sent, &Send));
    Require(sent.packets.size() == 1 && index == 0);
    index = 7;
    Require(ThrowsInvalidArgument([&] { Slot<Create>(remb, 3)(&remb, buffer.data(), &index, 30, nullptr, nullptr); }));
    Require(index == 7);
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
}

void TestDestructors() {
    const int allocated = allocations;
    const int freed = frees;
    WebRtcRtcpRemb remb = Constructed();
    Require(Parse(remb, Packet(4)));
    Require(allocations == allocated + 1);
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&remb);
    Require(frees == freed + 1);
    auto* heap = static_cast<WebRtcRtcpRemb*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpRemb)));
    _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(heap);
    Require(Parse(*heap, Packet(2)));
    Require(allocations == allocated + 3);
    Slot<DeletingDestructor>(*heap, 1)(heap);
    Require(frees == freed + 3);
    WebRtcRtcpRemb other = Constructed();
    Require(Parse(other, Packet(1)));
    Require(allocations == allocated + 4);
    Slot<DeletingDestructor>(other, 0)(&other);
    Require(frees == freed + 4);
    other = Constructed();
    std::uint8_t buffer[20] {};
    std::size_t index = 0;
    Require(ThrowsInvalidArgument([] { _ZN6webrtc4rtcp4RembC1Ev_nid_postfix(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(nullptr); }));
    Require(ThrowsInvalidArgument([&] { Slot<BlockLength>(other, 2)(nullptr); }));
    Require(ThrowsInvalidArgument([&] { Slot<Create>(other, 3)(nullptr, buffer, &index, 20, nullptr, nullptr); }));
    Require(ThrowsInvalidArgument([&] { Slot<Create>(other, 3)(&other, nullptr, &index, 20, nullptr, nullptr); }));
    Require(ThrowsInvalidArgument([&] { Slot<Create>(other, 3)(&other, buffer, nullptr, 20, nullptr, nullptr); }));
    _ZN6webrtc4rtcp4RembD1Ev_nid_postfix(&other);
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
        TestCreateRoundTrip();
        TestCreateBitrates();
        TestCreateFullBuffer();
        TestDestructors();
        Require(allocations == frees);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

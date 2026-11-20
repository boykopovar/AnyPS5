#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <cstdio>
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
        Require(allocations == frees);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

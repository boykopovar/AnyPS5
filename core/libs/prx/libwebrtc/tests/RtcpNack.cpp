#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<void*, std::size_t> allocations;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Nack check failed at line %u\n", location.line());
        std::abort();
    }
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    auto* pointer = std::malloc(size);
    allocations.emplace(pointer, size);
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

bool Parse(WebRtcRtcpNack& nack, const std::vector<std::uint8_t>& payload) {
    WebRtcRtcpCommonHeader header {205, 1, 0, static_cast<std::uint32_t>(payload.size()), payload.data()};
    return _ZN6webrtc4rtcp4Nack5ParseERKNS0_12CommonHeaderE_nid_postfix(&nack, &header);
}

void CheckIds(const WebRtcRtcpNack& nack, const std::vector<std::uint16_t>& expected) {
    Require(static_cast<std::size_t>(nack.packetIds.end - nack.packetIds.begin) == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
        Require(nack.packetIds.begin[i] == expected[i]);
}

void Destroy(WebRtcRtcpNack& nack) {
    if (nack.packed.begin) _ZdlPv_nid_postfix(nack.packed.begin);
    if (nack.packetIds.begin) _ZdlPv_nid_postfix(nack.packetIds.begin);
}

void TestWraparound() {
    WebRtcRtcpNack nack {};
    Require(Parse(nack, {0,0,0,1,0,0,0,2,0xff,0xff,0x80,0x01}));
    CheckIds(nack, {65535, 0, 15});
    Destroy(nack);
}

void TestParsing() {
    WebRtcRtcpNack nack {};
    const int sentinel = 1;
    nack.vtable = &sentinel;
    for (std::size_t size = 0; size < 12; ++size) {
        const auto before = nack;
        Require(!Parse(nack, std::vector<std::uint8_t>(size)));
        Require(std::memcmp(&before, &nack, sizeof(nack)) == 0);
    }
    Require(Parse(nack, {0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xf0,0x12,0x34,0x81,0x02}));
    Require(nack.vtable == &sentinel);
    Require(nack.senderSsrc == 0x12345678 && nack.mediaSsrc == 0x9abcdef0);
    Require(nack.packed.end - nack.packed.begin == 1);
    Require(nack.packed.begin[0].firstPid == 0x1234 && nack.packed.begin[0].bitmask == 0x8102);
    CheckIds(nack, {0x1234, 0x1236, 0x123d, 0x1244});
    Require(Parse(nack, {0,0,0,3,0,0,0,4,0,9,0,1,0,9,0,0,0,1,0,2}));
    Require(nack.senderSsrc == 3 && nack.mediaSsrc == 4);
    Require(nack.packed.end - nack.packed.begin == 3);
    Require(nack.packed.begin[1].firstPid == 9 && nack.packed.begin[1].bitmask == 0);
    Require(nack.packed.begin[2].firstPid == 1 && nack.packed.begin[2].bitmask == 2);
    CheckIds(nack, {9,10,9,1,3});
    for (std::uint16_t bit = 0; bit < 16; ++bit) {
        const std::uint16_t mask = 1u << bit;
        Require(Parse(nack, {0,0,0,1,0,0,0,2,0,100,static_cast<std::uint8_t>(mask >> 8),static_cast<std::uint8_t>(mask)}));
        CheckIds(nack, {100, static_cast<std::uint16_t>(101 + bit)});
    }
    auto* packed = nack.packed.begin;
    auto* ids = nack.packetIds.begin;
    for (std::size_t trailing = 0; trailing < 4; ++trailing) {
        std::vector<std::uint8_t> payload {0,0,0,1,0,0,0,2,0,7,0,0};
        payload.insert(payload.end(), trailing, 0xff);
        Require(Parse(nack, payload));
        CheckIds(nack, {7});
        Require(nack.packed.end == nack.packed.begin + 1);
        Require(nack.packed.begin == packed && nack.packetIds.begin == ids);
    }
    std::vector<std::uint8_t> payload {0,0,0,1,0,0,0,2,0,8,0,0};
    WebRtcRtcpCommonHeader header {0, 31, 0, 12, payload.data()};
    Require(_ZN6webrtc4rtcp4Nack5ParseERKNS0_12CommonHeaderE_nid_postfix(&nack, &header));
    CheckIds(nack, {8});
    const auto before = nack;
    Require(!Parse(nack, std::vector<std::uint8_t>(11)));
    Require(std::memcmp(&before, &nack, sizeof(nack)) == 0);
    CheckIds(nack, {8});
    Destroy(nack);
}

void TestNulls() {
    WebRtcRtcpNack nack {};
    WebRtcRtcpCommonHeader header {205,1,0,12,nullptr};
    for (int which = 0; which < 3; ++which) {
        bool threw = false;
        try {
            _ZN6webrtc4rtcp4Nack5ParseERKNS0_12CommonHeaderE_nid_postfix(which == 0 ? nullptr : &nack, which == 1 ? nullptr : &header);
        } catch (const std::invalid_argument&) { threw = true; }
        Require(threw);
    }
}

}

int main(int argc, char**) {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try {
        TestWraparound();
        if (argc == 1) {
            TestParsing();
            TestNulls();
        }
        Require(allocations.empty());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Nack exception: %s\n", error.what());
        return 1;
    }
}

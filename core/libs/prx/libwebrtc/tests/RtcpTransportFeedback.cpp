#include "prx/libwebrtc/libwebrtc.hpp"

#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <vector>
#include <unordered_set>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtcp::TransportFeedback check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_set<void*> allocations;
std::size_t allocated = 0;
std::size_t freed = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    auto* value = std::malloc(size);
    Require(value != nullptr);
    allocations.insert(value);
    ++allocated;
    return value;
}

void APS5_VABI Free_nid_no_patch(void* value) {
    Require(allocations.erase(value) == 1);
    ++freed;
    std::free(value);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* value, std::size_t size) { return std::realloc(value, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
void* APS5_VABI Realign_nid_no_patch(void* value, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(value, size); }
int APS5_VABI PosixAlign_nid_no_patch(void** value, std::size_t alignment, std::size_t size) {
    *value = Align_nid_no_patch(alignment, size);
    return *value ? 0 : 12;
}

std::vector<std::uint8_t> Payload(std::uint16_t count, std::initializer_list<std::uint16_t> chunks, std::initializer_list<std::uint8_t> deltas = {}) {
    std::vector<std::uint8_t> bytes{0x12, 0x34, 0x56, 0x78, 0x87, 0x65, 0x43, 0x21, 0xff, 0xfe,
        static_cast<std::uint8_t>(count >> 8), static_cast<std::uint8_t>(count), 1, 2, 3, 9};
    for (auto chunk : chunks) {
        bytes.push_back(chunk >> 8);
        bytes.push_back(chunk);
    }
    bytes.insert(bytes.end(), deltas);
    return bytes;
}

bool Parse(WebRtcRtcpTransportFeedback& feedback, const std::vector<std::uint8_t>& payload) {
    WebRtcRtcpCommonHeader header{205, 15, 0, static_cast<std::uint32_t>(payload.size()), payload.data()};
    return _ZN6webrtc4rtcp17TransportFeedback5ParseERKNS0_12CommonHeaderE_nid_postfix(&feedback, &header);
}

std::size_t Received(const WebRtcRtcpTransportFeedback& feedback) {
    return feedback.receivedPackets.begin ? feedback.receivedPackets.end - feedback.receivedPackets.begin : 0;
}

void TestParsing() {
    WebRtcRtcpTransportFeedback feedback;
    _ZN6webrtc4rtcp17TransportFeedbackC1Ev_nid_postfix(&feedback);
    Require(feedback.senderSsrc == 0 && feedback.mediaSsrc == 0 && feedback.baseSequenceNumber == 0);
    Require(feedback.sequenceNumberCount == 0 && feedback.baseTimeTicks == 0 && feedback.feedbackSequence == 0);
    Require(feedback.includeTimestamps && feedback.lastTimestamp == 0 && feedback.sizeBytes == 20);
    Require(!feedback.receivedPackets.begin && !feedback.allPackets.begin && !feedback.encodedChunks.begin);
    Require(feedback.lastChunk.size == 0 && feedback.lastChunk.allSame && !feedback.lastChunk.hasLargeDelta);
    Require(feedback.vtable && !feedback.vtable[-1] && !feedback.vtable[-2]);
    using Unsupported = void(APS5_VABI*)();
    for (auto slot : {2, 3}) {
        bool threw = false;
        try { reinterpret_cast<Unsupported>(feedback.vtable[slot])(); } catch (const std::exception&) { threw = true; }
        Require(threw);
    }
    const std::int64_t base = ((std::int64_t{1} << 24) + 0x010203) * 64000;
    Require(Parse(feedback, Payload(3, {0x2003}, {0, 255, 1})));
    Require(feedback.senderSsrc == 0x12345678 && feedback.mediaSsrc == 0x87654321);
    Require(feedback.baseSequenceNumber == 65534 && feedback.sequenceNumberCount == 3);
    Require(feedback.baseTimeTicks == 0x010203 && feedback.feedbackSequence == 9);
    Require(Received(feedback) == 3 && feedback.receivedPackets.begin[2].sequenceNumber == 0);
    Require(feedback.receivedPackets.begin[1].deltaTicks == 255 && feedback.lastTimestamp == base + 256 * 250);
    Require(feedback.sizeBytes == 25 && feedback.lastChunk.size == 3 && feedback.lastChunk.allSame);
    Require(Parse(feedback, Payload(4, {0xa800}, {1, 2})));
    Require(Received(feedback) == 2 && feedback.receivedPackets.begin[1].sequenceNumber == 0);
    Require(!feedback.lastChunk.allSame && !feedback.lastChunk.hasLargeDelta);
    Require(Parse(feedback, Payload(3, {0xd800}, {255, 0xff, 0xfe})));
    Require(Received(feedback) == 2 && feedback.receivedPackets.begin[1].deltaTicks == -2);
    Require(feedback.lastTimestamp == base + 253 * 250 && feedback.lastChunk.hasLargeDelta);
    Require(Parse(feedback, Payload(2, {0x4002}, {0x80, 0, 0x7f, 0xff})));
    Require(feedback.receivedPackets.begin[0].deltaTicks == -32768 && feedback.receivedPackets.begin[1].deltaTicks == 32767);
    Require(Parse(feedback, Payload(16, {0x000e, 0x2002}, {1, 2})));
    Require(feedback.encodedChunks.end - feedback.encodedChunks.begin == 1 && feedback.encodedChunks.begin[0] == 14);
    Require(Received(feedback) == 2 && feedback.receivedPackets.begin[0].sequenceNumber == 12);
    Require(Parse(feedback, Payload(3, {0x2003}, {7})));
    Require(!feedback.includeTimestamps && Received(feedback) == 3 && feedback.lastTimestamp == base && feedback.sizeBytes == 22);
    Require(feedback.receivedPackets.begin[0].deltaTicks == 0);
    Require(Parse(feedback, Payload(1, {0x2001}, {9})));
    Require(!feedback.includeTimestamps && feedback.receivedPackets.begin[0].deltaTicks == 9);
    Require(!Parse(feedback, std::vector<std::uint8_t>(17)));
    Require(Received(feedback) == 1 && feedback.sequenceNumberCount == 1);
    feedback.allPackets.begin = static_cast<WebRtcFeedbackReceivedPacket*>(_Znwm_nid_postfix(4));
    feedback.allPackets.end = feedback.allPackets.capacity = feedback.allPackets.begin + 1;
    for (auto bytes : {Payload(0, {0}), Payload(3, {1}), Payload(3, {1}, {0}), Payload(1, {0x6001}, {1, 2, 3}), Payload(2, {0xdc00}, {1, 2, 3, 4})}) {
        Require(!Parse(feedback, bytes));
        Require(feedback.sequenceNumberCount == 0 && Received(feedback) == 0 && feedback.lastChunk.size == 0);
        Require(feedback.encodedChunks.end == feedback.encodedChunks.begin && feedback.lastTimestamp == base && feedback.sizeBytes == 20);
        Require(feedback.allPackets.end == feedback.allPackets.begin);
    }
    Require(Parse(feedback, Payload(1, {0x6001})));
    Require(Received(feedback) == 1 && feedback.receivedPackets.begin[0].deltaTicks == 0);
    const auto before = freed;
    using Destroy = void(APS5_VABI*)(WebRtcRtcpTransportFeedback*);
    reinterpret_cast<Destroy>(feedback.vtable[0])(&feedback);
    Require(freed == before + 3 && allocations.empty());
    auto* heap = static_cast<WebRtcRtcpTransportFeedback*>(_Znwm_nid_postfix(sizeof(feedback)));
    _ZN6webrtc4rtcp17TransportFeedbackC1Ev_nid_postfix(heap);
    Require(Parse(*heap, Payload(1, {0x2001}, {1})));
    reinterpret_cast<Destroy>(heap->vtable[1])(heap);
    Require(allocations.empty() && allocated == freed);
}

void TestBaseSequence() {
    alignas(WebRtcRtcpTransportFeedback) unsigned char storage[sizeof(WebRtcRtcpTransportFeedback) + 64];
    std::memset(storage, 0xA5, sizeof(storage));
    auto* feedback = reinterpret_cast<WebRtcRtcpTransportFeedback*>(storage);
    for (std::uint16_t value : {std::uint16_t {0}, std::uint16_t {1}, std::uint16_t {0x1234}, std::uint16_t {0xFFFF}}) {
        feedback->baseSequenceNumber = value;
        Require(_ZNK6webrtc4rtcp17TransportFeedback15GetBaseSequenceEv_nid_postfix(feedback) == value);
    }
    feedback->baseSequenceNumber = 0x0102;
    feedback->sequenceNumberCount = 0xBEEF;
    feedback->senderSsrc = 0x11111111u;
    feedback->mediaSsrc = 0x22222222u;
    Require(_ZNK6webrtc4rtcp17TransportFeedback15GetBaseSequenceEv_nid_postfix(feedback) == 0x0102);
    unsigned char before[sizeof(storage)];
    std::memcpy(before, storage, sizeof(storage));
    _ZNK6webrtc4rtcp17TransportFeedback15GetBaseSequenceEv_nid_postfix(feedback);
    Require(std::memcmp(before, storage, sizeof(storage)) == 0);
}

void TestNullPacketThrows() {
    try {
        _ZNK6webrtc4rtcp17TransportFeedback15GetBaseSequenceEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try { TestParsing(); } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    TestBaseSequence();
    TestNullPacketThrows();
    return 0;
}

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<void*, std::size_t> allocations;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "RtpPacket check failed at line %u\n", location.line());
        std::abort();
    }
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    auto* pointer = std::malloc(size);
    allocations.emplace(pointer, size);
    std::memset(pointer, 0xa5, size);
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

using Bytes = std::vector<std::uint8_t>;

WebRtcRtpPacket Constructed(std::size_t capacity = 1500) {
    WebRtcRtpPacket packet;
    std::memset(&packet, 0, sizeof(packet));
    _ZN3rtc17CopyOnWriteBufferC1Emm_nid_postfix(&packet.buffer, 12, capacity);
    std::memset(packet.buffer.buffer->data, 0, 12);
    packet.buffer.buffer->data[0] = 0x80;
    packet.payloadOffset = 12;
    return packet;
}

void Destroy(WebRtcRtpPacket& packet) {
    if (packet.extensionEntries.begin)
        _ZdlPv_nid_postfix(packet.extensionEntries.begin);
    _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(&packet.buffer);
}

bool Parse(WebRtcRtpPacket& packet, const Bytes& bytes) {
    return _ZN6webrtc9RtpPacket5ParseEPKhm_nid_postfix(&packet, bytes.data(), bytes.size());
}

const std::uint8_t* Data(const WebRtcRtpPacket& packet) {
    return packet.buffer.buffer->data + packet.buffer.offset;
}

bool Holds(const WebRtcRtpPacket& packet, const Bytes& bytes) {
    return packet.buffer.size == bytes.size() && std::memcmp(Data(packet), bytes.data(), bytes.size()) == 0;
}

std::size_t EntryCount(const WebRtcRtpPacket& packet) {
    return packet.extensionEntries.end - packet.extensionEntries.begin;
}

bool Entry(const WebRtcRtpPacket& packet, std::size_t index, int id, int length, int offset) {
    const auto& entry = packet.extensionEntries.begin[index];
    return entry.id == id && entry.length == length && entry.offset == offset;
}

void RequireCleared(const WebRtcRtpPacket& packet) {
    Require(!packet.marker && packet.payloadType == 0 && packet.paddingSize == 0);
    Require(packet.sequenceNumber == 0 && packet.timestamp == 0 && packet.ssrc == 0);
    Require(packet.payloadOffset == 12 && packet.payloadSize == 0 && packet.extensionsSize == 0);
    Require(EntryCount(packet) == 0);
    Require(Holds(packet, {0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
}

Bytes Header(std::uint8_t first, std::uint8_t second = 0xe1) {
    return {first, second, 0x12, 0x34, 0x89, 0xab, 0xcd, 0xef, 0xfe, 0xdc, 0xba, 0x98};
}

Bytes Joined(Bytes bytes, const Bytes& tail) {
    bytes.insert(bytes.end(), tail.begin(), tail.end());
    return bytes;
}

template<typename TAction> bool ThrowsInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestHeader() {
    WebRtcRtpPacket packet = Constructed();
    auto* backing = packet.buffer.buffer;
    const Bytes bytes = Joined(Header(0x82), {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
    Require(Parse(packet, bytes));
    Require(packet.marker && packet.payloadType == 0x61 && packet.sequenceNumber == 0x1234);
    Require(packet.timestamp == 0x89abcdef && packet.ssrc == 0xfedcba98);
    Require(packet.payloadOffset == 20 && packet.payloadSize == 3 && packet.paddingSize == 0);
    Require(packet.extensionsSize == 0 && EntryCount(packet) == 0);
    Require(packet.buffer.buffer == backing && packet.buffer.offset == 0 && Holds(packet, bytes));
    Require(backing->size == bytes.size() && backing->capacity == 1500);
    Require(Parse(packet, Header(0x80, 0x00)));
    Require(!packet.marker && packet.payloadType == 0 && packet.payloadOffset == 12 && packet.payloadSize == 0);
    Require(Parse(packet, Joined(Header(0xa0), {9, 9, 0, 3})));
    Require(packet.paddingSize == 3 && packet.payloadSize == 1);
    Require(Parse(packet, Header(0xa0)));
    Require(packet.paddingSize == 0 && packet.payloadSize == 0);
    Destroy(packet);
}

void TestOneByteExtensions() {
    WebRtcRtpPacket packet = Constructed();
    const Bytes extensions {0xbe, 0xde, 0, 3, 0x10, 0xaa, 0, 0x21, 0xbb, 0xcc, 0x10, 0xdd, 0, 0, 0, 0};
    Require(Parse(packet, Joined(Joined(Header(0x91), {0, 0, 0, 7}), Joined(extensions, {0x55}))));
    Require(packet.payloadOffset == 32 && packet.payloadSize == 1 && packet.extensionsSize == 11);
    Require(EntryCount(packet) == 2);
    Require(Entry(packet, 0, 1, 1, 27) && Entry(packet, 1, 2, 2, 24));
    Require(Parse(packet, Joined(Header(0x90), {0xbe, 0xde, 0, 2, 0x11, 1, 2, 0xf0, 0x30, 0, 0, 0})));
    Require(EntryCount(packet) == 1 && Entry(packet, 0, 1, 2, 17) && packet.extensionsSize == 3);
    Require(packet.payloadOffset == 24 && packet.payloadSize == 0);
    Require(Parse(packet, Joined(Header(0x90), {0xbe, 0xde, 0, 1, 0x05, 0x10, 0, 0})));
    Require(EntryCount(packet) == 0 && packet.extensionsSize == 0);
    Require(Parse(packet, Joined(Header(0x90), {0xbe, 0xde, 0, 1, 0x10, 0x23, 1, 2})));
    Require(EntryCount(packet) == 1 && Entry(packet, 0, 1, 1, 17) && packet.extensionsSize == 2);
    Require(Parse(packet, Joined(Header(0x90), {0xbe, 0xde, 0, 0})));
    Require(EntryCount(packet) == 0 && packet.payloadOffset == 16);
    Destroy(packet);
}

void TestTwoByteExtensions() {
    WebRtcRtpPacket packet = Constructed();
    const Bytes extensions {0x10, 0x07, 0, 3, 5, 0, 0, 200, 2, 0xab, 0xcd, 0, 7, 3, 0, 0};
    Require(Parse(packet, Joined(Joined(Header(0x90), extensions), {0x99})));
    Require(packet.extensionsSize == 8 && packet.payloadOffset == 28 && packet.payloadSize == 1);
    Require(EntryCount(packet) == 2 && Entry(packet, 0, 5, 0, 18) && Entry(packet, 1, 200, 2, 21));
    Require(Parse(packet, Joined(Header(0x90), {0x10, 0x00, 0, 1, 9, 3, 1, 2})));
    Require(EntryCount(packet) == 0 && packet.extensionsSize == 0);
    Require(Parse(packet, Joined(Header(0x90), {0x12, 0x34, 0, 1, 9, 1, 1, 0})));
    Require(EntryCount(packet) == 0 && packet.extensionsSize == 0 && packet.payloadOffset == 20);
    Bytes large = Joined(Header(0x90), {0x10, 0x00, 0xff, 0xff});
    large.push_back(1);
    large.push_back(255);
    large.insert(large.end(), 255, 0x5a);
    large.resize(16 + 65600, 0);
    large.push_back(2);
    large.push_back(4);
    large.resize(16 + 0xffff * 4, 0);
    Require(Parse(packet, large));
    Require(packet.payloadOffset == large.size() && packet.payloadSize == 0);
    Require(packet.extensionsSize == 65600 && EntryCount(packet) == 2);
    Require(Entry(packet, 0, 1, 255, 18) && Entry(packet, 1, 2, 0, 0));
    Destroy(packet);
}

void TestFailuresClear() {
    WebRtcRtpPacket packet = Constructed();
    const Bytes valid = Joined(Header(0x90), {0xbe, 0xde, 0, 1, 0x10, 0x23, 0, 0, 0x44});
    const std::vector<Bytes> invalid {
        {},
        Bytes(11, 0x80),
        Joined(Header(0x40), {0}),
        Joined(Header(0xc0), {0}),
        Joined(Header(0x81), {0, 0, 0}),
        Joined(Header(0x90), {0xbe, 0xde, 0}),
        Joined(Header(0x90), {0xbe, 0xde, 0, 2, 0x10, 0x23, 0, 0}),
        Joined(Header(0xa0), {1, 0}),
        Joined(Header(0xa0), {1, 4}),
        Joined(Header(0xb0), {0xbe, 0xde, 0, 0, 5}),
    };
    for (const auto& bytes : invalid) {
        Require(Parse(packet, valid));
        Require(EntryCount(packet) == 1 && packet.payloadSize == 1);
        auto* entries = packet.extensionEntries.begin;
        auto* backing = packet.buffer.buffer;
        Require(!Parse(packet, bytes));
        RequireCleared(packet);
        Require(packet.extensionEntries.begin == entries && packet.buffer.buffer == backing);
        Require(backing->size == valid.size());
    }
    Require(Parse(packet, valid));
    Require(!_ZN6webrtc9RtpPacket5ParseEPKhm_nid_postfix(&packet, nullptr, 0));
    RequireCleared(packet);
    Destroy(packet);
}

void TestSharedBuffer() {
    WebRtcRtpPacket packet = Constructed(64);
    const Bytes first = Joined(Header(0x80), {1, 2, 3});
    Require(Parse(packet, first));
    WebRtcCopyOnWriteBuffer copy;
    _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(&copy, &packet.buffer);
    const Bytes second = Joined(Header(0x80), {4, 5});
    Require(Parse(packet, second));
    Require(packet.buffer.buffer != copy.buffer && Holds(packet, second));
    Require(packet.buffer.buffer->capacity == 64 && packet.buffer.buffer->refCount == 1);
    Require(copy.buffer->refCount == 1 && copy.size == first.size());
    Require(std::memcmp(copy.buffer->data, first.data(), first.size()) == 0);
    _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(&copy);
    _ZN3rtc17CopyOnWriteBufferC1ERKS0__nid_postfix(&copy, &packet.buffer);
    Require(!Parse(packet, {0}));
    RequireCleared(packet);
    Require(packet.buffer.buffer != copy.buffer && packet.buffer.buffer->capacity == 64);
    Require(packet.buffer.buffer->size == second.size());
    Require(copy.size == second.size() && std::memcmp(copy.buffer->data, second.data(), second.size()) == 0);
    _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(&copy);
    packet.buffer.offset = 4;
    packet.buffer.size = 8;
    Require(!Parse(packet, {}));
    Require(packet.buffer.offset == 4 && packet.buffer.buffer->size == 16);
    Require(packet.buffer.size == 12 && Data(packet)[0] == 0x80);
    Require(Parse(packet, second));
    Require(packet.buffer.offset == 0 && Holds(packet, second));
    Destroy(packet);
}

void TestGrowth() {
    WebRtcRtpPacket packet = Constructed(16);
    auto* backing = packet.buffer.buffer;
    const Bytes twenty = Joined(Header(0x80), Bytes(8, 7));
    Require(Parse(packet, twenty));
    Require(packet.buffer.buffer == backing && backing->capacity == 24 && Holds(packet, twenty));
    const Bytes forty = Joined(Header(0x80), Bytes(28, 9));
    Require(Parse(packet, forty));
    Require(backing->capacity == 40 && Holds(packet, forty));
    Require(Parse(packet, Joined(Header(0x80), {1})));
    Require(backing->capacity == 40 && backing->size == 13);
    _ZN3rtc17CopyOnWriteBufferD1Ev_nid_postfix(&packet.buffer);
    packet.buffer = {};
    Require(Parse(packet, twenty));
    Require(packet.buffer.buffer->capacity == 20 && packet.buffer.buffer->refCount == 1 && Holds(packet, twenty));
    WebRtcRtpPacket entries = Constructed();
    Bytes many = Joined(Header(0x90), {0x10, 0x00, 0, 3});
    for (int id = 1; id <= 6; ++id) {
        many.push_back(id);
        many.push_back(0);
    }
    Require(Parse(entries, many));
    Require(EntryCount(entries) == 5 && entries.extensionsSize == 10);
    Require(entries.extensionEntries.capacityEnd - entries.extensionEntries.begin == 8);
    for (int id = 1; id <= 5; ++id)
        Require(Entry(entries, id - 1, id, 0, 16 + 2 * id));
    Destroy(entries);
    Destroy(packet);
}

void TestThrows() {
    WebRtcRtpPacket packet = Constructed();
    const Bytes bytes = Header(0x80);
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc9RtpPacket5ParseEPKhm_nid_postfix(nullptr, bytes.data(), bytes.size()); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc9RtpPacket5ParseEPKhm_nid_postfix(&packet, nullptr, 12); }));
    Destroy(packet);
    WebRtcRtpPacket empty {};
    Require(ThrowsInvalidArgument([&] { Parse(empty, {}); }));
    WebRtcRtpPacket small = Constructed(12);
    small.buffer.offset = 1;
    small.buffer.size = 11;
    Require(ThrowsInvalidArgument([&] { Parse(small, {}); }));
    Destroy(small);
}

}

int main(int argc, char** argv) {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    const std::string_view test = argc > 1 ? argv[1] : "";
    try {
        if (test.empty() || test == "header") {
            TestHeader();
            TestFailuresClear();
            TestThrows();
        }
        if (test.empty() || test == "extensions") {
            TestOneByteExtensions();
            TestTwoByteExtensions();
        }
        if (test.empty() || test == "buffer") {
            TestSharedBuffer();
            TestGrowth();
        }
        Require(allocations.empty());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "RtpPacket exception: %s\n", error.what());
        return 1;
    }
}

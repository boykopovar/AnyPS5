#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtcp::RtcpPacket::Build check failed at line %u\n", location.line());
    std::abort();
}

struct Callback {
    void* function;
    void* call;
};

struct FakePacket {
    const void* const* vtable;
    std::size_t blockLength;
    std::uint8_t* seenPacket;
    std::size_t seenIndex;
    std::size_t seenMaxLength;
    Callback seenCallback;
    int creates;
    bool fail;
};

std::size_t APS5_VABI FakeBlockLength(const FakePacket* packet) {
    return packet->blockLength;
}

bool APS5_VABI FakeCreate(FakePacket* packet, std::uint8_t* buffer, std::size_t* index, std::size_t maxLength, Callback callback) {
    ++packet->creates;
    packet->seenPacket = buffer;
    packet->seenIndex = *index;
    packet->seenMaxLength = maxLength;
    packet->seenCallback = callback;
    if (packet->fail) throw std::runtime_error("create failed");
    for (std::size_t i = 0; i < packet->blockLength; ++i) buffer[*index + i] = static_cast<std::uint8_t>(0x10 + i);
    *index += packet->blockLength;
    return true;
}

const void* const FakeVtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&FakeBlockLength),
    reinterpret_cast<const void*>(&FakeCreate),
};

void TestBuildSizesTheBufferAndCreatesIntoIt() {
    FakePacket packet{FakeVtable, 12};
    packet.seenCallback = {reinterpret_cast<void*>(1), reinterpret_cast<void*>(1)};
    WebRtcBuffer buffer;
    std::memset(&buffer, 0xA5, sizeof(buffer));
    Require(_ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(&buffer, &packet) == &buffer);
    Require(buffer.size == 12 && buffer.capacity == 12 && buffer.data != nullptr);
    Require(packet.creates == 1 && packet.seenPacket == buffer.data && packet.seenIndex == 0 && packet.seenMaxLength == 12);
    Require(packet.seenCallback.call == nullptr);
    for (std::size_t i = 0; i < 12; ++i) Require(buffer.data[i] == 0x10 + i);
    _ZdaPv_nid_postfix(buffer.data);
}

void TestEmptyBlockHasNoData() {
    FakePacket packet{FakeVtable, 0};
    WebRtcBuffer buffer;
    std::memset(&buffer, 0xA5, sizeof(buffer));
    _ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(&buffer, &packet);
    Require(buffer.size == 0 && buffer.capacity == 0 && buffer.data == nullptr);
    Require(packet.creates == 1 && packet.seenPacket == nullptr && packet.seenMaxLength == 0);
}

void TestCreateExceptionPropagates() {
    FakePacket packet{FakeVtable, 8};
    packet.fail = true;
    WebRtcBuffer buffer{};
    bool thrown = false;
    try {
        _ZNK6webrtc4rtcp10RtcpPacket5BuildEv_nid_postfix(&buffer, &packet);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown && packet.creates == 1);
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    TestBuildSizesTheBufferAndCreatesIntoIt();
    TestEmptyBlockHasNoData();
    TestCreateExceptionPropagates();
    return 0;
}

#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "VideoEncoder InitEncode check failed at line %u\n", location.line());
    std::abort();
}

using Vtable = std::array<void (APS5_VABI *)(), 5>;

struct FakeEncoder {
    const void* vtable;
    WebRtcVideoEncoder* receiver{};
    const WebRtcVideoCodec* codec{};
    bool lossNotification{true};
    int cores{};
    std::uint64_t payload{};
    bool threadLimitEngaged{true};
    int calls{};
    int result{};
};

int APS5_VABI Record(WebRtcVideoEncoder* encoder, const WebRtcVideoCodec* codec, const void* settings) {
    auto& fake = *reinterpret_cast<FakeEncoder*>(encoder);
    const auto* bytes = static_cast<const unsigned char*>(settings);
    fake.receiver = encoder;
    fake.codec = codec;
    std::memcpy(&fake.lossNotification, bytes, 1);
    std::memcpy(&fake.cores, bytes + 4, 4);
    std::memcpy(&fake.payload, bytes + 8, 8);
    std::memcpy(&fake.threadLimitEngaged, bytes + 20, 1);
    ++fake.calls;
    return fake.result;
}

int APS5_VABI WrongSlot(WebRtcVideoEncoder*, const WebRtcVideoCodec*, const void*) {
    Require(false);
    return 0;
}

template <typename TCallback>
void SetCallback(Vtable& vtable, std::size_t slot, TCallback callback) {
    static_assert(sizeof(callback) == sizeof(vtable[slot]));
    std::memcpy(&vtable[slot], &callback, sizeof(callback));
}

void TestCase(int cores, std::size_t payload, int result, bool nullCodec) {
    Vtable vtable{};
    SetCallback(vtable, 3, WrongSlot);
    SetCallback(vtable, 4, Record);
    FakeEncoder fake{vtable.data()};
    fake.result = result;
    auto* encoder = reinterpret_cast<WebRtcVideoEncoder*>(&fake);
    const std::array<unsigned char, 17> codecBytes{};
    const auto* codec = nullCodec ? nullptr : reinterpret_cast<const WebRtcVideoCodec*>(codecBytes.data());
    Require(_ZN6webrtc12VideoEncoder10InitEncodeEPKNS_10VideoCodecEim_nid_postfix(encoder, codec, cores, payload) == result);
    Require(fake.calls == 1);
    Require(fake.receiver == encoder && fake.codec == codec);
    Require(!fake.lossNotification);
    Require(fake.cores == cores && fake.payload == payload);
    Require(!fake.threadLimitEngaged);
}

void ExpectThrow(WebRtcVideoEncoder* encoder) {
    try {
        _ZN6webrtc12VideoEncoder10InitEncodeEPKNS_10VideoCodecEim_nid_postfix(encoder, nullptr, 0, 0);
        Require(false);
    } catch (const std::invalid_argument&) {
    }
}

}

int main() {
    for (int cores : {0, -1, -37, 1, 8, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        for (std::size_t payload : {std::size_t{0}, std::size_t{1200}, std::size_t{0x100000001}, std::numeric_limits<std::size_t>::max()}) {
            TestCase(cores, payload, 0x3579, false);
            TestCase(cores, payload, -2345, true);
        }
    }
    ExpectThrow(nullptr);
    FakeEncoder fake{};
    ExpectThrow(reinterpret_cast<WebRtcVideoEncoder*>(&fake));
    Vtable vtable{};
    fake.vtable = vtable.data();
    ExpectThrow(reinterpret_cast<WebRtcVideoEncoder*>(&fake));
    return 0;
}

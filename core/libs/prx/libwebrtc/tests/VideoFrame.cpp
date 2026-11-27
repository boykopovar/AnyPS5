#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

int addRefs = 0;
int releases = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc VideoFrame check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

void APS5_VABI AddRef_nid_no_patch(const void*) { ++addRefs; }
int APS5_VABI Release_nid_no_patch(const void*) { ++releases; return 1; }

const void* const BufferVtable[] = {reinterpret_cast<const void*>(&AddRef_nid_no_patch), reinterpret_cast<const void*>(&Release_nid_no_patch)};
const void* const BrokenVtable[] = {nullptr, nullptr};

struct FakeBuffer {
    const void* vtable;
};

constexpr unsigned char Fill = 0xAB;

struct Written {
    std::size_t offset;
    std::size_t size;
};

constexpr Written WrittenBytes[] = {
    {8, 8}, {16, 4}, {24, 8}, {32, 8}, {40, 1}, {48, 1}, {56, 1}, {64, 1}, {72, 4}, {76, 1}, {144, 1},
    {148, 1}, {152, 1}, {156, 1}, {160, 1}, {176, 1}, {184, 8}, {192, 1}, {208, 1},
};

WebRtcVideoFrame Filled() {
    WebRtcVideoFrame frame;
    std::memset(&frame, Fill, sizeof(frame));
    return frame;
}

void RequireOnlyWritten(const WebRtcVideoFrame& frame) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&frame);
    for (std::size_t index = 0; index < sizeof(frame); ++index) {
        bool written = false;
        for (const auto& range : WrittenBytes)
            written = written || (index >= range.offset && index < range.offset + range.size);
        if (!written)
            Require(bytes[index] == Fill);
    }
}

void TestConstructor() {
    FakeBuffer buffer {BufferVtable};
    void* reference = &buffer;
    WebRtcVideoFrame frame = Filled();
    _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(&frame, &reference, 0x11223344u, 85, 90);
    Require(addRefs == 1 && releases == 0);
    Require(frame.videoFrameBuffer == &buffer && frame.timestampRtp == 0x11223344u && frame.ntpTimeMs == 0 && frame.timestampUs == 85000);
    Require(!frame.presentationTimestamp.engaged && !frame.referenceTime.engaged && frame.rotation == 90);
    Require(frame.colorSpace[0] == 0 && frame.colorSpace[68] == 0 && !frame.useLowLatencyRendering);
    Require(!frame.hasMaxCompositionDelayInFrames && !frame.hasUpdateRect && frame.packetInfos == nullptr && !frame.hasProcessingTime);
    Require(frame.id == 0xABAB);
    RequireOnlyWritten(frame);
    Require(_ZNK6webrtc10VideoFrame14render_time_msEv_nid_postfix(&frame) == 85);

    void* none = nullptr;
    frame = Filled();
    _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(&frame, &none, 0, -7, 270);
    Require(addRefs == 1 && frame.videoFrameBuffer == nullptr && frame.timestampUs == -7000 && frame.rotation == 270);
    RequireOnlyWritten(frame);

    frame = Filled();
    constexpr std::int64_t large = std::numeric_limits<std::int64_t>::max() / 1000 + 1;
    _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(&frame, &none, 0, large, 0);
    Require(frame.timestampUs == static_cast<std::int64_t>(static_cast<std::uint64_t>(large) * 1000u));
}

void TestDestructor() {
    FakeBuffer buffer {BufferVtable};
    void* reference = &buffer;
    WebRtcVideoFrame frame = Filled();
    _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(&frame, &reference, 1, 2, 0);
    _ZN6webrtc10VideoFrameD1Ev_nid_postfix(&frame);
    Require(addRefs == 1 && releases == 1);

    void* none = nullptr;
    frame = Filled();
    _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(&frame, &none, 1, 2, 0);
    _ZN6webrtc10VideoFrameD1Ev_nid_postfix(&frame);
    Require(releases == 1);

    frame = Filled();
    _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(&frame, &reference, 1, 2, 0);
    frame.packetInfos = &frame;
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc10VideoFrameD1Ev_nid_postfix(&frame); });
    Require(releases == 1);
    frame.packetInfos = nullptr;
    _ZN6webrtc10VideoFrameD1Ev_nid_postfix(&frame);
    Require(addRefs == 2 && releases == 2);
}

void TestInvalid() {
    FakeBuffer broken {BrokenVtable};
    FakeBuffer empty {nullptr};
    void* reference = &broken;
    void* none = nullptr;
    WebRtcVideoFrame frame = Filled();
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(nullptr, &none, 0, 0, 0); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(&frame, nullptr, 0, 0, 0); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(&frame, &reference, 0, 0, 0); });
    reference = &empty;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(&frame, &reference, 0, 0, 0); });
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc10VideoFrameD1Ev_nid_postfix(nullptr); });
    frame = Filled();
    _ZN6webrtc10VideoFrameC1ERKNS_13scoped_refptrINS_16VideoFrameBufferEEEjlNS_13VideoRotationE_nid_postfix(&frame, &none, 0, 0, 0);
    frame.videoFrameBuffer = &broken;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10VideoFrameD1Ev_nid_postfix(&frame); });
    Require(addRefs == 0 && releases == 0);
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2);
        if (std::strcmp(argv[1], "constructor") == 0) TestConstructor();
        else if (std::strcmp(argv[1], "destructor") == 0) TestDestructor();
        else if (std::strcmp(argv[1], "invalid") == 0) TestInvalid();
        else Require(false);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "VideoFrame exception: %s\n", error.what());
        return 1;
    }
}

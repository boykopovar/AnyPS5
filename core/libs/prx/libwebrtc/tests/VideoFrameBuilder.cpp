#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc VideoFrame::Builder check failed at line %u\n", location.line());
    std::abort();
}

WebRtcVideoFrameBuilder Filled() {
    WebRtcVideoFrameBuilder builder;
    std::memset(&builder, 0xA5, sizeof(builder));
    return builder;
}

bool OnlyChanged(const WebRtcVideoFrameBuilder& builder, std::size_t offset, std::size_t size) {
    WebRtcVideoFrameBuilder filled = Filled();
    const auto* left = reinterpret_cast<const unsigned char*>(&builder);
    const auto* right = reinterpret_cast<const unsigned char*>(&filled);
    for (std::size_t index = 0; index < sizeof(builder); ++index) {
        if (index >= offset && index < offset + size) continue;
        if (left[index] != right[index]) return false;
    }
    return true;
}

void TestTimestampMs() {
    WebRtcVideoFrameBuilder builder = Filled();
    Require(_ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(&builder, 1234) == &builder);
    Require(builder.timestampUs == 1234000);
    Require(OnlyChanged(builder, offsetof(WebRtcVideoFrameBuilder, timestampUs), 8));
    _ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(&builder, -5);
    Require(builder.timestampUs == -5000);
    _ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(&builder, 0);
    Require(builder.timestampUs == 0);
}

void TestTimestampRtp() {
    WebRtcVideoFrameBuilder builder = Filled();
    Require(_ZN6webrtc10VideoFrame7Builder17set_timestamp_rtpEj_nid_postfix(&builder, 0xFFFFFFFFu) == &builder);
    Require(builder.timestampRtp == 0xFFFFFFFFu);
    Require(OnlyChanged(builder, offsetof(WebRtcVideoFrameBuilder, timestampRtp), 4));
}

void TestNtpTime() {
    WebRtcVideoFrameBuilder builder = Filled();
    Require(_ZN6webrtc10VideoFrame7Builder15set_ntp_time_msEl_nid_postfix(&builder, std::numeric_limits<std::int64_t>::min()) == &builder);
    Require(builder.ntpTimeMs == std::numeric_limits<std::int64_t>::min());
    Require(OnlyChanged(builder, offsetof(WebRtcVideoFrameBuilder, ntpTimeMs), 8));
}

void TestId() {
    WebRtcVideoFrameBuilder builder = Filled();
    Require(_ZN6webrtc10VideoFrame7Builder6set_idEt_nid_postfix(&builder, 0xBEEF) == &builder);
    Require(builder.id == 0xBEEF);
    Require(OnlyChanged(builder, offsetof(WebRtcVideoFrameBuilder, id), 2));
}

void TestConstructor() {
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    Require(builder.id == 0 && builder.videoFrameBuffer == nullptr && builder.timestampUs == 0);
    Require(!builder.presentationTimestamp.engaged && !builder.referenceTime.engaged);
    Require(builder.timestampRtp == 0 && builder.ntpTimeMs == 0 && builder.rotation == 0);
    Require(builder.colorSpace[68] == 0 && !builder.useLowLatencyRendering && !builder.hasMaxCompositionDelayInFrames);
    Require(!builder.hasUpdateRect && builder.packetInfos == nullptr);
}

void TestDestructor() {
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    _ZN6webrtc10VideoFrame7Builder6set_idEt_nid_postfix(&builder, 5);
    _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    Require(builder.id == 5);
}

struct RefBuffer {
    const void* const* vtable;
    int addRefs = 0;
    int releases = 0;
};

std::vector<int> g_order;
const void* g_refVtable[4];

void APS5_VABI RefAddRef(const void* buffer) {
    ++const_cast<RefBuffer*>(static_cast<const RefBuffer*>(buffer))->addRefs;
    g_order.push_back(1);
}

int APS5_VABI RefRelease(const void* buffer) {
    ++const_cast<RefBuffer*>(static_cast<const RefBuffer*>(buffer))->releases;
    g_order.push_back(2);
    return 1;
}

void TestSetVideoFrameBuffer() {
    g_refVtable[0] = reinterpret_cast<const void*>(RefAddRef);
    g_refVtable[1] = reinterpret_cast<const void*>(RefRelease);
    RefBuffer first{g_refVtable};
    RefBuffer second{g_refVtable};
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    void* firstReference = &first;
    void* secondReference = &second;
    g_order.clear();
    Require(_ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(&builder, &firstReference) == &builder);
    Require(builder.videoFrameBuffer == &first && first.addRefs == 1 && first.releases == 0);
    Require(g_order == std::vector<int>{1});
    g_order.clear();
    _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(&builder, &secondReference);
    Require(builder.videoFrameBuffer == &second && second.addRefs == 1 && first.releases == 1 && second.releases == 0);
    Require(g_order == std::vector<int>({1, 2}));
    g_order.clear();
    _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(&builder, &secondReference);
    Require(builder.videoFrameBuffer == &second && second.addRefs == 2 && second.releases == 1);
    Require(g_order == std::vector<int>({1, 2}));
    void* nothing = nullptr;
    _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(&builder, &nothing);
    Require(builder.videoFrameBuffer == nullptr && second.releases == 2 && second.addRefs == 2);
}

void TestVideoFrameBuffer() {
    g_refVtable[0] = reinterpret_cast<const void*>(RefAddRef);
    g_refVtable[1] = reinterpret_cast<const void*>(RefRelease);
    RefBuffer buffer{g_refVtable};
    WebRtcVideoFrame frame;
    std::memset(&frame, 0, sizeof(frame));
    frame.videoFrameBuffer = &buffer;
    void* result = nullptr;
    Require(_ZNK6webrtc10VideoFrame18video_frame_bufferEv_nid_postfix(&result, &frame) == &result);
    Require(result == &buffer && buffer.addRefs == 1 && buffer.releases == 0 && frame.videoFrameBuffer == &buffer);
    frame.videoFrameBuffer = nullptr;
    result = &buffer;
    Require(_ZNK6webrtc10VideoFrame18video_frame_bufferEv_nid_postfix(&result, &frame) == &result);
    Require(result == nullptr && buffer.addRefs == 1);
}

template<typename TCall>
bool ThrowsInvalid(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

WebRtcVideoFrameBuilder BuilderWithEveryField(RefBuffer& buffer, WebRtcRtpPacketInfosData* packetInfos) {
    WebRtcVideoFrameBuilder builder = Filled();
    builder.id = 0x1234;
    builder.videoFrameBuffer = &buffer;
    builder.timestampUs = 1111111111111;
    builder.presentationTimestamp = {2222222222222, true};
    builder.referenceTime = {3333333333333, true};
    builder.timestampRtp = 0x89abcdef;
    builder.ntpTimeMs = 4444444444444;
    builder.rotation = 270;
    for (std::size_t index = 0; index < sizeof(builder.colorSpace); ++index) builder.colorSpace[index] = static_cast<unsigned char>(index + 1);
    builder.useLowLatencyRendering = true;
    builder.maxCompositionDelayInFrames = 7;
    builder.hasMaxCompositionDelayInFrames = true;
    builder.updateRect[0] = 10;
    builder.updateRect[1] = 20;
    builder.updateRect[2] = 30;
    builder.updateRect[3] = 40;
    builder.hasUpdateRect = true;
    builder.packetInfos = packetInfos;
    return builder;
}

void TestBuild() {
    g_refVtable[0] = reinterpret_cast<const void*>(RefAddRef);
    g_refVtable[1] = reinterpret_cast<const void*>(RefRelease);
    RefBuffer buffer{g_refVtable};
    WebRtcRtpPacketInfosData packetInfos{5, nullptr, nullptr, nullptr};
    WebRtcVideoFrameBuilder builder = BuilderWithEveryField(buffer, &packetInfos);
    const WebRtcVideoFrameBuilder before = builder;
    WebRtcVideoFrame frame;
    std::memset(&frame, 0xCD, sizeof(frame));
    g_order.clear();
    Require(_ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder) == &frame);
    Require(std::memcmp(&builder, &before, sizeof(builder)) == 0);
    Require(buffer.addRefs == 1 && buffer.releases == 0 && g_order == std::vector<int>{1});
    Require(packetInfos.refCount.load() == 6);
    Require(frame.id == 0x1234 && frame.videoFrameBuffer == &buffer);
    Require(frame.timestampRtp == 0x89abcdef && frame.ntpTimeMs == 4444444444444 && frame.timestampUs == 1111111111111);
    Require(frame.presentationTimestamp.value == 2222222222222 && frame.presentationTimestamp.engaged);
    Require(frame.referenceTime.value == 3333333333333 && frame.referenceTime.engaged);
    Require(frame.rotation == 270);
    Require(std::memcmp(frame.colorSpace, builder.colorSpace, sizeof(frame.colorSpace)) == 0);
    Require(frame.useLowLatencyRendering && frame.maxCompositionDelayInFrames == 7 && frame.hasMaxCompositionDelayInFrames);
    Require(std::memcmp(frame.updateRect, builder.updateRect, sizeof(frame.updateRect)) == 0 && frame.hasUpdateRect);
    Require(frame.packetInfos == &packetInfos);
    Require(reinterpret_cast<const unsigned char*>(frame.processingTime)[0] == 0 && !frame.hasProcessingTime);
    Require(reinterpret_cast<const unsigned char*>(frame.processingTime)[1] == 0xCD);

    builder.presentationTimestamp.engaged = false;
    builder.referenceTime.engaged = false;
    builder.hasMaxCompositionDelayInFrames = false;
    builder.hasUpdateRect = false;
    builder.useLowLatencyRendering = false;
    builder.packetInfos = nullptr;
    std::memset(&frame, 0xCD, sizeof(frame));
    Require(_ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder) == &frame);
    Require(buffer.addRefs == 2 && packetInfos.refCount.load() == 6);
    Require(!frame.presentationTimestamp.engaged && !frame.referenceTime.engaged);
    Require(!frame.hasMaxCompositionDelayInFrames && !frame.hasUpdateRect && !frame.useLowLatencyRendering);
    Require(frame.packetInfos == nullptr && !frame.hasProcessingTime);
}

void TestBuildRefusals() {
    g_refVtable[0] = reinterpret_cast<const void*>(RefAddRef);
    g_refVtable[1] = reinterpret_cast<const void*>(RefRelease);
    RefBuffer buffer{g_refVtable};
    WebRtcRtpPacketInfosData packetInfos{5, nullptr, nullptr, nullptr};
    WebRtcVideoFrameBuilder builder = BuilderWithEveryField(buffer, &packetInfos);
    WebRtcVideoFrame frame;
    std::memset(&frame, 0xCD, sizeof(frame));
    WebRtcVideoFrame untouched;
    std::memset(&untouched, 0xCD, sizeof(untouched));
    Require(ThrowsInvalid([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(nullptr, &builder); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, nullptr); }));
    builder.videoFrameBuffer = nullptr;
    Require(ThrowsInvalid([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder); }));
    RefBuffer withoutVtable{nullptr};
    builder.videoFrameBuffer = &withoutVtable;
    Require(ThrowsInvalid([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder); }));
    const void* withoutAddRefVtable[] = {nullptr, reinterpret_cast<const void*>(RefRelease)};
    RefBuffer withoutAddRef{withoutAddRefVtable};
    builder.videoFrameBuffer = &withoutAddRef;
    Require(ThrowsInvalid([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder); }));
    Require(std::memcmp(&frame, &untouched, sizeof(frame)) == 0);
    Require(buffer.addRefs == 0 && withoutAddRef.addRefs == 0 && packetInfos.refCount.load() == 5);
}

int releaseCalls = 0;
void* releasedBuffer = nullptr;

int APS5_VABI ReleaseBuffer(const void* buffer) {
    ++releaseCalls;
    releasedBuffer = const_cast<void*>(buffer);
    return 1;
}

const void* bufferVtable[] = {nullptr, reinterpret_cast<const void*>(&ReleaseBuffer)};

const void* emptyVtable[] = {nullptr, nullptr};

void TestDestructorReleasesBuffer() {
    const void* const* buffer = bufferVtable;
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    builder.videoFrameBuffer = &buffer;
    releaseCalls = 0;
    releasedBuffer = nullptr;
    _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    Require(releaseCalls == 1 && releasedBuffer == &buffer);
    Require(builder.videoFrameBuffer == &buffer);
}

void TestDestructorWithoutBufferReleasesNothing() {
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    releaseCalls = 0;
    _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    Require(releaseCalls == 0);
}

void TestDestructorWithoutReleaseThrows() {
    const void* const* buffer = emptyVtable;
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    builder.videoFrameBuffer = &buffer;
    bool threw = false;
    try {
        _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(releaseCalls == 0);
}

void TestDestructorWithPacketInfosThrows() {
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    builder.packetInfos = &builder;
    bool threw = false;
    try {
        _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

void TestNullDestructorThrows() {
    bool threw = false;
    try {
        _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void TestRenderTimeMs() {
    WebRtcVideoFrame frame;
    std::memset(&frame, 0xA5, sizeof(frame));
    for (const auto [microseconds, milliseconds] : {std::pair<std::int64_t, std::int64_t>{0, 0}, {999, 0}, {1000, 1}, {123456789, 123456}, {-1, 0}, {-999, 0}, {-1000, -1}, {-1500, -1},
             {std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::max() / 1000}}) {
        frame.timestampUs = microseconds;
        Require(_ZNK6webrtc10VideoFrame14render_time_msEv_nid_postfix(&frame) == milliseconds);
    }
    bool threw = false;
    try {
        _ZNK6webrtc10VideoFrame14render_time_msEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void TestNullBuilderThrows() {
    try {
        _ZN6webrtc10VideoFrame7Builder6set_idEt_nid_postfix(nullptr, 1);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    TestTimestampMs();
    TestTimestampRtp();
    TestNtpTime();
    TestId();
    TestConstructor();
    TestDestructor();
    TestSetVideoFrameBuffer();
    TestVideoFrameBuffer();
    TestBuild();
    TestBuildRefusals();
    TestDestructorReleasesBuffer();
    TestDestructorWithoutBufferReleasesNothing();
    TestDestructorWithoutReleaseThrows();
    TestDestructorWithPacketInfosThrows();
    TestNullDestructorThrows();
    TestRenderTimeMs();
    TestNullBuilderThrows();
    return 0;
}

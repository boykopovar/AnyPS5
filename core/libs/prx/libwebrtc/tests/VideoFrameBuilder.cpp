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

WebRtcVideoFrame FilledFrame() {
    WebRtcVideoFrame frame;
    std::memset(&frame, 0xA5, sizeof(frame));
    return frame;
}

bool FrameUntouched(const WebRtcVideoFrame& frame) {
    const WebRtcVideoFrame filled = FilledFrame();
    return std::memcmp(&frame, &filled, sizeof(frame)) == 0;
}

template<typename Exception, typename Call> bool Throws(Call call) {
    try {
        call();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

void TestBuildCopiesEveryField() {
    g_refVtable[0] = reinterpret_cast<const void*>(RefAddRef);
    g_refVtable[1] = reinterpret_cast<const void*>(RefRelease);
    RefBuffer buffer{g_refVtable};
    WebRtcVideoFrameBuilder builder = Filled();
    builder.id = 0x1234;
    builder.videoFrameBuffer = &buffer;
    builder.timestampUs = -7000;
    builder.presentationTimestamp = {11, true};
    builder.referenceTime = {22, true};
    builder.timestampRtp = 0xDEADBEEFu;
    builder.ntpTimeMs = 987654321;
    builder.rotation = 270;
    for (std::size_t index = 0; index < 68; ++index)
        builder.colorSpace[index] = static_cast<unsigned char>(index + 1);
    builder.colorSpace[68] = 1;
    builder.useLowLatencyRendering = true;
    builder.maxCompositionDelayInFrames = 3;
    builder.hasMaxCompositionDelayInFrames = true;
    builder.updateRect[0] = 1;
    builder.updateRect[1] = 2;
    builder.updateRect[2] = 30;
    builder.updateRect[3] = 40;
    builder.hasUpdateRect = true;
    builder.packetInfos = nullptr;
    const WebRtcVideoFrameBuilder before = builder;
    WebRtcVideoFrame frame = FilledFrame();
    g_order.clear();
    Require(_ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder) == &frame);
    Require(std::memcmp(&before, &builder, sizeof(builder)) == 0);
    Require(buffer.addRefs == 1 && buffer.releases == 0 && g_order == std::vector<int>{1});
    Require(frame.id == 0x1234 && frame.videoFrameBuffer == &buffer);
    Require(frame.timestampRtp == 0xDEADBEEFu && frame.ntpTimeMs == 987654321 && frame.timestampUs == -7000);
    Require(frame.presentationTimestamp.engaged && frame.presentationTimestamp.value == 11);
    Require(frame.referenceTime.engaged && frame.referenceTime.value == 22);
    Require(frame.rotation == 270);
    Require(std::memcmp(frame.colorSpace, builder.colorSpace, 69) == 0);
    Require(frame.useLowLatencyRendering && frame.hasMaxCompositionDelayInFrames && frame.maxCompositionDelayInFrames == 3);
    Require(frame.hasUpdateRect && frame.updateRect[0] == 1 && frame.updateRect[1] == 2 && frame.updateRect[2] == 30 && frame.updateRect[3] == 40);
    Require(frame.packetInfos == nullptr && !frame.hasProcessingTime);
    _ZN6webrtc10VideoFrameD1Ev_nid_postfix(&frame);
    Require(buffer.addRefs == 1 && buffer.releases == 1);
}

void TestBuildLeavesUnsetOptionalsUnset() {
    g_refVtable[0] = reinterpret_cast<const void*>(RefAddRef);
    g_refVtable[1] = reinterpret_cast<const void*>(RefRelease);
    RefBuffer buffer{g_refVtable};
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    void* reference = &buffer;
    _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(&builder, &reference);
    _ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(&builder, 42);
    WebRtcVideoFrame frame = FilledFrame();
    _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder);
    Require(buffer.addRefs == 2);
    Require(frame.id == 0 && frame.videoFrameBuffer == &buffer && frame.timestampUs == 42000 && frame.timestampRtp == 0 && frame.ntpTimeMs == 0);
    Require(!frame.presentationTimestamp.engaged && !frame.referenceTime.engaged && frame.colorSpace[68] == 0);
    Require(frame.rotation == 0 && !frame.useLowLatencyRendering && !frame.hasMaxCompositionDelayInFrames && !frame.hasUpdateRect);
    Require(frame.packetInfos == nullptr && !frame.hasProcessingTime);
    Require(_ZNK6webrtc10VideoFrame14render_time_msEv_nid_postfix(&frame) == 42);
    _ZN6webrtc10VideoFrameD1Ev_nid_postfix(&frame);
    _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(&builder);
    Require(buffer.addRefs == 2 && buffer.releases == 2);
}

void TestBuildRejectsInvalidInput() {
    g_refVtable[0] = reinterpret_cast<const void*>(RefAddRef);
    g_refVtable[1] = reinterpret_cast<const void*>(RefRelease);
    RefBuffer buffer{g_refVtable};
    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    WebRtcVideoFrame frame = FilledFrame();
    Require(Throws<std::invalid_argument>([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder); }));
    Require(FrameUntouched(frame));
    builder.videoFrameBuffer = &buffer;
    Require(Throws<std::invalid_argument>([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(nullptr, &builder); }));
    Require(Throws<std::invalid_argument>([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, nullptr); }));
    builder.packetInfos = &builder;
    Require(Throws<std::runtime_error>([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder); }));
    Require(FrameUntouched(frame) && buffer.addRefs == 0 && buffer.releases == 0);
    const void* withoutAddRef[] = {nullptr, reinterpret_cast<const void*>(RefRelease)};
    RefBuffer broken{withoutAddRef};
    builder.packetInfos = nullptr;
    builder.videoFrameBuffer = &broken;
    Require(Throws<std::invalid_argument>([&] { _ZN6webrtc10VideoFrame7Builder5buildEv_nid_postfix(&frame, &builder); }));
    Require(FrameUntouched(frame) && broken.releases == 0);
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
    TestDestructorReleasesBuffer();
    TestDestructorWithoutBufferReleasesNothing();
    TestDestructorWithoutReleaseThrows();
    TestDestructorWithPacketInfosThrows();
    TestNullDestructorThrows();
    TestRenderTimeMs();
    TestNullBuilderThrows();
    TestBuildCopiesEveryField();
    TestBuildLeavesUnsetOptionalsUnset();
    TestBuildRejectsInvalidInput();
    return 0;
}

#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <utility>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc VideoFrame::Builder check failed at line %u\n", location.line());
    std::abort();
}

enum class Access { ReadWrite, ReadOnly, None };

struct GuardedPages {
    std::size_t pageSize = 0;
    std::size_t size = 0;
    unsigned char* base = nullptr;

    explicit GuardedPages(std::size_t bytes = 1) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        size = (bytes + pageSize - 1) / pageSize * pageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        size = (bytes + pageSize - 1) / pageSize * pageSize;
        void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        base = mapped == MAP_FAILED ? nullptr : static_cast<unsigned char*>(mapped);
#endif
        Require(base != nullptr);
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    template<typename T> T* As(std::size_t offset = 0) const { return reinterpret_cast<T*>(base + offset); }

    void Protect(Access access, std::size_t offset = 0) const {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD mode = access == Access::ReadWrite ? PAGE_READWRITE : access == Access::ReadOnly ? PAGE_READONLY : PAGE_NOACCESS;
        Require(VirtualProtect(base + offset, size - offset, mode, &previous) != 0);
#else
        const int mode = access == Access::ReadWrite ? PROT_READ | PROT_WRITE : access == Access::ReadOnly ? PROT_READ : PROT_NONE;
        Require(mprotect(base + offset, size - offset, mode) == 0);
#endif
    }
};

template<typename TAction> bool ThrowsInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
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

void TestNullBuilderThrows() {
    try {
        _ZN6webrtc10VideoFrame7Builder6set_idEt_nid_postfix(nullptr, 1);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

void TestInaccessibleGuestPointersThrow() {
    g_refVtable[0] = reinterpret_cast<const void*>(RefAddRef);
    g_refVtable[1] = reinterpret_cast<const void*>(RefRelease);
    GuardedPages hidden(sizeof(WebRtcVideoFrame));
    hidden.Protect(Access::None);
    auto* hiddenBuilder = hidden.As<WebRtcVideoFrameBuilder>();
    RefBuffer buffer {g_refVtable};
    void* reference = &buffer;
    void* result = nullptr;
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(hiddenBuilder); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc10VideoFrame7BuilderD1Ev_nid_postfix(hiddenBuilder); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(hiddenBuilder, 1); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(hiddenBuilder, &reference); }));
    Require(ThrowsInvalidArgument([&] { _ZNK6webrtc10VideoFrame18video_frame_bufferEv_nid_postfix(&result, hidden.As<WebRtcVideoFrame>()); }));
    Require(ThrowsInvalidArgument([&] { _ZNK6webrtc10VideoFrame14render_time_msEv_nid_postfix(hidden.As<WebRtcVideoFrame>()); }));

    GuardedPages readOnly(sizeof(WebRtcVideoFrameBuilder));
    auto* constant = readOnly.As<WebRtcVideoFrameBuilder>();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(constant);
    readOnly.Protect(Access::ReadOnly);
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(constant); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc10VideoFrame7Builder16set_timestamp_msEl_nid_postfix(constant, 1); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc10VideoFrame7Builder17set_timestamp_rtpEj_nid_postfix(constant, 1); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc10VideoFrame7Builder15set_ntp_time_msEl_nid_postfix(constant, 1); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc10VideoFrame7Builder6set_idEt_nid_postfix(constant, 1); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(constant, &reference); }));
    Require(constant->id == 0 && constant->timestampUs == 0 && constant->videoFrameBuffer == nullptr);

    WebRtcVideoFrameBuilder builder = Filled();
    _ZN6webrtc10VideoFrame7BuilderC1Ev_nid_postfix(&builder);
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc10VideoFrame7Builder22set_video_frame_bufferERKNS_13scoped_refptrINS_16VideoFrameBufferEEE_nid_postfix(&builder, hidden.As<void* const>()); }));
    WebRtcVideoFrame frame;
    std::memset(&frame, 0, sizeof(frame));
    frame.videoFrameBuffer = &buffer;
    GuardedPages readOnlyResult;
    readOnlyResult.Protect(Access::ReadOnly);
    Require(ThrowsInvalidArgument([&] { _ZNK6webrtc10VideoFrame18video_frame_bufferEv_nid_postfix(readOnlyResult.As<void*>(), &frame); }));
    Require(buffer.addRefs == 0 && buffer.releases == 0 && builder.videoFrameBuffer == nullptr);
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
    TestInaccessibleGuestPointersThrow();
    return 0;
}

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <unordered_map>

namespace {

std::unordered_map<void*, std::size_t> allocations;
std::size_t allocated = 0;
std::size_t freed = 0;
std::size_t failAllocationOfSize = 0;
int guestReleases = 0;

void Require(bool condition) {
    if (!condition) throw std::runtime_error("EncodedImage assertion failed");
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (failAllocationOfSize && size == failAllocationOfSize) {
        failAllocationOfSize = 0;
        return nullptr;
    }
    auto* pointer = std::malloc(size);
    if (pointer) {
        allocations.emplace(pointer, size);
        ++allocated;
        std::memset(pointer, 0xa5, size);
    }
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(allocations.erase(pointer) == 1);
    ++freed;
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

std::int32_t APS5_VABI GuestRelease_nid_no_patch(const void*) {
    ++guestReleases;
    return 1;
}

void* const GuestVtable[] = {nullptr, nullptr, nullptr, reinterpret_cast<void*>(&GuestRelease_nid_no_patch)};
void* const BrokenVtable[] = {nullptr, nullptr, nullptr, nullptr};

template<typename TFunction>
void ExpectInvalid(TFunction function) {
    bool threw = false;
    try { function(); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
}

template<typename TResult, std::size_t TSlot, typename... TArguments>
TResult Call(const WebRtcEncodedImageBuffer* buffer, TArguments... arguments) {
    using Function = TResult(APS5_VABI*)(const WebRtcEncodedImageBuffer*, TArguments...);
    return reinterpret_cast<Function>(const_cast<void*>(static_cast<const void* const*>(buffer->vtable)[TSlot]))(buffer, arguments...);
}

WebRtcEncodedImageBuffer* Create(const std::uint8_t* data, std::size_t size) {
    WebRtcEncodedImageBuffer* buffer = nullptr;
    Require(_ZN6webrtc18EncodedImageBuffer6CreateEPKhm_nid_postfix(&buffer, data, size) == &buffer);
    return buffer;
}

void Construct(WebRtcEncodedImage& image) {
    std::memset(&image, 0xa5, sizeof(image));
    _ZN6webrtc12EncodedImageC1Ev_nid_postfix(&image);
}

WebRtcEncodedImageMapNode* Node(std::int32_t key, WebRtcEncodedImageMapNode* parent) {
    auto* node = new (_Znwm_nid_postfix(sizeof(WebRtcEncodedImageMapNode))) WebRtcEncodedImageMapNode{{nullptr, nullptr, parent, true}, key, 100u + static_cast<std::uint64_t>(key)};
    return node;
}

WebRtcRtpPacketInfosData* PacketInfos(std::int32_t refCount, std::size_t count) {
    auto* data = static_cast<WebRtcRtpPacketInfosData*>(_Znwm_nid_postfix(sizeof(WebRtcRtpPacketInfosData)));
    auto* entries = static_cast<WebRtcRtpPacketInfoEntry*>(_Znwm_nid_postfix(count * sizeof(WebRtcRtpPacketInfoEntry)));
    std::memset(entries, 0, count * sizeof(WebRtcRtpPacketInfoEntry));
    for (std::size_t i = 0; i < count; i += 2) {
        entries[i].csrcsBegin = static_cast<std::uint32_t*>(_Znwm_nid_postfix((i + 1) * sizeof(std::uint32_t)));
        entries[i].csrcsEnd = entries[i].csrcsBegin + i;
        entries[i].csrcsCapacity = entries[i].csrcsBegin + i + 1;
    }
    return new (data) WebRtcRtpPacketInfosData{refCount, entries, entries + count, entries + count};
}

void TestDefault() {
    WebRtcEncodedImage image;
    Construct(image);
    Require(image.encodedWidth == 0 && image.encodedHeight == 0 && image.ntpTimeMs == 0 && image.captureTimeMs == 0);
    Require(image.frameType == 4 && image.rotation == 0 && image.contentType == 0 && image.qp == -1);
    Require(image.timing.flags == 0xff && image.timing.encodeStartMs == 0 && image.timing.receiveFinishMs == 0);
    Require(!image.hasPlayoutDelay && !image.encodedData && image.size == 0 && image.timestampRtp == 0);
    Require(!image.simulcastIndex.engaged && !image.presentationTimestamp.engaged && !image.spatialIndex.engaged && !image.temporalIndex.engaged);
    Require(image.spatialLayerFrameSizeBytes.beginNode == reinterpret_cast<WebRtcEncodedImageMapNode*>(&image.spatialLayerFrameSizeBytes.root));
    Require(!image.spatialLayerFrameSizeBytes.root && image.spatialLayerFrameSizeBytes.size == 0);
    Require(!image.colorSpace.engaged && !image.videoFrameTrackingId.engaged && !image.packetInfos);
    Require(image.retransmissionAllowed && !image.atTargetQuality && !image.isSteadyStateRefreshFrame);
    Require(!image.hasCorruptionDetectionFilterSettings);
    _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image);
    Require(allocated == 0 && freed == 0);
    ExpectInvalid([] { _ZN6webrtc12EncodedImageC1Ev_nid_postfix(nullptr); });
    ExpectInvalid([] { _ZN6webrtc12EncodedImageD1Ev_nid_postfix(nullptr); });
}

void TestBuffer() {
    const std::uint8_t bytes[] = {1, 2, 3, 4, 5};
    auto* buffer = Create(bytes, sizeof(bytes));
    Require(allocated == 2);
    Require(buffer->buffer.size == 5 && buffer->buffer.capacity == 5 && buffer->buffer.data != bytes);
    Require(std::memcmp(buffer->buffer.data, bytes, sizeof(bytes)) == 0 && buffer->refCount.load() == 1);
    Require(static_cast<const void* const*>(buffer->vtable)[-1] == nullptr && static_cast<const void* const*>(buffer->vtable)[-2] == nullptr);
    Require((Call<const std::uint8_t*, 4>(buffer)) == buffer->buffer.data);
    Require((Call<std::uint8_t*, 5>(buffer)) == buffer->buffer.data);
    Require((Call<std::size_t, 6>(buffer)) == 5);
    Require((Call<bool, 7>(buffer)));
    Call<void, 0>(buffer);
    Require(buffer->refCount.load() == 2 && !(Call<bool, 7>(buffer)));
    Require((Call<std::int32_t, 1>(buffer)) == 1 && buffer->refCount.load() == 1 && freed == 0);
    Require((Call<std::int32_t, 1>(buffer)) == 0 && allocations.empty());

    buffer = Create(nullptr, 0);
    Require(allocated == 3 && buffer->buffer.size == 0 && buffer->buffer.capacity == 0 && !buffer->buffer.data);
    Require((Call<const std::uint8_t*, 4>(buffer)) == nullptr && (Call<std::size_t, 6>(buffer)) == 0);
    Require((Call<std::int32_t, 1>(buffer)) == 0 && allocations.empty());

    buffer = Create(bytes, 3);
    Call<void, 2>(buffer);
    Require(allocations.size() == 1 && allocations.count(buffer));
    _ZdlPv_nid_postfix(buffer);
    buffer = Create(bytes, 3);
    Call<void, 3>(buffer);
    Require(allocations.empty());

    failAllocationOfSize = 4096;
    bool threw = false;
    std::uint8_t large[4096] = {};
    try { Create(large, sizeof(large)); }
    catch (const std::bad_alloc&) { threw = true; }
    Require(threw && allocations.empty());

    ExpectInvalid([&] { Create(nullptr, 1); });
    ExpectInvalid([&] { _ZN6webrtc18EncodedImageBuffer6CreateEPKhm_nid_postfix(nullptr, bytes, 1); });
    Require(allocations.empty());
}

void TestDestructor() {
    const std::uint8_t bytes[] = {9, 8, 7};
    WebRtcEncodedImage image;

    Construct(image);
    image.encodedData = Create(bytes, sizeof(bytes));
    image.size = sizeof(bytes);
    _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image);
    Require(allocations.empty());

    Construct(image);
    auto* shared = Create(bytes, sizeof(bytes));
    Call<void, 0>(shared);
    image.encodedData = shared;
    _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image);
    Require(shared->refCount.load() == 1 && allocations.size() == 2);
    Require((Call<std::int32_t, 1>(shared)) == 0 && allocations.empty());

    struct { const void* vtable; } guestBuffer{GuestVtable + 2};
    Construct(image);
    image.encodedData = &guestBuffer;
    _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image);
    Require(guestReleases == 1);
    guestBuffer.vtable = BrokenVtable + 2;
    ExpectInvalid([&] { _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image); });
    guestBuffer.vtable = nullptr;
    ExpectInvalid([&] { _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image); });

    Construct(image);
    auto& map = image.spatialLayerFrameSizeBytes;
    auto* end = reinterpret_cast<WebRtcEncodedImageMapNode*>(&map.root);
    map.root = Node(1, end);
    map.root->links.left = Node(0, map.root);
    map.root->links.right = Node(2, map.root);
    map.root->links.right->links.right = Node(3, map.root->links.right);
    map.beginNode = map.root->links.left;
    map.size = 4;
    image.packetInfos = PacketInfos(1, 3);
    image.encodedData = Create(bytes, sizeof(bytes));
    Require(allocations.size() == 10);
    _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image);
    Require(allocations.empty());

    Construct(image);
    auto* sharedInfos = PacketInfos(2, 2);
    image.packetInfos = sharedInfos;
    _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image);
    Require(sharedInfos->refCount.load() == 1 && allocations.size() == 3);
    _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image);
    Require(allocations.empty());

    Construct(image);
    image.packetInfos = PacketInfos(1, 1);
    auto* entries = image.packetInfos->begin;
    image.packetInfos->end = reinterpret_cast<WebRtcRtpPacketInfoEntry*>(reinterpret_cast<std::uint8_t*>(entries) + 8);
    ExpectInvalid([&] { _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image); });
    _ZdlPv_nid_postfix(entries[0].csrcsBegin);
    _ZdlPv_nid_postfix(entries);
    _ZdlPv_nid_postfix(image.packetInfos);
    Require(allocations.empty());

    Construct(image);
    map.root = Node(5, end);
    map.root->links.left = Node(4, map.root);
    map.beginNode = map.root->links.left;
    map.size = 1;
    image.encodedData = Create(bytes, sizeof(bytes));
    ExpectInvalid([&] { _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image); });
    map.size = 3;
    ExpectInvalid([&] { _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image); });
    map.root->links.left->links.left = map.root;
    map.size = 2;
    ExpectInvalid([&] { _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image); });
    Require(allocations.size() == 4);
    map.root->links.left->links.left = nullptr;
    _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image);
    Require(allocations.empty());

    Construct(image);
    map.beginNode = nullptr;
    ExpectInvalid([&] { _ZN6webrtc12EncodedImageD1Ev_nid_postfix(&image); });
}

}

int main(int argc, char** argv) {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try {
        Require(argc == 2);
        if (std::strcmp(argv[1], "default") == 0) TestDefault();
        else if (std::strcmp(argv[1], "buffer") == 0) TestBuffer();
        else if (std::strcmp(argv[1], "destructor") == 0) TestDestructor();
        else Require(false);
        Require(allocations.empty() && allocated == freed);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "EncodedImage exception: %s\n", error.what());
        return 1;
    }
}

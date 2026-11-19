#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

std::set<void*> live;
std::size_t failAfterAllocations = static_cast<std::size_t>(-1);
std::size_t allocationCounter = 0;

void* APS5_VABI TestAllocate(std::size_t size) {
    if (allocationCounter >= failAfterAllocations)
        throw std::bad_alloc();
    ++allocationCounter;
    void* pointer = std::malloc(size ? size : 1);
    if (!pointer)
        throw std::bad_alloc();
    live.insert(pointer);
    return pointer;
}

void APS5_VABI TestFree(void* pointer) {
    if (!pointer)
        return;
    if (live.erase(pointer) != 1)
        std::abort();
    std::free(pointer);
}

void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    void* pointer = TestAllocate(count * size);
    std::memset(pointer, 0, count * size);
    return pointer;
}

void* APS5_VABI TestReallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return TestAllocate(size); }
void* APS5_VABI TestRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = TestAllocate(size);
    return 0;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc SdpVideoFormat check failed at line %u\n", location.line());
    std::abort();
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

struct ReadOnlyPage {
    void* address = nullptr;
    std::size_t size = 0;

    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        address = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(address != nullptr);
        std::memset(address, 0, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(address, size, PAGE_READONLY, &oldProtect) != 0);
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        address = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(address != MAP_FAILED);
        Require(mprotect(address, size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!address)
            return;
#ifdef _WIN32
        VirtualFree(address, 0, MEM_RELEASE);
#else
        munmap(address, size);
#endif
    }
};

WebRtcStringMapNode* EndNode(WebRtcStringMap& map) {
    return reinterpret_cast<WebRtcStringMapNode*>(&map.root);
}

void InitGuestString(WebRtcGuestString& target, std::string_view text) {
    target.reserved = 0;
    target.size = text.size();
    if (text.size() <= WebRtcGuestStringInlineCapacity) {
        std::memset(target.buffer, 0, sizeof(target.buffer));
        if (!text.empty())
            std::memcpy(target.buffer, text.data(), text.size());
        target.capacity = WebRtcGuestStringInlineCapacity;
        return;
    }
    const std::size_t cap = text.size() | WebRtcGuestStringInlineCapacity;
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(cap + 1));
    std::memcpy(data, text.data(), text.size());
    data[text.size()] = '\0';
    target.pointer = data;
    target.capacity = cap;
}

std::string ReadGuestString(const WebRtcGuestString& value) {
    return std::string(WebRtcGuestStringData(value), value.size);
}

void InitEmptyFormat(WebRtcSdpVideoFormat& format, std::string_view name) {
    std::memset(&format, 0, sizeof(format));
    InitGuestString(format.name, name);
    format.parameters.beginNode = EndNode(format.parameters);
    format.parameters.root = nullptr;
    format.parameters.size = 0;
    format.scalabilityModes.metadata = 0;
}

WebRtcStringMapNode* MakeNode(std::string_view key, std::string_view value, bool isBlack) {
    auto* storage = _Znwm_nid_postfix(sizeof(WebRtcStringMapNode));
    auto* node = new (storage) WebRtcStringMapNode{};
    node->links.isBlack = isBlack;
    InitGuestString(node->key, key);
    InitGuestString(node->value, value);
    return node;
}

void AttachSampleTree(WebRtcSdpVideoFormat& format) {
    auto* root = MakeNode("profile-id", "0", true);
    auto* left = MakeNode("level-id", "93", false);
    auto* right = MakeNode("tx-mode", "SRST", false);
    root->links.left = left;
    root->links.right = right;
    root->links.parent = EndNode(format.parameters);
    left->links.parent = root;
    right->links.parent = root;
    format.parameters.root = root;
    format.parameters.beginNode = left;
    format.parameters.size = 3;
}

void SetInlineModes(WebRtcSdpVideoFormat& format, std::initializer_list<std::uint8_t> modes) {
    std::memset(format.scalabilityModes.inlined, 0xee, sizeof(format.scalabilityModes.inlined));
    std::size_t index = 0;
    for (std::uint8_t mode : modes)
        format.scalabilityModes.inlined[index++] = mode;
    format.scalabilityModes.metadata = index << 1;
}

void SetAllocatedModes(WebRtcSdpVideoFormat& format, std::size_t count, std::size_t capacity) {
    auto* data = static_cast<std::uint8_t*>(_Znwm_nid_postfix(capacity));
    for (std::size_t index = 0; index < count; ++index)
        data[index] = static_cast<std::uint8_t>(index % WebRtcScalabilityModeCount);
    format.scalabilityModes.allocated.data = data;
    format.scalabilityModes.allocated.capacity = capacity;
    format.scalabilityModes.metadata = (count << 1) | 1;
}

void TestCopyInlineModes() {
    WebRtcSdpVideoFormat source;
    InitEmptyFormat(source, "H265-with-a-heap-allocated-name");
    AttachSampleTree(source);
    SetInlineModes(source, {0, 2, 19, 33});

    WebRtcSdpVideoFormat copied;
    std::memset(&copied, 0xcd, sizeof(copied));
    errno = 41;
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copied, &source);
    Require(errno == 41);
    Require(ReadGuestString(copied.name) == "H265-with-a-heap-allocated-name");
    Require(copied.name.pointer != source.name.pointer);
    Require(copied.parameters.size == 3);
    Require(copied.parameters.root != source.parameters.root);
    Require(copied.parameters.root->links.parent == EndNode(copied.parameters));
    Require(copied.parameters.beginNode == copied.parameters.root->links.left);
    Require(ReadGuestString(copied.parameters.beginNode->key) == "level-id");
    Require(ReadGuestString(copied.parameters.root->links.right->value) == "SRST");
    Require(copied.scalabilityModes.metadata == (4u << 1));
    Require(std::memcmp(copied.scalabilityModes.inlined, source.scalabilityModes.inlined, sizeof(copied.scalabilityModes.inlined)) == 0);

    WebRtcSdpVideoFormat empty;
    InitEmptyFormat(empty, "VP8");
    WebRtcSdpVideoFormat emptyCopy;
    std::memset(&emptyCopy, 0xcd, sizeof(emptyCopy));
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&emptyCopy, &empty);
    Require(ReadGuestString(emptyCopy.name) == "VP8");
    Require(emptyCopy.parameters.size == 0 && emptyCopy.parameters.root == nullptr);
    Require(emptyCopy.parameters.beginNode == EndNode(emptyCopy.parameters));
    Require(emptyCopy.scalabilityModes.metadata == 0);

    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copied);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&empty);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&emptyCopy);
    Require(live.empty());
}

void TestCopyAllocatedModes() {
    WebRtcSdpVideoFormat large;
    InitEmptyFormat(large, "AV1");
    SetAllocatedModes(large, 90, 120);
    WebRtcSdpVideoFormat largeCopy;
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&largeCopy, &large);
    Require(largeCopy.scalabilityModes.metadata == ((90u << 1) | 1));
    Require(largeCopy.scalabilityModes.allocated.capacity == 90);
    Require(largeCopy.scalabilityModes.allocated.data != large.scalabilityModes.allocated.data);
    Require(std::memcmp(largeCopy.scalabilityModes.allocated.data, large.scalabilityModes.allocated.data, 90) == 0);

    WebRtcSdpVideoFormat small;
    InitEmptyFormat(small, "VP9");
    SetAllocatedModes(small, 5, 80);
    WebRtcSdpVideoFormat smallCopy;
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&smallCopy, &small);
    Require(smallCopy.scalabilityModes.metadata == ((5u << 1) | 1));
    Require(smallCopy.scalabilityModes.allocated.capacity == WebRtcScalabilityModeCount * 2);
    Require(std::memcmp(smallCopy.scalabilityModes.allocated.data, small.scalabilityModes.allocated.data, 5) == 0);

    WebRtcSdpVideoFormat drained;
    InitEmptyFormat(drained, "H264");
    SetAllocatedModes(drained, 0, 68);
    WebRtcSdpVideoFormat drainedCopy;
    std::memset(&drainedCopy, 0xcd, sizeof(drainedCopy));
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&drainedCopy, &drained);
    Require(drainedCopy.scalabilityModes.metadata == 0);

    for (auto* format : {&large, &largeCopy, &small, &smallCopy, &drained, &drainedCopy})
        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(format);
    Require(live.empty());
}

void TestMove() {
    WebRtcSdpVideoFormat source;
    InitEmptyFormat(source, "a-codec-name-longer-than-fifteen");
    AttachSampleTree(source);
    SetAllocatedModes(source, 40, 68);
    auto* savedName = source.name.pointer;
    auto* savedRoot = source.parameters.root;
    auto* savedBegin = source.parameters.beginNode;
    auto* savedModes = source.scalabilityModes.allocated.data;
    const auto liveBefore = live;

    WebRtcSdpVideoFormat moved;
    std::memset(&moved, 0xcd, sizeof(moved));
    errno = 42;
    _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&moved, &source);
    Require(errno == 42);
    Require(live == liveBefore);
    Require(moved.name.pointer == savedName);
    Require(moved.parameters.root == savedRoot && moved.parameters.beginNode == savedBegin);
    Require(moved.parameters.size == 3);
    Require(moved.parameters.root->links.parent == EndNode(moved.parameters));
    Require(moved.scalabilityModes.metadata == ((40u << 1) | 1));
    Require(moved.scalabilityModes.allocated.data == savedModes && moved.scalabilityModes.allocated.capacity == 68);
    Require(source.name.size == 0 && source.name.capacity == WebRtcGuestStringInlineCapacity);
    Require(source.parameters.size == 0 && source.parameters.root == nullptr);
    Require(source.parameters.beginNode == EndNode(source.parameters));
    Require(source.scalabilityModes.metadata == 0);

    WebRtcSdpVideoFormat inlineSource;
    InitEmptyFormat(inlineSource, "VP9");
    SetInlineModes(inlineSource, {7, 8});
    WebRtcSdpVideoFormat inlineMoved;
    _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&inlineMoved, &inlineSource);
    Require(ReadGuestString(inlineMoved.name) == "VP9");
    Require(inlineMoved.scalabilityModes.metadata == (2u << 1));
    Require(inlineMoved.scalabilityModes.inlined[0] == 7 && inlineMoved.scalabilityModes.inlined[1] == 8);
    Require(inlineSource.scalabilityModes.metadata == 0);

    for (auto* format : {&source, &moved, &inlineSource, &inlineMoved})
        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(format);
    Require(live.empty());
}

void TestAllocationFailureCleanup() {
    WebRtcSdpVideoFormat source;
    InitEmptyFormat(source, "a-codec-name-longer-than-fifteen");
    AttachSampleTree(source);
    SetAllocatedModes(source, 3, 68);
    const auto liveBaseline = live;
    for (std::size_t failAt = 0; failAt < 5; ++failAt) {
        WebRtcSdpVideoFormat target;
        std::memset(&target, 0xcd, sizeof(target));
        bool threw = false;
        failAfterAllocations = allocationCounter + failAt;
        try {
            _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&target, &source);
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        failAfterAllocations = static_cast<std::size_t>(-1);
        Require(threw);
        Require(live == liveBaseline);
    }
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source);
    Require(live.empty());
}

void TestInvalidGuestInputs() {
    ReadOnlyPage readOnly;
    auto* roFormat = static_cast<WebRtcSdpVideoFormat*>(readOnly.address);
    auto* badPtr = reinterpret_cast<WebRtcSdpVideoFormat*>(0x10);

    WebRtcSdpVideoFormat valid;
    InitEmptyFormat(valid, "H264");
    AttachSampleTree(valid);
    SetInlineModes(valid, {1});
    WebRtcSdpVideoFormat scratch;
    const auto liveBaseline = live;

    errno = 55;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(nullptr, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(badPtr, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(roFormat, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, badPtr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(nullptr, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&scratch, roFormat); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&valid, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(badPtr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(roFormat); }));
    Require(errno == 55);

    const auto savedModes = valid.scalabilityModes;
    valid.scalabilityModes.metadata = (WebRtcScalabilityModeCount + 1) << 1;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&valid); }));
    valid.scalabilityModes.metadata = (1u << 1) | 1;
    valid.scalabilityModes.allocated.data = nullptr;
    valid.scalabilityModes.allocated.capacity = 68;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&valid); }));
    valid.scalabilityModes.allocated.data = badPtr;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, &valid); }));
    valid.scalabilityModes.metadata = (69u << 1) | 1;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&scratch, &valid); }));
    valid.scalabilityModes = savedModes;

    valid.parameters.size = 2;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&scratch, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&valid); }));
    valid.parameters.size = 3;
    Require(live == liveBaseline);
    Require(errno == 55);

    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&valid);
    Require(live.empty());
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestCopyInlineModes();
    TestCopyAllocatedModes();
    TestMove();
    TestAllocationFailureCleanup();
    TestInvalidGuestInputs();
    std::puts("libwebrtc SdpVideoFormat tests passed");
    return 0;
}

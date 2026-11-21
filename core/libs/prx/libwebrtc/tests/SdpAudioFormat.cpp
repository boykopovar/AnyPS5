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
    std::fprintf(stderr, "libwebrtc SdpAudioFormat check failed at line %u\n", location.line());
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

    void SetWritable() const {
#ifdef _WIN32
        DWORD oldProtect = 0;
        Require(VirtualProtect(address, size, PAGE_READWRITE, &oldProtect) != 0);
#else
        Require(mprotect(address, size, PROT_READ | PROT_WRITE) == 0);
#endif
    }

    void SetReadOnly() const {
#ifdef _WIN32
        DWORD oldProtect = 0;
        Require(VirtualProtect(address, size, PAGE_READONLY, &oldProtect) != 0);
#else
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

void InitEmptyFormat(WebRtcSdpAudioFormat& format, std::string_view name, int clockrateHz, std::uint64_t numChannels) {
    std::memset(&format, 0, sizeof(format));
    InitGuestString(format.name, name);
    format.clockrateHz = clockrateHz;
    format.numChannels = numChannels;
    format.parameters.beginNode = EndNode(format.parameters);
    format.parameters.root = nullptr;
    format.parameters.size = 0;
}

WebRtcStringMapNode* MakeNode(std::string_view key, std::string_view value, bool isBlack) {
    auto* storage = _Znwm_nid_postfix(sizeof(WebRtcStringMapNode));
    auto* node = new (storage) WebRtcStringMapNode{};
    node->links.isBlack = isBlack;
    InitGuestString(node->key, key);
    InitGuestString(node->value, value);
    return node;
}

void AttachSampleTree(WebRtcSdpAudioFormat& format) {
    auto* root = MakeNode("minptime", "10", true);
    auto* left = MakeNode("cbr", "1", false);
    auto* right = MakeNode("useinbandfec", "1", false);
    auto* rightChild = MakeNode("very-long-parameter-name-key", std::string(4200, 'x'), true);
    root->links.left = left;
    root->links.right = right;
    root->links.parent = EndNode(format.parameters);
    left->links.parent = root;
    right->links.parent = root;
    right->links.right = rightChild;
    rightChild->links.parent = right;
    format.parameters.root = root;
    format.parameters.beginNode = left;
    format.parameters.size = 4;
}

void TestCopyAndMoveInlineAndHeap() {
    WebRtcSdpAudioFormat source;
    InitEmptyFormat(source, "opus", 48000, 2);
    AttachSampleTree(source);

    WebRtcSdpAudioFormat copied;
    std::memset(&copied, 0xcd, sizeof(copied));
    errno = 41;
    _ZN6webrtc14SdpAudioFormatC1ERKS0__nid_postfix(&copied, &source);
    Require(errno == 41);
    Require(ReadGuestString(copied.name) == "opus");
    Require(copied.clockrateHz == 48000 && copied.numChannels == 2);
    Require(copied.parameters.size == 4);
    Require(copied.parameters.root != source.parameters.root);
    Require(copied.parameters.root->links.parent == EndNode(copied.parameters));
    Require(copied.parameters.beginNode == copied.parameters.root->links.left);
    Require(ReadGuestString(copied.parameters.beginNode->key) == "cbr");
    Require(ReadGuestString(copied.parameters.beginNode->value) == "1");
    Require(ReadGuestString(copied.parameters.root->key) == "minptime");
    Require(ReadGuestString(copied.parameters.root->value) == "10");
    Require(copied.parameters.root->links.right->links.parent == copied.parameters.root);
    Require(copied.parameters.root->links.right->links.isBlack == false);
    Require(copied.parameters.root->links.right->links.right->links.isBlack == true);
    Require(ReadGuestString(copied.parameters.root->links.right->links.right->value) == std::string(4200, 'x'));

    const auto liveBeforeMove = live;
    const auto* originalRoot = source.parameters.root;
    const auto* originalBegin = source.parameters.beginNode;
    WebRtcSdpAudioFormat moved;
    std::memset(&moved, 0xcd, sizeof(moved));
    errno = 42;
    _ZN6webrtc14SdpAudioFormatC1EOS0__nid_postfix(&moved, &source);
    Require(errno == 42);
    Require(live == liveBeforeMove);
    Require(ReadGuestString(moved.name) == "opus");
    Require(moved.clockrateHz == 48000 && moved.numChannels == 2);
    Require(moved.parameters.size == 4);
    Require(moved.parameters.root == originalRoot);
    Require(moved.parameters.beginNode == originalBegin);
    Require(moved.parameters.root->links.parent == EndNode(moved.parameters));
    Require(source.name.size == 0 && source.name.capacity == WebRtcGuestStringInlineCapacity);
    Require(source.parameters.size == 0 && source.parameters.root == nullptr);
    Require(source.parameters.beginNode == EndNode(source.parameters));

    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&source);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&copied);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&moved);
    Require(live.empty());
}

void TestCopyAssignmentAndSelfAssign() {
    WebRtcSdpAudioFormat dst;
    InitEmptyFormat(dst, "long-codec-name-exceeding-inline-buffer", 16000, 1);
    AttachSampleTree(dst);

    WebRtcSdpAudioFormat src;
    InitEmptyFormat(src, "another-very-long-codec-name-with-heap-storage", 32000, 2);
    auto* onlyNode = MakeNode("stereo", "1", true);
    onlyNode->links.parent = EndNode(src.parameters);
    src.parameters.root = onlyNode;
    src.parameters.beginNode = onlyNode;
    src.parameters.size = 1;

    errno = 43;
    Require(_ZN6webrtc14SdpAudioFormataSERKS0__nid_postfix(&dst, &dst) == &dst);
    Require(errno == 43);
    Require(dst.parameters.size == 4 && dst.clockrateHz == 16000);

    Require(_ZN6webrtc14SdpAudioFormataSERKS0__nid_postfix(&dst, &src) == &dst);
    Require(errno == 43);
    Require(ReadGuestString(dst.name) == "another-very-long-codec-name-with-heap-storage");
    Require(dst.clockrateHz == 32000 && dst.numChannels == 2);
    Require(dst.parameters.size == 1);
    Require(dst.parameters.root != onlyNode);
    Require(dst.parameters.beginNode == dst.parameters.root);
    Require(dst.parameters.root->links.parent == EndNode(dst.parameters));
    Require(ReadGuestString(dst.parameters.root->key) == "stereo");

    WebRtcSdpAudioFormat empty;
    InitEmptyFormat(empty, "PCMU", 8000, 1);
    Require(_ZN6webrtc14SdpAudioFormataSERKS0__nid_postfix(&dst, &empty) == &dst);
    Require(ReadGuestString(dst.name) == "PCMU" && dst.clockrateHz == 8000 && dst.numChannels == 1);
    Require(dst.parameters.size == 0 && dst.parameters.root == nullptr);
    Require(dst.parameters.beginNode == EndNode(dst.parameters));

    WebRtcSdpAudioFormat movedLong;
    InitEmptyFormat(movedLong, "heap-moved-codec-name-longer-than-15", 48000, 2);
    auto* savedPointer = movedLong.name.pointer;
    WebRtcSdpAudioFormat moveTarget;
    std::memset(&moveTarget, 0xcd, sizeof(moveTarget));
    _ZN6webrtc14SdpAudioFormatC1EOS0__nid_postfix(&moveTarget, &movedLong);
    Require(moveTarget.name.pointer == savedPointer);
    Require(movedLong.name.size == 0 && movedLong.name.capacity == WebRtcGuestStringInlineCapacity);

    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&movedLong);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&moveTarget);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&dst);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&src);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&empty);
    Require(live.empty());
}

void TestAllocationFailureCleanup() {
    WebRtcSdpAudioFormat src;
    InitEmptyFormat(src, "heap-source-codec-name-1234567890", 48000, 2);
    AttachSampleTree(src);

    WebRtcSdpAudioFormat dst;
    InitEmptyFormat(dst, "existing-codec-name-1234567890", 16000, 1);
    auto* dstNode = MakeNode("ptime", "20", true);
    dstNode->links.parent = EndNode(dst.parameters);
    dst.parameters.root = dstNode;
    dst.parameters.beginNode = dstNode;
    dst.parameters.size = 1;

    const auto liveBaseline = live;
    bool threw = false;
    failAfterAllocations = allocationCounter + 3;
    try {
        _ZN6webrtc14SdpAudioFormataSERKS0__nid_postfix(&dst, &src);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    failAfterAllocations = static_cast<std::size_t>(-1);
    Require(threw);
    Require(live == liveBaseline);
    Require(ReadGuestString(dst.name) == "existing-codec-name-1234567890");
    Require(dst.clockrateHz == 16000 && dst.parameters.size == 1);

    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&src);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&dst);
    Require(live.empty());
}

void TestInvalidGuestInputs() {
    ReadOnlyPage readOnly;
    auto* roFormat = static_cast<WebRtcSdpAudioFormat*>(readOnly.address);
    auto* badPtr = reinterpret_cast<WebRtcSdpAudioFormat*>(0x10);

    WebRtcSdpAudioFormat valid;
    InitEmptyFormat(valid, "opus", 48000, 2);
    AttachSampleTree(valid);
    const auto liveBaseline = live;

    errno = 55;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatC1ERKS0__nid_postfix(nullptr, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatC1ERKS0__nid_postfix(badPtr, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatC1ERKS0__nid_postfix(roFormat, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatC1ERKS0__nid_postfix(&valid, nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatC1ERKS0__nid_postfix(&valid, badPtr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatC1EOS0__nid_postfix(nullptr, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatC1EOS0__nid_postfix(&valid, roFormat); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormataSERKS0__nid_postfix(nullptr, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormataSERKS0__nid_postfix(roFormat, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormataSERKS0__nid_postfix(&valid, badPtr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(badPtr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(roFormat); }));
    Require(errno == 55);

    valid.name.capacity = 3;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&valid); }));
    valid.name.capacity = WebRtcGuestStringInlineCapacity;

    valid.parameters.size = 3;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&valid); }));
    valid.parameters.size = 4;

    auto* savedParent = valid.parameters.root->links.left->links.parent;
    valid.parameters.root->links.left->links.parent = valid.parameters.root->links.left;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&valid); }));
    valid.parameters.root->links.left->links.parent = savedParent;
    Require(live == liveBaseline);
    Require(errno == 55);

    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&valid);
    Require(live.empty());
}

void TestMoveWithReadOnlyRootThrows() {
    ReadOnlyPage page;
    page.SetWritable();
    WebRtcSdpAudioFormat source;
    InitEmptyFormat(source, "opus", 48000, 2);
    auto* root = new (page.address) WebRtcStringMapNode{};
    root->links.isBlack = true;
    root->links.parent = EndNode(source.parameters);
    InitGuestString(root->key, "ptime");
    InitGuestString(root->value, "20");
    source.parameters.root = root;
    source.parameters.beginNode = root;
    source.parameters.size = 1;
    page.SetReadOnly();

    const WebRtcSdpAudioFormat sourceBefore = source;
    WebRtcSdpAudioFormat target;
    std::memset(&target, 0xcd, sizeof(target));
    const WebRtcSdpAudioFormat targetBefore = target;
    errno = 57;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpAudioFormatC1EOS0__nid_postfix(&target, &source); }));
    Require(errno == 57);
    Require(std::memcmp(&source, &sourceBefore, sizeof(source)) == 0);
    Require(std::memcmp(&target, &targetBefore, sizeof(target)) == 0);
    Require(root->links.parent == EndNode(source.parameters));

    WebRtcSdpAudioFormat copied;
    std::memset(&copied, 0xcd, sizeof(copied));
    _ZN6webrtc14SdpAudioFormatC1ERKS0__nid_postfix(&copied, &source);
    Require(copied.parameters.size == 1 && copied.parameters.root != root);
    Require(ReadGuestString(copied.parameters.root->key) == "ptime");
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&copied);
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
    TestCopyAndMoveInlineAndHeap();
    TestCopyAssignmentAndSelfAssign();
    TestAllocationFailureCleanup();
    TestInvalidGuestInputs();
    TestMoveWithReadOnlyRootThrows();
    std::puts("libwebrtc SdpAudioFormat tests passed");
    return 0;
}

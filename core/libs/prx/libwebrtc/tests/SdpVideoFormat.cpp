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

WebRtcStringMapNode* MakeNode(std::string_view key, std::string_view value, bool isBlack) {
    auto* storage = _Znwm_nid_postfix(sizeof(WebRtcStringMapNode));
    auto* node = new (storage) WebRtcStringMapNode{};
    node->links.isBlack = isBlack;
    InitGuestString(node->key, key);
    InitGuestString(node->value, value);
    return node;
}

void InitEmptyVideoFormat(WebRtcSdpVideoFormat& format, std::string_view name) {
    std::memset(&format, 0xee, sizeof(format));
    InitGuestString(format.name, name);
    format.parameters.beginNode = EndNode(format.parameters);
    format.parameters.root = nullptr;
    format.parameters.size = 0;
    format.scalabilityModes.sizeAndIsAllocated = 0;
}

void AttachVideoParameters(WebRtcSdpVideoFormat& format) {
    auto* root = MakeNode("profile-id", "0", true);
    auto* right = MakeNode("x-long-parameter", std::string(60, 'p'), false);
    root->links.right = right;
    root->links.parent = EndNode(format.parameters);
    right->links.parent = root;
    format.parameters.root = root;
    format.parameters.beginNode = root;
    format.parameters.size = 2;
}

void SetInlineModes(WebRtcSdpVideoFormat& format, std::initializer_list<std::uint8_t> modes) {
    std::memset(format.scalabilityModes.inlined, 0x5a, sizeof(format.scalabilityModes.inlined));
    std::size_t index = 0;
    for (const std::uint8_t mode : modes) format.scalabilityModes.inlined[index++] = mode;
    format.scalabilityModes.sizeAndIsAllocated = static_cast<std::uint64_t>(modes.size()) << 1;
}

void SetAllocatedModes(WebRtcSdpVideoFormat& format, std::uint64_t count, std::uint64_t capacity) {
    auto* data = static_cast<std::uint8_t*>(GuestStringStorage::Allocate(capacity));
    for (std::uint64_t index = 0; index < capacity; ++index) data[index] = static_cast<std::uint8_t>(index % 34);
    format.scalabilityModes.allocated.data = data;
    format.scalabilityModes.allocated.capacity = capacity;
    format.scalabilityModes.sizeAndIsAllocated = (count << 1) | 1;
}

void TestVideoFormatCopyAndMove() {
    WebRtcSdpVideoFormat source;
    InitEmptyVideoFormat(source, "VP9-with-a-name-longer-than-fifteen");
    AttachVideoParameters(source);
    SetInlineModes(source, {1, 5, 9});

    WebRtcSdpVideoFormat copied;
    std::memset(&copied, 0xcd, sizeof(copied));
    errno = 51;
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copied, &source);
    Require(errno == 51);
    Require(ReadGuestString(copied.name) == "VP9-with-a-name-longer-than-fifteen");
    Require(copied.name.pointer != source.name.pointer);
    Require(copied.parameters.size == 2 && copied.parameters.root != source.parameters.root);
    Require(copied.parameters.root->links.parent == EndNode(copied.parameters));
    Require(copied.parameters.beginNode == copied.parameters.root);
    Require(ReadGuestString(copied.parameters.root->key) == "profile-id");
    Require(ReadGuestString(copied.parameters.root->links.right->value) == std::string(60, 'p'));
    Require(std::memcmp(&copied.scalabilityModes, &source.scalabilityModes, sizeof(source.scalabilityModes)) == 0);

    const auto liveBeforeMove = live;
    const auto* originalRoot = source.parameters.root;
    const char* originalName = source.name.pointer;
    const WebRtcScalabilityModeVector originalModes = source.scalabilityModes;
    WebRtcSdpVideoFormat moved;
    std::memset(&moved, 0xcd, sizeof(moved));
    errno = 52;
    _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&moved, &source);
    Require(errno == 52);
    Require(live == liveBeforeMove);
    Require(moved.name.pointer == originalName && ReadGuestString(moved.name) == "VP9-with-a-name-longer-than-fifteen");
    Require(moved.parameters.size == 2 && moved.parameters.root == originalRoot);
    Require(moved.parameters.root->links.parent == EndNode(moved.parameters));
    Require(std::memcmp(&moved.scalabilityModes, &originalModes, sizeof(originalModes)) == 0);
    Require(source.name.size == 0 && source.name.capacity == WebRtcGuestStringInlineCapacity);
    Require(source.parameters.size == 0 && source.parameters.root == nullptr);
    Require(source.parameters.beginNode == EndNode(source.parameters));
    Require(source.scalabilityModes.sizeAndIsAllocated == 0);
    Require(std::memcmp(source.scalabilityModes.inlined, originalModes.inlined, sizeof(originalModes.inlined)) == 0);

    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copied);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&moved);
    Require(live.empty());
}

void TestVideoFormatScalabilityModes() {
    WebRtcSdpVideoFormat empty;
    InitEmptyVideoFormat(empty, "AV1");
    WebRtcSdpVideoFormat copied;
    std::memset(&copied, 0xcd, sizeof(copied));
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copied, &empty);
    Require(ReadGuestString(copied.name) == "AV1" && copied.parameters.size == 0);
    Require(copied.parameters.beginNode == EndNode(copied.parameters));
    Require(copied.scalabilityModes.sizeAndIsAllocated == 0);
    for (const std::uint8_t byte : copied.scalabilityModes.inlined) Require(byte == 0xcd);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copied);

    WebRtcSdpVideoFormat full;
    InitEmptyVideoFormat(full, "AV1");
    std::memset(full.scalabilityModes.inlined, 0, sizeof(full.scalabilityModes.inlined));
    for (std::size_t index = 0; index < WebRtcScalabilityModeCount; ++index) full.scalabilityModes.inlined[index] = static_cast<std::uint8_t>(index);
    full.scalabilityModes.sizeAndIsAllocated = WebRtcScalabilityModeCount << 1;
    std::memset(&copied, 0xcd, sizeof(copied));
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copied, &full);
    Require(std::memcmp(&copied.scalabilityModes, &full.scalabilityModes, sizeof(full.scalabilityModes)) == 0);
    Require(live.empty());
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copied);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&full);

    for (const auto& [count, capacity, expectedCapacity] :
         {std::array<std::uint64_t, 3>{70, 100, 70}, std::array<std::uint64_t, 3>{10, 136, 68},
          std::array<std::uint64_t, 3>{68, 68, 68}, std::array<std::uint64_t, 3>{5000, 5000, 5000}}) {
        WebRtcSdpVideoFormat heap;
        InitEmptyVideoFormat(heap, "H264");
        SetAllocatedModes(heap, count, capacity);
        std::memset(&copied, 0xcd, sizeof(copied));
        _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copied, &heap);
        Require(copied.scalabilityModes.sizeAndIsAllocated == heap.scalabilityModes.sizeAndIsAllocated);
        Require(copied.scalabilityModes.allocated.data != heap.scalabilityModes.allocated.data);
        Require(copied.scalabilityModes.allocated.capacity == expectedCapacity);
        Require(std::memcmp(copied.scalabilityModes.allocated.data, heap.scalabilityModes.allocated.data, count) == 0);

        const auto liveBeforeMove = live;
        std::uint8_t* const heapData = heap.scalabilityModes.allocated.data;
        WebRtcSdpVideoFormat moved;
        std::memset(&moved, 0xcd, sizeof(moved));
        _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&moved, &heap);
        Require(live == liveBeforeMove);
        Require(moved.scalabilityModes.allocated.data == heapData && moved.scalabilityModes.allocated.capacity == capacity);
        Require(moved.scalabilityModes.sizeAndIsAllocated == ((count << 1) | 1));
        Require(heap.scalabilityModes.sizeAndIsAllocated == 0);
        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&heap);
        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&moved);
        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copied);
        Require(live.empty());
    }

    WebRtcSdpVideoFormat emptyHeap;
    InitEmptyVideoFormat(emptyHeap, "H265");
    SetAllocatedModes(emptyHeap, 0, 68);
    std::memset(&copied, 0xcd, sizeof(copied));
    const auto liveBeforeCopy = live;
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copied, &emptyHeap);
    Require(live == liveBeforeCopy);
    Require(copied.scalabilityModes.sizeAndIsAllocated == 0);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copied);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&emptyHeap);
    Require(emptyHeap.scalabilityModes.sizeAndIsAllocated == 0);
    Require(live.empty());
}

void TestVideoFormatAllocationFailure() {
    WebRtcSdpVideoFormat source;
    InitEmptyVideoFormat(source, "VP9-with-a-name-longer-than-fifteen");
    AttachVideoParameters(source);
    SetAllocatedModes(source, 70, 70);
    const auto liveBaseline = live;
    std::size_t successes = 0;
    for (std::size_t allowed = 0; allowed < 8 && successes == 0; ++allowed) {
        WebRtcSdpVideoFormat copied;
        std::memset(&copied, 0xcd, sizeof(copied));
        bool threw = false;
        failAfterAllocations = allocationCounter + allowed;
        try {
            _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copied, &source);
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        failAfterAllocations = static_cast<std::size_t>(-1);
        if (threw) {
            Require(live == liveBaseline);
            Require(copied.scalabilityModes.sizeAndIsAllocated == 0xcdcdcdcdcdcdcdcdull);
            continue;
        }
        Require(allowed == 6);
        ++successes;
        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copied);
        Require(live == liveBaseline);
    }
    Require(successes == 1);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source);
    Require(live.empty());
}

void TestVideoFormatInvalid() {
    WebRtcSdpVideoFormat valid;
    InitEmptyVideoFormat(valid, "VP8");
    SetInlineModes(valid, {2});
    WebRtcSdpVideoFormat target;
    std::memset(&target, 0xcd, sizeof(target));
    const ReadOnlyPage readOnly;
    auto* const readOnlyFormat = static_cast<WebRtcSdpVideoFormat*>(readOnly.address);

    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(nullptr, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&target, nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(readOnlyFormat, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(nullptr, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&target, nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&valid, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&target, readOnlyFormat); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(readOnlyFormat); }));

    valid.scalabilityModes.sizeAndIsAllocated = (WebRtcScalabilityModeCount + 1) << 1;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&target, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&target, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&valid); }));
    valid.scalabilityModes.allocated.data = nullptr;
    valid.scalabilityModes.allocated.capacity = 8;
    valid.scalabilityModes.sizeAndIsAllocated = (4 << 1) | 1;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&target, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&valid); }));
    std::uint8_t storage[8]{};
    valid.scalabilityModes.allocated.data = storage;
    valid.scalabilityModes.allocated.capacity = 3;
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&target, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&target, &valid); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&valid); }));
    Require(target.scalabilityModes.sizeAndIsAllocated == 0xcdcdcdcdcdcdcdcdull);

    SetInlineModes(valid, {2});
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
    TestVideoFormatCopyAndMove();
    TestVideoFormatScalabilityModes();
    TestVideoFormatAllocationFailure();
    TestVideoFormatInvalid();
    std::puts("libwebrtc SdpVideoFormat tests passed");
    return 0;
}

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

using CodecDestructor = void(APS5_VABI*)(WebRtcCodec*);
using CodecToParameters = WebRtcRtpCodecParameters*(APS5_VABI*)(WebRtcRtpCodecParameters*, const WebRtcCodec*);
using ParametersDestructor = void(APS5_VABI*)(WebRtcRtpCodecParameters*);

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
    std::fprintf(stderr, "libwebrtc Codec check failed at line %u\n", location.line());
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

void InitFormat(WebRtcSdpVideoFormat& format, std::string_view name) {
    std::memset(&format, 0xee, sizeof(format));
    InitGuestString(format.name, name);
    format.parameters.beginNode = EndNode(format.parameters);
    format.parameters.root = nullptr;
    format.parameters.size = 0;
    format.scalabilityModes.sizeAndIsAllocated = 0;
}

void AttachParameters(WebRtcSdpVideoFormat& format) {
    auto* root = MakeNode("profile-id", "0", true);
    auto* right = MakeNode("x-long-parameter", std::string(60, 'p'), false);
    root->links.right = right;
    root->links.parent = EndNode(format.parameters);
    right->links.parent = root;
    format.parameters.root = root;
    format.parameters.beginNode = root;
    format.parameters.size = 2;
}

void SetInlineModes(WebRtcSdpVideoFormat& format, std::uint64_t count) {
    for (std::size_t index = 0; index < sizeof(format.scalabilityModes.inlined); ++index)
        format.scalabilityModes.inlined[index] = static_cast<std::uint8_t>(index < count ? index + 1 : 0x5a);
    format.scalabilityModes.sizeAndIsAllocated = count << 1;
}

void SetAllocatedModes(WebRtcSdpVideoFormat& format, std::uint64_t count, std::uint64_t capacity) {
    auto* data = static_cast<std::uint8_t*>(GuestStringStorage::Allocate(capacity));
    for (std::uint64_t index = 0; index < capacity; ++index) data[index] = static_cast<std::uint8_t>(index % 34);
    format.scalabilityModes.allocated.data = data;
    format.scalabilityModes.allocated.capacity = capacity;
    format.scalabilityModes.sizeAndIsAllocated = (count << 1) | 1;
}

void DestroyCodec(WebRtcCodec& codec) {
    const auto* const vtable = static_cast<void* const*>(codec.vtable);
    reinterpret_cast<CodecDestructor>(vtable[0])(&codec);
}

void TestCreateFromFormat() {
    WebRtcSdpVideoFormat format;
    InitFormat(format, "VP9-with-a-name-longer-than-fifteen");
    AttachParameters(format);
    auto* left = MakeNode("level-id", "1", false);
    left->links.parent = format.parameters.root;
    format.parameters.root->links.left = left;
    format.parameters.beginNode = left;
    ++format.parameters.size;
    SetInlineModes(format, 3);
    const WebRtcSdpVideoFormat formatBefore = format;
    const auto liveBefore = live;

    WebRtcCodec codec;
    std::memset(&codec, 0xcd, sizeof(codec));
    errno = 61;
    Require(_ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(&codec, &format) == &codec);
    Require(errno == 61);
    Require(std::memcmp(&format, &formatBefore, sizeof(format)) == 0);

    const auto* const vtable = static_cast<void* const*>(codec.vtable);
    Require(vtable && vtable[0] && vtable[1] && vtable[2]);
    Require(codec.type == WebRtcCodecTypeVideo && codec.type == 1);
    Require(codec.id == -1);
    Require(ReadGuestString(codec.name) == "VP9-with-a-name-longer-than-fifteen");
    Require(codec.name.pointer != format.name.pointer);
    Require(codec.clockrate == 90000);
    Require(codec.bitrate == 0);
    Require(codec.channels == 0);
    const auto* const packetizationBytes = reinterpret_cast<const unsigned char*>(&codec.packetization.value);
    Require(!codec.packetization.engaged && packetizationBytes[0] == 0 && packetizationBytes[1] == 0xcd && packetizationBytes[39] == 0xcd);
    Require(codec.scalabilityModes.sizeAndIsAllocated == (3u << 1));
    Require(codec.scalabilityModes.inlined[0] == 1 && codec.scalabilityModes.inlined[1] == 2 && codec.scalabilityModes.inlined[2] == 3);
    Require(codec.scalabilityModes.inlined[3] == 0xcd && codec.scalabilityModes.inlined[33] == 0xcd);
    const auto* const txModeBytes = reinterpret_cast<const unsigned char*>(&codec.txMode.value);
    Require(!codec.txMode.engaged && txModeBytes[0] == 0 && txModeBytes[1] == 0xcd);
    Require(codec.params.size == 3 && codec.params.root != format.parameters.root);
    Require(codec.params.root->links.parent == EndNode(codec.params));
    Require(codec.params.beginNode == codec.params.root->links.left);
    Require(ReadGuestString(codec.params.root->key) == "profile-id" && ReadGuestString(codec.params.root->value) == "0");
    Require(ReadGuestString(codec.params.root->links.right->key) == "x-long-parameter");
    Require(ReadGuestString(codec.params.root->links.right->value) == std::string(60, 'p'));
    Require(codec.params.root->links.right->links.parent == codec.params.root);
    Require(!codec.feedbackParams.begin && !codec.feedbackParams.end && !codec.feedbackParams.capacityEnd);
    Require(live.size() == liveBefore.size() + 6);

    const auto convert = reinterpret_cast<CodecToParameters>(vtable[2]);
    Require(ThrowsInvalid([&] { convert(nullptr, &codec); }));
    WebRtcRtpCodecParameters parameters{};
    const auto codecAllocations = live;
    for (std::size_t allowed = 0; allowed < 6; ++allowed) {
        failAfterAllocations = allocationCounter + allowed;
        try {
            convert(&parameters, &codec);
            Require(false);
        } catch (const std::bad_alloc&) {
            Require(live == codecAllocations && !parameters.codec.vtable);
        }
    }
    failAfterAllocations = static_cast<std::size_t>(-1);
    Require(convert(&parameters, &codec) == &parameters);
    Require(parameters.codec.kind == 1 && parameters.payloadType == -1);
    Require(parameters.codec.hasClockRate && parameters.codec.clockRate == 90000 && !parameters.codec.hasNumChannels);
    Require(!parameters.codec.rtcpFeedback.begin && !parameters.codec.rtcpFeedback.end && !parameters.codec.rtcpFeedback.capacityEnd);
    Require(parameters.codec.name.pointer != codec.name.pointer);
    Require(parameters.codec.parameters.size == 3 && parameters.codec.parameters.root != codec.params.root);
    Require(parameters.codec.parameters.root->links.parent == EndNode(parameters.codec.parameters));
    const auto* copiedRoot = parameters.codec.parameters.root;
    Require(parameters.codec.parameters.beginNode == copiedRoot->links.left);
    Require(copiedRoot->links.left->links.parent == copiedRoot && copiedRoot->links.right->links.parent == copiedRoot);
    auto* originalRight = codec.params.root->links.right;
    const auto* copiedRight = copiedRoot->links.right;
    Require(copiedRight->key.pointer != originalRight->key.pointer && copiedRight->value.pointer != originalRight->value.pointer);
    originalRight->key.pointer[0] = 'y';
    originalRight->value.pointer[0] = 'q';
    Require(ReadGuestString(copiedRight->key) == "x-long-parameter" && ReadGuestString(copiedRight->value) == std::string(60, 'p'));
    errno = 62;
    DestroyCodec(codec);
    Require(errno == 62);
    Require(ReadGuestString(parameters.codec.name) == "VP9-with-a-name-longer-than-fifteen");
    Require(ReadGuestString(parameters.codec.parameters.root->links.right->value) == std::string(60, 'p'));
    reinterpret_cast<ParametersDestructor>(parameters.codec.vtable[0])(&parameters);
    Require(live == liveBefore);
    Require(codec.params.size == 0 && codec.params.root == nullptr && codec.params.beginNode == EndNode(codec.params));
    Require(std::memcmp(&format, &formatBefore, sizeof(format)) == 0);

    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&format);
    Require(live.empty());
}

void TestEmptyFormat() {
    WebRtcSdpVideoFormat format;
    InitFormat(format, "VP8");
    WebRtcCodec codec;
    std::memset(&codec, 0xcd, sizeof(codec));
    const std::size_t allocationsBefore = allocationCounter;
    _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(&codec, &format);
    Require(allocationCounter == allocationsBefore);
    Require(ReadGuestString(codec.name) == "VP8" && codec.name.capacity == WebRtcGuestStringInlineCapacity);
    Require(codec.scalabilityModes.sizeAndIsAllocated == 0 && codec.scalabilityModes.inlined[0] == 0xcd);
    Require(codec.params.size == 0 && codec.params.root == nullptr && codec.params.beginNode == EndNode(codec.params));
    Require(codec.type == 1 && codec.id == -1 && codec.clockrate == 90000);
    const auto* vtable = static_cast<void* const*>(codec.vtable);
    auto* parameters = static_cast<WebRtcRtpCodecParameters*>(_Znwm_nid_postfix(sizeof(WebRtcRtpCodecParameters)));
    codec.type = 0;
    codec.channels = 2;
    codec.id = 111;
    codec.clockrate = 48000;
    Require(reinterpret_cast<CodecToParameters>(vtable[2])(parameters, &codec) == parameters);
    Require(parameters->codec.kind == 0 && parameters->payloadType == 111);
    Require(parameters->codec.hasClockRate && parameters->codec.clockRate == 48000);
    Require(parameters->codec.hasNumChannels && parameters->codec.numChannels == 2);
    Require(parameters->codec.parameters.beginNode == EndNode(parameters->codec.parameters));
    reinterpret_cast<ParametersDestructor>(parameters->codec.vtable[1])(parameters);
    DestroyCodec(codec);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&format);
    Require(live.empty());
}

void TestScalabilityModes() {
    {
        WebRtcSdpVideoFormat format;
        InitFormat(format, "AV1");
        SetInlineModes(format, 34);
        WebRtcCodec codec;
        std::memset(&codec, 0xcd, sizeof(codec));
        _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(&codec, &format);
        Require(codec.scalabilityModes.sizeAndIsAllocated == (34u << 1));
        Require(std::memcmp(codec.scalabilityModes.inlined, format.scalabilityModes.inlined, 34) == 0);
        DestroyCodec(codec);
        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&format);
        Require(live.empty());
    }
    {
        WebRtcSdpVideoFormat format;
        InitFormat(format, "AV1");
        SetAllocatedModes(format, 10, 136);
        WebRtcCodec codec;
        std::memset(&codec, 0xcd, sizeof(codec));
        const std::size_t allocationsBefore = allocationCounter;
        _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(&codec, &format);
        Require(allocationCounter == allocationsBefore);
        Require(codec.scalabilityModes.sizeAndIsAllocated == (10u << 1));
        Require(std::memcmp(codec.scalabilityModes.inlined, format.scalabilityModes.allocated.data, 10) == 0);
        Require(codec.scalabilityModes.inlined[10] == 0xcd);
        DestroyCodec(codec);
        Require(live.size() == 1);
        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&format);
        Require(live.empty());
    }
    const std::array<std::array<std::uint64_t, 3>, 4> heapCases {{{70, 100, 70}, {40, 100, 68}, {35, 35, 68}, {5000, 5000, 5000}}};
    for (const auto& heapCase : heapCases) {
        WebRtcSdpVideoFormat format;
        InitFormat(format, "AV1");
        SetAllocatedModes(format, heapCase[0], heapCase[1]);
        WebRtcCodec codec;
        std::memset(&codec, 0xcd, sizeof(codec));
        _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(&codec, &format);
        Require(codec.scalabilityModes.sizeAndIsAllocated == ((heapCase[0] << 1) | 1));
        Require(codec.scalabilityModes.allocated.capacity == heapCase[2]);
        Require(codec.scalabilityModes.allocated.data != format.scalabilityModes.allocated.data);
        Require(std::memcmp(codec.scalabilityModes.allocated.data, format.scalabilityModes.allocated.data, heapCase[0]) == 0);
        Require(live.size() == 2);
        DestroyCodec(codec);
        Require(live.size() == 1);
        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&format);
        Require(live.empty());
    }
}

void TestDeletingDestructor() {
    WebRtcSdpVideoFormat format;
    InitFormat(format, "H264-with-a-name-longer-than-fifteen");
    AttachParameters(format);
    const auto liveBefore = live;
    auto* const codec = static_cast<WebRtcCodec*>(_Znwm_nid_postfix(sizeof(WebRtcCodec)));
    std::memset(codec, 0xcd, sizeof(*codec));
    _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(codec, &format);
    Require(live.size() == liveBefore.size() + 6);
    const auto* const vtable = static_cast<void* const*>(codec->vtable);
    reinterpret_cast<CodecDestructor>(vtable[1])(codec);
    Require(live == liveBefore);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&format);
    Require(live.empty());
}

WebRtcFeedbackParam* MakeFeedback(WebRtcCodec& codec, std::size_t count, std::size_t capacity) {
    const std::size_t bytes = capacity * sizeof(WebRtcFeedbackParam);
    WebRtcFeedbackParam* entries = nullptr;
    if (bytes >= 0x1000) {
        void* raw = malloc_nid_postfix(bytes + 0x27);
        Require(raw != nullptr);
        const auto aligned = (reinterpret_cast<std::uintptr_t>(raw) + 0x27) & ~static_cast<std::uintptr_t>(0x1f);
        reinterpret_cast<void**>(aligned)[-1] = raw;
        entries = reinterpret_cast<WebRtcFeedbackParam*>(aligned);
    } else {
        entries = static_cast<WebRtcFeedbackParam*>(malloc_nid_postfix(bytes));
        Require(entries != nullptr);
    }
    for (std::size_t index = 0; index < count; ++index) {
        InitGuestString(entries[index].id, index % 2 ? "nack" : "a-feedback-identifier-longer-than-fifteen");
        InitGuestString(entries[index].param, index % 2 ? std::string(40, 'q') : std::string("pli"));
    }
    codec.feedbackParams.begin = entries;
    codec.feedbackParams.end = entries + count;
    codec.feedbackParams.capacityEnd = entries + capacity;
    return entries;
}

void TestDestructorReleasesEveryMember() {
    for (const std::size_t capacity : {std::size_t{3}, std::size_t{52}}) {
        WebRtcSdpVideoFormat format;
        InitFormat(format, "VP9-with-a-name-longer-than-fifteen");
        AttachParameters(format);
        SetAllocatedModes(format, 70, 70);
        const auto liveBefore = live;

        WebRtcCodec codec;
        std::memset(&codec, 0xcd, sizeof(codec));
        _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(&codec, &format);
        InitGuestString(codec.packetization.value, "a-packetization-longer-than-fifteen");
        codec.packetization.engaged = true;
        InitGuestString(codec.txMode.value, "a-transmission-mode-longer-than-fifteen");
        codec.txMode.engaged = true;
        MakeFeedback(codec, 2, capacity);
        Require(live.size() == liveBefore.size() + 6 + 2 + 1 + 2);

        DestroyCodec(codec);
        Require(live == liveBefore);
        Require(!codec.feedbackParams.begin && !codec.feedbackParams.end && !codec.feedbackParams.capacityEnd);
        Require(codec.scalabilityModes.sizeAndIsAllocated == 0);
        Require(codec.name.size == 0 && codec.name.capacity == WebRtcGuestStringInlineCapacity);

        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&format);
        Require(live.empty());
    }
}

void TestAllocationFailure() {
    WebRtcSdpVideoFormat format;
    InitFormat(format, "VP9-with-a-name-longer-than-fifteen");
    AttachParameters(format);
    SetAllocatedModes(format, 70, 70);
    const auto liveBaseline = live;
    std::size_t successes = 0;
    for (std::size_t allowed = 0; allowed < 8 && successes == 0; ++allowed) {
        WebRtcCodec codec;
        std::memset(&codec, 0xcd, sizeof(codec));
        bool threw = false;
        failAfterAllocations = allocationCounter + allowed;
        try {
            _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(&codec, &format);
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        failAfterAllocations = static_cast<std::size_t>(-1);
        if (threw) {
            Require(live == liveBaseline);
            const auto* const bytes = reinterpret_cast<const unsigned char*>(&codec);
            for (std::size_t index = 0; index < sizeof(codec); ++index) Require(bytes[index] == 0xcd);
            continue;
        }
        Require(allowed == 6);
        ++successes;
        DestroyCodec(codec);
        Require(live == liveBaseline);
    }
    Require(successes == 1);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&format);
    Require(live.empty());
}

void TestInvalid() {
    WebRtcSdpVideoFormat format;
    InitFormat(format, "VP8");
    SetInlineModes(format, 1);
    WebRtcCodec target;
    std::memset(&target, 0xcd, sizeof(target));
    const ReadOnlyPage readOnly;
    auto* const readOnlyCodec = static_cast<WebRtcCodec*>(readOnly.address);

    Require(ThrowsInvalid([&] { _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(nullptr, &format); }));
    Require(ThrowsInvalid([&] { _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(&target, nullptr); }));
    Require(ThrowsInvalid([&] { _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(readOnlyCodec, &format); }));
    format.scalabilityModes.sizeAndIsAllocated = (WebRtcScalabilityModeCount + 1) << 1;
    Require(ThrowsInvalid([&] { _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(&target, &format); }));
    format.scalabilityModes.allocated.data = nullptr;
    format.scalabilityModes.allocated.capacity = 8;
    format.scalabilityModes.sizeAndIsAllocated = (4 << 1) | 1;
    Require(ThrowsInvalid([&] { _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(&target, &format); }));
    const auto* const targetBytes = reinterpret_cast<const unsigned char*>(&target);
    for (std::size_t index = 0; index < sizeof(target); ++index) Require(targetBytes[index] == 0xcd);
    SetInlineModes(format, 1);

    Require(ThrowsInvalid([&] { _ZN7cricket5CodecD1Ev_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([&] { _ZN7cricket5CodecD1Ev_nid_postfix(readOnlyCodec); }));

    WebRtcCodec codec;
    std::memset(&codec, 0xcd, sizeof(codec));
    _ZN7cricket16CreateVideoCodecERKN6webrtc14SdpVideoFormatE_nid_postfix(&codec, &format);
    const auto liveBefore = live;
    WebRtcFeedbackParam entries[2];
    InitGuestString(entries[0].id, "nack");
    InitGuestString(entries[0].param, "pli");
    InitGuestString(entries[1].id, "ccm");
    InitGuestString(entries[1].param, "fir");

    codec.feedbackParams.end = entries + 1;
    Require(ThrowsInvalid([&] { _ZN7cricket5CodecD1Ev_nid_postfix(&codec); }));
    codec.feedbackParams.begin = entries + 1;
    codec.feedbackParams.end = entries;
    codec.feedbackParams.capacityEnd = entries + 2;
    Require(ThrowsInvalid([&] { _ZN7cricket5CodecD1Ev_nid_postfix(&codec); }));
    codec.feedbackParams.begin = entries;
    codec.feedbackParams.end = entries + 2;
    codec.feedbackParams.capacityEnd = entries + 1;
    Require(ThrowsInvalid([&] { _ZN7cricket5CodecD1Ev_nid_postfix(&codec); }));
    codec.feedbackParams.end = reinterpret_cast<WebRtcFeedbackParam*>(reinterpret_cast<char*>(entries) + 8);
    codec.feedbackParams.capacityEnd = entries + 2;
    Require(ThrowsInvalid([&] { _ZN7cricket5CodecD1Ev_nid_postfix(&codec); }));
    codec.feedbackParams.end = entries + 2;
    entries[1].param.capacity = 3;
    Require(ThrowsInvalid([&] { _ZN7cricket5CodecD1Ev_nid_postfix(&codec); }));
    entries[1].param.capacity = WebRtcGuestStringInlineCapacity;
    codec.feedbackParams.begin = nullptr;
    codec.feedbackParams.end = nullptr;
    codec.feedbackParams.capacityEnd = nullptr;

    codec.packetization.engaged = true;
    codec.packetization.value.capacity = 3;
    Require(ThrowsInvalid([&] { _ZN7cricket5CodecD1Ev_nid_postfix(&codec); }));
    codec.packetization.engaged = false;
    codec.txMode.engaged = true;
    codec.txMode.value.capacity = 20;
    codec.txMode.value.size = 4;
    codec.txMode.value.pointer = nullptr;
    Require(ThrowsInvalid([&] { _ZN7cricket5CodecD1Ev_nid_postfix(&codec); }));
    codec.txMode.engaged = false;
    Require(live == liveBefore);
    Require(ReadGuestString(codec.name) == "VP8");

    DestroyCodec(codec);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&format);
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
    TestCreateFromFormat();
    TestEmptyFormat();
    TestScalabilityModes();
    TestDeletingDestructor();
    TestDestructorReleasesEveryMember();
    TestAllocationFailure();
    TestInvalid();
    std::puts("libwebrtc Codec tests passed");
    return 0;
}

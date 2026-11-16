#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/libc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

std::set<void*> live;

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
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
    if (condition)
        return;
    std::fprintf(stderr, "libwebrtc SdpVideoFormat check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        address_ = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS);
        Require(address_ != nullptr);
#else
        void* mapped = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        address_ = mapped;
#endif
    }

    ~GuardedPage() {
        if (!address_)
            return;
#ifdef _WIN32
        VirtualFree(address_, 0, MEM_RELEASE);
#else
        munmap(address_, 4096);
#endif
    }

    template<typename T>
    T* As() const {
        return static_cast<T*>(address_);
    }

private:
    void* address_ = nullptr;
};

WebRtcVideoStringMapNode* EndNode(WebRtcSdpVideoFormat& format) {
    return reinterpret_cast<WebRtcVideoStringMapNode*>(&format.parameters.root);
}

void InitEmptyString(WebRtcGuestString& str) {
    str.reserved = 0;
    str.buffer[0] = '\0';
    str.size = 0;
    str.capacity = WebRtcGuestStringInlineCapacity;
}

void MakeGuestString(WebRtcGuestString& out, const std::string& text) {
    InitEmptyString(out);
    if (text.size() <= WebRtcGuestStringInlineCapacity) {
        if (!text.empty())
            std::memcpy(out.buffer, text.data(), text.size());
        out.buffer[text.size()] = '\0';
        out.size = text.size();
        return;
    }
    const std::size_t cap = text.size() | WebRtcGuestStringInlineCapacity;
    auto* ptr = static_cast<char*>(GuestStringStorage::Allocate(cap + 1));
    std::memcpy(ptr, text.data(), text.size());
    ptr[text.size()] = '\0';
    out.pointer = ptr;
    out.size = text.size();
    out.capacity = cap;
}

std::string ReadHostString(const WebRtcGuestString& str) {
    return std::string(WebRtcGuestStringData(str), str.size);
}

struct NodeSpec {
    std::string key;
    std::string value;
};

WebRtcVideoStringMapNode* BuildBalancedSubtree(const std::vector<NodeSpec>& sorted, std::size_t lo, std::size_t hi,
    WebRtcVideoStringMapNode* parent, WebRtcVideoStringMapNode*& firstNode) {
    if (lo >= hi)
        return nullptr;
    const std::size_t mid = lo + (hi - lo) / 2;
    auto* node = new (_Znwm_nid_postfix(sizeof(WebRtcVideoStringMapNode))) WebRtcVideoStringMapNode{};
    node->links.parent = parent;
    node->links.isBlack = true;
    MakeGuestString(node->key, sorted[mid].key);
    MakeGuestString(node->value, sorted[mid].value);
    node->links.left = BuildBalancedSubtree(sorted, lo, mid, node, firstNode);
    if (mid == 0)
        firstNode = node;
    node->links.right = BuildBalancedSubtree(sorted, mid + 1, hi, node, firstNode);
    return node;
}

void InitFormat(WebRtcSdpVideoFormat& format, const std::string& name, std::map<std::string, std::string> params,
    const std::vector<std::uint8_t>& scalabilityModes) {
    std::memset(&format, 0, sizeof(format));
    MakeGuestString(format.name, name);

    if (params.empty()) {
        format.parameters.beginNode = EndNode(format);
        format.parameters.root = nullptr;
        format.parameters.size = 0;
    } else {
        std::vector<NodeSpec> sorted;
        for (const auto& [k, v] : params)
            sorted.push_back({k, v});
        WebRtcVideoStringMapNode* first = nullptr;
        format.parameters.root = BuildBalancedSubtree(sorted, 0, sorted.size(), EndNode(format), first);
        format.parameters.beginNode = first;
        format.parameters.size = sorted.size();
    }

    if (scalabilityModes.size() <= WebRtcScalabilityModeCount) {
        format.scalabilityModes.metadata = static_cast<std::uint64_t>(scalabilityModes.size()) << 1;
        if (!scalabilityModes.empty())
            std::memcpy(format.scalabilityModes.inlined, scalabilityModes.data(), scalabilityModes.size());
    } else {
        void* heap = _Znwm_nid_postfix(scalabilityModes.size());
        std::memcpy(heap, scalabilityModes.data(), scalabilityModes.size());
        format.scalabilityModes.metadata = (static_cast<std::uint64_t>(scalabilityModes.size()) << 1) | 1;
        format.scalabilityModes.allocated.data = heap;
        format.scalabilityModes.allocated.capacity = scalabilityModes.size();
    }
}

std::map<std::string, std::string> ReadHostMap(const WebRtcSdpVideoFormat& format) {
    std::map<std::string, std::string> result;
    if (format.parameters.size == 0)
        return result;
    std::vector<const WebRtcVideoStringMapNode*> stack{format.parameters.root};
    while (!stack.empty()) {
        const auto* node = stack.back();
        stack.pop_back();
        result.emplace(ReadHostString(node->key), ReadHostString(node->value));
        if (node->links.left)
            stack.push_back(node->links.left);
        if (node->links.right)
            stack.push_back(node->links.right);
    }
    return result;
}

std::vector<std::uint8_t> ReadHostScalabilityModes(const WebRtcSdpVideoFormat& format) {
    const std::size_t count = static_cast<std::size_t>(format.scalabilityModes.metadata >> 1);
    const std::uint8_t* ptr = (format.scalabilityModes.metadata & 1)
        ? static_cast<const std::uint8_t*>(format.scalabilityModes.allocated.data)
        : format.scalabilityModes.inlined;
    return std::vector<std::uint8_t>(ptr, ptr + count);
}

void TestCopyAndDestructor() {
    WebRtcSdpVideoFormat source{};
    InitFormat(source, "H264", {{"level-asymmetry-allowed", "1"}, {"packetization-mode", "1"}, {"profile-level-id", "42e01f"}},
        {0, 1, 2, 10});

    WebRtcSdpVideoFormat copy{};
    std::memset(&copy, 0xa5, sizeof(copy));
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&copy, &source);

    Require(ReadHostString(copy.name) == "H264");
    Require(ReadHostMap(copy) == ReadHostMap(source));
    Require(ReadHostScalabilityModes(copy) == std::vector<std::uint8_t>({0, 1, 2, 10}));
    Require(copy.parameters.beginNode != EndNode(copy));
    Require(copy.parameters.root->links.parent == EndNode(copy));

    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&copy);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source);

    const std::string longName(4200, 'V');
    std::vector<std::uint8_t> manyModes(38);
    for (std::size_t i = 0; i < manyModes.size(); ++i)
        manyModes[i] = static_cast<std::uint8_t>(i % 34);

    WebRtcSdpVideoFormat large{};
    InitFormat(large, longName, {{"very-long-parameter-key", std::string(30, 'x')}}, manyModes);

    WebRtcSdpVideoFormat largeCopy{};
    _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&largeCopy, &large);
    Require(ReadHostString(largeCopy.name) == longName);
    Require(largeCopy.name.pointer != large.name.pointer);
    Require(ReadHostMap(largeCopy) == std::map<std::string, std::string>{{"very-long-parameter-key", std::string(30, 'x')}});
    Require((largeCopy.scalabilityModes.metadata & 1) == 1);
    Require(largeCopy.scalabilityModes.allocated.data != large.scalabilityModes.allocated.data);
    Require(ReadHostScalabilityModes(largeCopy) == manyModes);

    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&largeCopy);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&large);
}

void TestMoveConstructor() {
    WebRtcSdpVideoFormat source{};
    const std::string longName = "VeryLongVideoCodecFormatName";
    std::vector<std::uint8_t> manyModes(36, 7);
    InitFormat(source, longName, {{"profile-id", "2"}, {"tier", "1"}}, manyModes);
    const char* originalNamePtr = source.name.pointer;
    const auto* originalRoot = source.parameters.root;
    const void* originalModesPtr = source.scalabilityModes.allocated.data;

    WebRtcSdpVideoFormat moved{};
    _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&moved, &source);

    Require(ReadHostString(moved.name) == longName);
    Require(moved.name.pointer == originalNamePtr);
    Require(moved.parameters.root == originalRoot);
    Require(moved.parameters.root->links.parent == EndNode(moved));
    Require(moved.scalabilityModes.allocated.data == originalModesPtr);
    Require(ReadHostScalabilityModes(moved) == manyModes);

    Require(ReadHostString(source.name).empty());
    Require(source.parameters.size == 0 && source.parameters.root == nullptr);
    Require(source.parameters.beginNode == EndNode(source));
    Require(source.scalabilityModes.metadata == 0);

    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&source);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&moved);

    WebRtcSdpVideoFormat singleSource{};
    InitFormat(singleSource, "VP8", {{"key", "val"}}, {});
    const auto* singleRoot = singleSource.parameters.root;
    Require(singleSource.parameters.beginNode == singleRoot);
    WebRtcSdpVideoFormat singleMoved{};
    _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&singleMoved, &singleSource);
    Require(singleMoved.parameters.root == singleRoot);
    Require(singleMoved.parameters.beginNode == singleRoot);
    Require(singleMoved.parameters.root->links.parent == EndNode(singleMoved));
    Require(singleSource.parameters.size == 0 && singleSource.parameters.root == nullptr);
    Require(singleSource.parameters.beginNode == EndNode(singleSource));
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&singleSource);
    Require(ReadHostMap(singleMoved) == std::map<std::string, std::string>{{"key", "val"}});
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&singleMoved);
}

void TestIsCodecInList() {
    WebRtcSdpVideoFormat vp8Query{};
    InitFormat(vp8Query, "vp8", {}, {});
    Require(!_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&vp8Query, {nullptr, 0}));

    std::vector<WebRtcSdpVideoFormat> list(5);
    InitFormat(list[0], "VP8", {{"x", "1"}}, {});
    InitFormat(list[1], "VP9", {{"profile-id", "2"}}, {});
    InitFormat(list[2], "AV1", {{"profile", "1"}, {"tier", "0"}, {"level-idx", "5"}}, {});
    InitFormat(list[3], "H264", {{"profile-level-id", "42e02a"}, {"packetization-mode", "1"}}, {});
    InitFormat(list[4], "red", {}, {});

    const WebRtcSdpVideoFormatView view{list.data(), list.size()};
    Require(_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&vp8Query, view));

    WebRtcSdpVideoFormat h264Match{};
    InitFormat(h264Match, "h264", {{"profile-level-id", "42e01f"}, {"packetization-mode", "1"}}, {});
    Require(_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&h264Match, view));

    WebRtcSdpVideoFormat h264DiffMode{};
    InitFormat(h264DiffMode, "H264", {{"profile-level-id", "42e01f"}}, {});
    Require(!_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&h264DiffMode, view));

    WebRtcSdpVideoFormat h264Baseline{};
    InitFormat(h264Baseline, "H264", {{"profile-level-id", "42001f"}, {"packetization-mode", "1"}}, {});
    Require(!_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&h264Baseline, view));

    WebRtcSdpVideoFormat h264BadHex{};
    InitFormat(h264BadHex, "H264", {{"profile-level-id", "42e01z"}, {"packetization-mode", "1"}}, {});
    Require(!_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&h264BadHex, view));
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&h264BadHex);

    WebRtcSdpVideoFormat h264AllInvalid{};
    InitFormat(h264AllInvalid, "H264", {{"profile-level-id", "zzzzzz"}, {"packetization-mode", "1"}}, {});
    Require(!_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&h264AllInvalid, view));
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&h264AllInvalid);

    WebRtcSdpVideoFormat vp9Match{};
    InitFormat(vp9Match, "VP9", {{"profile-id", "2"}}, {});
    Require(_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&vp9Match, view));

    WebRtcSdpVideoFormat vp9Bad{};
    InitFormat(vp9Bad, "VP9", {{"profile-id", "9"}}, {});
    Require(!_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&vp9Bad, view));

    WebRtcSdpVideoFormat av1DefaultTierMatch{};
    InitFormat(av1DefaultTierMatch, "av1", {{"profile", "1"}}, {});
    Require(_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&av1DefaultTierMatch, view));

    WebRtcSdpVideoFormat av1xNameMismatch{};
    InitFormat(av1xNameMismatch, "AV1X", {{"profile", "1"}}, {});
    Require(!_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&av1xNameMismatch, view));
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&av1xNameMismatch);

    WebRtcSdpVideoFormat av1DiffLevel{};
    InitFormat(av1DiffLevel, "AV1", {{"profile", "1"}, {"level-idx", "8"}}, {});
    Require(!_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&av1DiffLevel, view));

    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&av1DiffLevel);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&av1DefaultTierMatch);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&vp9Bad);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&vp9Match);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&h264Baseline);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&h264DiffMode);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&h264Match);
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&vp8Query);
    for (auto& item : list)
        _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&item);
}

void TestNegativeAndGuardedPointers() {
    GuardedPage guard;
    WebRtcSdpVideoFormat valid{};
    InitFormat(valid, "VP8", {{"a", "b"}}, {1, 2});

    WebRtcSdpVideoFormat scratch{};
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(nullptr, &valid); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, nullptr); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(guard.As<WebRtcSdpVideoFormat>(), &valid); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, guard.As<const WebRtcSdpVideoFormat>()); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(nullptr, &valid); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&scratch, nullptr); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&valid, &valid); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(guard.As<WebRtcSdpVideoFormat>(), &valid); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1EOS0__nid_postfix(&scratch, guard.As<WebRtcSdpVideoFormat>()); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(guard.As<WebRtcSdpVideoFormat>()); });

    RequireInvalidArgument([&]() {
        _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(nullptr, {nullptr, 0});
    });
    RequireInvalidArgument([&]() {
        _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(guard.As<const WebRtcSdpVideoFormat>(), {nullptr, 0});
    });
    RequireInvalidArgument([&]() {
        _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&valid, {nullptr, 1});
    });
    RequireInvalidArgument([&]() {
        _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&valid, {guard.As<const WebRtcSdpVideoFormat>(), 1});
    });
    RequireInvalidArgument([&]() {
        const auto* nearEnd = reinterpret_cast<const WebRtcSdpVideoFormat*>(std::numeric_limits<std::uintptr_t>::max() - 16);
        _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&valid, {nearEnd, 2});
    });

    WebRtcSdpVideoFormat badString{};
    InitFormat(badString, "VP8", {}, {});
    badString.name.pointer = guard.As<char>();
    badString.name.size = 20;
    badString.name.capacity = 31;
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, &badString); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&badString); });

    char dummyBuf[32] = "VP8";
    WebRtcSdpVideoFormat hugeString{};
    InitFormat(hugeString, "VP8", {}, {});
    hugeString.name.pointer = dummyBuf;
    hugeString.name.size = 1ULL << 40;
    hugeString.name.capacity = 1ULL << 40;
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, &hugeString); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&hugeString); });
    RequireInvalidArgument([&]() {
        _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&hugeString, {&valid, 1});
    });

    WebRtcSdpVideoFormat sharedChildMap{};
    InitFormat(sharedChildMap, "VP8", {{"a", "1"}, {"b", "2"}}, {});
    WebRtcVideoStringMapNode* rootNode = sharedChildMap.parameters.root;
    WebRtcVideoStringMapNode* childNode = rootNode->links.left ? rootNode->links.left : rootNode->links.right;
    Require(childNode != nullptr);
    rootNode->links.left = childNode;
    rootNode->links.right = childNode;
    sharedChildMap.parameters.beginNode = childNode;
    sharedChildMap.parameters.size = 3;
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, &sharedChildMap); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&sharedChildMap); });
    RequireInvalidArgument([&]() {
        _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(&sharedChildMap, {&valid, 1});
    });
    if (childNode->links.parent == rootNode) {
        rootNode->links.left = childNode;
        rootNode->links.right = nullptr;
        sharedChildMap.parameters.beginNode = childNode;
    }
    sharedChildMap.parameters.size = 2;
    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&sharedChildMap);

    WebRtcSdpVideoFormat badMap{};
    InitFormat(badMap, "VP8", {}, {});
    badMap.parameters.root = guard.As<WebRtcVideoStringMapNode>();
    badMap.parameters.beginNode = guard.As<WebRtcVideoStringMapNode>();
    badMap.parameters.size = 1;
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, &badMap); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&badMap); });

    WebRtcSdpVideoFormat badModes{};
    InitFormat(badModes, "VP8", {}, {});
    badModes.scalabilityModes.metadata = (40ULL << 1) | 1ULL;
    badModes.scalabilityModes.allocated.data = guard.As<void>();
    badModes.scalabilityModes.allocated.capacity = 40;
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatC1ERKS0__nid_postfix(&scratch, &badModes); });
    RequireInvalidArgument([&]() { _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&badModes); });

    _ZN6webrtc14SdpVideoFormatD1Ev_nid_postfix(&valid);
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestCopyAndDestructor();
    TestMoveConstructor();
    TestIsCodecInList();
    TestNegativeAndGuardedPointers();
    Require(live.empty());
    return 0;
}

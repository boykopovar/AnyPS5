#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/GuestStringStorage.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::size_t allocations = 0;
std::size_t frees = 0;

void* APS5_VABI TestAllocate(std::size_t size) {
    ++allocations;
    return std::malloc(size);
}
void APS5_VABI TestFree(void* pointer) {
    if (pointer)
        ++frees;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc VideoDecoderFactory check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

template<typename TAction> void RequireNotImplemented(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        Require(false, location);
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, location);
}

struct TestFormat {
    WebRtcGuestString name;
    WebRtcStringMap parameters;
    std::uint64_t modesWord;
    union {
        std::uint8_t inlined[34];
        struct {
            std::uint8_t* data;
            std::uint64_t capacity;
        } allocated;
    };
};

static_assert(sizeof(TestFormat) == 112);

using Params = std::vector<std::pair<std::string, std::string>>;

void MakeString(WebRtcGuestString& string, const std::string& text) {
    std::memset(&string, 0, sizeof(string));
    if (text.size() <= WebRtcGuestStringInlineCapacity) {
        std::memcpy(string.buffer, text.data(), text.size());
        string.capacity = WebRtcGuestStringInlineCapacity;
    } else {
        string.capacity = text.size() | WebRtcGuestStringInlineCapacity;
        string.pointer = static_cast<char*>(GuestStringStorage::Allocate(string.capacity + 1));
        std::memcpy(string.pointer, text.c_str(), text.size() + 1);
    }
    string.size = text.size();
}

void FreeString(WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
}

WebRtcStringMapNode* EndNode(TestFormat* format) {
    return reinterpret_cast<WebRtcStringMapNode*>(&format->parameters.root);
}

void MakeFormat(TestFormat* format, const std::string& name, Params params, std::size_t allocatedModes = 0) {
    std::memset(static_cast<void*>(format), 0, sizeof(*format));
    MakeString(format->name, name);
    std::sort(params.begin(), params.end());
    WebRtcStringMapNode* parent = EndNode(format);
    WebRtcStringMapNode** slot = &format->parameters.root;
    format->parameters.beginNode = EndNode(format);
    for (const auto& [key, value] : params) {
        auto* node = static_cast<WebRtcStringMapNode*>(_Znwm_nid_postfix(sizeof(WebRtcStringMapNode)));
        std::memset(static_cast<void*>(node), 0, sizeof(*node));
        node->links.parent = parent;
        MakeString(node->key, key);
        MakeString(node->value, value);
        if (format->parameters.beginNode == EndNode(format))
            format->parameters.beginNode = node;
        *slot = node;
        slot = &node->links.right;
        parent = node;
    }
    format->parameters.size = params.size();
    if (allocatedModes > 0) {
        format->allocated.data = static_cast<std::uint8_t*>(_Znwm_nid_postfix(allocatedModes));
        format->allocated.capacity = allocatedModes;
        format->modesWord = (allocatedModes << 1) | 1;
    } else {
        format->modesWord = 2;
    }
}

void DestroyFormat(TestFormat* format) {
    WebRtcStringMapNode* node = format->parameters.root;
    while (node) {
        WebRtcStringMapNode* next = node->links.right;
        FreeString(node->key);
        FreeString(node->value);
        _ZdlPv_nid_postfix(node);
        node = next;
    }
    FreeString(format->name);
    if (format->modesWord & 1)
        _ZdlPv_nid_postfix(format->allocated.data);
}

const WebRtcSdpVideoFormat* AsFormat(const TestFormat* format) {
    return reinterpret_cast<const WebRtcSdpVideoFormat*>(format);
}

bool InList(const std::string& name, const Params& params, const std::vector<std::pair<std::string, Params>>& list) {
    TestFormat query;
    MakeFormat(&query, name, params);
    std::vector<TestFormat> formats(list.size());
    for (std::size_t i = 0; i < list.size(); ++i)
        MakeFormat(&formats[i], list[i].first, list[i].second);
    const bool result = _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(AsFormat(&query), AsFormat(formats.data()), formats.size());
    for (TestFormat& format : formats)
        DestroyFormat(&format);
    DestroyFormat(&query);
    return result;
}

bool Same(const std::string& name, const Params& left, const Params& right) {
    return InList(name, left, {{name, right}});
}

void TestNamesAndGenericCodecs() {
    Require(InList("VP8", {}, {{"vp8", {{"x", "1"}}}}));
    Require(!InList("VP8", {}, {}));
    Require(!InList("VP8", {}, {{"VP9", {}}, {"VP80", {}}}));
    Require(InList("Generic-Codec-Name", {{"a", "1"}}, {{"VP9", {}}, {"generic-codec-name", {{"b", "2"}}}}));
    Require(InList("VP8", {}, {{"VP9", {{"profile-id", "2"}}}, {"VP8", {}}}));
}

void TestH264() {
    Require(Same("H264", {}, {{"profile-level-id", "42e01f"}}));
    Require(Same("h264", {{"profile-level-id", "4d0032"}}, {{"profile-level-id", "4D0028"}}));
    Require(!Same("H264", {}, {{"profile-level-id", "42001f"}}));
    Require(Same("H264", {{"profile-level-id", "640c1f"}}, {{"profile-level-id", "640c34"}}));
    Require(!Same("H264", {{"profile-level-id", "640c1f"}}, {{"profile-level-id", "64001f"}}));
    Require(!Same("H264", {{"profile-level-id", "42e0ff"}}, {{"profile-level-id", "42e0ff"}}));
    Require(!Same("H264", {{"profile-level-id", "42e01f0"}}, {{"profile-level-id", "42e01f0"}}));
    Require(!Same("H264", {{"profile-level-id", "000000"}}, {{"profile-level-id", "000000"}}));
    Require(!Same("H264", {{"profile-level-id", " 42e01"}}, {}));
    Require(Same("H264", {{"profile-level-id", std::string("42e01f\0zz", 9)}}, {}));
    Require(Same("H264", {{"profile-level-id", "f4001f"}}, {{"profile-level-id", "F4000A"}}));
    Require(Same("H264", {{"packetization-mode", "0"}}, {}));
    Require(!Same("H264", {{"packetization-mode", "1"}}, {}));
    Require(Same("H264", {{"packetization-mode", "1"}}, {{"packetization-mode", "1"}}));
}

void TestVp9() {
    Require(Same("VP9", {{"profile-id", "2"}}, {{"profile-id", "2"}}));
    Require(!Same("VP9", {{"profile-id", "2"}}, {}));
    Require(Same("VP9", {{"profile-id", "-0"}}, {}));
    Require(Same("VP9", {{"profile-id", "00"}}, {{"profile-id", "0"}}));
    Require(!Same("VP9", {{"profile-id", "4"}}, {{"profile-id", "4"}}));
    Require(!Same("VP9", {{"profile-id", "+1"}}, {{"profile-id", "+1"}}));
    Require(!Same("VP9", {{"profile-id", "1 "}}, {{"profile-id", "1 "}}));
    Require(!Same("VP9", {{"profile-id", "99999999999999999999"}}, {{"profile-id", "99999999999999999999"}}));
    Require(Same("VP9", {{"other", "3"}}, {}));
}

void TestAv1() {
    Require(Same("AV1", {}, {{"profile", "0"}, {"tier", "0"}, {"level-idx", "5"}}));
    Require(Same("AV1X", {{"profile", "1"}}, {{"profile", "1"}}));
    Require(!Same("AV1", {{"profile", "3"}}, {{"profile", "3"}}));
    Require(!Same("AV1", {{"tier", "1"}}, {}));
    Require(!Same("AV1", {{"level-idx", "6"}}, {}));
    Require(!Same("AV1", {{"tier", "00"}}, {}));
}

void TestH265Throws() {
    Require(!InList("H265", {}, {{"VP8", {}}}));
    RequireNotImplemented([] { InList("H265", {}, {{"h265", {}}}); });
}

void TestInvalidListArguments() {
    TestFormat query;
    MakeFormat(&query, "VP8", {});
    RequireInvalidArgument([&] { _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(AsFormat(&query), nullptr, 1); });
    RequireInvalidArgument([&] { _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(nullptr, AsFormat(&query), 1); });
    Require(!_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(AsFormat(&query), nullptr, 0));
    TestFormat broken = query;
    broken.name.capacity = 3;
    RequireInvalidArgument([&] { _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(AsFormat(&query), AsFormat(&broken), 1); });
    TestFormat withParams;
    MakeFormat(&withParams, "VP9", {{"a", "1"}, {"b", "2"}});
    const std::uint64_t size = withParams.parameters.size;
    withParams.parameters.size = 1;
    RequireInvalidArgument([&] { _ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(AsFormat(&query), AsFormat(&withParams), 1); });
    withParams.parameters.size = size;
    Require(!_ZNK6webrtc14SdpVideoFormat13IsCodecInListEN3rtc9ArrayViewIKS0_Lln4711EEE_nid_postfix(AsFormat(&query), AsFormat(&withParams), 1));
    DestroyFormat(&withParams);
    DestroyFormat(&query);
}

struct FakeFactory {
    const void* const* vtable;
    std::vector<std::pair<std::string, Params>> formats;
    std::size_t calls;
    const FakeFactory* self;
};

struct FakeVector {
    TestFormat* begin;
    TestFormat* end;
    TestFormat* capacityEnd;
};

void APS5_VABI GetSupportedFormats(FakeVector* result, const WebRtcVideoDecoderFactory* factory) {
    auto* fake = reinterpret_cast<FakeFactory*>(const_cast<WebRtcVideoDecoderFactory*>(factory));
    ++fake->calls;
    fake->self = fake;
    std::memset(result, 0, sizeof(*result));
    if (fake->formats.empty())
        return;
    const std::size_t capacity = fake->formats.size() + 1;
    auto* storage = static_cast<TestFormat*>(_Znwm_nid_postfix(capacity * sizeof(TestFormat)));
    for (std::size_t i = 0; i < fake->formats.size(); ++i)
        MakeFormat(storage + i, fake->formats[i].first, fake->formats[i].second, i % 2 ? 40 : 0);
    result->begin = storage;
    result->end = storage + fake->formats.size();
    result->capacityEnd = storage + capacity;
}

std::array<const void*, 5> FactoryVtable() {
    std::array<const void*, 5> vtable{};
    vtable[2] = reinterpret_cast<const void*>(&GetSupportedFormats);
    return vtable;
}

WebRtcVideoDecoderFactoryCodecSupport Query(FakeFactory& factory, const std::string& name, const Params& params, bool referenceScaling) {
    TestFormat query;
    MakeFormat(&query, name, params);
    const std::size_t allocatedBefore = allocations;
    const std::size_t freedBefore = frees;
    WebRtcVideoDecoderFactoryCodecSupport support{true, true};
    try {
        support = _ZNK6webrtc19VideoDecoderFactory17QueryCodecSupportERKNS_14SdpVideoFormatEb_nid_postfix(
            reinterpret_cast<const WebRtcVideoDecoderFactory*>(&factory), AsFormat(&query), referenceScaling);
    } catch (...) {
        Require(allocations - allocatedBefore == frees - freedBefore);
        DestroyFormat(&query);
        throw;
    }
    Require(allocations - allocatedBefore == frees - freedBefore);
    DestroyFormat(&query);
    return support;
}

void TestQueryCodecSupport() {
    const auto vtable = FactoryVtable();
    const std::string longValue(5000, 'v');
    FakeFactory factory{vtable.data(),
        {{"VP8", {}}, {"VP9", {{"profile-id", "2"}, {"a-very-long-parameter-name", longValue}}}, {"H264", {{"packetization-mode", "1"}}}},
        0, nullptr};
    auto support = Query(factory, "vp9", {{"profile-id", "2"}}, false);
    Require(support.isSupported && !support.isPowerEfficient);
    Require(factory.calls == 1 && factory.self == &factory);
    support = Query(factory, "VP9", {}, false);
    Require(!support.isSupported && !support.isPowerEfficient);
    support = Query(factory, "H264", {{"packetization-mode", "1"}, {"profile-level-id", "42e01f"}}, false);
    Require(support.isSupported);
    support = Query(factory, "H264", {}, false);
    Require(!support.isSupported);
    support = Query(factory, "VP8", {}, true);
    Require(!support.isSupported && !support.isPowerEfficient);
    Require(factory.calls == 4);
    support = Query(factory, "AV1", {}, false);
    Require(!support.isSupported);
    factory.formats.clear();
    support = Query(factory, "VP8", {}, false);
    Require(!support.isSupported);
}

void TestQueryH265ReleasesFormats() {
    const auto vtable = FactoryVtable();
    FakeFactory factory{vtable.data(), {{"VP8", {{"k", std::string(40, 'k')}}}, {"H265", {}}}, 0, nullptr};
    RequireNotImplemented([&] { Query(factory, "H265", {}, false); });
    Require(Query(factory, "VP8", {}, false).isSupported);
}

void TestQueryInvalidArguments() {
    auto vtable = FactoryVtable();
    FakeFactory factory{vtable.data(), {{"VP8", {}}}, 0, nullptr};
    TestFormat query;
    MakeFormat(&query, "VP8", {});
    RequireInvalidArgument([&] { _ZNK6webrtc19VideoDecoderFactory17QueryCodecSupportERKNS_14SdpVideoFormatEb_nid_postfix(nullptr, AsFormat(&query), false); });
    RequireInvalidArgument([&] {
        _ZNK6webrtc19VideoDecoderFactory17QueryCodecSupportERKNS_14SdpVideoFormatEb_nid_postfix(reinterpret_cast<const WebRtcVideoDecoderFactory*>(&factory), nullptr, false);
    });
    vtable[2] = nullptr;
    RequireInvalidArgument([&] {
        _ZNK6webrtc19VideoDecoderFactory17QueryCodecSupportERKNS_14SdpVideoFormatEb_nid_postfix(reinterpret_cast<const WebRtcVideoDecoderFactory*>(&factory), AsFormat(&query), false);
    });
    FakeFactory empty{nullptr, {}, 0, nullptr};
    RequireInvalidArgument([&] {
        _ZNK6webrtc19VideoDecoderFactory17QueryCodecSupportERKNS_14SdpVideoFormatEb_nid_postfix(reinterpret_cast<const WebRtcVideoDecoderFactory*>(&empty), AsFormat(&query), false);
    });
    Require(factory.calls == 0);
    DestroyFormat(&query);
}

}

int main(int argc, char** argv) {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "is_codec_in_list") {
        TestNamesAndGenericCodecs();
        TestH264();
        TestVp9();
        TestAv1();
        TestH265Throws();
        TestInvalidListArguments();
        return 0;
    }
    if (mode == "query_codec_support") {
        TestQueryCodecSupport();
        TestQueryH265ReleasesFormats();
        TestQueryInvalidArguments();
        return 0;
    }
    return 1;
}

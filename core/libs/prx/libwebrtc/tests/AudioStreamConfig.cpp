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

void* APS5_VABI TestAllocate(std::size_t size) {
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
    std::fprintf(stderr, "libwebrtc AudioStreamConfig check failed at line %u\n", location.line());
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

struct FakeReference {
    const void* const* vtable;
    char name;
};

std::string releases;

void APS5_VABI FakeAddRef(const FakeReference*) { std::abort(); }

std::int32_t APS5_VABI FakeRelease(const FakeReference* object) {
    releases.push_back(object->name);
    return 1;
}

const void* const FakeVtable[] = {
    reinterpret_cast<const void*>(&FakeAddRef),
    reinterpret_cast<const void*>(&FakeRelease),
};

const void* const NoReleaseVtable[] = {
    reinterpret_cast<const void*>(&FakeAddRef),
    nullptr,
};

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
    const std::size_t capacity = text.size() | WebRtcGuestStringInlineCapacity;
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    std::memcpy(data, text.data(), text.size());
    data[text.size()] = '\0';
    target.pointer = data;
    target.capacity = capacity;
}

std::string ReadGuestString(const WebRtcGuestString& value) {
    return std::string(WebRtcGuestStringData(value), value.size);
}

WebRtcStringMapNode* EndNode(WebRtcStringMap& map) {
    return reinterpret_cast<WebRtcStringMapNode*>(&map.root);
}

WebRtcAudioDecoderMapNode* EndNode(WebRtcAudioDecoderMap& map) {
    return reinterpret_cast<WebRtcAudioDecoderMapNode*>(&map.root);
}

void InitFormat(WebRtcSdpAudioFormat& format, std::string_view name, int clockrateHz, std::uint64_t numChannels) {
    std::memset(&format, 0, sizeof(format));
    InitGuestString(format.name, name);
    format.clockrateHz = clockrateHz;
    format.numChannels = numChannels;
    format.parameters.beginNode = EndNode(format.parameters);
}

void AddParameter(WebRtcSdpAudioFormat& format, std::string_view key, std::string_view value) {
    auto* node = new (_Znwm_nid_postfix(sizeof(WebRtcStringMapNode))) WebRtcStringMapNode{};
    node->links.isBlack = true;
    node->links.parent = EndNode(format.parameters);
    InitGuestString(node->key, key);
    InitGuestString(node->value, value);
    format.parameters.root = node;
    format.parameters.beginNode = node;
    format.parameters.size = 1;
}

void InitExtensions(WebRtcRtpExtensionVector& extensions) {
    auto* storage = static_cast<WebRtcRtpExtension*>(_Znwm_nid_postfix(3 * sizeof(WebRtcRtpExtension)));
    std::memset(storage, 0, 3 * sizeof(WebRtcRtpExtension));
    InitGuestString(storage[0].uri, "urn:ietf:params:rtp-hdrext:ssrc-audio-level");
    storage[0].id = 1;
    InitGuestString(storage[1].uri, "urn:ietf:mid");
    storage[1].id = 2;
    extensions.begin = storage;
    extensions.end = storage + 2;
    extensions.capacityEnd = storage + 3;
}

void TestSendCodecSpec() {
    WebRtcSdpAudioFormat format;
    InitFormat(format, "multiopus-codec-name", 48000, 6);
    AddParameter(format, "channel_mapping", "0,4,1,2,3,5");

    WebRtcAudioSendCodecSpec spec;
    std::memset(&spec, 0xcd, sizeof(spec));
    errno = 31;
    _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(&spec, 111, &format);
    Require(errno == 31);
    Require(spec.payloadType == 111);
    Require(ReadGuestString(spec.format.name) == "multiopus-codec-name");
    Require(spec.format.name.pointer != format.name.pointer);
    Require(spec.format.clockrateHz == 48000 && spec.format.numChannels == 6);
    Require(spec.format.parameters.size == 1 && spec.format.parameters.root != format.parameters.root);
    Require(spec.format.parameters.root->links.parent == EndNode(spec.format.parameters));
    Require(ReadGuestString(spec.format.parameters.root->value) == "0,4,1,2,3,5");
    for (bool flag : spec.flags)
        Require(!flag);
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&spec);
    for (std::size_t offset : {92u, 96u, 100u, 104u, 108u, 112u})
        Require(bytes[offset] == 0);
    Require(bytes[93] == 0xcd && bytes[116] == 0xcd);
    Require(ReadGuestString(format.name) == "multiopus-codec-name" && format.parameters.size == 1);

    _ZN6webrtc15AudioSendStream6Config13SendCodecSpecD1Ev_nid_postfix(&spec);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&format);
    Require(live.empty());

    ReadOnlyPage readOnly;
    InitFormat(format, "opus", 48000, 2);
    const auto liveBaseline = live;
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(nullptr, 111, &format); }));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(
            static_cast<WebRtcAudioSendCodecSpec*>(readOnly.address), 111, &format);
    }));
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(&spec, 111, nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream6Config13SendCodecSpecD1Ev_nid_postfix(nullptr); }));
    Require(live == liveBaseline);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&format);
    Require(live.empty());
}

void TestSendStats() {
    WebRtcAudioSendStreamStats stats;
    std::memset(&stats, 0, sizeof(stats));
    InitGuestString(stats.codecName, std::string(5000, 'o'));
    stats.reportBlockDatas.begin = static_cast<unsigned char*>(_Znwm_nid_postfix(256));
    stats.reportBlockDatas.end = stats.reportBlockDatas.begin + 128;
    stats.reportBlockDatas.capacityEnd = stats.reportBlockDatas.begin + 256;
    errno = 32;
    _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(&stats);
    Require(errno == 32);
    Require(live.empty());

    std::memset(&stats, 0, sizeof(stats));
    InitGuestString(stats.codecName, "opus");
    _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(&stats);
    Require(live.empty());

    InitGuestString(stats.codecName, "a-heap-allocated-codec-name");
    stats.reportBlockDatas.begin = static_cast<unsigned char*>(_Znwm_nid_postfix(64));
    stats.reportBlockDatas.end = stats.reportBlockDatas.begin + 72;
    stats.reportBlockDatas.capacityEnd = stats.reportBlockDatas.begin + 64;
    const auto liveBaseline = live;
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(&stats); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(nullptr); }));
    Require(live == liveBaseline);
    stats.reportBlockDatas.end = stats.reportBlockDatas.begin;
    _ZN6webrtc15AudioSendStream5StatsD1Ev_nid_postfix(&stats);
    Require(live.empty());
}

void InitSendConfig(WebRtcAudioSendStreamConfig& config) {
    std::memset(&config, 0, sizeof(config));
    InitGuestString(config.rtp.rid, "");
    InitGuestString(config.rtp.mid, "");
    InitGuestString(config.rtp.cName, "");
    InitGuestString(config.audioNetworkAdaptorConfig.value, "");
    InitGuestString(config.trackId, "");
}

void TestSendConfig() {
    WebRtcAudioSendStreamConfig config;
    InitSendConfig(config);
    releases.clear();
    _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(&config);
    Require(releases.empty() && live.empty());

    FakeReference factory{FakeVtable, 'f'};
    FakeReference encryptor{FakeVtable, 'e'};
    FakeReference transformer{FakeVtable, 't'};
    InitSendConfig(config);
    InitGuestString(config.rtp.rid, "a-long-rid-value-on-the-heap");
    InitGuestString(config.rtp.mid, "audio-mid-heap-value");
    InitGuestString(config.rtp.cName, std::string(4100, 'c'));
    InitExtensions(config.rtp.extensions);
    InitGuestString(config.audioNetworkAdaptorConfig.value, "serialized-adaptor-config-proto");
    config.audioNetworkAdaptorConfig.engaged = true;
    WebRtcSdpAudioFormat format;
    InitFormat(format, "opus-with-a-long-name", 48000, 2);
    AddParameter(format, "useinbandfec", "1");
    _ZN6webrtc15AudioSendStream6Config13SendCodecSpecC1EiRKNS_14SdpAudioFormatE_nid_postfix(&config.sendCodecSpec.value, 111, &format);
    _ZN6webrtc14SdpAudioFormatD1Ev_nid_postfix(&format);
    config.sendCodecSpec.engaged = true;
    config.encoderFactory = &factory;
    InitGuestString(config.trackId, "microphone-track-identifier");
    config.frameEncryptor = &encryptor;
    config.frameTransformer = &transformer;
    errno = 33;
    _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(&config);
    Require(errno == 33);
    Require(releases == "tef");
    Require(live.empty());

    InitSendConfig(config);
    InitGuestString(config.audioNetworkAdaptorConfig.value, "stale-adaptor-config-not-engaged");
    const void* stale = config.audioNetworkAdaptorConfig.value.pointer;
    config.sendCodecSpec.value.format.name.capacity = 3;
    _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(&config);
    Require(live.size() == 1 && live.count(const_cast<void*>(stale)) == 1);
    _ZdlPv_nid_postfix(const_cast<void*>(stale));
}

void TestSendConfigInvalid() {
    FakeReference factory{FakeVtable, 'f'};
    FakeReference broken{NoReleaseVtable, 'b'};
    FakeReference empty{nullptr, 'n'};
    WebRtcAudioSendStreamConfig config;
    InitSendConfig(config);
    InitGuestString(config.rtp.rid, "a-long-rid-value-on-the-heap");
    InitExtensions(config.rtp.extensions);
    config.encoderFactory = &factory;
    const auto liveBaseline = live;
    releases.clear();

    ReadOnlyPage readOnly;
    errno = 34;
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(static_cast<WebRtcAudioSendStreamConfig*>(readOnly.address)); }));
    config.frameTransformer = &broken;
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(&config); }));
    config.frameTransformer = &empty;
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(&config); }));
    config.frameTransformer = nullptr;
    config.rtp.extensions.end = config.rtp.extensions.begin + 4;
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(&config); }));
    config.rtp.extensions.end = config.rtp.extensions.begin + 2;
    config.rtp.extensions.begin[1].uri.capacity = 7;
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(&config); }));
    config.rtp.extensions.begin[1].uri.capacity = WebRtcGuestStringInlineCapacity;
    config.sendCodecSpec.engaged = true;
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(&config); }));
    config.sendCodecSpec.engaged = false;
    config.trackId.size = 16;
    Require(ThrowsInvalid([&] { _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(&config); }));
    config.trackId.size = 0;
    Require(errno == 34);
    Require(releases.empty());
    Require(live == liveBaseline);

    _ZN6webrtc15AudioSendStream6ConfigD1Ev_nid_postfix(&config);
    Require(releases == "f");
    Require(live.empty());
}

void TestReceiveStats() {
    WebRtcAudioReceiveStreamStatsPrefix stats;
    std::memset(&stats, 0, sizeof(stats));
    InitGuestString(stats.codecName, "telephone-event-codec");
    errno = 35;
    _ZN6webrtc27AudioReceiveStreamInterface5StatsD1Ev_nid_postfix(&stats);
    Require(errno == 35);
    Require(live.empty());

    InitGuestString(stats.codecName, "PCMU");
    _ZN6webrtc27AudioReceiveStreamInterface5StatsD1Ev_nid_postfix(&stats);
    stats.codecName.size = 20;
    Require(ThrowsInvalid([&] { _ZN6webrtc27AudioReceiveStreamInterface5StatsD1Ev_nid_postfix(&stats); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc27AudioReceiveStreamInterface5StatsD1Ev_nid_postfix(nullptr); }));
    Require(live.empty());
}

WebRtcAudioDecoderMapNode* MakeDecoderNode(std::int32_t payloadType, std::string_view name, int clockrateHz, bool withParameter) {
    auto* node = new (_Znwm_nid_postfix(sizeof(WebRtcAudioDecoderMapNode))) WebRtcAudioDecoderMapNode{};
    node->payloadType = payloadType;
    InitFormat(node->format, name, clockrateHz, 1);
    if (withParameter)
        AddParameter(node->format, "maxaveragebitrate", "a-heap-parameter-value-40000");
    return node;
}

void InitReceiveConfig(WebRtcAudioReceiveStreamConfig& config) {
    std::memset(&config, 0, sizeof(config));
    InitGuestString(config.syncGroup, "");
    config.decoderMap.beginNode = EndNode(config.decoderMap);
}

void AttachDecoders(WebRtcAudioReceiveStreamConfig& config) {
    auto* root = MakeDecoderNode(9, "G722", 8000, false);
    auto* left = MakeDecoderNode(0, "PCMU", 8000, false);
    auto* right = MakeDecoderNode(111, "opus-long-format-name", 48000, true);
    root->links.isBlack = true;
    root->links.left = left;
    root->links.right = right;
    root->links.parent = EndNode(config.decoderMap);
    left->links.parent = root;
    right->links.parent = root;
    config.decoderMap.root = root;
    config.decoderMap.beginNode = left;
    config.decoderMap.size = 3;
}

void TestReceiveConfig() {
    WebRtcAudioReceiveStreamConfig config;
    InitReceiveConfig(config);
    releases.clear();
    _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(&config);
    Require(releases.empty() && live.empty());

    FakeReference factory{FakeVtable, 'f'};
    FakeReference decryptor{FakeVtable, 'd'};
    FakeReference transformer{FakeVtable, 't'};
    InitReceiveConfig(config);
    InitGuestString(config.syncGroup, "a-sync-group-on-the-heap");
    AttachDecoders(config);
    config.decoderFactory = &factory;
    config.frameDecryptor = &decryptor;
    config.frameTransformer = &transformer;
    errno = 36;
    _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(&config);
    Require(errno == 36);
    Require(releases == "tdf");
    Require(live.empty());
}

void TestReceiveConfigInvalid() {
    FakeReference broken{NoReleaseVtable, 'b'};
    WebRtcAudioReceiveStreamConfig config;
    InitReceiveConfig(config);
    InitGuestString(config.syncGroup, "a-sync-group-on-the-heap");
    AttachDecoders(config);
    const auto liveBaseline = live;
    releases.clear();

    ReadOnlyPage readOnly;
    errno = 37;
    Require(ThrowsInvalid([&] { _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(nullptr); }));
    Require(ThrowsInvalid([&] { _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(static_cast<WebRtcAudioReceiveStreamConfig*>(readOnly.address)); }));
    config.decoderMap.size = 2;
    Require(ThrowsInvalid([&] { _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(&config); }));
    config.decoderMap.size = 4;
    Require(ThrowsInvalid([&] { _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(&config); }));
    config.decoderMap.size = 3;
    auto* right = config.decoderMap.root->links.right;
    right->links.parent = right;
    Require(ThrowsInvalid([&] { _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(&config); }));
    right->links.parent = config.decoderMap.root;
    config.decoderMap.beginNode = right;
    Require(ThrowsInvalid([&] { _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(&config); }));
    config.decoderMap.beginNode = config.decoderMap.root->links.left;
    right->format.parameters.size = 2;
    Require(ThrowsInvalid([&] { _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(&config); }));
    right->format.parameters.size = 1;
    config.frameDecryptor = &broken;
    Require(ThrowsInvalid([&] { _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(&config); }));
    config.frameDecryptor = nullptr;
    Require(errno == 37);
    Require(releases.empty());
    Require(live == liveBaseline);

    _ZN6webrtc27AudioReceiveStreamInterface6ConfigD1Ev_nid_postfix(&config);
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
    TestSendCodecSpec();
    TestSendStats();
    TestSendConfig();
    TestSendConfigInvalid();
    TestReceiveStats();
    TestReceiveConfig();
    TestReceiveConfigInvalid();
    std::puts("libwebrtc AudioStreamConfig tests passed");
    return 0;
}

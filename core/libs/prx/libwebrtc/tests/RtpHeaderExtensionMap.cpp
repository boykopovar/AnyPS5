#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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
    std::fprintf(stderr, "libwebrtc RtpHeaderExtensionMap check failed at line %u\n", location.line());
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

WebRtcRtpExtension MakeExtension(std::string_view uri, std::int32_t id, bool encrypt = false) {
    WebRtcRtpExtension extension{};
    InitGuestString(extension.uri, uri);
    extension.id = id;
    extension.encrypt = encrypt;
    return extension;
}

void DestroyExtensions(std::vector<WebRtcRtpExtension>& extensions) {
    for (WebRtcRtpExtension& extension : extensions)
        _ZN6webrtc12RtpExtensionD1Ev_nid_postfix(&extension);
    extensions.clear();
}

void TestEmptyAndAllKnownExtensions() {
    WebRtcRtpHeaderExtensionMap map;
    std::memset(&map, 0xAA, sizeof(map));
    errno = 51;
    _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(&map, WebRtcRtpExtensionView{nullptr, 0});
    Require(errno == 51);
    for (std::uint8_t id : map.ids)
        Require(id == 0);
    Require(!map.extmapAllowMixed);

    const std::array<const char*, 21> uris = {
        "urn:ietf:params:rtp-hdrext:toffset",
        "urn:ietf:params:rtp-hdrext:ssrc-audio-level",
        "urn:ietf:params:rtp-hdrext:csrc-audio-level",
        "http://www.webrtc.org/experiments/rtp-hdrext/inband-cn",
        "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
        "http://www.webrtc.org/experiments/rtp-hdrext/abs-capture-time",
        "urn:3gpp:video-orientation",
        "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01",
        "http://www.webrtc.org/experiments/rtp-hdrext/transport-wide-cc-02",
        "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay",
        "http://www.webrtc.org/experiments/rtp-hdrext/video-content-type",
        "http://www.webrtc.org/experiments/rtp-hdrext/video-layers-allocation00",
        "http://www.webrtc.org/experiments/rtp-hdrext/video-timing",
        "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id",
        "urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id",
        "urn:ietf:params:rtp-hdrext:sdes:mid",
        "http://www.webrtc.org/experiments/rtp-hdrext/generic-frame-descriptor-00",
        "https://aomediacodec.github.io/av1-rtp-spec/#dependency-descriptor-rtp-header-extension",
        "http://www.webrtc.org/experiments/rtp-hdrext/color-space",
        "http://www.webrtc.org/experiments/rtp-hdrext/video-frame-tracking-id",
        "http://www.webrtc.org/experiments/rtp-hdrext/corruption-detection",
    };

    std::vector<WebRtcRtpExtension> extensions;
    for (std::size_t i = 0; i < uris.size(); ++i)
        extensions.push_back(MakeExtension(uris[i], static_cast<std::int32_t>(i + 1)));

    std::memset(&map, 0x55, sizeof(map));
    errno = 52;
    _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
        &map, WebRtcRtpExtensionView{extensions.data(), extensions.size()});
    Require(errno == 52);
    Require(map.ids[0] == 0);
    for (std::size_t i = 0; i < uris.size(); ++i)
        Require(map.ids[i + 1] == static_cast<std::uint8_t>(i + 1));
    Require(!map.extmapAllowMixed);

    DestroyExtensions(extensions);
    Require(live.empty());
}

void TestFilteringAndConflicts() {
    std::vector<WebRtcRtpExtension> extensions;
    extensions.push_back(MakeExtension("unknown-inline", 1));
    extensions.push_back(MakeExtension(std::string(5000, 'u'), 2));
    extensions.push_back(MakeExtension("urn:3gpp:video-orientation", 0));
    extensions.push_back(MakeExtension("urn:3gpp:video-orientation", -5));
    extensions.push_back(MakeExtension("urn:3gpp:video-orientation", 256));
    extensions.push_back(MakeExtension("urn:3gpp:video-orientation", INT_MIN));
    extensions.push_back(MakeExtension("urn:3gpp:video-orientation", INT_MAX));
    extensions.push_back(MakeExtension("urn:3gpp:video-orientation", 1));
    extensions.push_back(MakeExtension("urn:3gpp:video-orientation", 1));
    extensions.push_back(MakeExtension("urn:3gpp:video-orientation", 9));
    extensions.push_back(MakeExtension("urn:ietf:params:rtp-hdrext:sdes:mid", 1));
    extensions.push_back(MakeExtension("urn:ietf:params:rtp-hdrext:sdes:mid", 255));

    WebRtcRtpHeaderExtensionMap map;
    std::memset(&map, 0xCC, sizeof(map));
    _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
        &map, WebRtcRtpExtensionView{extensions.data(), extensions.size()});
    Require(map.ids[7] == 1);
    Require(map.ids[16] == 255);
    for (std::size_t type = 0; type < WebRtcRtpExtensionNumberOfExtensions; ++type) {
        if (type != 7 && type != 16)
            Require(map.ids[type] == 0);
    }
    Require(!map.extmapAllowMixed);

    DestroyExtensions(extensions);
    Require(live.empty());
}

void TestInvalidGuestInputs() {
    WebRtcRtpHeaderExtensionMap map;
    std::memset(&map, 0x77, sizeof(map));
    const WebRtcRtpHeaderExtensionMap preserved = map;
    ReadOnlyPage readOnly;

    errno = 53;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
            nullptr, WebRtcRtpExtensionView{nullptr, 0});
    }));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
            reinterpret_cast<WebRtcRtpHeaderExtensionMap*>(static_cast<std::uintptr_t>(0x10)),
            WebRtcRtpExtensionView{nullptr, 0});
    }));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
            static_cast<WebRtcRtpHeaderExtensionMap*>(readOnly.address),
            WebRtcRtpExtensionView{nullptr, 0});
    }));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
            &map, WebRtcRtpExtensionView{nullptr, 1});
    }));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
            &map, WebRtcRtpExtensionView{reinterpret_cast<const WebRtcRtpExtension*>(static_cast<std::uintptr_t>(0x10)), 1});
    }));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
            &map, WebRtcRtpExtensionView{reinterpret_cast<const WebRtcRtpExtension*>(&map),
                                         std::numeric_limits<std::size_t>::max() / sizeof(WebRtcRtpExtension) + 1});
    }));

    WebRtcRtpExtension bad{};
    bad.id = 1;
    bad.uri.capacity = 4;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
            &map, WebRtcRtpExtensionView{&bad, 1});
    }));
    bad.uri.capacity = WebRtcGuestStringInlineCapacity;
    bad.uri.size = WebRtcGuestStringInlineCapacity + 1;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
            &map, WebRtcRtpExtensionView{&bad, 1});
    }));
    bad.uri.capacity = 31;
    bad.uri.size = 20;
    bad.uri.pointer = nullptr;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
            &map, WebRtcRtpExtensionView{&bad, 1});
    }));
    bad.uri.pointer = reinterpret_cast<char*>(static_cast<std::uintptr_t>(0x10));
    Require(ThrowsInvalid([&] {
        _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
            &map, WebRtcRtpExtensionView{&bad, 1});
    }));
    bad.uri.capacity = 8191;
    bad.uri.size = 5000;
    Require(ThrowsInvalid([&] {
        _ZN6webrtc21RtpHeaderExtensionMapC1EN3rtc9ArrayViewIKNS_12RtpExtensionELln4711EEE_nid_postfix(
            &map, WebRtcRtpExtensionView{&bad, 1});
    }));
    Require(errno == 53);
    Require(std::memcmp(&map, &preserved, sizeof(map)) == 0);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestEmptyAndAllKnownExtensions();
    TestFilteringAndConflicts();
    TestInvalidGuestInputs();
    std::puts("libwebrtc RtpHeaderExtensionMap tests passed");
    return 0;
}

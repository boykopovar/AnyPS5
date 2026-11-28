#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

std::set<void*> live;
int allocations = 0;
int failAfter = -1;

void* APS5_VABI TestAllocate(std::size_t size) {
    if (failAfter == 0)
        return nullptr;
    if (failAfter > 0)
        --failAfter;
    void* pointer = std::malloc(size ? size : 1);
    live.insert(pointer);
    ++allocations;
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
    if (pointer)
        std::memset(pointer, 0, count * size);
    return pointer;
}

void* APS5_VABI TestReallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return TestAllocate(size); }
void* APS5_VABI TestRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = TestAllocate(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc IceServer check failed at line %u\n", location.line());
    std::abort();
}

std::string Text(const WebRtcGuestString& string) {
    return std::string(WebRtcGuestStringData(string), string.size);
}

void RequireEmptyString(const WebRtcGuestString& string) {
    Require(string.reserved == 0 && string.size == 0 && string.capacity == 15 && string.buffer[0] == '\0');
}

void RequireEmptyVector(const WebRtcGuestStringVector& vector) {
    Require(!vector.begin && !vector.end && !vector.capacityEnd);
}

void SetString(WebRtcGuestString& string, const std::string& text) {
    std::memset(&string, 0, sizeof(string));
    if (text.size() <= 15) {
        std::memcpy(string.buffer, text.c_str(), text.size() + 1);
        string.capacity = 15;
    } else {
        string.capacity = text.size() + 7;
        string.pointer = static_cast<char*>(GuestStringStorage::Allocate(string.capacity + 1));
        std::memcpy(string.pointer, text.c_str(), text.size() + 1);
    }
    string.size = text.size();
}

void SetVector(WebRtcGuestStringVector& vector, const std::initializer_list<std::string>& texts, std::size_t spare) {
    const std::size_t capacity = texts.size() + spare;
    vector.begin = static_cast<WebRtcGuestString*>(GuestStringStorage::Allocate(capacity * sizeof(WebRtcGuestString)));
    vector.end = vector.begin;
    vector.capacityEnd = vector.begin + capacity;
    for (const std::string& text : texts)
        SetString(*vector.end++, text);
}

const std::string LongUri = "turns:turn.example.org:5349?transport=tcp";
const std::string LongName = "a-hostname-that-is-longer-than-fifteen.example";

void Fill(WebRtcIceServer& server) {
    _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(&server);
    SetString(server.uri, "stun:a.org");
    SetVector(server.urls, {"stun:a.org", LongUri, ""}, 2);
    SetString(server.username, "user");
    SetString(server.password, "a password longer than 15");
    server.tlsCertPolicy = 1;
    SetString(server.hostname, LongName);
    SetVector(server.tlsAlpnProtocols, {"h2"}, 0);
}

void TestDefault() {
    WebRtcIceServer server;
    std::memset(&server, 0xcd, sizeof(server));
    const int before = allocations;
    _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(&server);
    Require(allocations == before);
    RequireEmptyString(server.uri);
    RequireEmptyVector(server.urls);
    RequireEmptyString(server.username);
    RequireEmptyString(server.password);
    Require(server.tlsCertPolicy == 0);
    RequireEmptyString(server.hostname);
    RequireEmptyVector(server.tlsAlpnProtocols);
    RequireEmptyVector(server.tlsEllipticCurves);
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&server);
    Require(live.empty());
}

void TestCopyDefault() {
    WebRtcIceServer source;
    _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(&source);
    WebRtcIceServer copy;
    std::memset(&copy, 0xcd, sizeof(copy));
    const int before = allocations;
    _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(&copy, &source);
    Require(allocations == before);
    RequireEmptyString(copy.uri);
    RequireEmptyVector(copy.urls);
    RequireEmptyString(copy.username);
    RequireEmptyString(copy.password);
    Require(copy.tlsCertPolicy == 0);
    RequireEmptyString(copy.hostname);
    RequireEmptyVector(copy.tlsAlpnProtocols);
    RequireEmptyVector(copy.tlsEllipticCurves);
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&copy);
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&source);
    Require(live.empty());
}

void TestCopy() {
    WebRtcIceServer source;
    Fill(source);
    WebRtcIceServer copy;
    std::memset(&copy, 0xcd, sizeof(copy));
    _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(&copy, &source);
    Require(Text(copy.uri) == "stun:a.org" && copy.uri.capacity == 15 && copy.uri.reserved == 0);
    Require(copy.urls.end - copy.urls.begin == 3 && copy.urls.capacityEnd == copy.urls.end);
    Require(copy.urls.begin != source.urls.begin);
    Require(Text(copy.urls.begin[0]) == "stun:a.org");
    Require(Text(copy.urls.begin[1]) == LongUri && copy.urls.begin[1].capacity == (LongUri.size() | 15));
    Require(copy.urls.begin[1].pointer != source.urls.begin[1].pointer);
    Require(copy.urls.begin[1].pointer[LongUri.size()] == '\0');
    RequireEmptyString(copy.urls.begin[2]);
    Require(Text(copy.username) == "user");
    Require(Text(copy.password) == "a password longer than 15" && copy.password.pointer != source.password.pointer);
    Require(copy.tlsCertPolicy == 1);
    Require(Text(copy.hostname) == LongName);
    Require(copy.tlsAlpnProtocols.end - copy.tlsAlpnProtocols.begin == 1 && Text(copy.tlsAlpnProtocols.begin[0]) == "h2");
    RequireEmptyVector(copy.tlsEllipticCurves);
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&source);
    Require(Text(copy.urls.begin[1]) == LongUri && Text(copy.hostname) == LongName);
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&copy);
    Require(live.empty());
}

void TestLargeString() {
    WebRtcIceServer source;
    _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(&source);
    const std::string large(5000, 'x');
    SetString(source.username, large);
    WebRtcIceServer copy;
    _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(&copy, &source);
    Require(Text(copy.username) == large && copy.username.capacity == (large.size() | 15));
    Require(reinterpret_cast<std::uintptr_t>(copy.username.pointer) % 32 == 0);
    void* raw = reinterpret_cast<void**>(copy.username.pointer)[-1];
    Require(live.count(raw) == 1 && live.count(copy.username.pointer) == 0);
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&copy);
    Require(live.count(raw) == 0);
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&source);
    Require(live.empty());
}

void TestAllocationFailure() {
    WebRtcIceServer source;
    Fill(source);
    for (int limit = 0; limit < 5; ++limit) {
        WebRtcIceServer copy;
        std::memset(&copy, 0xcd, sizeof(copy));
        const std::set<void*> before = live;
        failAfter = limit;
        bool threw = false;
        try {
            _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(&copy, &source);
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        failAfter = -1;
        Require(threw && live == before);
        Require(reinterpret_cast<const unsigned char*>(&copy)[0] == 0xcd);
    }
    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&source);
    Require(live.empty());
}

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestInvalid() {
    WebRtcIceServer server;
    _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(&server);
    WebRtcIceServer copy;
    Require(Throws([] { _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(nullptr); }));
    Require(Throws([] { _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(nullptr); }));
    Require(Throws([&] { _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(nullptr, &server); }));
    Require(Throws([&] { _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(&copy, nullptr); }));
    Require(Throws([&] { _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(&server, &server); }));

    SetString(server.hostname, LongName);
    SetVector(server.urls, {LongUri}, 0);
    WebRtcGuestString* begin = server.urls.begin;
    server.urls.end = begin + 2;
    const int before = allocations;
    const std::set<void*> held = live;
    Require(Throws([&] { _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(&copy, &server); }));
    Require(Throws([&] { _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&server); }));
    Require(allocations == before && live == held);
    server.urls.end = begin + 1;

    server.password.size = 16;
    Require(Throws([&] { _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&server); }));
    server.password.size = 0;
    server.password.capacity = 3;
    Require(Throws([&] { _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&server); }));
    server.password.capacity = 15;
    Require(live == held);

    _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(&server);
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
    TestDefault();
    TestCopyDefault();
    TestCopy();
    TestLargeString();
    TestAllocationFailure();
    TestInvalid();
    return 0;
}

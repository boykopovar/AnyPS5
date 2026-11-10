#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/GuestStringStorage.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

int frees = 0;

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { ++frees; std::free(pointer); }
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
    std::fprintf(stderr, "libwebrtc SocketAddress copy check failed at line %u\n", location.line());
    std::abort();
}

void SetInline(WebRtcGuestString& string, const char* value) {
    string.size = std::strlen(value);
    string.capacity = WebRtcGuestStringInlineCapacity;
    std::memcpy(string.buffer, value, string.size + 1);
}

void SetHeap(WebRtcGuestString& string, const char* value, std::size_t capacity) {
    string.size = std::strlen(value);
    string.capacity = capacity;
    string.pointer = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    std::memcpy(string.pointer, value, string.size + 1);
}

void Release(WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
}

void RequireCopy(const WebRtcSocketAddress& destination, const WebRtcSocketAddress& source) {
    Require(destination.hostname.size == source.hostname.size);
    Require(std::strcmp(WebRtcGuestStringData(destination.hostname), WebRtcGuestStringData(source.hostname)) == 0);
    Require(destination.ip.family == source.ip.family);
    Require(std::memcmp(&destination.ip.address, &source.ip.address, sizeof(source.ip.address)) == 0);
    Require(destination.port == source.port);
    Require(destination.scopeId == source.scopeId);
    Require(destination.literal == source.literal);
}

void TestInlineCopyConstructorAndAssignment() {
    WebRtcSocketAddress source{};
    SetInline(source.hostname, "stun.example");
    source.ip.family = WebRtcAfInet;
    source.ip.address.ip4.sAddr = 0x0100007F;
    source.port = 3478;
    source.scopeId = 3;
    source.literal = true;

    WebRtcSocketAddress copy{};
    _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(&copy, &source);
    RequireCopy(copy, source);
    Require(copy.hostname.capacity == WebRtcGuestStringInlineCapacity);

    WebRtcSocketAddress assigned{};
    SetInline(assigned.hostname, "");
    _ZN3rtc13SocketAddressaSERKS0__nid_postfix(&assigned, &source);
    RequireCopy(assigned, source);
    Require(assigned.hostname.capacity == WebRtcGuestStringInlineCapacity);
    Require(_ZN3rtc13SocketAddressaSERKS0__nid_postfix(&assigned, &assigned) == &assigned);
}

void TestHeapCopyAndAssignment() {
    char text[4096];
    std::memset(text, 'a', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    WebRtcSocketAddress source{};
    SetHeap(source.hostname, text, sizeof(text) - 1);
    source.ip.family = WebRtcAfInet6;
    source.ip.address.ip6.bytes[15] = 1;
    source.port = 443;
    source.scopeId = 11;

    WebRtcSocketAddress copy{};
    _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(&copy, &source);
    RequireCopy(copy, source);
    Require(copy.hostname.pointer != source.hostname.pointer);
    Require(copy.hostname.capacity == ((sizeof(text) - 1) | 15));

    WebRtcSocketAddress assigned{};
    SetInline(assigned.hostname, "old");
    const int beforeAssignment = frees;
    Require(_ZN3rtc13SocketAddressaSERKS0__nid_postfix(&assigned, &source) == &assigned);
    RequireCopy(assigned, source);
    Require(assigned.hostname.pointer != source.hostname.pointer);
    Require(frees == beforeAssignment);

    WebRtcSocketAddress inlineSource{};
    SetInline(inlineSource.hostname, "short");
    const int beforeReplacement = frees;
    char* const heapBuffer = assigned.hostname.pointer;
    _ZN3rtc13SocketAddressaSERKS0__nid_postfix(&assigned, &inlineSource);
    RequireCopy(assigned, inlineSource);
    Require(frees == beforeReplacement);
    Require(assigned.hostname.pointer == heapBuffer);

    Release(source.hostname);
    Release(copy.hostname);
    Release(assigned.hostname);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestInlineCopyConstructorAndAssignment();
    TestHeapCopyAndAssignment();
    return 0;
}

#include "prx/libwebrtc/libwebrtc.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/GuestStringStorage.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

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

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 4096;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize ? static_cast<std::size_t>(info.dwPageSize) : 4096;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD oldProtect = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &oldProtect) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &oldProtect) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        pageSize = size > 0 ? static_cast<std::size_t>(size) : 4096;
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template <typename Function>
bool ThrowsInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestGuardedPointers() {
    GuardedPages pages;
    auto* guardAddr = reinterpret_cast<WebRtcSocketAddress*>(pages.Guard());
    auto* readOnlyAddr = reinterpret_cast<WebRtcSocketAddress*>(pages.ReadOnly());

    WebRtcSocketAddress valid{};
    SetInline(valid.hostname, "stun.example");
    WebRtcSocketAddress dest{};
    SetInline(dest.hostname, "old");

    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(guardAddr, &valid); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(readOnlyAddr, &valid); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(&dest, guardAddr); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressaSERKS0__nid_postfix(guardAddr, &valid); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressaSERKS0__nid_postfix(readOnlyAddr, &valid); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressaSERKS0__nid_postfix(&dest, guardAddr); }));

    WebRtcSocketAddress badHeapSource{};
    badHeapSource.hostname.pointer = reinterpret_cast<char*>(pages.Guard());
    badHeapSource.hostname.size = 32;
    badHeapSource.hostname.capacity = 63;
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(&dest, &badHeapSource); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressaSERKS0__nid_postfix(&dest, &badHeapSource); }));

    WebRtcSocketAddress readOnlyHeapDest{};
    readOnlyHeapDest.hostname.pointer = reinterpret_cast<char*>(pages.ReadOnly());
    readOnlyHeapDest.hostname.size = 4;
    readOnlyHeapDest.hostname.capacity = 63;
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressaSERKS0__nid_postfix(&readOnlyHeapDest, &valid); }));
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestGuardedPointers();
    TestInlineCopyConstructorAndAssignment();
    TestHeapCopyAndAssignment();
    return 0;
}

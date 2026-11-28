#include "prx/libwebrtc/libwebrtc.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace {

std::unordered_set<void*> allocations;
int destroyed = 0;

void Require(bool condition) {
    if (!condition) throw std::runtime_error("RtpPacketReceived assertion failed");
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    auto* pointer = std::malloc(size);
    if (pointer) allocations.insert(pointer);
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

struct RefCounted {
    const void* const* vtable;
    std::int32_t refCount;
};

void APS5_VABI CompleteDestructor(void*) {}
void APS5_VABI DeletingDestructor(void* object) {
    Require(static_cast<RefCounted*>(object)->refCount == 0);
    ++destroyed;
}

const void* const refCountedVTable[] = {reinterpret_cast<const void*>(&CompleteDestructor), reinterpret_cast<const void*>(&DeletingDestructor)};

void TestConstructor() {
    WebRtcRtpPacketReceived packet;
    std::memset(&packet, 0xcd, sizeof(packet));
    _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(&packet, nullptr, std::numeric_limits<std::int64_t>::min());
    Require(!packet.marker && packet.payloadType == 0 && packet.paddingSize == 0 && packet.sequenceNumber == 0);
    Require(packet.timestamp == 0 && packet.ssrc == 0 && packet.payloadOffset == 12 && packet.payloadSize == 0);
    for (auto id : packet.extensions.ids) Require(id == 0);
    Require(!packet.extensions.extmapAllowMixed);
    Require(!packet.extensionEntriesBegin && !packet.extensionEntriesEnd && !packet.extensionEntriesCapacity && packet.extensionsSize == 0);
    Require(packet.buffer.offset == 0 && packet.buffer.size == 12);
    Require(packet.buffer.buffer->size == 12 && packet.buffer.buffer->capacity == 1500 && packet.buffer.buffer->refCount == 1);
    Require(packet.buffer.buffer->data[0] == 0x80);
    for (int i = 1; i < 12; ++i) Require(packet.buffer.buffer->data[i] == 0);
    Require(packet.arrivalTimeUs == std::numeric_limits<std::int64_t>::min());
    Require(packet.ecn == 0 && packet.payloadTypeFrequency == 0 && !packet.recovered && !packet.additionalData);
    _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(&packet);

    WebRtcRtpHeaderExtensionMap map{};
    map.ids[3] = 5;
    map.extmapAllowMixed = true;
    _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(&packet, &map, 123456);
    Require(packet.extensions.ids[3] == 5 && packet.extensions.extmapAllowMixed && packet.arrivalTimeUs == 123456);
    _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(&packet);

    bool threw = false;
    try {
        _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(nullptr, nullptr, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void TestInvalidExtensionsPointers() {
    WebRtcRtpPacketReceived packet;
    WebRtcRtpHeaderExtensionMap map{};
    map.ids[3] = 5;
    map.extmapAllowMixed = true;
    std::memset(&packet, 0x5A, sizeof(packet));
    errno = 77;
    bool threw = false;
    try {
        _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(
            &packet, reinterpret_cast<const WebRtcRtpHeaderExtensionMap*>(static_cast<std::uintptr_t>(0x10)), 1);
    } catch (const std::invalid_argument&) { threw = true; }
    Require(threw && errno == 77);
    for (std::size_t i = 0; i < sizeof(packet); ++i) Require(reinterpret_cast<unsigned char*>(&packet)[i] == 0x5A);
    WebRtcRtpPacketReceived staged{};
    std::memset(&staged, 0x5A, sizeof(staged));
    _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(&staged, &map, 9);
    Require(staged.extensions.ids[3] == 5 && staged.extensions.extmapAllowMixed && staged.arrivalTimeUs == 9);
    _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(&staged);
}

void TestCopy() {
    WebRtcRtpPacketReceived packet;
    _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(&packet, nullptr, 77);
    packet.marker = true;
    packet.payloadType = 96;
    packet.sequenceNumber = 4242;
    packet.ssrc = 0x12345678;
    packet.ecn = 3;
    packet.payloadTypeFrequency = 90000;
    packet.recovered = true;
    auto* entries = static_cast<WebRtcRtpPacketExtensionInfo*>(_Znwm_nid_postfix(4 * sizeof(WebRtcRtpPacketExtensionInfo)));
    entries[0] = {1, 2, 16};
    entries[1] = {4, 3, 20};
    packet.extensionEntriesBegin = entries;
    packet.extensionEntriesEnd = entries + 2;
    packet.extensionEntriesCapacity = entries + 4;
    packet.extensionsSize = 8;
    RefCounted data{refCountedVTable, 1};
    packet.additionalData = &data;

    WebRtcRtpPacketReceived copy;
    _ZN6webrtc17RtpPacketReceivedC1ERKS0__nid_postfix(&copy, &packet);
    Require(copy.marker && copy.payloadType == 96 && copy.sequenceNumber == 4242 && copy.ssrc == 0x12345678);
    Require(copy.arrivalTimeUs == 77 && copy.ecn == 3 && copy.payloadTypeFrequency == 90000 && copy.recovered);
    Require(copy.extensionEntriesBegin != entries && copy.extensionEntriesEnd == copy.extensionEntriesBegin + 2);
    Require(copy.extensionEntriesCapacity == copy.extensionEntriesEnd && copy.extensionsSize == 8);
    Require(copy.extensionEntriesBegin[1].id == 4 && copy.extensionEntriesBegin[1].length == 3 && copy.extensionEntriesBegin[1].offset == 20);
    Require(copy.buffer.buffer == packet.buffer.buffer && packet.buffer.buffer->refCount == 2);
    Require(copy.additionalData == &data && data.refCount == 2);

    _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(&packet);
    Require(data.refCount == 1 && destroyed == 0 && copy.buffer.buffer->refCount == 1);
    _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(&copy);
    Require(data.refCount == 0 && destroyed == 1);

    bool threw = false;
    try {
        _ZN6webrtc17RtpPacketReceivedC1ERKS0__nid_postfix(&copy, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
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
    auto* guardPacket = reinterpret_cast<WebRtcRtpPacketReceived*>(pages.Guard());
    auto* readOnlyPacket = reinterpret_cast<WebRtcRtpPacketReceived*>(pages.ReadOnly());
    auto* guardExtensions = reinterpret_cast<const WebRtcRtpHeaderExtensionMap*>(pages.Guard());

    WebRtcRtpPacketReceived valid{};
    _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(&valid, nullptr, 12);
    WebRtcRtpPacketReceived dest{};

    const std::size_t allocsBefore = allocations.size();
    Require(ThrowsInvalidArgument([&] {
        _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(guardPacket, nullptr, 0);
    }));
    Require(ThrowsInvalidArgument([&] {
        _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(readOnlyPacket, nullptr, 0);
    }));
    Require(ThrowsInvalidArgument([&] {
        _ZN6webrtc17RtpPacketReceivedC1EPKNS_21RtpHeaderExtensionMapENS_9TimestampE_nid_postfix(&dest, guardExtensions, 0);
    }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc17RtpPacketReceivedC1ERKS0__nid_postfix(guardPacket, &valid); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc17RtpPacketReceivedC1ERKS0__nid_postfix(readOnlyPacket, &valid); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc17RtpPacketReceivedC1ERKS0__nid_postfix(&dest, guardPacket); }));

    WebRtcRtpPacketReceived badEntries = valid;
    badEntries.extensionEntriesBegin = reinterpret_cast<WebRtcRtpPacketExtensionInfo*>(pages.Guard());
    badEntries.extensionEntriesEnd = badEntries.extensionEntriesBegin + 1;
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc17RtpPacketReceivedC1ERKS0__nid_postfix(&dest, &badEntries); }));

    WebRtcRtpPacketReceived badAdditional = valid;
    badAdditional.additionalData = pages.ReadOnly();
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc17RtpPacketReceivedC1ERKS0__nid_postfix(&dest, &badAdditional); }));

    Require(ThrowsInvalidArgument([&] { _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(guardPacket); }));
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(readOnlyPacket); }));

    const void* const badDtorVTable[] = {nullptr, pages.ReadOnly()};
    RefCounted nonExecObj{badDtorVTable, 1};
    valid.additionalData = &nonExecObj;
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(&valid); }));
    Require(valid.additionalData == &nonExecObj && nonExecObj.refCount == 1);
    valid.additionalData = nullptr;

    Require(allocations.size() == allocsBefore);

    _ZN6webrtc17RtpPacketReceivedD1Ev_nid_postfix(&valid);
}

}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    try {
        TestGuardedPointers();
        TestConstructor();
        TestInvalidExtensionsPointers();
        TestCopy();
        Require(allocations.empty());
    } catch (const std::exception& error) {
        std::fprintf(stderr, "RtpPacketReceived exception: %s\n", error.what());
        return 1;
    }
}

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void* lastFreed = nullptr;
int allocations = 0;
int frees = 0;

void* APS5_VABI TestAllocate(std::size_t size) { ++allocations; return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { lastFreed = pointer; ++frees; std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { ++allocations; return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { ++allocations; return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    ++allocations;
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioEncoder::EncodedInfo check failed at line %u\n", location.line());
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

WebRtcAudioEncodedInfoLeaf Leaf(std::size_t bytes, int payloadType) {
    WebRtcAudioEncodedInfoLeaf leaf {};
    leaf.encodedBytes = bytes;
    leaf.encodedTimestamp = static_cast<std::uint32_t>(bytes * 3);
    leaf.payloadType = payloadType;
    leaf.sendEvenIfEmpty = true;
    leaf.speech = false;
    leaf.encoderType = 1;
    return leaf;
}

bool SameLeaf(const WebRtcAudioEncodedInfoLeaf& left, const WebRtcAudioEncodedInfoLeaf& right) {
    return left.encodedBytes == right.encodedBytes && left.encodedTimestamp == right.encodedTimestamp && left.payloadType == right.payloadType &&
           left.sendEvenIfEmpty == right.sendEvenIfEmpty && left.speech == right.speech && left.encoderType == right.encoderType;
}

std::size_t Count(const WebRtcAudioEncodedInfo& info) {
    return static_cast<std::size_t>(info.redundantEnd - info.redundantBegin);
}

std::size_t Capacity(const WebRtcAudioEncodedInfo& info) {
    return static_cast<std::size_t>(info.redundantCapacity - info.redundantBegin);
}

WebRtcAudioEncodedInfo Constructed() {
    WebRtcAudioEncodedInfo info;
    std::memset(&info, 0xA5, sizeof(info));
    _ZN6webrtc12AudioEncoder11EncodedInfoC1Ev_nid_postfix(&info);
    return info;
}

WebRtcAudioEncodedInfo WithRedundant(std::size_t count) {
    WebRtcAudioEncodedInfo info = Constructed();
    info.leaf = Leaf(100 + count, 96);
    if (count == 0) return info;
    auto* leaves = static_cast<WebRtcAudioEncodedInfoLeaf*>(_Znwm_nid_postfix(count * sizeof(WebRtcAudioEncodedInfoLeaf)));
    for (std::size_t index = 0; index < count; ++index) leaves[index] = Leaf(index + 1, static_cast<int>(index + 10));
    info.redundantBegin = leaves;
    info.redundantEnd = leaves + count;
    info.redundantCapacity = leaves + count;
    return info;
}

bool HoldsRedundant(const WebRtcAudioEncodedInfo& info, std::size_t count) {
    if (Count(info) != count) return false;
    for (std::size_t index = 0; index < count; ++index)
        if (!SameLeaf(info.redundantBegin[index], Leaf(index + 1, static_cast<int>(index + 10)))) return false;
    return true;
}

void TestConstructor() {
    WebRtcAudioEncodedInfo info = Constructed();
    Require(info.leaf.encodedBytes == 0 && info.leaf.encodedTimestamp == 0 && info.leaf.payloadType == 0);
    Require(!info.leaf.sendEvenIfEmpty && info.leaf.speech && info.leaf.encoderType == 0);
    Require(info.redundantBegin == nullptr && info.redundantEnd == nullptr && info.redundantCapacity == nullptr);
    const int before = frees;
    _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(&info);
    Require(frees == before);
}

void TestCopyConstructor() {
    WebRtcAudioEncodedInfo source = WithRedundant(3);
    WebRtcAudioEncodedInfo copy;
    std::memset(&copy, 0xA5, sizeof(copy));
    _ZN6webrtc12AudioEncoder11EncodedInfoC1ERKS1__nid_postfix(&copy, &source);
    Require(SameLeaf(copy.leaf, source.leaf) && HoldsRedundant(copy, 3) && Capacity(copy) == 3);
    Require(copy.redundantBegin != source.redundantBegin);
    _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(&copy);
    Require(lastFreed == copy.redundantBegin);
    _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(&source);
}

void TestCopyOfAnEmptyListAllocatesNothing() {
    WebRtcAudioEncodedInfo source = WithRedundant(2);
    source.redundantEnd = source.redundantBegin;
    WebRtcAudioEncodedInfo copy;
    const int before = allocations;
    _ZN6webrtc12AudioEncoder11EncodedInfoC1ERKS1__nid_postfix(&copy, &source);
    Require(allocations == before && copy.redundantBegin == nullptr && copy.redundantCapacity == nullptr);
    _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(&source);
}

void TestAssignmentReusesCapacity() {
    WebRtcAudioEncodedInfo target = WithRedundant(4);
    WebRtcAudioEncodedInfo source = WithRedundant(2);
    WebRtcAudioEncodedInfoLeaf* storage = target.redundantBegin;
    const int before = allocations;
    Require(_ZN6webrtc12AudioEncoder11EncodedInfoaSERKS1__nid_postfix(&target, &source) == &target);
    Require(allocations == before && target.redundantBegin == storage && Capacity(target) == 4);
    Require(SameLeaf(target.leaf, source.leaf) && HoldsRedundant(target, 2));
    _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(&target);
    _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(&source);
}

void TestAssignmentGrowsToTheSourceSize() {
    WebRtcAudioEncodedInfo target = WithRedundant(1);
    WebRtcAudioEncodedInfo source = WithRedundant(5);
    WebRtcAudioEncodedInfoLeaf* storage = target.redundantBegin;
    Require(_ZN6webrtc12AudioEncoder11EncodedInfoaSERKS1__nid_postfix(&target, &source) == &target);
    Require(lastFreed == storage && Capacity(target) == 5 && HoldsRedundant(target, 5));
    WebRtcAudioEncodedInfo empty = Constructed();
    Require(_ZN6webrtc12AudioEncoder11EncodedInfoaSERKS1__nid_postfix(&target, &empty) == &target);
    Require(Count(target) == 0 && Capacity(target) == 5 && target.leaf.speech && target.leaf.encodedBytes == 0);
    _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(&target);
    _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(&source);
}

void TestSelfAssignment() {
    WebRtcAudioEncodedInfo info = WithRedundant(2);
    WebRtcAudioEncodedInfoLeaf* storage = info.redundantBegin;
    const int before = allocations;
    Require(_ZN6webrtc12AudioEncoder11EncodedInfoaSERKS1__nid_postfix(&info, &info) == &info);
    Require(allocations == before && info.redundantBegin == storage && HoldsRedundant(info, 2));
    _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(&info);
}

void TestNullArgumentsThrow() {
    WebRtcAudioEncodedInfo info = Constructed();
    RequireInvalidArgument([] { _ZN6webrtc12AudioEncoder11EncodedInfoC1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([&] { _ZN6webrtc12AudioEncoder11EncodedInfoC1ERKS1__nid_postfix(nullptr, &info); });
    RequireInvalidArgument([&] { _ZN6webrtc12AudioEncoder11EncodedInfoC1ERKS1__nid_postfix(&info, nullptr); });
    RequireInvalidArgument([&] { _ZN6webrtc12AudioEncoder11EncodedInfoaSERKS1__nid_postfix(nullptr, &info); });
    RequireInvalidArgument([&] { _ZN6webrtc12AudioEncoder11EncodedInfoaSERKS1__nid_postfix(&info, nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc12AudioEncoder11EncodedInfoD1Ev_nid_postfix(nullptr); });
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestConstructor();
    TestCopyConstructor();
    TestCopyOfAnEmptyListAllocatesNothing();
    TestAssignmentReusesCapacity();
    TestAssignmentGrowsToTheSourceSize();
    TestSelfAssignment();
    TestNullArgumentsThrow();
    return 0;
}

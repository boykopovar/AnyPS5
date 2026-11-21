#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

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
    std::fprintf(stderr, "libwebrtc rtcp::CompoundPacket check failed at line %u\n", location.line());
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

using PacketReady = void (APS5_VABI*)(void* function, const std::uint8_t* data, std::size_t size);
using BlockLengthFunction = std::size_t (APS5_VABI*)(const void* packet);
using CreateFunction = bool (APS5_VABI*)(const void* packet, std::uint8_t* buffer, std::size_t* index, std::size_t maxLength, void* function, PacketReady call);
using DeletingDestructor = void (APS5_VABI*)(void* packet);

struct FakePacket {
    const void* vtable;
    int id;
    std::size_t length;
    bool createSucceeds;
};

std::vector<int> deleted;
std::vector<int> created;

void APS5_VABI FakeDestroy(void*) {}
void APS5_VABI FakeDelete(void* packet) { deleted.push_back(static_cast<FakePacket*>(packet)->id); }
std::size_t APS5_VABI FakeBlockLength(const void* packet) { return static_cast<const FakePacket*>(packet)->length; }
bool APS5_VABI FakeCreate(const void* packet, std::uint8_t* buffer, std::size_t* index, std::size_t, void*, PacketReady) {
    const auto* fake = static_cast<const FakePacket*>(packet);
    created.push_back(fake->id);
    if (!fake->createSucceeds) return false;
    buffer[*index] = static_cast<std::uint8_t>(fake->id);
    *index += 1;
    return true;
}

void* const FakeVtable[] = {
    reinterpret_cast<void*>(&FakeDestroy), reinterpret_cast<void*>(&FakeDelete),
    reinterpret_cast<void*>(&FakeBlockLength), reinterpret_cast<void*>(&FakeCreate)};

FakePacket Fake(int id, std::size_t length = 4, bool createSucceeds = true) {
    return FakePacket {FakeVtable, id, length, createSucceeds};
}

void* ConsoleAllocate(std::size_t bytes) {
    if (bytes < 0x1000) return malloc_nid_postfix(bytes);
    auto* raw = static_cast<std::uint8_t*>(malloc_nid_postfix(bytes + 0x27));
    const auto aligned = (reinterpret_cast<std::uintptr_t>(raw) + 0x27) & ~std::uintptr_t {0x1f};
    reinterpret_cast<void**>(aligned)[-1] = raw;
    return reinterpret_cast<void*>(aligned);
}

void Append(WebRtcRtcpCompoundPacket& compound, std::vector<FakePacket>& packets, std::size_t capacity) {
    auto** storage = static_cast<void**>(ConsoleAllocate(capacity * sizeof(void*)));
    for (std::size_t index = 0; index < packets.size(); ++index) storage[index] = &packets[index];
    compound.appendedBegin = storage;
    compound.appendedEnd = storage + packets.size();
    compound.appendedCapacity = storage + capacity;
}

template<typename TFunction> TFunction Slot(const WebRtcRtcpCompoundPacket& compound, int slot) {
    return reinterpret_cast<TFunction>(static_cast<void* const*>(compound.vtable)[slot]);
}

void TestConstructor() {
    WebRtcRtcpCompoundPacket compound;
    compound.senderSsrc = 0xffffffffu;
    compound.appendedBegin = reinterpret_cast<void**>(std::uintptr_t {8});
    compound.appendedEnd = compound.appendedBegin;
    compound.appendedCapacity = compound.appendedBegin;
    const int before = allocations;
    _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(&compound);
    Require(allocations == before && compound.vtable != nullptr && compound.senderSsrc == 0);
    Require(compound.appendedBegin == nullptr && compound.appendedEnd == nullptr && compound.appendedCapacity == nullptr);
    Require(Slot<BlockLengthFunction>(compound, 2)(&compound) == 0);
    std::size_t index = 3;
    Require(Slot<CreateFunction>(compound, 3)(&compound, nullptr, &index, 0, nullptr, nullptr) && index == 3);
    const int freesBefore = frees;
    _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(&compound);
    Require(frees == freesBefore);
}

void TestDestructorDeletesEveryAppendedPacket() {
    std::vector<FakePacket> packets {Fake(1), Fake(2), Fake(3)};
    WebRtcRtcpCompoundPacket compound;
    _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(&compound);
    Append(compound, packets, 4);
    void** storage = compound.appendedBegin;
    deleted.clear();
    _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(&compound);
    Require(deleted == std::vector<int> {1, 2, 3});
    Require(lastFreed == storage);
    Require(compound.appendedBegin == nullptr && compound.appendedEnd == nullptr && compound.appendedCapacity == nullptr);
}

void TestStorageOf4096BytesKeepsTheRawPointer() {
    std::vector<FakePacket> small(511, Fake(7));
    WebRtcRtcpCompoundPacket compound;
    _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(&compound);
    Append(compound, small, 511);
    void** smallStorage = compound.appendedBegin;
    deleted.clear();
    _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(&compound);
    Require(deleted.size() == 511 && lastFreed == smallStorage);

    std::vector<FakePacket> big(2, Fake(8));
    _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(&compound);
    Append(compound, big, 512);
    void* raw = compound.appendedBegin[-1];
    deleted.clear();
    _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(&compound);
    Require(deleted.size() == 2 && lastFreed == raw);
}

void TestVtableForwardsToAppendedPackets() {
    std::vector<FakePacket> packets {Fake(1, 12), Fake(2, 20, false), Fake(3, 8)};
    WebRtcRtcpCompoundPacket compound;
    _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(&compound);
    Append(compound, packets, 3);
    Require(Slot<BlockLengthFunction>(compound, 2)(&compound) == 40);
    std::array<std::uint8_t, 4> buffer {};
    std::size_t index = 0;
    created.clear();
    Require(!Slot<CreateFunction>(compound, 3)(&compound, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require(created == std::vector<int> {1, 2} && index == 1 && buffer[0] == 1);
    packets[1].createSucceeds = true;
    index = 0;
    created.clear();
    Require(Slot<CreateFunction>(compound, 3)(&compound, buffer.data(), &index, buffer.size(), nullptr, nullptr));
    Require(created == std::vector<int> {1, 2, 3} && index == 3 && buffer[1] == 2 && buffer[2] == 3);
    _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(&compound);
}

void TestDeletingDestructorFreesTheObject() {
    auto* compound = static_cast<WebRtcRtcpCompoundPacket*>(_Znwm_nid_postfix(sizeof(WebRtcRtcpCompoundPacket)));
    _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(compound);
    std::vector<FakePacket> packets {Fake(5)};
    Append(*compound, packets, 1);
    deleted.clear();
    Slot<DeletingDestructor>(*compound, 1)(compound);
    Require(deleted == std::vector<int> {5} && lastFreed == compound);
}

void TestNullArgumentsThrow() {
    RequireInvalidArgument([] { _ZN6webrtc4rtcp14CompoundPacketC1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZN6webrtc4rtcp14CompoundPacketD1Ev_nid_postfix(nullptr); });
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
    TestDestructorDeletesEveryAppendedPacket();
    TestStorageOf4096BytesKeepsTheRawPointer();
    TestVtableForwardsToAppendedPackets();
    TestDeletingDestructorFreesTheObject();
    TestNullArgumentsThrow();
    return 0;
}

#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>

namespace {

int* fixtureDestroyed = nullptr;
std::int64_t physical = -1;
void* mapping = nullptr;
bool watched = false;

struct StaticExitMarker {
    ~StaticExitMarker() { if (fixtureDestroyed != nullptr) *fixtureDestroyed = 1; }
} marker;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void CheckPhysical(std::int64_t expected) {
    std::int64_t start = -1, end = -1;
    int type = -1;
    Require(DirectMemoryFind(expected, false, &start, &end, &type), "physical pool lost a live allocation during exit");
    Require(start == expected && end == expected + static_cast<std::int64_t>(PS5_PAGE_SIZE) && type == 0, "physical pool range changed during exit");
}

void CheckMapping(void* address, std::int64_t expected) {
    std::uintptr_t start = 0, end = 0;
    std::uint64_t offset = 0;
    int type = -1;
    Require(QueryDirectMapping(reinterpret_cast<std::uintptr_t>(address), &start, &end, &offset, &type), "direct backing map lost a live allocation during exit");
    Require(start == reinterpret_cast<std::uintptr_t>(address) && end == start + PS5_PAGE_SIZE && offset == static_cast<std::uint64_t>(expected) && type == 0, "direct backing range changed during exit");
    GuestAllocations::Mutation mutation;
    Require(mutation.Covers(address, PS5_PAGE_SIZE), "guest allocation registry lost a live mapping during exit");
    Require(mutation.Find(address).bytes == PS5_PAGE_SIZE, "guest allocation size changed during exit");
}

void CheckUnmapped(void* address) {
    std::uintptr_t begin = 0, end = 0;
    std::uint64_t offset = 0;
    int type = -1;
    Require(!QueryDirectMapping(reinterpret_cast<std::uintptr_t>(address), &begin, &end, &offset, &type), "explicit unmap did not remove the direct mapping");
    int protection = -1;
    Require(!GuestProtection(reinterpret_cast<std::uintptr_t>(address), &protection), "explicit unmap did not remove protection metadata");
    GuestAllocations::Mutation mutation;
    Require(!mutation.Overlaps(address, PS5_PAGE_SIZE), "explicit unmap did not remove the guest allocation");
}

void IgnoreWritten(void*, std::uintptr_t, std::uintptr_t) {}

}

extern "C" int GuestMemoryLifetimePrepare(int* destroyed) {
    try {
        fixtureDestroyed = destroyed;
        Require(DirectMemoryAlloc(0, DIRECT_MEMORY_SIZE, PS5_PAGE_SIZE, PS5_PAGE_SIZE, 0, &physical) == 0, "prepare physical allocation failed");
        Require(DoMapDirect(&mapping, PS5_PAGE_SIZE, 3, 0, physical, PS5_PAGE_SIZE) == 0, "prepare direct mapping failed");
        CheckPhysical(physical);
        CheckMapping(mapping, physical);
        std::memset(mapping, 0x6d, PS5_PAGE_SIZE);
        watched = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
        if (watched) Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping), PS5_PAGE_SIZE), "prepare write watch missing");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "memory lifetime prepare: %s\n", error.what());
        return 1;
    }
}

extern "C" int GuestMemoryLifetimeExercise() {
    const char* stage = "physical allocation query";
    try {
        CheckPhysical(physical);
        stage = "backing mapping query";
        CheckMapping(mapping, physical);
        Require(static_cast<unsigned char*>(mapping)[0] == 0x6d, "physical data changed during exit");
        stage = "write watch";
        if (watched) {
            Require(GuestWriteWatch::GuestWriteWatchCovers_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping), PS5_PAGE_SIZE), "write watch lost a live mapping during exit");
            Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(mapping), PS5_PAGE_SIZE, IgnoreWritten, nullptr), "write watch collection failed during exit");
            static_cast<unsigned char*>(mapping)[0] = 0x2a;
        }
        stage = "mapping protection";
        Require(DoMprotect(mapping, PS5_PAGE_SIZE, 1) == 0, "late read-only protection failed");
        int protection = -1;
        Require(GuestProtection(reinterpret_cast<std::uintptr_t>(mapping), &protection) && protection == 1, "late protection registry failed");
        Require(DoMprotect(mapping, PS5_PAGE_SIZE, 3) == 0, "late read/write protection failed");
        stage = "explicit unmap and free";
        Require(DoMunmap(mapping, PS5_PAGE_SIZE) == 0, "late direct unmap failed");
        CheckUnmapped(mapping);
        DirectMemoryFree(physical, PS5_PAGE_SIZE);
        std::int64_t start = -1, end = -1;
        int type = -1;
        Require(!DirectMemoryFind(physical, false, &start, &end, &type), "explicit physical free did not remove the allocation");

        stage = "physical offset reuse";
        std::int64_t reused = -1;
        Require(DirectMemoryAlloc(physical, physical + PS5_PAGE_SIZE, PS5_PAGE_SIZE, PS5_PAGE_SIZE, 0, &reused) == 0 && reused == physical, "late physical allocation failed");
        mapping = nullptr;
        Require(DoMapDirect(&mapping, PS5_PAGE_SIZE, 3, 0, reused, PS5_PAGE_SIZE) == 0, "late direct mapping failed");
        CheckPhysical(reused);
        CheckMapping(mapping, reused);
        const auto* bytes = static_cast<const unsigned char*>(mapping);
        for (std::size_t i = 0; i < PS5_PAGE_SIZE; ++i) Require(bytes[i] == 0, "reused physical backing was not zeroed");
        std::memset(mapping, 0x17, PS5_PAGE_SIZE);
        Require(DoMunmap(mapping, PS5_PAGE_SIZE) == 0, "late reused mapping unmap failed");
        CheckUnmapped(mapping);
        DirectMemoryFree(reused, PS5_PAGE_SIZE);

        stage = "late arena reservation";
        void* reserved = nullptr;
        Require(DoReserveVirtual(&reserved, PS5_PAGE_SIZE, 0, PS5_PAGE_SIZE) == 0, "late arena reservation failed");
        std::uintptr_t begin = 0, finish = 0;
        Require(GuestReservation(reinterpret_cast<std::uintptr_t>(reserved), &begin, &finish) && begin == reinterpret_cast<std::uintptr_t>(reserved) && finish == begin + PS5_PAGE_SIZE, "late reservation registry failed");
        Require(DoMunmap(reserved, PS5_PAGE_SIZE) == 0, "late reserved mapping unmap failed");
        Require(!GuestReservation(reinterpret_cast<std::uintptr_t>(reserved), &begin, &finish), "explicit unmap did not remove the reservation");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "memory lifetime exit probe (%s): %s\n", stage, error.what());
        return 1;
    }
}

#include "prx/libSceLibcInternal/include/TimeGetOrder.hpp"
#include <array>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <type_traits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Query = std::int32_t (APS5_VABI*)(const void*);

static_assert(std::is_same_v<decltype(&_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE13do_date_orderEv_nid_postfix), Query>);
static_assert(std::is_same_v<decltype(&_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE13do_date_orderEv_nid_postfix), Query>);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "time get order: %s\n", message);
    std::abort();
}

template<typename TFunction>
void RequireInvalid(TFunction query, const void* facet) {
    bool threw = false;
    errno = 71;
    try {
        query(facet);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw && errno == 71, "invalid facet memory throws without changing errno");
}

class GuardedPages {
public:
    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        memory = static_cast<std::byte*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr, "guarded allocation succeeds");
        DWORD previous = 0;
        Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "guard-page protection succeeds");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "host page size is available");
        pageSize = static_cast<std::size_t>(size);
        void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED, "guarded allocation succeeds");
        memory = static_cast<std::byte*>(allocation);
        Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0, "guard-page protection succeeds");
#endif
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, pageSize * 2);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    std::byte* End() { return memory + pageSize; }

    void ReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(memory, pageSize, PAGE_READONLY, &previous) != 0, "read-only protection succeeds");
#else
        Require(mprotect(memory, pageSize, PROT_READ) == 0, "read-only protection succeeds");
#endif
    }

private:
    std::byte* memory = nullptr;
    std::size_t pageSize = 0;
};

void CheckValues(Query query) {
    constexpr std::array<std::uint32_t, 10> values{0, 1, 2, 3, 4, 0xffffffff, 0x80000000, 0x7fffffff, 0x80000001, 0xdeadbeef};
    alignas(8) std::array<std::byte, 48> facet{};
    for (const std::size_t offset : {0u, 1u}) {
        for (const std::uint32_t bits : values) {
            facet.fill(std::byte{0xa5});
            std::memcpy(facet.data() + offset + 0x28, &bits, sizeof(bits));
            const auto before = facet;
            errno = 71;
            Require(query(facet.data() + offset) == std::bit_cast<std::int32_t>(bits), "the stored 32-bit order is returned unchanged");
            Require(facet == before && errno == 71, "querying order leaves all facet bytes and errno unchanged");
        }
    }

    alignas(8) std::array<std::byte, 48> other{};
    const std::int32_t firstOrder = 1;
    const std::int32_t secondOrder = 4;
    std::memcpy(facet.data() + 0x28, &firstOrder, sizeof(firstOrder));
    std::memcpy(other.data() + 0x28, &secondOrder, sizeof(secondOrder));
    Require(query(facet.data()) == firstOrder && query(other.data()) == secondOrder, "distinct caller facets retain their own date orders");
}

void CheckMemory(Query query) {
    GuardedPages pages;
    const std::int32_t order = -123;
    std::memcpy(pages.End() - sizeof(order), &order, sizeof(order));
    const void* facet = pages.End() - sizeof(order) - 0x28;
    Require(query(facet) == order, "only the four date-order bytes are read at a guarded boundary");
    RequireInvalid(query, pages.End() - sizeof(order) + 1 - 0x28);
    pages.ReadOnly();
    errno = 71;
    Require(query(facet) == order && errno == 71, "a read-only date-order field is accepted without writes");

    RequireInvalid(query, nullptr);
    RequireInvalid(query, reinterpret_cast<void*>(std::uintptr_t{0x10}));
    RequireInvalid(query, reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - 0x10));
}

}

int main() {
    const std::array<Query, 2> queries{
        _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE13do_date_orderEv_nid_postfix,
        _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE13do_date_orderEv_nid_postfix};
    for (Query query : queries) {
        CheckValues(query);
        CheckMemory(query);
    }
    std::puts("time get order checks passed");
    return 0;
}

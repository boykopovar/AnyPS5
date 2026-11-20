#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <udatamem.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace UdataClose {

using IcuRecoveredTest::Require;

static_assert(std::is_same_v<decltype(&udata_close_nid_postfix), void (APS5_VABI*)(UDataMemory*)>);
static_assert(sizeof(UDataMemory) == 56);
static_assert(alignof(UDataMemory) == 8);
static_assert(offsetof(UDataMemory, vFuncs) == 0);
static_assert(offsetof(UDataMemory, pHeader) == 8);
static_assert(offsetof(UDataMemory, toc) == 16);
static_assert(offsetof(UDataMemory, heapAllocated) == 24);
static_assert(offsetof(UDataMemory, mapAddr) == 32);
static_assert(offsetof(UDataMemory, map) == 40);
static_assert(offsetof(UDataMemory, length) == 48);

namespace {

struct GuardedData {
    std::array<std::uint8_t, 32> before;
    UDataMemory data;
    std::array<std::uint8_t, 32> after;
};

void RequireGuards(const GuardedData& guarded) {
    for (auto value : guarded.before) Require(value == 0xa5);
    for (auto value : guarded.after) Require(value == 0x5a);
}

void RequireInitialized(const UDataMemory& data) {
    UDataMemory expected{};
    expected.length = -1;
    Require(std::memcmp(&data, &expected, sizeof(data)) == 0);
}

void Null() {
    errno = EDOM;
    udata_close_nid_postfix(nullptr);
    Require(errno == EDOM);
}

void Stack() {
    GuardedData guarded{};
    guarded.before.fill(0xa5);
    guarded.after.fill(0x5a);
    std::memset(&guarded.data, 0, sizeof(guarded.data));
    udata_close_nid_postfix(&guarded.data);
    RequireInitialized(guarded.data);
    RequireGuards(guarded);

    udata_close_nid_postfix(&guarded.data);
    RequireInitialized(guarded.data);
    RequireGuards(guarded);
}

void Parity() {
    UDataMemory expected{};
    UDataMemory actual{};
    expected.vFuncs = reinterpret_cast<const commonDataFuncs*>(static_cast<std::uintptr_t>(0x10));
    expected.pHeader = reinterpret_cast<const DataHeader*>(static_cast<std::uintptr_t>(0x20));
    expected.toc = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x30));
    expected.length = 71;
    actual = expected;
    udata_close(&expected);
    udata_close_nid_postfix(&actual);
    Require(std::memcmp(&actual, &expected, sizeof(actual)) == 0);
}

#if defined(_WIN32)
void Mapping() {
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, nullptr);
    Require(mapping != nullptr);
    void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
    Require(view != nullptr);

    GuardedData guarded{};
    guarded.before.fill(0xa5);
    guarded.after.fill(0x5a);
    guarded.data.pHeader = static_cast<const DataHeader*>(view);
    guarded.data.mapAddr = view;
    guarded.data.map = mapping;
    guarded.data.length = 4096;
    udata_close_nid_postfix(&guarded.data);

    MEMORY_BASIC_INFORMATION information{};
    Require(VirtualQuery(view, &information, sizeof(information)) == sizeof(information));
    Require(information.State == MEM_FREE);
    RequireInitialized(guarded.data);
    RequireGuards(guarded);
}
#else
void Mapping() {
    const auto pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* mapping = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED);

    GuardedData guarded{};
    guarded.before.fill(0xa5);
    guarded.after.fill(0x5a);
    guarded.data.pHeader = static_cast<const DataHeader*>(mapping);
    guarded.data.mapAddr = mapping;
    guarded.data.map = static_cast<char*>(mapping) + pageSize;
    guarded.data.length = static_cast<std::int32_t>(pageSize);
    udata_close_nid_postfix(&guarded.data);

    unsigned char residency = 0;
    errno = 0;
    Require(mincore(mapping, pageSize, &residency) == -1);
    Require(errno == ENOMEM);
    RequireInitialized(guarded.data);
    RequireGuards(guarded);
}
#endif

void Heap() {
    UErrorCode status = U_ZERO_ERROR;
    UDataMemory* data = UDataMemory_createNewInstance(&status);
    Require(status == U_ZERO_ERROR);
    Require(data != nullptr);
    Require(data->heapAllocated);

#if defined(_WIN32)
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 4096, nullptr);
    Require(mapping != nullptr);
    void* view = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
    Require(view != nullptr);
    data->pHeader = static_cast<const DataHeader*>(view);
    data->mapAddr = view;
    data->map = mapping;
    data->length = 4096;
    udata_close_nid_postfix(data);

    MEMORY_BASIC_INFORMATION information{};
    Require(VirtualQuery(view, &information, sizeof(information)) == sizeof(information));
    Require(information.State == MEM_FREE);
#else
    const auto pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* mapping = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED);
    data->pHeader = static_cast<const DataHeader*>(mapping);
    data->mapAddr = mapping;
    data->map = static_cast<char*>(mapping) + pageSize;
    data->length = static_cast<std::int32_t>(pageSize);
    udata_close_nid_postfix(data);

    unsigned char residency = 0;
    errno = 0;
    Require(mincore(mapping, pageSize, &residency) == -1);
    Require(errno == ENOMEM);
#endif
}

}

void Run() {
    Null();
    Stack();
    Parity();
    Mapping();
    Heap();
}

}

void CheckIcuDataMemory() {
    UdataClose::Run();
}

#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static_assert(sizeof(Asn1String) == 24 && offsetof(Asn1String, data) == 8 && offsetof(Asn1String, flags) == 16);

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL 3 ASN.1 check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static void TestGet0Data() {
    unsigned char bytes[] {1, 2, 3};
    Asn1String string {3, 2, bytes, 0};
    Require(ASN1_STRING_get0_data_nid_postfix(&string) == bytes);
    string = {0, 4, nullptr, 0};
    Require(ASN1_STRING_get0_data_nid_postfix(&string) == nullptr);
    string = {-1, 0x102, bytes, 0x010};
    Require(ASN1_STRING_get0_data_nid_postfix(&string) == bytes);
    RequireThrows([] { ASN1_STRING_get0_data_nid_postfix(nullptr); });
}

static void InitializeHeap() {
    void* const api[10] {};
    ApplicationHeapRegister_nid_no_patch(api);
}

static Asn1String* NewInteger(int length, int type, std::int64_t flags) {
    auto* integer = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    Require(integer != nullptr);
    integer->length = length;
    integer->type = type;
    integer->flags = flags;
    integer->data = nullptr;
    if (length > 0) {
        integer->data = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(length)));
        Require(integer->data != nullptr);
        std::memset(integer->data, 0x5A, static_cast<std::size_t>(length));
    }
    return integer;
}

static void TestIntegerFree() {
    ASN1_INTEGER_free_nid_postfix(nullptr);
    ASN1_INTEGER_free_nid_postfix(NewInteger(4, 2, 0));
    ASN1_INTEGER_free_nid_postfix(NewInteger(4, 2, 0x001));
    ASN1_INTEGER_free_nid_postfix(NewInteger(16, 0x102, 0));
    ASN1_INTEGER_free_nid_postfix(NewInteger(0, 2, 0));
    ASN1_INTEGER_free_nid_postfix(NewInteger(4, 6, 0));

    Asn1String* borrowed = NewInteger(4, 2, 0x010);
    unsigned char* const data = borrowed->data;
    ASN1_INTEGER_free_nid_postfix(borrowed);
    Require(data[0] == 0x5A && data[3] == 0x5A);
    free_nid_postfix(data);

    Asn1String* borrowedNegative = NewInteger(4, 2, 0x010);
    unsigned char* const negativeData = borrowedNegative->data;
    borrowedNegative->length = -1;
    ASN1_INTEGER_free_nid_postfix(borrowedNegative);
    Require(negativeData[0] == 0x5A && negativeData[3] == 0x5A);
    free_nid_postfix(negativeData);

    Asn1String* negative = NewInteger(4, 2, 0);
    negative->length = -1;
    RequireThrows([&] { ASN1_INTEGER_free_nid_postfix(negative); });
    Require(negative->data != nullptr && negative->data[0] == 0x5A && negative->data[3] == 0x5A);
    negative->length = 4;
    ASN1_INTEGER_free_nid_postfix(negative);

    Asn1String* empty = NewInteger(0, 2, 0);
    empty->length = -1;
    ASN1_INTEGER_free_nid_postfix(empty);
}

template<typename TAction> static void RequireRejected(TAction action, std::source_location location = std::source_location::current()) {
    errno = 71;
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected && errno == 71, location);
}

static void TestGuestPointers() {
    auto* unmapped = reinterpret_cast<Asn1String*>(std::uintptr_t{0x10});
    RequireRejected([&] { ASN1_STRING_get0_data_nid_postfix(unmapped); });
    RequireRejected([&] { ASN1_INTEGER_free_nid_postfix(unmapped); });

#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr);
#else
    const std::size_t page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* memory = static_cast<unsigned char*>(mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(memory != MAP_FAILED);
#endif
    std::memset(memory, 0x5A, 4);
    auto* readOnly = reinterpret_cast<Asn1String*>(memory + page - sizeof(Asn1String));
    *readOnly = {4, 2, memory, 0};
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(memory, page, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(memory, page, PROT_READ) == 0);
#endif
    errno = 71;
    Require(ASN1_STRING_get0_data_nid_postfix(readOnly) == memory && errno == 71);
    Asn1String* integer = NewInteger(0, 2, 0);
    integer->length = 4;
    integer->data = memory;
    RequireRejected([&] { ASN1_INTEGER_free_nid_postfix(integer); });
    Require(memory[0] == 0x5A && memory[3] == 0x5A && integer->data == memory);
    integer->data = nullptr;
    ASN1_INTEGER_free_nid_postfix(integer);
#ifdef _WIN32
    VirtualFree(memory, 0, MEM_RELEASE);
#else
    munmap(memory, page);
#endif
}

static unsigned char* MapPages(std::size_t& page, std::size_t count) {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    page = info.dwPageSize;
    auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * count, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr);
#else
    page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* memory = static_cast<unsigned char*>(mmap(nullptr, page * count, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(memory != MAP_FAILED);
#endif
    return memory;
}

static void ProtectReadOnly(unsigned char* memory, std::size_t size) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(memory, size, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(memory, size, PROT_READ) == 0);
#endif
}

static void UnmapPages(unsigned char* memory, std::size_t size) {
#ifdef _WIN32
    static_cast<void>(size);
    VirtualFree(memory, 0, MEM_RELEASE);
#else
    munmap(memory, size);
#endif
}

static Asn1Object_nid_no_patch* NewObject(int flags) {
    auto* object = static_cast<Asn1Object_nid_no_patch*>(malloc_nid_postfix(sizeof(Asn1Object_nid_no_patch)));
    Require(object != nullptr);
    *object = {nullptr, nullptr, 0, 0, nullptr, flags};
    return object;
}

static void TestFreeGuestPointers() {
    RequireRejected([] { ASN1_OCTET_STRING_free_nid_postfix(reinterpret_cast<Asn1String*>(std::uintptr_t{0x10})); });
    RequireRejected([] { ASN1_OBJECT_free_nid_postfix(reinterpret_cast<Asn1Object_nid_no_patch*>(std::uintptr_t{0x10})); });

    Asn1String* huge = NewInteger(0, 4, 0);
    huge->length = INT32_MAX;
    huge->data = reinterpret_cast<unsigned char*>(std::uintptr_t{0x10});
    RequireRejected([&] { ASN1_OCTET_STRING_free_nid_postfix(huge); });
    Require(huge->length == INT32_MAX && huge->data == reinterpret_cast<unsigned char*>(std::uintptr_t{0x10}));
    huge->data = nullptr;
    ASN1_OCTET_STRING_free_nid_postfix(huge);

    std::size_t page = 0;
    unsigned char* memory = MapPages(page, 1);
    std::memset(memory, 0x5A, 4);
    auto* readOnlyString = reinterpret_cast<Asn1String*>(memory + page - sizeof(Asn1String));
    *readOnlyString = {4, 4, memory, 0};
    auto* readOnlyObject = reinterpret_cast<Asn1Object_nid_no_patch*>(memory + 64);
    *readOnlyObject = {nullptr, nullptr, 0, 0, nullptr, 0x04};
    ProtectReadOnly(memory, page);

    RequireRejected([&] { ASN1_OCTET_STRING_free_nid_postfix(readOnlyString); });
    Require(memory[0] == 0x5A && memory[3] == 0x5A);
    RequireRejected([&] { ASN1_OBJECT_free_nid_postfix(readOnlyObject); });
    Require(readOnlyObject->flags == 0x04);

    Asn1String* owned = NewInteger(0, 4, 0);
    owned->length = 4;
    owned->data = memory;
    RequireRejected([&] { ASN1_OCTET_STRING_free_nid_postfix(owned); });
    Require(memory[0] == 0x5A && memory[3] == 0x5A && owned->data == memory);
    owned->data = nullptr;
    ASN1_OCTET_STRING_free_nid_postfix(owned);

    Asn1Object_nid_no_patch* object = NewObject(0x08);
    object->length = 4;
    object->data = memory;
    RequireRejected([&] { ASN1_OBJECT_free_nid_postfix(object); });
    Require(memory[0] == 0x5A && memory[3] == 0x5A && object->data == memory && object->length == 4);
    object->data = nullptr;
    object->flags = 0x01;
    ASN1_OBJECT_free_nid_postfix(object);
    UnmapPages(memory, page);

    Asn1String* string = NewInteger(4, 4, 0);
    ASN1_OCTET_STRING_free_nid_postfix(string);
    Asn1String* borrowed = NewInteger(4, 4, 0x010);
    unsigned char* const borrowedData = borrowed->data;
    ASN1_OCTET_STRING_free_nid_postfix(borrowed);
    Require(borrowedData[0] == 0x5A && borrowedData[3] == 0x5A);
    free_nid_postfix(borrowedData);

    Asn1Object_nid_no_patch* staticObject = NewObject(0x04 | 0x08);
    staticObject->shortName = static_cast<char*>(malloc_nid_postfix(4));
    staticObject->longName = static_cast<char*>(malloc_nid_postfix(4));
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(4));
    Require(staticObject->shortName && staticObject->longName && data);
    std::memset(data, 0x5A, 4);
    staticObject->data = data;
    staticObject->length = 4;
    ASN1_OBJECT_free_nid_postfix(staticObject);
    Require(!staticObject->shortName && !staticObject->longName && !staticObject->data && staticObject->length == 0 && staticObject->flags == 0x0C);
    free_nid_postfix(staticObject);
    ASN1_OBJECT_free_nid_postfix(NewObject(0x01 | 0x04 | 0x08));
}

int main() {
    InitializeHeap();
    TestGet0Data();
    TestIntegerFree();
    TestGuestPointers();
    TestFreeGuestPointers();
    std::puts("libSceLibreSsl3 ASN.1 tests passed");
}

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

int main() {
    InitializeHeap();
    TestGet0Data();
    TestIntegerFree();
    TestGuestPointers();
    std::puts("libSceLibreSsl3 ASN.1 tests passed");
}

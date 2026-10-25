#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

static_assert(sizeof(LibreSslAsn1String) == 24 && offsetof(LibreSslAsn1String, data) == 8 && offsetof(LibreSslAsn1String, flags) == 16);

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
    LibreSslAsn1String string {3, 2, bytes, 0};
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

static LibreSslAsn1String* NewInteger(int length, int type, std::int64_t flags) {
    auto* integer = static_cast<LibreSslAsn1String*>(malloc_nid_postfix(sizeof(LibreSslAsn1String)));
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
    ASN1_INTEGER_free_nid_postfix(NewInteger(16, 0x102, 0));
    ASN1_INTEGER_free_nid_postfix(NewInteger(0, 2, 0));
    ASN1_INTEGER_free_nid_postfix(NewInteger(4, 6, 0));

    LibreSslAsn1String* borrowed = NewInteger(4, 2, 0x010);
    unsigned char* const data = borrowed->data;
    ASN1_INTEGER_free_nid_postfix(borrowed);
    Require(data[0] == 0x5A && data[3] == 0x5A);
    free_nid_postfix(data);

    LibreSslAsn1String* negative = NewInteger(4, 2, 0);
    negative->length = -1;
    RequireThrows([&] { ASN1_INTEGER_free_nid_postfix(negative); });
    Require(negative->data != nullptr && negative->data[0] == 0x5A);
    negative->length = 4;
    ASN1_INTEGER_free_nid_postfix(negative);

    LibreSslAsn1String* empty = NewInteger(0, 2, 0);
    empty->length = -1;
    ASN1_INTEGER_free_nid_postfix(empty);
}

int main() {
    InitializeHeap();
    TestGet0Data();
    TestIntegerFree();
    std::puts("libSceLibreSsl3 ASN.1 tests passed");
}

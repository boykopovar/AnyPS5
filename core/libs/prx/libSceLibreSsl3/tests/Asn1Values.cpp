#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ASN.1 value check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> allocations;
bool failAllocation = false;
void* clearing = nullptr;
std::size_t clearingLength = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (std::exchange(failAllocation, false)) return nullptr;
    void* result = std::malloc(size);
    Require(result != nullptr);
    Require(allocations.emplace(result, size).second);
    std::memset(result, 0xa5, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1);
    if (pointer == clearing) {
        const auto* data = static_cast<const unsigned char*>(pointer);
        Require(std::all_of(data, data + clearingLength, [](auto byte) { return byte == 0; }));
        clearing = nullptr;
    }
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) {
    auto* result = Allocate_nid_no_patch(count * size);
    if (result) std::memset(result, 0, count * size);
    return result;
}

void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void ExpectClear(const Asn1String& value) {
    clearing = value.data;
    clearingLength = static_cast<std::size_t>(value.length);
}

void Release(Asn1String& value) {
    free_nid_postfix(value.data);
    value.data = nullptr;
    value.length = 0;
}

void Strings() {
    Asn1String string{0, 12, nullptr, 0x80};
    const unsigned char binary[] = {'a', 0, 0xff, 'z'};
    Require(ASN1_STRING_set_nid_postfix(&string, binary, sizeof(binary)) == 1);
    Require(string.length == 4 && string.type == 12 && string.flags == 0x80);
    Require(std::memcmp(string.data, binary, 4) == 0 && string.data[4] == 0);
    ExpectClear(string);
    Require(ASN1_STRING_set_nid_postfix(&string, "replacement", -1) == 1);
    Require(clearing == nullptr && string.length == 11 && string.data[11] == 0);
    ExpectClear(string);
    Require(ASN1_STRING_set_nid_postfix(&string, nullptr, 7) == 1);
    Require(clearing == nullptr && string.length == 7);
    Require(std::all_of(string.data, string.data + 8, [](auto byte) { return byte == 0; }));
    ExpectClear(string);
    Require(ASN1_STRING_set_nid_postfix(&string, nullptr, 0) == 1);
    Require(string.data && string.length == 0 && string.data[0] == 0);
    Release(string);
    unsigned char borrowed[] = {1, 2, 3};
    string = {3, 4, borrowed, 0x90};
    Require(ASN1_STRING_set_nid_postfix(&string, borrowed, 3) == 1);
    Require(string.data != borrowed && string.flags == 0x80);
    Require(borrowed[0] == 1 && borrowed[1] == 2 && borrowed[2] == 3);
    Release(string);
    Require(allocations.empty());
}

void Failures() {
    Asn1String string{0, 22, nullptr, 0x40};
    Require(ASN1_STRING_set_nid_postfix(&string, "old", 3) == 1);
    auto* previous = string.data;
    Require(ASN1_STRING_set_nid_postfix(&string, nullptr, -1) == 0);
    Require(string.data == previous && string.length == 3);
    bool threw = false;
    try { ASN1_STRING_set_nid_postfix(&string, string.data + 1, 2); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw && string.data == previous && string.length == 3);
    for (int invalid : {-2, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        Require(ASN1_STRING_set_nid_postfix(&string, "old", 3) == 1);
        ExpectClear(string);
        ERR_clear_error_nid_postfix();
        Require(ASN1_STRING_set_nid_postfix(&string, nullptr, invalid) == 0);
        Require(clearing == nullptr && string.data == nullptr && string.length == 0);
        Require(string.type == 22 && string.flags == 0x40 && ERR_get_error_nid_postfix() == 0);
    }
    Require(ASN1_STRING_set_nid_postfix(&string, "old", 3) == 1);
    ExpectClear(string);
    failAllocation = true;
    Require(ASN1_STRING_set_nid_postfix(&string, "new", 3) == 0);
    Require(!failAllocation && clearing == nullptr && !string.data && !string.length);
    Require(ERR_get_error_nid_postfix() == 0x0dfff041 && ERR_get_error_nid_postfix() == 0);
    Require(allocations.empty());
}

void Integers() {
    Asn1String integer{0, 2, nullptr, 0};
    for (auto number : std::array<std::int64_t, 18>{0, 1, -1, 127, 128, 255, 256, -128, -255, -256,
        65535, 65536, -65536, 0x7fffffff, 0x80000000LL, -0x80000000LL,
        std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::min()}) {
        ExpectClear(integer);
        Require(ASN1_INTEGER_set_nid_postfix(&integer, number) == 1);
        Require(clearing == nullptr && integer.flags == 0 && integer.length >= 1 && integer.length <= 8);
        Require(integer.type == (number < 0 ? 0x102 : 2));
        const auto bits = static_cast<std::uint64_t>(number);
        const auto magnitude = number < 0 ? std::uint64_t{0} - bits : bits;
        std::uint64_t decoded = 0;
        for (int index = 0; index < integer.length; ++index) decoded = (decoded << 8) | integer.data[index];
        Require(decoded == magnitude && (integer.length == 1 || integer.data[0] != 0));
        Require(ASN1_INTEGER_get_nid_postfix(&integer) == number);
    }
    ExpectClear(integer);
    integer.flags = 0x80;
    failAllocation = true;
    ERR_clear_error_nid_postfix();
    Require(ASN1_INTEGER_set_nid_postfix(&integer, -123) == 0);
    Require(!failAllocation && clearing == nullptr && integer.data == nullptr && integer.length == 0);
    Require(integer.type == 0x102 && integer.flags == 0 && ERR_get_error_nid_postfix() == 0);
    bool threw = false;
    try { ASN1_INTEGER_set_nid_postfix(nullptr, 7); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw && allocations.empty());
}

void Compare() {
    unsigned char a[] = {0x80, 0};
    unsigned char b[] = {0, 0xff};
    Asn1String first{2, 4, a, 0};
    Asn1String second{1, 4, b, 0};
    Require(ASN1_OCTET_STRING_cmp_nid_postfix(&first, &second) == 1);
    Require(ASN1_OCTET_STRING_cmp_nid_postfix(&second, &first) == -1);
    second.length = 2;
    Require(ASN1_OCTET_STRING_cmp_nid_postfix(&first, &second) > 0);
    Require(ASN1_OCTET_STRING_cmp_nid_postfix(&second, &first) < 0);
    second.data = a;
    Require(ASN1_OCTET_STRING_cmp_nid_postfix(&first, &second) == 0);
    second.type = 12;
    Require(ASN1_OCTET_STRING_cmp_nid_postfix(&first, &second) == -8);
    first = {0, 4, nullptr, 0};
    second = first;
    Require(ASN1_OCTET_STRING_cmp_nid_postfix(&first, &second) == 0);
    Require(ASN1_OCTET_STRING_cmp_nid_postfix(nullptr, &second) == -1);
    Require(ASN1_OCTET_STRING_cmp_nid_postfix(&first, nullptr) == -1);
    Require(ASN1_OCTET_STRING_cmp_nid_postfix(nullptr, nullptr) == -1);
}

void Destruction() {
    ASN1_OCTET_STRING_free_nid_postfix(nullptr);
    auto* string = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    *string = {};
    Require(ASN1_STRING_set_nid_postfix(string, "secret", 6) == 1);
    ExpectClear(*string);
    ASN1_OCTET_STRING_free_nid_postfix(string);
    Require(clearing == nullptr && allocations.empty());
    unsigned char borrowed[] = {9, 8, 7};
    string = static_cast<Asn1String*>(malloc_nid_postfix(sizeof(Asn1String)));
    *string = {3, 4, borrowed, 0x10};
    ASN1_OCTET_STRING_free_nid_postfix(string);
    Require(borrowed[0] == 9 && borrowed[1] == 8 && borrowed[2] == 7 && allocations.empty());
}

} // namespace

int main() {
    std::array<void*, 10> api{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Calloc_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch), reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Strings();
    Failures();
    Integers();
    Compare();
    Destruction();
    Require(allocations.empty());
    std::puts("LibreSSL3 ASN.1 value tests passed");
}

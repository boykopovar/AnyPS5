#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <span>
#include <utility>
#include <stdexcept>
#include <vector>

namespace {

static_assert(sizeof(Asn1String) == 24);
static_assert(offsetof(Asn1String, data) == 8);
static_assert(offsetof(Asn1String, flags) == 16);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

template<typename TAction>
void RequireInvalidArgument(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

void CheckConversion(int type, std::span<const unsigned char> input, std::span<const unsigned char> expected, int length = -2) {
    std::vector<unsigned char> storage(input.begin(), input.end());
    storage.push_back(0);
    Asn1String value{length == -2 ? static_cast<int>(input.size()) : length, type, storage.data(), 0x123456789abcdef};
    const Asn1String original = value;
    unsigned char sentinel = 0xab;
    unsigned char* result = &sentinel;
    Require(ASN1_STRING_to_UTF8_nid_postfix(&result, &value) == static_cast<int>(expected.size()), "UTF-8 byte length");
    Require(result && result != &sentinel && result != storage.data(), "conversion owns a new buffer");
    Require(expected.empty() || std::memcmp(result, expected.data(), expected.size()) == 0, "converted UTF-8 bytes");
    Require(result[expected.size()] == 0, "conversion terminator");
    Require(value.length == original.length && value.type == original.type && value.data == original.data
        && value.flags == original.flags, "conversion preserves the input object");
    Require(std::equal(input.begin(), input.end(), storage.begin()), "conversion preserves the input bytes");
    free_nid_postfix(result);
}

constexpr std::uint64_t Asn1Error(int reason) {
    return (std::uint64_t{13} << 24) | (std::uint64_t{0xfff} << 12) | static_cast<std::uint64_t>(reason);
}

void RequireConversionError(unsigned char** output, int type, const unsigned char* data, int length, int reason, const char* message) {
    ERR_clear_error_nid_postfix();
    Asn1String value{length, type, const_cast<unsigned char*>(data), 0};
    const unsigned char* before = output ? *output : nullptr;
    Require(ASN1_STRING_to_UTF8_nid_postfix(output, &value) == -1, message);
    Require(!output || *output == before, "a failed conversion leaves the output alone");
    Require(ERR_get_error_nid_postfix() == Asn1Error(reason) && ERR_get_error_nid_postfix() == 0, message);
}

void MalformedStrings() {
    unsigned char sentinel = 0xab;
    unsigned char* result = &sentinel;
    constexpr unsigned char oddBmp[] = {0, 0x41, 0};
    RequireConversionError(&result, 30, oddBmp, 3, 129, "odd BMPString length");
    RequireConversionError(&result, 30, nullptr, 1, 129, "BMPString length is checked before the data");
    constexpr unsigned char bmpSurrogate[] = {0, 0x41, 0xd8, 0};
    RequireConversionError(&result, 30, bmpSurrogate, 4, 124, "BMPString surrogate");
    constexpr unsigned char shortUniversal[] = {0, 0, 0, 0x41, 0};
    RequireConversionError(&result, 28, shortUniversal, 5, 133, "UniversalString length not a multiple of four");
    RequireConversionError(&result, 28, nullptr, 2, 133, "UniversalString length is checked before the data");
    constexpr unsigned char universalBeyond[] = {0, 0x11, 0, 0};
    RequireConversionError(&result, 28, universalBeyond, 4, 124, "UniversalString above U+10FFFF");
    constexpr unsigned char universalSurrogate[] = {0, 0, 0xdc, 0};
    RequireConversionError(&result, 28, universalSurrogate, 4, 124, "UniversalString surrogate");
    constexpr unsigned char overlong[] = {0xc0, 0x80};
    constexpr unsigned char truncated[] = {'A', 0xe2, 0x82};
    constexpr unsigned char surrogate[] = {0xed, 0xa0, 0x80};
    constexpr unsigned char beyond[] = {0xf5, 0x80, 0x80, 0x80};
    constexpr unsigned char continuation[] = {0x80};
    for (const auto& [data, length] : {std::pair{overlong, 2}, std::pair{truncated, 3}, std::pair{surrogate, 3},
             std::pair{beyond, 4}, std::pair{continuation, 1}})
        RequireConversionError(&result, 12, data, length, 134, "malformed UTF8String");
    RequireConversionError(nullptr, 30, oddBmp, 3, 129, "a malformed string fails before the output is used");
}

void Accessors() {
    unsigned char data[] = {'a', 0, 'b'};
    Asn1String value{3, 12, data, std::numeric_limits<std::int64_t>::max()};
    Require(ASN1_STRING_data_nid_postfix(&value) == data, "data accessor preserves pointer identity");
    ASN1_STRING_data_nid_postfix(&value)[2] = 'c';
    Require(data[2] == 'c', "data accessor permits writes");
    Require(ASN1_STRING_length_nid_postfix(&value) == 3, "length includes embedded NUL");
    Require(ASN1_STRING_type_nid_postfix(&value) == 12, "type accessor");
    value = {-9, 0x7fffffff, nullptr, -1};
    Require(ASN1_STRING_data_nid_postfix(&value) == nullptr, "null data is preserved");
    Require(ASN1_STRING_length_nid_postfix(&value) == -9, "length is returned without normalization");
    Require(ASN1_STRING_type_nid_postfix(&value) == 0x7fffffff, "type is returned without normalization");
    RequireInvalidArgument([] { ASN1_STRING_data_nid_postfix(nullptr); }, "null data accessor input");
    RequireInvalidArgument([] { ASN1_STRING_length_nid_postfix(nullptr); }, "null length accessor input");
    RequireInvalidArgument([] { ASN1_STRING_type_nid_postfix(nullptr); }, "null type accessor input");
}

void Integers() {
    Require(ASN1_INTEGER_get_nid_postfix(nullptr) == 0, "null integer");
    unsigned char data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0};
    constexpr std::array<std::int64_t, 9> expected{
        0, 0x01, 0x0123, 0x012345, 0x01234567, 0x0123456789, 0x0123456789ab,
        0x0123456789abcd, 0x0123456789abcdef};
    for (int length = 0; length <= 8; ++length) {
        Asn1String value{length, 2, data, -1};
        Require(ASN1_INTEGER_get_nid_postfix(&value) == expected[length], "positive integer and 64-bit guest long");
        value.type = 0x102;
        Require(ASN1_INTEGER_get_nid_postfix(&value) == -expected[length], "negative integer magnitude");
    }
    Asn1String value{9, 2, nullptr, 0};
    Require(ASN1_INTEGER_get_nid_postfix(&value) == -1, "overlength is rejected before null data");
    value.length = 8;
    Require(ASN1_INTEGER_get_nid_postfix(&value) == 0, "integer with null data");
    for (int length : {std::numeric_limits<std::int32_t>::min(), -8, -1, 0}) {
        for (int type : {2, 0x102}) {
            for (auto* bytes : {data, static_cast<unsigned char*>(nullptr), reinterpret_cast<unsigned char*>(1)}) {
                value = {length, type, bytes, -1};
                const Asn1String original = value;
                Require(ASN1_INTEGER_get_nid_postfix(&value) == 0, "LibreSSL 2.5.4 nonpositive integer length");
                Require(value.length == original.length && value.type == original.type && value.data == original.data
                    && value.flags == original.flags, "nonpositive integer length preserves input");
            }
        }
    }
    value = {-1, 2, data, 0};
    for (int type : {-1, 0, 1, 3, 12, 0x100, 0x202, 0x302}) {
        value.type = type;
        Require(ASN1_INTEGER_get_nid_postfix(&value) == -1, "only integer types are accepted");
    }
    std::array<unsigned char, 8> maximum{0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    value = {8, 2, maximum.data(), 0};
    Require(ASN1_INTEGER_get_nid_postfix(&value) == std::numeric_limits<std::int64_t>::max(), "maximum signed integer");
    value.type = 0x102;
    Require(ASN1_INTEGER_get_nid_postfix(&value) == -std::numeric_limits<std::int64_t>::max(), "negative maximum magnitude");
    std::array<unsigned char, 8> minimum{0x80, 0, 0, 0, 0, 0, 0, 0};
    value = {8, 0x102, minimum.data(), -1};
    const Asn1String original = value;
    Require(ASN1_INTEGER_get_nid_postfix(&value) == std::numeric_limits<std::int64_t>::min(), "minimum signed integer");
    Require(value.length == original.length && value.type == original.type && value.data == original.data
        && value.flags == original.flags && minimum == std::array<unsigned char, 8>{0x80, 0, 0, 0, 0, 0, 0, 0}, "minimum integer preserves input");
    value.type = 2;
    RequireThrows([&] { ASN1_INTEGER_get_nid_postfix(&value); }, "positive magnitude above signed maximum");
    value.type = 0x102;
    minimum.back() = 1;
    RequireThrows([&] { ASN1_INTEGER_get_nid_postfix(&value); }, "negative magnitude below signed minimum");
    value.data = maximum.data();
    maximum[0] = 0x80;
    for (int type : {2, 0x102}) {
        value.type = type;
        RequireThrows([&] { ASN1_INTEGER_get_nid_postfix(&value); }, "source signed-overflow path throws");
    }
    std::array<unsigned char, 9> zero{};
    value = {9, 2, zero.data(), 0};
    Require(ASN1_INTEGER_get_nid_postfix(&value) == -1, "overlength leading zeros are not trimmed");
}

void Strings() {
    constexpr unsigned char latin[] = {0, 0x41, 0x7f, 0x80, 0xa3, 0xff};
    constexpr unsigned char latinUtf8[] = {0, 0x41, 0x7f, 0xc2, 0x80, 0xc2, 0xa3, 0xc3, 0xbf};
    constexpr unsigned char bmp[] = {0, 0, 0, 0x7f, 0, 0x80, 7, 0xff, 8, 0, 0x20, 0xac, 0xd7, 0xff, 0xe0, 0, 0xff, 0xff};
    constexpr unsigned char bmpUtf8[] = {0, 0x7f, 0xc2, 0x80, 0xdf, 0xbf, 0xe0, 0xa0, 0x80, 0xe2, 0x82, 0xac,
        0xed, 0x9f, 0xbf, 0xee, 0x80, 0x80, 0xef, 0xbf, 0xbf};
    constexpr unsigned char universal[] = {0, 0, 0, 0x41, 0, 1, 0, 0, 0, 1, 0xf6, 0, 0, 0x10, 0xff, 0xff};
    constexpr unsigned char universalUtf8[] = {0x41, 0xf0, 0x90, 0x80, 0x80, 0xf0, 0x9f, 0x98, 0x80, 0xf4, 0x8f, 0xbf, 0xbf};
    for (int type : {19, 20, 22, 23, 24, 26}) CheckConversion(type, latin, latinUtf8);
    CheckConversion(30, bmp, bmpUtf8);
    CheckConversion(28, universal, universalUtf8);
    CheckConversion(12, bmpUtf8, bmpUtf8);
    CheckConversion(12, universalUtf8, universalUtf8);
    for (int type : {12, 19, 20, 22, 23, 24, 26, 28, 30}) {
        CheckConversion(type, {}, {});
        Asn1String empty{0, type, nullptr, 0};
        unsigned char* result = nullptr;
        Require(ASN1_STRING_to_UTF8_nid_postfix(&result, &empty) == 0 && result && *result == 0, "empty null-data string");
        free_nid_postfix(result);
    }
    constexpr unsigned char terminated[] = {'A', 0xc2, 0xa3, 0, 'X'};
    CheckConversion(12, terminated, std::span(terminated).first(3), -1);
    CheckConversion(12, terminated, std::span(terminated).first(3), -7);
    constexpr unsigned char ascii[] = {'A', 'B', 'C'};
    CheckConversion(19, ascii, ascii, -1);

    unsigned char sentinel = 0xab;
    unsigned char* result = &sentinel;
    Require(ASN1_STRING_to_UTF8_nid_postfix(&result, nullptr) == -1 && result == &sentinel, "null conversion input");
    Require(ASN1_STRING_to_UTF8_nid_postfix(nullptr, nullptr) == -1, "null input checked before output");
    for (int type = -1; type <= 32; ++type) {
        if (type == 12 || type == 19 || type == 20 || type == 22 || type == 23 || type == 24 || type == 26 || type == 28 || type == 30) continue;
        Asn1String value{0, type, nullptr, 0};
        Require(ASN1_STRING_to_UTF8_nid_postfix(&result, &value) == -1 && result == &sentinel, "unsupported string tag");
        Require(ASN1_STRING_to_UTF8_nid_postfix(nullptr, &value) == -1, "tag checked before output");
    }
    Asn1String value{0, 12, nullptr, 0};
    RequireThrows([&] { ASN1_STRING_to_UTF8_nid_postfix(nullptr, &value); }, "null conversion output");
    value.length = 1;
    RequireThrows([&] { ASN1_STRING_to_UTF8_nid_postfix(&result, &value); }, "null nonempty string data");
    value.length = -1;
    RequireThrows([&] { ASN1_STRING_to_UTF8_nid_postfix(&result, &value); }, "null terminated-string data");
}

std::size_t lastAllocationSize = 0;
int allocationCount = 0;
bool failAllocation = false;
int freeCount = 0;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    lastAllocationSize = size;
    ++allocationCount;
    return failAllocation ? nullptr : std::malloc(size);
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    ++freeCount;
    std::free(pointer);
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected calloc"); }
void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Allocator() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    constexpr unsigned char input[] = {0xff};
    constexpr unsigned char expected[] = {0xc3, 0xbf};
    CheckConversion(20, input, expected);
    Require(allocationCount == 1 && lastAllocationSize == 3 && freeCount == 1, "conversion uses the application allocator");
    constexpr unsigned char universal[] = {0, 0, 0, 'A', 0, 0, 0x03, 0xa9, 0, 0, 0x20, 0xac, 0, 0x01, 0xf6, 0x00};
    constexpr unsigned char mixed[] = {'A', 0xce, 0xa9, 0xe2, 0x82, 0xac, 0xf0, 0x9f, 0x98, 0x80};
    CheckConversion(28, universal, mixed);
    Require(allocationCount == 2 && lastAllocationSize == sizeof(mixed) + 1 && freeCount == 2,
        "a conversion with every UTF-8 length makes one allocation of the exact size");
    unsigned char badUtf8[] = {'A', 0xc0, 0x80};
    Asn1String malformed{3, 12, badUtf8, 0};
    unsigned char* untouched = nullptr;
    ERR_clear_error_nid_postfix();
    Require(ASN1_STRING_to_UTF8_nid_postfix(&untouched, &malformed) == -1 && untouched == nullptr && allocationCount == 2,
        "a malformed string allocates nothing");
    ERR_clear_error_nid_postfix();
    failAllocation = true;
    unsigned char sentinel = 0xab;
    unsigned char* result = &sentinel;
    unsigned char latin[] = {0xff};
    Asn1String converted{1, 20, latin, 0};
    ERR_clear_error_nid_postfix();
    Require(ASN1_STRING_to_UTF8_nid_postfix(&result, &converted) == -1 && result == &sentinel, "allocation failure of a converted string");
    Require(ERR_get_error_nid_postfix() == Asn1Error(65) && ERR_get_error_nid_postfix() == 0, "one malloc failure for a converted string");
    unsigned char utf8[] = {'A'};
    Asn1String copied{1, 12, utf8, 0};
    Require(ASN1_STRING_to_UTF8_nid_postfix(&result, &copied) == -1 && result == &sentinel, "allocation failure of a UTF8String copy");
    Require(ERR_get_error_nid_postfix() == Asn1Error(65) && ERR_get_error_nid_postfix() == Asn1Error(65) && ERR_get_error_nid_postfix() == 0,
        "ASN1_STRING_set and ASN1_mbstring_ncopy both report the malloc failure of a UTF8String copy");
    failAllocation = false;
}

} // namespace

int main(int argc, char**) {
    if (argc > 1) {
        Allocator();
        return 0;
    }
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Accessors();
    Integers();
    Strings();
    MalformedStrings();
}

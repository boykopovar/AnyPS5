#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <source_location>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>


namespace {

using String = Asn1String;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 ASN.1 check failed at line %u\n", location.line());
    std::abort();
}

std::unordered_map<void*, std::size_t> allocations;
std::size_t freeCalls = 0;
void* dataToClear = nullptr;
std::size_t clearLength = 0;
int allocationsUntilFailure = -1;

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    if (allocationsUntilFailure == 0) {
        allocationsUntilFailure = -1;
        return nullptr;
    }
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    void* result = std::malloc(size);
    Require(result != nullptr);
    allocations.emplace(result, size);
    return result;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    const auto found = allocations.find(pointer);
    Require(found != allocations.end());
    if (found->second == sizeof(String)) {
        const auto* value = static_cast<const String*>(pointer);
        Require(value->length == 0 && value->data == nullptr && !(value->flags & 0x10));
    }
    Require(allocations.erase(pointer) == 1);
    if (pointer == dataToClear) {
        const auto* bytes = static_cast<const unsigned char*>(pointer);
        Require(std::all_of(bytes, bytes + clearLength, [](auto byte) { return byte == 0; }));
        dataToClear = nullptr;
    }
    ++freeCalls;
    std::free(pointer);
}

void* APS5_VABI UnexpectedCalloc_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected calloc"); }
void* APS5_VABI UnexpectedReallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void Release(String* value) {
    if (!value) return;
    const auto data = value->data;
    value->length = 0;
    value->data = nullptr;
    value->flags &= ~std::int64_t{0x10};
    free_nid_postfix(data);
    free_nid_postfix(value);
}

void Check(std::span<const unsigned char> encoded, std::span<const unsigned char> magnitude, bool negative = false) {
    std::vector<unsigned char> bytes(encoded.begin(), encoded.end());
    bytes.push_back(0xa5);
    const unsigned char* cursor = bytes.data();
    void* output = nullptr;
    auto* value = static_cast<String*>(d2i_ASN1_INTEGER_nid_postfix(&output, &cursor, bytes.size()));
    Require(value && output == value && cursor == bytes.data() + encoded.size());
    Require(*cursor == 0xa5 && value->length == static_cast<int>(magnitude.size()));
    Require(value->type == (negative ? 0x102 : 2) && value->flags == 0);
    Require(value->data != bytes.data());
    Require(std::equal(magnitude.begin(), magnitude.end(), value->data));
    Require(std::equal(encoded.begin(), encoded.end(), bytes.begin()));
    Release(value);
}

void Check(std::initializer_list<unsigned char> encoded, std::initializer_list<unsigned char> magnitude, bool negative = false) {
    Check(std::span(encoded.begin(), encoded.size()), std::span(magnitude.begin(), magnitude.size()), negative);
}

void Integers() {
    Check({2, 1, 0}, {0});
    Check({2, 2, 0, 0x80}, {0x80});
    Check({2, 1, 0xff}, {1}, true);
    Check({2, 1, 0x80}, {0x80}, true);
    Check({2, 2, 0xff, 0x7f}, {0x81}, true);
    Check({2, 2, 0xff, 0}, {1, 0}, true);
    Check({2, 3, 0xff, 0, 0}, {1, 0, 0}, true);
    Check({2, 0x82, 0, 1, 0x7f}, {0x7f});
    Check({0x1f, 0x80, 2, 1, 0x7f}, {0x7f});
    Check({2, 9, 0, 0x80, 0, 0, 0, 0, 0, 0, 0}, {0x80, 0, 0, 0, 0, 0, 0, 0});
    Check({2, 8, 0x80, 0, 0, 0, 0, 0, 0, 0}, {0x80, 0, 0, 0, 0, 0, 0, 0}, true);
    std::vector<unsigned char> large(131, 0xff);
    large[0] = 2;
    large[1] = 0x81;
    large[2] = 128;
    large[3] = 0x7f;
    std::vector<unsigned char> expected(128, 0xff);
    expected[0] = 0x7f;
    Check(large, expected);
    for (int number = -32768; number <= 32767; ++number) {
        const auto bits = static_cast<std::uint16_t>(number);
        std::array<unsigned char, 4> encoded{2, 2, static_cast<unsigned char>(bits >> 8), static_cast<unsigned char>(bits)};
        std::span<const unsigned char> content(encoded);
        if (number >= -128 && number <= 127) {
            encoded[1] = 1;
            encoded[2] = encoded[3];
            content = content.first(3);
        }
        const auto absolute = static_cast<unsigned int>(number < 0 ? -number : number);
        std::array<unsigned char, 2> magnitude{static_cast<unsigned char>(absolute >> 8), static_cast<unsigned char>(absolute)};
        Check(content, std::span(magnitude).subspan(absolute < 256 ? 1 : 0), number < 0);
    }
}

void Ownership() {
    const unsigned char encoded[] = {2, 1, 42};
    const unsigned char* cursor = encoded;
    auto* value = static_cast<String*>(d2i_ASN1_INTEGER_nid_postfix(nullptr, &cursor, sizeof(encoded)));
    void* output = value;
    dataToClear = value->data;
    clearLength = 1;
    const auto previousFreeCalls = freeCalls;
    cursor = encoded;
    value = static_cast<String*>(d2i_ASN1_INTEGER_nid_postfix(&output, &cursor, sizeof(encoded)));
    Require(output == value && value->data[0] == 42 && freeCalls == previousFreeCalls + 2 && dataToClear == nullptr);
    Release(value);
    auto* borrowed = static_cast<String*>(Allocate_nid_no_patch(sizeof(String)));
    unsigned char bytes[] = {0xa5, 0x5a};
    *borrowed = {2, 2, bytes, 0x10};
    output = borrowed;
    cursor = encoded;
    value = static_cast<String*>(d2i_ASN1_INTEGER_nid_postfix(&output, &cursor, sizeof(encoded)));
    Require(bytes[0] == 0xa5 && bytes[1] == 0x5a);
    Release(value);
    for (const auto length : {0, -1, std::numeric_limits<int>::min()}) {
        auto* empty = static_cast<String*>(Allocate_nid_no_patch(sizeof(String)));
        *empty = {length, 2, nullptr, 0};
        output = empty;
        cursor = encoded;
        const auto before = freeCalls;
        value = static_cast<String*>(d2i_ASN1_INTEGER_nid_postfix(&output, &cursor, sizeof(encoded)));
        Require(value == output && value->data[0] == 42 && freeCalls == before + 1);
        Release(value);
    }
    Require(allocations.empty());

}

void BerExtremes() {
    std::vector<unsigned char> negative(131, 0);
    negative[0] = 2;
    negative[1] = 0x81;
    negative[2] = 128;
    negative[3] = 0xff;
    std::vector<unsigned char> magnitude(128, 0);
    magnitude[0] = 1;
    Check(negative, magnitude, true);
    std::vector<unsigned char> positive(132, 0);
    positive[0] = 2;
    positive[1] = 0x81;
    positive[2] = 129;
    positive[4] = 0x80;
    magnitude[0] = 0x80;
    Check(positive, magnitude);
    std::vector<unsigned char> tag(1004, 0x80);
    tag[0] = 0x1f;
    tag[1001] = 2;
    tag[1002] = 1;
    tag[1003] = 42;
    const unsigned char expected[] = {42};
    Check(tag, expected);
    std::vector<unsigned char> length(129, 0);
    length[0] = 2;
    length[1] = 0xfe;
    length[127] = 1;
    length[128] = 42;
    Check(length, expected);
}

constexpr std::uint64_t Asn1Error(std::uint64_t reason) {
    return 0x0dfff000 | reason;
}

constexpr std::uint64_t NestedError = Asn1Error(0x3a);
constexpr std::uint64_t LengthMismatchError = Asn1Error(0x3e);
constexpr std::uint64_t MallocError = Asn1Error(0x41);
constexpr std::uint64_t BadHeaderError = Asn1Error(0x66);
constexpr std::uint64_t TooLongError = Asn1Error(0x9b);
constexpr std::uint64_t WrongTagError = Asn1Error(0xa8);
constexpr std::uint64_t NotPrimitiveError = Asn1Error(0xe7);

void RequireQueue(std::initializer_list<std::uint64_t> expected) {
    for (const auto code : expected) Require(ERR_get_error_nid_postfix() == code);
    Require(ERR_get_error_nid_postfix() == 0);
}

String* OwnedInteger() {
    const unsigned char encoded[] = {2, 1, 42};
    const unsigned char* cursor = encoded;
    return static_cast<String*>(d2i_ASN1_INTEGER_nid_postfix(nullptr, &cursor, sizeof(encoded)));
}

void RejectKeepsOutput(std::span<const unsigned char> encoded, std::int64_t length, std::initializer_list<std::uint64_t> errors) {
    const unsigned char* cursor = encoded.data();
    Require(d2i_ASN1_INTEGER_nid_postfix(nullptr, &cursor, length) == nullptr);
    Require(cursor == encoded.data());
    RequireQueue(errors);
    auto* previous = OwnedInteger();
    void* output = previous;
    const auto before = freeCalls;
    Require(d2i_ASN1_INTEGER_nid_postfix(&output, &cursor, length) == nullptr);
    Require(cursor == encoded.data() && output == previous && freeCalls == before && previous->data[0] == 42);
    RequireQueue(errors);
    Release(previous);
}

void RejectKeepsOutput(std::initializer_list<unsigned char> encoded, std::initializer_list<std::uint64_t> errors) {
    RejectKeepsOutput(std::span(encoded.begin(), encoded.size()), static_cast<std::int64_t>(encoded.size()), errors);
}

void RejectFreesOutput(std::initializer_list<unsigned char> encoded) {
    const std::span bytes(encoded.begin(), encoded.size());
    const unsigned char* cursor = bytes.data();
    Require(d2i_ASN1_INTEGER_nid_postfix(nullptr, &cursor, static_cast<std::int64_t>(bytes.size())) == nullptr);
    Require(cursor == bytes.data());
    RequireQueue({LengthMismatchError});
    auto* previous = OwnedInteger();
    void* output = previous;
    dataToClear = previous->data;
    clearLength = 1;
    const auto before = freeCalls;
    Require(d2i_ASN1_INTEGER_nid_postfix(&output, &cursor, static_cast<std::int64_t>(bytes.size())) == nullptr);
    Require(cursor == bytes.data() && output == nullptr && freeCalls == before + 2 && dataToClear == nullptr);
    RequireQueue({LengthMismatchError});
}

void Rejections() {
    RejectKeepsOutput({0x03, 1, 42}, {WrongTagError, NestedError});
    RejectKeepsOutput({0x42, 1, 42}, {WrongTagError, NestedError});
    RejectKeepsOutput({0x82, 1, 42}, {WrongTagError, NestedError});
    RejectKeepsOutput({0x03, 0xff}, {WrongTagError, NestedError});
    RejectKeepsOutput({0x22, 1, 42}, {NotPrimitiveError});
    RejectKeepsOutput({0x22, 0x80, 0, 0}, {NotPrimitiveError});
    RejectKeepsOutput({0x22, 2, 42}, {TooLongError, NestedError});
    RejectKeepsOutput({2, 0x80, 42, 0, 0}, {BadHeaderError, NestedError});
    RejectKeepsOutput({2, 0xff, 1, 42}, {BadHeaderError, NestedError});
    RejectKeepsOutput({2, 0x82, 0}, {BadHeaderError, NestedError});
    RejectKeepsOutput({2, 0x89, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {BadHeaderError, NestedError});
    RejectKeepsOutput({0x1f}, {BadHeaderError, NestedError});
    RejectKeepsOutput({0x1f, 0x90, 0x80, 0x80, 0x80, 0x80, 2, 1, 42}, {BadHeaderError, NestedError});
    RejectKeepsOutput({2, 2, 42}, {TooLongError, NestedError});
    const unsigned char integer[] = {2, 1, 42};
    RejectKeepsOutput(integer, 0, {BadHeaderError, NestedError});
    RejectKeepsOutput(integer, 1, {BadHeaderError, NestedError});
    RejectKeepsOutput(integer, 2, {TooLongError, NestedError});
    for (const auto length : std::initializer_list<std::int64_t>{-1, -2, -1000, std::numeric_limits<std::int32_t>::min(), -(std::int64_t{1} << 40)})
        RejectKeepsOutput(integer, length, {});
    const unsigned char oversized[] = {2, 0x84, 0x80, 0, 0, 0};
    RejectKeepsOutput(oversized, (std::int64_t{1} << 31) + sizeof(oversized), {});
    RejectFreesOutput({2, 0});
    RejectFreesOutput({2, 2, 0, 0x7f});
    RejectFreesOutput({2, 2, 0xff, 0x80});
    RejectFreesOutput({2, 2, 0, 0});
    RejectFreesOutput({2, 3, 0xff, 0xff, 0});
    Require(allocations.empty());
}

void AllocationFailures() {
    const unsigned char positive[] = {2, 1, 42};
    const unsigned char negative[] = {2, 1, 0xd6};
    for (const auto encoded : {std::span<const unsigned char>(positive), std::span<const unsigned char>(negative)}) {
        for (const bool replace : {false, true}) {
            for (const int failure : {0, 1}) {
                void* output = replace ? OwnedInteger() : nullptr;
                const unsigned char* cursor = encoded.data();
                allocationsUntilFailure = failure;
                Require(d2i_ASN1_INTEGER_nid_postfix(&output, &cursor, static_cast<std::int64_t>(encoded.size())) == nullptr);
                Require(allocationsUntilFailure == -1 && output == nullptr && cursor == encoded.data());
                if (failure == 0) RequireQueue({});
                else RequireQueue({MallocError, MallocError});
                Require(allocations.empty());
            }
        }
    }
}

} // namespace

int main() {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(UnexpectedCalloc_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedReallocate_nid_no_patch), reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Integers();
    BerExtremes();
    Ownership();
    Rejections();
    AllocationFailures();
    Require(allocations.empty());
    std::puts("libSceLibreSsl3 ASN.1 tests passed");
}

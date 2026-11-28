#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

constexpr auto InsertCString = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmPKc_nid_postfix;
constexpr auto InsertCounted = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmPKcm_nid_postfix;
constexpr auto InsertIterator = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertENS_11__wrap_iterIPKcEEc_nid_postfix;
constexpr auto Reserve = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7reserveEm_nid_postfix;
constexpr auto Destroy = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED2Ev_nid_postfix;

void Require(bool condition, const char* failure) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", failure);
    std::abort();
}

struct alignas(8) GuestString {
    std::array<unsigned char, 24> bytes{};

    std::uint64_t Field(std::size_t offset) const {
        std::uint64_t value;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }

    bool IsLong() const { return (bytes[0] & 1u) != 0; }

    const char* Data() const {
        return IsLong() ? reinterpret_cast<const char*>(Field(16)) : reinterpret_cast<const char*>(bytes.data() + 1);
    }

    void RequireContent(std::string_view expected) const {
        const auto size = IsLong() ? Field(8) : static_cast<std::uint64_t>(bytes[0] >> 1u);
        Require(size == expected.size(), "wrong string size after insertion");
        Require(std::memcmp(Data(), expected.data(), expected.size()) == 0, "wrong string bytes after insertion");
        Require(Data()[size] == '\0', "insertion failed to terminate the string");
    }
};

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(ApplicationHeapDefault_nid_no_patch(), "the fixture requires the guest heap");

    GuestString string;
    Require(InsertCString(&string, 0, "abcd") == &string, "C-string insertion returned the wrong object");
    constexpr char input[]{'X', '\0', 'Y'};
    Require(InsertCounted(&string, 1, input, sizeof(input)) == &string, "counted insertion returned the wrong object");
    std::string expected = "abcd";
    expected.insert(1, input, sizeof(input));
    string.RequireContent(expected);
    const auto* position = string.Data() + 5;
    Require(InsertIterator(&string, position, 'Z') == position, "inline iterator insertion returned the wrong address");
    expected.insert(5, 1, 'Z');
    string.RequireContent(expected);
    const std::string localAlias(expected.data() + 2, 4);
    InsertCounted(&string, 3, string.Data() + 2, localAlias.size());
    expected.insert(3, localAlias);
    string.RequireContent(expected);

    std::string bulk(8321, '\0');
    for (std::size_t i = 0; i < bulk.size(); ++i) bulk[i] = static_cast<char>(i % 127);
    InsertCounted(&string, 2, bulk.data(), bulk.size());
    expected.insert(2, bulk);
    Require(string.IsLong(), "bulk insertion did not allocate long storage");
    string.RequireContent(expected);
    Reserve(&string, 28000);
    const auto* reserved = string.Data();
    const std::string longAlias(expected.data() + 713, 5702);
    InsertCounted(&string, 911, reserved + 713, longAlias.size());
    expected.insert(911, longAlias);
    Require(string.Data() == reserved, "overlapping insertion discarded usable allocation");
    string.RequireContent(expected);
    const auto* end = string.Data() + expected.size();
    Require(InsertIterator(&string, end, 'T') == end, "end iterator insertion returned the wrong address");
    expected.push_back('T');
    string.RequireContent(expected);

    const auto beforeFailure = string.bytes;
    Require(InsertCounted(&string, expected.size(), nullptr, 0) == &string, "empty insertion returned the wrong object");
    Require(string.bytes == beforeFailure, "empty insertion changed the representation");
    try {
        InsertCounted(&string, expected.size() + 1, nullptr, 0);
        Require(false, "invalid position should throw a guest out-of-range error");
    } catch (const std::out_of_range&) {
        Require(string.bytes == beforeFailure, "invalid insertion changed the representation");
        string.RequireContent(expected);
    }
    try {
        InsertIterator(&string, input, 'Q');
        Require(false, "foreign iterator should throw a guest invalid-argument error");
    } catch (const std::invalid_argument&) {
        Require(string.bytes == beforeFailure, "invalid iterator changed the representation");
    }
    try {
        InsertCounted(&string, 0, input, std::numeric_limits<std::uint64_t>::max());
        Require(false, "oversized insertion should throw a guest length error");
    } catch (const std::length_error&) {
        Require(string.bytes == beforeFailure, "oversized insertion changed the representation");
    }
    Destroy(&string);

    GuestString full;
    InsertCString(&full, 0, "aaaaaaaaaaaaaaaaaaaaaa");
    const auto* result = InsertIterator(&full, full.Data() + 11, 'B');
    Require(full.IsLong() && result == full.Data() + 11 && *result == 'B', "iterator result did not follow reallocation");
    full.RequireContent("aaaaaaaaaaaBaaaaaaaaaaa");
    Destroy(&full);
    std::puts("libc++ string insertion ABI checks passed");
    return 0;
}

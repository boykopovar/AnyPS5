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

constexpr auto ReplaceCounted = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7replaceEmmPKcm_nid_postfix;
constexpr auto ReplaceRepeated = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7replaceEmmmc_nid_postfix;
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
        Require(size == expected.size(), "wrong string size after replacement");
        Require(std::memcmp(Data(), expected.data(), expected.size()) == 0, "wrong string bytes after replacement");
        Require(Data()[size] == '\0', "replacement failed to terminate the string");
    }
};

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(ApplicationHeapDefault_nid_no_patch(), "the fixture requires the guest heap");

    constexpr auto npos = std::numeric_limits<std::uint64_t>::max();
    GuestString string;
    Require(ReplaceCounted(&string, 0, 10, "abcdef", 6) == &string, "counted replacement returned the wrong object");
    std::string expected = "abcdef";
    constexpr char input[]{'X', '\0', 'Y'};
    ReplaceCounted(&string, 2, 3, input, sizeof(input));
    expected.replace(2, 3, input, sizeof(input));
    string.RequireContent(expected);
    Require(ReplaceRepeated(&string, 3, npos, 2, '!') == &string, "repeated replacement returned the wrong object");
    expected.replace(3, std::string::npos, 2, '!');
    string.RequireContent(expected);

    std::string bulk(9401, '\0');
    for (std::size_t i = 0; i < bulk.size(); ++i) bulk[i] = static_cast<char>(i % 127);
    ReplaceCounted(&string, 1, 1, bulk.data(), bulk.size());
    expected.replace(1, 1, bulk);
    Require(string.IsLong(), "large replacement failed to allocate long storage");
    string.RequireContent(expected);
    Reserve(&string, 24000);
    const auto* storage = string.Data();
    const auto capacity = string.Field(0);

    const std::string growthSource(expected.data() + 2200, 5371);
    ReplaceCounted(&string, 712, 91, storage + 2200, growthSource.size());
    expected.replace(712, 91, growthSource);
    Require(string.Data() == storage, "aliased expansion discarded usable allocation");
    string.RequireContent(expected);
    const std::string shrinkSource(expected.data() + 4096, 4123);
    ReplaceCounted(&string, 471, 7334, storage + 4096, shrinkSource.size());
    expected.replace(471, 7334, shrinkSource);
    Require(string.Data() == storage && string.Field(0) == capacity, "aliased shrink discarded usable allocation");
    string.RequireContent(expected);

    const auto beforeFailure = string.bytes;
    try {
        ReplaceRepeated(&string, 0, npos, npos, 'q');
        Require(false, "oversized replacement should throw a guest length error");
    } catch (const std::length_error&) {
        Require(string.bytes == beforeFailure, "oversized replacement changed the string representation");
    }
    try {
        ReplaceCounted(&string, expected.size() + 1, 0, nullptr, 0);
        Require(false, "invalid replacement position should throw a guest out-of-range error");
    } catch (const std::out_of_range&) {
        Require(string.bytes == beforeFailure, "invalid replacement changed the string representation");
        string.RequireContent(expected);
    }
    Require(ReplaceCounted(&string, 0, npos, nullptr, 0) == &string, "empty replacement returned the wrong object");
    Require(string.IsLong() && string.Data() == storage && string.Field(0) == capacity, "replacement released storage on clear");
    string.RequireContent("");
    Destroy(&string);
    std::puts("libc++ string replacement ABI checks passed");
    return 0;
}

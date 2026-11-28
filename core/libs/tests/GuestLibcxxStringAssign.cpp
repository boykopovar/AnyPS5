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

constexpr auto AssignCString = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKc_nid_postfix;
constexpr auto AssignCounted = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKcm_nid_postfix;
constexpr auto AssignRepeated = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEmc_nid_postfix;
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
        Require(size == expected.size(), "wrong string size after assignment");
        Require(std::memcmp(Data(), expected.data(), expected.size()) == 0, "wrong string bytes after assignment");
        Require(Data()[size] == '\0', "assignment failed to terminate the string");
    }
};

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(ApplicationHeapDefault_nid_no_patch(), "the fixture requires the guest heap");

    GuestString string;
    Require(AssignCString(&string, "abcdef") == &string, "C-string assignment returned the wrong object");
    Require(AssignCString(&string, string.Data() + 2) == &string, "aliased C-string assignment returned the wrong object");
    string.RequireContent("cdef");
    Require(AssignRepeated(&string, 22, 'z') == &string, "repeated assignment returned the wrong object");
    Require(!string.IsLong(), "assignment allocated within inline capacity");
    string.RequireContent("zzzzzzzzzzzzzzzzzzzzzz");
    const std::string withOldTerminator(string.Data(), 23);
    Require(AssignCounted(&string, string.Data(), 23) == &string, "counted assignment returned the wrong object");
    Require(string.IsLong(), "assignment failed to grow past inline capacity");
    string.RequireContent(withOldTerminator);

    std::string payload(9281, '\0');
    for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>(i % 127);
    Require(AssignCounted(&string, payload.data(), payload.size()) == &string, "bulk assignment returned the wrong object");
    string.RequireContent(payload);
    const auto* data = string.Data();
    const auto capacity = string.Field(0);
    const std::string overlap(payload.c_str() + 7, payload.size() - 6);
    Require(AssignCounted(&string, data + 7, overlap.size()) == &string, "overlapping assignment returned the wrong object");
    Require(string.Data() == data && string.Field(0) == capacity, "assignment discarded usable allocation");
    string.RequireContent(overlap);

    const auto beforeFailure = string.bytes;
    try {
        AssignRepeated(&string, std::numeric_limits<std::uint64_t>::max(), 'q');
        Require(false, "oversized assignment should throw a guest length error");
    } catch (const std::length_error&) {
        Require(string.bytes == beforeFailure, "oversized assignment changed the string representation");
        string.RequireContent(overlap);
    }
    Require(AssignCounted(&string, nullptr, 0) == &string, "empty assignment returned the wrong object");
    Require(string.IsLong() && string.Data() == data && string.Field(0) == capacity, "empty assignment released storage");
    string.RequireContent("");
    Destroy(&string);
    std::puts("libc++ string assignment ABI checks passed");
    return 0;
}

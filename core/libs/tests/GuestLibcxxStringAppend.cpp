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

constexpr auto AppendCString = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKc_nid_postfix;
constexpr auto AppendCounted = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKcm_nid_postfix;
constexpr auto AppendRepeated = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEmc_nid_postfix;
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
        Require(size == expected.size(), "append set the wrong guest size");
        Require(std::memcmp(Data(), expected.data(), expected.size()) == 0, "append changed the expected bytes");
        Require(Data()[size] == '\0', "append did not write a terminator at the new size");
    }
};

void RequireFreed(const void* pointer) {
    try {
        static_cast<void>(ApplicationHeapUsableSize_nid_no_patch(const_cast<void*>(pointer)));
    } catch (const std::exception&) {
        return;
    }
    Require(false, "append left the previous guest allocation live");
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(ApplicationHeapDefault_nid_no_patch(), "the fixture requires the default guest heap");

    GuestString string;
    Require(AppendCString(&string, "abcdef") == &string, "C-string append returned the wrong object");
    const auto* inlineBytes = string.Data();
    Require(AppendCString(&string, inlineBytes + 2) == &string, "inline suffix append returned the wrong object");
    string.RequireContent("abcdefcdef");
    constexpr char counted[]{'x', '\0', 'y'};
    Require(AppendCounted(&string, counted, sizeof(counted)) == &string, "counted append returned the wrong object");
    std::string expected = "abcdefcdef";
    expected.append(counted, sizeof(counted));
    string.RequireContent(expected);
    Require(AppendRepeated(&string, 9, 'z') == &string, "repeated append returned the wrong object");
    expected.append(9, 'z');
    Require(!string.IsLong() && string.Data() == inlineBytes, "append promoted a string that fit inline");
    string.RequireContent(expected);

    Require(AppendRepeated(&string, 1, 'Q') == &string, "promotion returned the wrong object");
    expected += 'Q';
    Require(string.IsLong(), "append failed to promote a full inline string");
    string.RequireContent(expected);
    const auto* oldBytes = string.Data();
    const auto capacity = (string.Field(0) & ~std::uint64_t{1}) - 1u;
    const auto aliasSize = expected.size();
    if (capacity >= aliasSize * 2) {
        const auto extra = static_cast<std::size_t>(capacity + 1u - aliasSize * 2);
        AppendRepeated(&string, extra, 'w');
        expected.append(extra, 'w');
    }
    Require(AppendCounted(&string, oldBytes, aliasSize) == &string, "growing self-append returned the wrong object");
    expected.append(expected.data(), aliasSize);
    Require(string.Data() != oldBytes, "self-append failed to allocate space before copying an aliased source");
    string.RequireContent(expected);
    RequireFreed(oldBytes);

    Reserve(&string, 18000);
    const auto* reservedBytes = string.Data();
    Require(AppendCounted(&string, reservedBytes + 9, 5) == &string, "reserved self-append returned the wrong object");
    expected.append(expected.data() + 9, 5);
    Require(string.Data() == reservedBytes, "append discarded usable long-string capacity");
    string.RequireContent(expected);

    std::string bulk(8301, 'b');
    for (std::size_t i = 0; i < bulk.size(); ++i) bulk[i] = static_cast<char>(i % 127);
    AppendCounted(&string, bulk.data(), bulk.size());
    expected += bulk;
    Require(string.Data() == reservedBytes, "multiblock append replaced a sufficient allocation");
    string.RequireContent(expected);

    const auto previousSize = expected.size();
    const std::string overlap(expected.c_str() + 1, previousSize);
    Require(AppendCounted(&string, reservedBytes + 1, previousSize) == &string,
        "multiblock overlapping append returned the wrong object");
    expected += overlap;
    Require(string.Data() == reservedBytes, "overlapping append replaced a sufficient allocation");
    string.RequireContent(expected);

    const auto beforeFailure = string.bytes;
    Require(AppendCounted(&string, nullptr, 0) == &string, "zero counted append returned the wrong object");
    Require(AppendRepeated(&string, 0, 'a') == &string, "zero repeated append returned the wrong object");
    Require(string.bytes == beforeFailure, "zero append changed the representation");
    try {
        AppendRepeated(&string, std::numeric_limits<std::uint64_t>::max(), 'u');
        Require(false, "oversized append should throw a guest length error");
    } catch (const std::length_error&) {
        Require(string.bytes == beforeFailure, "oversized append modified the representation");
        string.RequireContent(expected);
    }
    Destroy(&string);
    RequireFreed(reservedBytes);
    std::puts("libc++ string append ABI checks passed");
    return 0;
}

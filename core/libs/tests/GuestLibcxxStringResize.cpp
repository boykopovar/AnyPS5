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

constexpr auto Resize = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6resizeEmc_nid_postfix;
constexpr auto PushBack = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE9push_backEc_nid_postfix;
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

    void RequireContent(std::string_view expected) const {
        const auto size = IsLong() ? Field(8) : static_cast<std::uint64_t>(bytes[0] >> 1u);
        const auto* data = IsLong() ? reinterpret_cast<const char*>(Field(16)) : reinterpret_cast<const char*>(bytes.data() + 1);
        Require(size == expected.size(), "wrong logical string size");
        Require(std::memcmp(data, expected.data(), expected.size()) == 0, "wrong resized string bytes");
        Require(data[size] == '\0', "missing terminator after resize");
    }
};

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(ApplicationHeapDefault_nid_no_patch(), "the fixture requires the guest heap");
    GuestString string;
    Resize(&string, 22, 'a');
    Require(!string.IsLong(), "resize promoted a string that fits inline");
    string.RequireContent("aaaaaaaaaaaaaaaaaaaaaa");
    PushBack(&string, 'b');
    Require(string.IsLong(), "push_back did not promote the full inline string");
    std::string expected = "aaaaaaaaaaaaaaaaaaaaaab";
    string.RequireContent(expected);

    Resize(&string, 8207, '\0');
    expected.resize(8207, '\0');
    string.RequireContent(expected);
    const auto longAddress = string.Field(16);
    const auto longCapacity = string.Field(0);
    const auto unchanged = string.bytes;
    Resize(&string, expected.size(), 'X');
    Require(string.bytes == unchanged, "same-size resize changed the representation");
    string.RequireContent(expected);

    Resize(&string, 4, 'x');
    string.RequireContent("aaaa");
    Resize(&string, 0, 'x');
    string.RequireContent("");
    PushBack(&string, 'y');
    string.RequireContent("y");
    Require(string.IsLong() && string.Field(16) == longAddress && string.Field(0) == longCapacity,
        "shrinking or subsequent growth discarded the existing allocation");
    const auto beforeOverflow = string.bytes;
    try {
        Resize(&string, std::numeric_limits<std::uint64_t>::max(), 'z');
        Require(false, "oversized resize should throw a guest length error");
    } catch (const std::length_error&) {
        Require(string.bytes == beforeOverflow, "oversized resize changed the existing string");
        string.RequireContent("y");
    }
    Destroy(&string);

    GuestString shortString;
    PushBack(&shortString, 'z');
    Resize(&shortString, 0, 'x');
    Require(!shortString.IsLong(), "short shrink changed the representation kind");
    shortString.RequireContent("");
    std::puts("libc++ string resize ABI checks passed");
    return 0;
}

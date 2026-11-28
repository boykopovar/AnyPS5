#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

constexpr auto Reserve = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7reserveEm_nid_postfix;
constexpr auto Destroy = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED2Ev_nid_postfix;

struct alignas(8) String {
    std::array<char, 24> bytes{};
};

std::uint64_t Field(const String& string, std::size_t offset) {
    std::uint64_t value;
    std::memcpy(&value, string.bytes.data() + offset, sizeof(value));
    return value;
}

void Require(bool condition, const char* failure) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", failure);
    std::abort();
}

void RequireFreed(void* pointer) {
    try {
        static_cast<void>(ApplicationHeapUsableSize_nid_no_patch(pointer));
    } catch (const std::exception&) {
        return;
    }
    Require(false, "the previous guest allocation is still live");
}

}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(ApplicationHeapDefault_nid_no_patch(), "the fixture requires the default guest heap");
    String string;
    string.bytes[0] = 8;
    constexpr std::array contents{'A', '\0', 'B', 'C', '\0'};
    std::memcpy(string.bytes.data() + 1, contents.data(), contents.size());
    const String original = string;
    Reserve(&string, 22);
    Require(string.bytes == original.bytes, "reserve grew inside inline capacity");

    Reserve(&string, 23);
    auto* first = reinterpret_cast<void*>(Field(string, 16));
    const auto allocated = Field(string, 0) & ~std::uint64_t{1};
    Require(Field(string, 0) == 33 && Field(string, 8) == 4, "wrong promoted string layout");
    Require(first && ApplicationHeapUsableSize_nid_no_patch(first) >= allocated, "storage did not use the guest heap");
    Require(std::memcmp(first, contents.data(), contents.size()) == 0, "promotion lost bytes or the terminator");
    const auto unchanged = string.bytes;
    Reserve(&string, allocated - 1);
    Reserve(&string, 2);
    Require(string.bytes == unchanged, "reserve changed storage within its current capacity");

    Reserve(&string, 8192);
    auto* second = reinterpret_cast<void*>(Field(string, 16));
    Require(second && second != first && Field(string, 8) == 4, "long reserve did not replace the allocation");
    Require((Field(string, 0) & ~std::uint64_t{1}) >= 8193, "long reserve returned insufficient capacity");
    Require(ApplicationHeapUsableSize_nid_no_patch(second) >= 8193, "long reserve lost guest heap ownership");
    Require(std::memcmp(second, contents.data(), contents.size()) == 0, "long reserve lost inline-origin data");
    RequireFreed(first);

    std::array<char, 6151> data{};
    for (std::size_t i = 0; i < data.size() - 1; ++i) data[i] = static_cast<char>(i % 127);
    std::memcpy(second, data.data(), data.size());
    constexpr std::uint64_t logicalSize = 6150;
    std::memcpy(string.bytes.data() + 8, &logicalSize, sizeof(logicalSize));
    Reserve(&string, 12288);
    auto* third = reinterpret_cast<void*>(Field(string, 16));
    Require(third && third != second && Field(string, 8) == logicalSize, "block copy changed string size");
    Require(std::memcmp(third, data.data(), data.size()) == 0, "block copy lost bytes or the terminator");
    RequireFreed(second);

    const auto beforeFailure = string.bytes;
    constexpr auto maxCapacity = std::numeric_limits<std::uint64_t>::max() - 17u;
    for (const auto capacity : {std::uint64_t{1} << 63u, maxCapacity}) {
        try {
            Reserve(&string, capacity);
            Require(false, "an impractically large reserve should fail allocation");
        } catch (const std::bad_alloc&) {
            Require(string.bytes == beforeFailure, "allocation failure modified the string");
        }
    }
    for (const auto capacity : {maxCapacity + 1u, std::numeric_limits<std::uint64_t>::max()}) {
        try {
            Reserve(&string, capacity);
            Require(false, "oversized reserve should throw a guest length error");
        } catch (const std::length_error&) {
            Require(string.bytes == beforeFailure, "oversized reserve modified the string");
        }
    }
    Destroy(&string);
    RequireFreed(third);
    String other = original;
    Destroy(&other);
    std::puts("libc++ string storage ABI checks passed");
    return 0;
}

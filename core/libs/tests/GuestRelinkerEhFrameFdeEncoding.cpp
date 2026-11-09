#include "prx/libc/include/exceptions/CompactUnwind.hpp"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "guest_relinker_ehframe_fde_encoding: %s\n", message);
        std::abort();
    }
}

void Append32(std::vector<unsigned char>& bytes, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<unsigned char>(value >> shift));
}

} // namespace

int main() {
    std::vector<unsigned char> bytes;
    Append32(bytes, 1);
    Append32(bytes, 0x1c);
    Append32(bytes, 1);
    Append32(bytes, 0x20);
    Append32(bytes, 2);
    Append32(bytes, 0x30);
    Append32(bytes, 3);
    Append32(bytes, 0x02010000u);
    Append32(bytes, 0x40);
    Append32(bytes, static_cast<std::uint32_t>(-8));
    Append32(bytes, 0x1000);
    Append32(bytes, 0x0abc);
    Append32(bytes, 0x1000);
    Append32(bytes, 0x54);
    Append32(bytes, 0x28);
    Append32(bytes, 0x2000);
    Append32(bytes, 0x84);
    Append32(bytes, 0x30);
    Append32(bytes, 0x3000);
    Append32(bytes, 0);

    std::span<const unsigned char> span(bytes);
    auto result = AnyPS5::ParseCompactUnwind(span);
    Require(result.has_value(), "parse compact unwind should succeed");
    std::fprintf(stderr, "guest_relinker_ehframe_fde_encoding: PASS\n");
    return 0;
}

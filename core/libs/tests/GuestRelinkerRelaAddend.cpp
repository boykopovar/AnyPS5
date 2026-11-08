#include "prx/libc/include/GuestHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "GuestRelinkerRelaAddend check failed at line %u\n", location.line());
    std::abort();
}

} // namespace

int main() {
    auto* memory = GuestHeap::GuestHeapAllocate_nid_postfix(4096);
    Require(memory != nullptr);
    std::memset(memory, 0, 4096);

    auto* slot = reinterpret_cast<std::int64_t*>(static_cast<char*>(memory) + 512);
    auto base = reinterpret_cast<std::int64_t>(memory);

    std::int64_t negativeAddend = -128;
    *slot = base + negativeAddend;

    Require(*slot == base - 128);
    GuestHeap::GuestHeapFree_nid_postfix(memory);
    std::puts("guest relinker rela addend sign checks passed");
    return 0;
}

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
    std::fprintf(stderr, "GuestRelinkerRelativeReloc check failed at line %u\n", location.line());
    std::abort();
}

} // namespace

int main() {
    auto* memory = GuestHeap::GuestHeapAllocate_nid_postfix(4096);
    Require(memory != nullptr);
    std::memset(memory, 0, 4096);

    auto* slot = reinterpret_cast<std::uint64_t*>(static_cast<char*>(memory) + 1024);
    *slot = 0x12345678;

    auto base = reinterpret_cast<std::uintptr_t>(memory);
    auto target = base + 2048;
    *slot = target;

    Require(*slot == target);
    GuestHeap::GuestHeapFree_nid_postfix(memory);
    std::puts("guest relinker relative relocation checks passed");
    return 0;
}

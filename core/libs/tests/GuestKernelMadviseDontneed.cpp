#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "GuestKernelMadviseDontneed check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

int main() {
    constexpr std::size_t page = 0x4000;
    void* memory = mmap_nid_postfix(nullptr, page * 4, 3, 0x1000 | 0x2, -1, 0);
    Require(memory != reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));

    char* bytes = static_cast<char*>(memory);
    bytes[0] = 'A';
    bytes[page] = 'B';

    Require(madvise_nid_postfix(memory, page, 4) == 0);
    Require(posix_madvise_nid_postfix(memory, page, 4) == 0);

    Require(madvise_nid_postfix(memory, page, 3) == 0);
    Require(posix_madvise_nid_postfix(memory, page, 3) == 0);

    Require(madvise_nid_postfix(memory, page, 0) == 0);
    Require(posix_madvise_nid_postfix(memory, page, 0) == 0);

    Require(madvise_nid_postfix(memory, page, 10) == 0);
    Require(posix_madvise_nid_postfix(memory, page, 10) == 0);

    Require(madvise_nid_postfix(memory, std::numeric_limits<std::size_t>::max(), 4) == -1);
    Require(posix_madvise_nid_postfix(memory, std::numeric_limits<std::size_t>::max(), 4) == 22);

    Require(madvise_nid_postfix(nullptr, page, 4) == -1);
    Require(posix_madvise_nid_postfix(nullptr, page, 4) == 22);

    Require(munmap_nid_postfix(memory, page * 4) == 0);
    std::printf("ALL GUEST KERNEL MADVISE DONTNEED & ALIGNMENT BOUNDARY TESTS PASSED\n");
    return 0;
}

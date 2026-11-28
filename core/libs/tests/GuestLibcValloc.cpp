#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "valloc check failed\n");
        std::abort();
    }
}

int main() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    const auto page = static_cast<std::size_t>(getpagesize_nid_postfix());

    void* first = valloc_nid_postfix(1);
    Require(first != nullptr);
    Require(reinterpret_cast<std::uintptr_t>(first) % page == 0);
    free_nid_postfix(first);

    const std::size_t size = 3 * page + 7;
    auto* block = static_cast<unsigned char*>(valloc_nid_postfix(size));
    Require(block != nullptr);
    Require(reinterpret_cast<std::uintptr_t>(block) % page == 0);
    std::memset(block, 0x5a, size);
    Require(block[0] == 0x5a && block[size - 1] == 0x5a);
    free_nid_postfix(block);
    return 0;
}

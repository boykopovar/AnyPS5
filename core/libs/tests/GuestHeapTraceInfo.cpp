#include "prx/libc/include/HeapDiagnostics.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

}

int main() {
    LibcHeapInfo info{sizeof(LibcHeapInfo), 0, 1, nullptr, nullptr};
    LibcHeapTraceInfo_nid_no_patch(&info);
    Require(info.unknown2 == 0, "heap trace info did not clear unknown2");
    Require(info.mspace_atomic_id_mask != nullptr, "heap trace info returned no mspace mask");
    Require(info.mstate_table != nullptr, "heap trace info returned no mstate table");

    constexpr std::uint64_t first = 0x0123456789abcdefull;
    constexpr std::uint64_t last = 0xfedcba9876543210ull;
    info.mstate_table[0] = first;
    info.mstate_table[64] = last;
    Require(info.mstate_table[0] == first, "heap trace info base slot is not writable");
    Require(info.mstate_table[64] == last, "heap trace info final 64-bit mask slot is not writable");
}

#include "prx/libc/include/HeapDiagnostics.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template <typename TAction>
void RequireInvalidArgument(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

}

int main() {
    RequireInvalidArgument([] { LibcHeapTraceInfo_nid_no_patch(nullptr); }, "null heap trace info was not rejected");

    std::uint64_t dummyMask = 123;
    std::uint64_t dummyTable = 456;
    for (const std::uint64_t badSize : {std::uint64_t{0}, sizeof(LibcHeapInfo) - 1, sizeof(LibcHeapInfo) + 8}) {
        LibcHeapInfo bad{badSize, 7, 9, &dummyMask, &dummyTable};
        RequireInvalidArgument([&] { LibcHeapTraceInfo_nid_no_patch(&bad); }, "unsupported LibcHeapInfo size was not rejected");
        Require(bad.size == badSize && bad.unknown1 == 7 && bad.unknown2 == 9, "rejected call modified metadata fields");
        Require(bad.mspace_atomic_id_mask == &dummyMask && bad.mstate_table == &dummyTable, "rejected call modified pointer fields");
    }

    LibcHeapInfo info{sizeof(LibcHeapInfo), 0, 1, nullptr, nullptr};
    LibcHeapTraceInfo_nid_no_patch(&info);
    Require(info.unknown2 == 0, "heap trace info did not clear unknown2");
    Require(info.mspace_atomic_id_mask != nullptr, "heap trace info returned no mspace mask");
    Require(info.mstate_table != nullptr, "heap trace info returned no mstate table");

    LibcHeapInfo second{sizeof(LibcHeapInfo), 0, 99, nullptr, nullptr};
    LibcHeapTraceInfo_nid_no_patch(&second);
    Require(second.unknown2 == 0, "repeated heap trace info did not clear unknown2");
    Require(second.mspace_atomic_id_mask == info.mspace_atomic_id_mask, "repeated calls returned different mspace mask addresses");
    Require(second.mstate_table == info.mstate_table, "repeated calls returned different mstate table addresses");

    constexpr std::uint64_t first = 0x0123456789abcdefull;
    constexpr std::uint64_t last = 0xfedcba9876543210ull;
    info.mstate_table[0] = first;
    info.mstate_table[64] = last;
    Require(info.mstate_table[0] == first, "heap trace info base slot is not writable");
    Require(info.mstate_table[64] == last, "heap trace info final 64-bit mask slot is not writable");
}

#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "bmalloc large zeroed check failed at line %u\n", location.line());
        std::abort();
    }
}

void VerifyZeroedAlignedAllocation(std::size_t requestedAlignment, std::size_t effectiveAlignment, std::size_t size, std::uint32_t heapKind) {
    void* ptr = _ZN7bmalloc3api29tryLargeZeroedMemalignVirtualEmmNS_8HeapKindE(requestedAlignment, size, heapKind);
    Require(ptr != nullptr);
    Require((reinterpret_cast<std::uintptr_t>(ptr) & (effectiveAlignment - 1)) == 0);
    Require(_ZN3WTF14fastMallocSizeEPKv(ptr) == size);

    auto* bytes = static_cast<unsigned char*>(ptr);
    for (std::size_t i = 0; i < size; ++i) {
        Require(bytes[i] == 0);
    }

    for (std::size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<unsigned char>((i * 37 + 11) & 0xFF);
    }
    for (std::size_t i = 0; i < size; ++i) {
        Require(bytes[i] == static_cast<unsigned char>((i * 37 + 11) & 0xFF));
    }

    _ZN7bmalloc3api13freeOutOfLineEPvNS_8HeapKindE(ptr, heapKind);
}

void TestValidLargeAlignments() {
    VerifyZeroedAlignedAllocation(4096, 4096, 4096 + 137, 0);
    VerifyZeroedAlignedAllocation(16384, 16384, 16384 * 2 + 509, 0);
    VerifyZeroedAlignedAllocation(65536, 65536, 12345, 0);
    VerifyZeroedAlignedAllocation(65536, 65536, 65536 + 777, 2);
}

void TestSmallAndZeroCases() {
    VerifyZeroedAlignedAllocation(1, alignof(std::max_align_t), 19, 0);
    VerifyZeroedAlignedAllocation(4096, 4096, 0, 0);
    _ZN7bmalloc3api13freeOutOfLineEPvNS_8HeapKindE(nullptr, 0);
}

void TestInvalidAlignmentsRejected() {
    constexpr std::size_t minAlign = alignof(std::max_align_t);
    const std::size_t invalidAlignments[] = {
        minAlign + 1,
        minAlign * 3,
        4096 + 1,
        65536 + 8,
        std::numeric_limits<std::size_t>::max(),
    };

    for (std::size_t alignment : invalidAlignments) {
        void* ptr = _ZN7bmalloc3api29tryLargeZeroedMemalignVirtualEmmNS_8HeapKindE(alignment, 64, 0);
        Require(ptr == nullptr);
    }

    void* overflowSize = _ZN7bmalloc3api29tryLargeZeroedMemalignVirtualEmmNS_8HeapKindE(
        4096, std::numeric_limits<std::size_t>::max() - 100, 0);
    Require(overflowSize == nullptr);
}

}  // namespace

int main() {
    TestValidLargeAlignments();
    TestSmallAndZeroCases();
    TestInvalidAlignmentsRejected();
    return 0;
}

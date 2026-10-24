#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HeapDiagnostics.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

std::uint64_t mspaceAtomicIdMask = 0;
constexpr std::size_t MspaceTableCapacity = 65;
std::array<std::uint64_t, MspaceTableCapacity> mstateTable{};

static_assert(MspaceTableCapacity == 65);
static_assert(mstateTable.size() == MspaceTableCapacity);

static_assert(sizeof(LibcHeapInfo) == 32);
static_assert(offsetof(LibcHeapInfo, mspace_atomic_id_mask) == 16);
static_assert(offsetof(LibcHeapInfo, mstate_table) == 24);

}

extern "C" {

void LibcHeapTraceInfo_nid_no_patch(LibcHeapInfo* info) {
    if (info == nullptr)
        throw std::invalid_argument("heap trace info: null output");
    if (info->size != sizeof(LibcHeapInfo))
        throw std::invalid_argument("heap trace info: unsupported structure size");
    info->unknown2 = 0;
    info->mspace_atomic_id_mask = &mspaceAtomicIdMask;
    info->mstate_table = mstateTable.data();
}

void APS5_VABI sceLibcInternalBacktraceForGame_nid_postfix(const char* heapName) {
    std::fprintf(stderr, "[%s] backtrace requested from guest 0x%llx\n", heapName ? heapName : "",
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))));
}

void APS5_VABI sceLibcInternalHeapErrorReportForGame_nid_postfix(void* heap, void* block, std::uint32_t error) {
    std::fprintf(stderr, "[libc] heap error 0x%x: heap %p, block %p, reported from guest 0x%llx\n", error, heap, block,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))));
}

}

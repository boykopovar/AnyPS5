#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HeapDiagnostics.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#ifndef _WIN32
#include <execinfo.h>
#endif

namespace {

std::uint64_t mspaceAtomicIdMask = 0;
std::array<std::uint64_t, 64> mstateTable{};

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
#ifndef _WIN32
    void* frames[64];
    const auto count = backtrace(frames, 64);
    std::fprintf(stderr, "[libc] backtrace for heap '%s':\n", heapName != nullptr ? heapName : "(null)");
    backtrace_symbols_fd(frames, static_cast<int>(count), 2);
#else
    std::fprintf(stderr, "[libc] backtrace for heap '%s' requested\n", heapName != nullptr ? heapName : "(null)");
#endif
}

void APS5_VABI sceLibcInternalHeapErrorReportForGame_nid_postfix(void* heap, void* block, std::uint32_t error) {
    std::fprintf(stderr, "[libc] heap error report: heap=%p block=%p error=%u\n", heap, block, error);
}

APS5_EXPORT("BnMAMrsfVWo", sceLibcUnknown_BnMAMrsfVWo);
void APS5_VABI sceLibcUnknown_BnMAMrsfVWo() {
    throw std::bad_weak_ptr();
}

}

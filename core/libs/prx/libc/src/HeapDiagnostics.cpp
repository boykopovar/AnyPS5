#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HeapDiagnostics.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif

extern "C" std::FILE* GuestStandardOutputFile_nid_no_patch();

namespace {

std::uint64_t mspaceAtomicIdMask = 0;
constexpr std::size_t MspaceTableCapacity = 65;
std::array<std::uint64_t, MspaceTableCapacity> mstateTable{};

static_assert(MspaceTableCapacity == 65);
static_assert(mstateTable.size() == MspaceTableCapacity);

static_assert(sizeof(LibcHeapInfo) == 32);
static_assert(offsetof(LibcHeapInfo, mspace_atomic_id_mask) == 16);
static_assert(offsetof(LibcHeapInfo, mstate_table) == 24);

constexpr std::size_t MspaceNameOffset = 0x3B0;
constexpr std::size_t MspaceNumberOffset = 0x3F4;
constexpr std::uint32_t SegmentOutOfRange = 1;

unsigned long CurrentProcessId() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

unsigned long CurrentThreadId() {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return static_cast<unsigned long>(::syscall(SYS_gettid));
#endif
}

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
    if (heap == nullptr) throw std::invalid_argument("sceLibcInternalHeapErrorReportForGame: null heap");
    const auto* state = static_cast<const char*>(heap);
    std::uint32_t number = 0;
    std::memcpy(&number, state + MspaceNumberOffset, sizeof(number));
    std::FILE* output = GuestStandardOutputFile_nid_no_patch();
    std::fprintf(output, "[SceLibc] A heap error is detected.\n[SceLibc] -- Mspace Name --\n[SceLibc] %s", state + MspaceNameOffset);
    if (number != 0) std::fprintf(output, " (%2u)", number);
    std::fputc('\n', output);
    if (error == SegmentOutOfRange) std::fprintf(output, "[SceLibc] -- Error reason --\n[SceLibc] segment out of range.\n");
    std::fprintf(output, "[SceLibc] pid:<0x%lx> tid:<0x%lx>\n", CurrentProcessId(), CurrentThreadId());
    std::fflush(output);
    std::fprintf(stderr, "[libc] heap error 0x%x: heap %p, block %p, reported from guest 0x%llx\n", error, heap, block,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))));
}

}

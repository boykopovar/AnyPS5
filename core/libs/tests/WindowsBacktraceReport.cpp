#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include "../prx/libc/include/general/VabiMacros.hpp"
#include "../prx/libc/include/FailureReport.hpp"
#include "../prx/libc/include/GuestArena.hpp"

namespace {

void KeptReport() noexcept {
    std::fputs("kept failure report ran\n", stderr);
}

void DroppedReport() noexcept {
    std::fputs("dropped failure report ran\n", stderr);
}

LONG WINAPI EndAfterReport(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) TerminateProcess(GetCurrentProcess(), 3);
    return EXCEPTION_CONTINUE_SEARCH;
}

[[gnu::noinline]] void FaultHoldingObject() {
    AddVectoredExceptionHandler(0, EndAfterReport);
    auto* object = static_cast<std::uint64_t*>(VirtualAlloc(nullptr, 0x10000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (object == nullptr) std::exit(1);
    for (std::uint64_t i = 0; i < 8; ++i) object[i] = 0xc0ffee0000000000ull + i;
    std::uint64_t* volatile held = object;
    volatile int* volatile target = nullptr;
    *target = static_cast<int>(reinterpret_cast<std::uintptr_t>(held));
}

constexpr std::size_t GuardedBytes = 0x10000;

std::byte* GuardedSource() {
    auto* source = static_cast<std::byte*>(GuestArena::GuestArenaAllocate_nid_postfix(GuardedBytes, GuardedBytes));
    if (source == nullptr) std::exit(1);
    GuestArena::GuestArenaCommit_nid_postfix(source, GuardedBytes, PAGE_READWRITE, GuardedBytes);
    return source;
}

std::byte* NoAccessPages() {
    auto* pages = static_cast<std::byte*>(VirtualAlloc(nullptr, GuardedBytes, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS));
    if (pages == nullptr) std::exit(1);
    return pages;
}

LONG WINAPI ShrinkSourceInterval(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) info->ContextRecord->R9 = info->ContextRecord->Rcx;
    return EXCEPTION_CONTINUE_SEARCH;
}

[[gnu::noinline]] void GuardedWildFault(const char* mode) {
    AddVectoredExceptionHandler(0, EndAfterReport);
    auto* source = GuardedSource();
    const auto address = reinterpret_cast<std::uintptr_t>(source);
    bool equal = false;
    if (std::strcmp(mode, "guarded-destination") == 0) GuestArena::GuestArenaCopyOwned_nid_no_patch(address, NoAccessPages(), GuardedBytes);
    if (std::strcmp(mode, "guarded-expected") == 0) GuestArena::GuestArenaCompareOwned_nid_no_patch(address, NoAccessPages(), GuardedBytes, &equal);
    if (std::strcmp(mode, "guarded-outside") == 0) {
        DWORD previous = 0;
        if (!VirtualProtect(source, 0x1000, PAGE_NOACCESS, &previous)) std::exit(1);
        AddVectoredExceptionHandler(1, ShrinkSourceInterval);
        static std::byte destination[GuardedBytes];
        GuestArena::GuestArenaCopyOwned_nid_no_patch(address, destination, GuardedBytes);
    }
    std::exit(1);
}

}

int main(int argc, char** argv) {
    if (__error_nid_postfix() == nullptr) return 1;
    if (argc == 2 && std::strcmp(argv[1], "fault") == 0) FaultHoldingObject();
    if (argc == 2 && std::strncmp(argv[1], "guarded-", 8) == 0) GuardedWildFault(argv[1]);
    if (argc == 2 && std::strcmp(argv[1], "report") == 0) {
        LibcRegisterFailureReport_nid_no_patch(DroppedReport);
        LibcRegisterFailureReport_nid_no_patch(KeptReport);
        LibcRegisterFailureReport_nid_no_patch(KeptReport);
        LibcUnregisterFailureReport_nid_no_patch(DroppedReport);
    }
    if (argc == 2 && std::strcmp(argv[1], "report-once") == 0) {
        LibcRegisterFailureReport_nid_no_patch(KeptReport);
        LibcRunFailureReports_nid_no_patch();
    }
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    std::fputs("aborting\n", stderr);
    std::abort();
}

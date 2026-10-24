#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include "../prx/libc/include/general/VabiMacros.hpp"
#include "../prx/libc/include/FailureReport.hpp"

extern "C" int* APS5_VABI __error_nid_postfix();

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

}

int main(int argc, char** argv) {
    if (__error_nid_postfix() == nullptr) return 1;
    if (argc == 2 && std::strcmp(argv[1], "fault") == 0) FaultHoldingObject();
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

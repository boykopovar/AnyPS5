#include <cstdio>
#include <cstdlib>
#include <cstring>
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

}

int main(int argc, char** argv) {
    if (__error_nid_postfix() == nullptr) return 1;
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

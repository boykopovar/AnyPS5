#include "prx/libc/include/General.hpp"

[[noreturn]] void LiftedReportRejectHostArguments() {
    NotImplemented_nid_no_patch("sceAudioPropagationReportApi from host code in a lifted build, whose va_list is not the SysV one a report handler reads");
}

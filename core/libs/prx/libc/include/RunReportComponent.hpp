#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_RUNREPORTCOMPONENT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_RUNREPORTCOMPONENT_HPP

#include "prx/libc/include/RunReport.hpp"

#if __has_include("RunReportRevision.hpp")
#include "RunReportRevision.hpp"
#endif
#ifndef APS5_REPORT_COMMIT
#define APS5_REPORT_COMMIT ""
#endif
#ifndef APS5_REPORT_MODIFIED
#define APS5_REPORT_MODIFIED (-1)
#endif

#define APS5_RUN_REPORT_COMPONENT(name) \
    namespace { [[maybe_unused]] const bool g_runReportComponent = (LibcRunReportRegisterComponent_nid_no_patch(name, APS5_REPORT_COMMIT, APS5_REPORT_MODIFIED), true); }

#endif

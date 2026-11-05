#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_FAILUREREPORT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_FAILUREREPORT_HPP

extern "C" void LibcRegisterFailureReport_nid_no_patch(void (*report)() noexcept);
extern "C" void LibcUnregisterFailureReport_nid_no_patch(void (*report)() noexcept) noexcept;
extern "C" void LibcRunFailureReports_nid_no_patch() noexcept;

#endif

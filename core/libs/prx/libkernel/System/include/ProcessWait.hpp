#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_PROCESSWAIT_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_PROCESSWAIT_HPP

struct GuestResourceUsage;

extern "C" int WaitProcess_nid_no_patch(int pid, int* status, int options, GuestResourceUsage* usage);

#endif

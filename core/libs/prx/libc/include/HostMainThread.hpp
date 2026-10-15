#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_HOSTMAINTHREAD_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_HOSTMAINTHREAD_HPP

extern "C" bool HostMainThreadServing_nid_no_patch();
extern "C" void HostMainThreadPost_nid_no_patch(void (*task)(void*), void* context);
extern "C" [[noreturn]] void HostMainThreadServe_nid_no_patch();

#endif

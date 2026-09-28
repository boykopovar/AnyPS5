#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_THREADLIFECYCLE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_THREADLIFECYCLE_HPP

#include "SceTypes.hpp"

class ThreadLifecycle {
public:
    static void SetThreadDtors(thread_dtors_func_t callback);
    static void SetThreadAtexitCount(get_thread_atexit_count_func_t callback);
    static void SetThreadAtexitReport(thread_atexit_report_func_t callback);
};

extern "C" void APS5_VABI scePthreadExit(void* retval);

#endif

#include "prx/libc/include/Api.hpp"

extern "C" const char* const sys_siglist_nid_postfix[32] = {
    "Signal 0", "Hangup", "Interrupt", "Quit",
    "Illegal instruction", "Trace/BPT trap", "Abort trap", "EMT trap",
    "Floating point exception", "Killed", "Bus error", "Segmentation fault",
    "Bad system call", "Broken pipe", "Alarm clock", "Terminated",
    "Urgent I/O condition", "Suspended (signal)", "Suspended", "Continued",
    "Child exited", "Stopped (tty input)", "Stopped (tty output)", "I/O possible",
    "Cputime limit exceeded", "Filesize limit exceeded", "Virtual timer expired", "Profiling timer expired",
    "Window size changes", "Information request", "User defined signal 1", "User defined signal 2",
};

extern "C" const int sys_nsig_nid_postfix = sizeof(sys_siglist_nid_postfix) / sizeof(sys_siglist_nid_postfix[0]);

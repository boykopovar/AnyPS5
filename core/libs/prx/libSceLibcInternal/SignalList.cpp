#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {

extern const char* const sys_siglist_nid_postfix[32] = {
    "Signal 0",
    "Hangup",
    "Interrupt",
    "Quit",
    "Illegal instruction",
    "Trace/BPT trap",
    "Abort trap",
    "EMT trap",
    "Floating point exception",
    "Killed",
    "Bus error",
    "Segmentation fault",
    "Bad system call",
    "Broken pipe",
    "Alarm clock",
    "Terminated",
    "Urgent I/O condition",
    "Suspended (signal)",
    "Suspended",
    "Continued",
    "Child exited",
    "Stopped (tty input)",
    "Stopped (tty output)",
    "I/O possible",
    "Cputime limit exceeded",
    "Filesize limit exceeded",
    "Virtual timer expired",
    "Profiling timer expired",
    "Window size changes",
    "Information request",
    "User defined signal 1",
    "User defined signal 2",
};

extern const int sys_nsig_nid_postfix = sizeof(sys_siglist_nid_postfix) / sizeof(sys_siglist_nid_postfix[0]);

extern const char* const sys_signame_nid_postfix[32] = {
    "Signal 0",
    "HUP",
    "INT",
    "QUIT",
    "ILL",
    "TRAP",
    "ABRT",
    "EMT",
    "FPE",
    "KILL",
    "BUS",
    "SEGV",
    "SYS",
    "PIPE",
    "ALRM",
    "TERM",
    "URG",
    "STOP",
    "TSTP",
    "CONT",
    "CHLD",
    "TTIN",
    "TTOU",
    "IO",
    "XCPU",
    "XFSZ",
    "VTALRM",
    "PROF",
    "WINCH",
    "INFO",
    "USR1",
    "USR2",
};

void APS5_VABI psignal_nid_postfix(unsigned int signal, const char* prefix) {
    const char* const message = signal < 32 ? sys_siglist_nid_postfix[signal] : "Unknown signal";
    if (prefix != nullptr && prefix[0] != 0) {
        _write_nid_postfix(2, prefix, std::strlen(prefix));
        _write_nid_postfix(2, ": ", 2);

    }
    _write_nid_postfix(2, message, std::strlen(message));
    _write_nid_postfix(2, "\n", 1);

}

}

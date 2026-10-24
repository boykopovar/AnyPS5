#include "prx/libSceLibcInternal/include/Spawn.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

struct GuestSigaction {
    std::uintptr_t handler;
    int flags;
    GuestSignalSet mask;
};
static_assert(sizeof(GuestSigaction) == 32 && offsetof(GuestSigaction, flags) == 8 && offsetof(GuestSigaction, mask) == 12);

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

std::int64_t APS5_VABI _write_nid_postfix(int descriptor, const void* buffer, std::size_t count);

void APS5_VABI psignal_nid_postfix(unsigned int signal, const char* prefix) {
    const char* const message = signal < 32 ? sys_siglist_nid_postfix[signal] : "Unknown signal";
    if (prefix != nullptr && prefix[0] != 0) {
        _write_nid_postfix(2, prefix, std::strlen(prefix));
        _write_nid_postfix(2, ": ", 2);
    }
    _write_nid_postfix(2, message, std::strlen(message));
    _write_nid_postfix(2, "\n", 1);
}

extern GuestSignalSet _sigintr_nid_postfix;
int APS5_VABI _sigaction_nid_postfix(int guest, const GuestSigaction* action, GuestSigaction* previous);

int APS5_VABI siginterrupt_nid_postfix(int guest, int flag) {
    constexpr int SaRestart = 0x2;
    GuestSigaction action{};
    if (_sigaction_nid_postfix(guest, nullptr, &action) != 0) return -1;
    const auto index = static_cast<std::uint32_t>(guest - 1);
    if (flag) {
        _sigintr_nid_postfix.bits[index >> 5] |= 1u << (index & 31);
        action.flags &= ~SaRestart;
    } else {
        _sigintr_nid_postfix.bits[index >> 5] &= ~(1u << (index & 31));
        action.flags |= SaRestart;
    }
    return _sigaction_nid_postfix(guest, &action, nullptr);
}

}

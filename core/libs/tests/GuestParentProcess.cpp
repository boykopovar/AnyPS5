#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

extern "C" {
int APS5_VABI getpid_nid_postfix(void);
int APS5_VABI getppid_nid_postfix(void);
int APS5_VABI getsid_nid_postfix(int);
int APS5_VABI setpgid_nid_postfix(int, int);
int APS5_VABI setpgrp_nid_postfix(int, int);
int* APS5_VABI __error_nid_postfix();
}
static void Require(bool value) { if (!value) std::abort(); }
int main() {
    const int self = getpid_nid_postfix();
    Require(getsid_nid_postfix(0) == self);
    Require(getsid_nid_postfix(self) == self);
    *__error_nid_postfix() = 0;
    Require(getsid_nid_postfix(123456) == -1);
    Require(*__error_nid_postfix() == 3);
    Require(getsid_nid_postfix(-1) == -1);
    Require(*__error_nid_postfix() == 3);
    *__error_nid_postfix() = 0;
    Require(setpgid_nid_postfix(0, 0) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgid_nid_postfix(self, 0) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgid_nid_postfix(0, self) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgid_nid_postfix(self, self) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgid_nid_postfix(0, 123456) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgid_nid_postfix(self, 123456) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgid_nid_postfix(-1, -1) == -1);
    Require(*__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(setpgid_nid_postfix(0, -1) == -1);
    Require(*__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(setpgid_nid_postfix(self, -1) == -1);
    Require(*__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(setpgid_nid_postfix(-1, 0) == -1);
    Require(*__error_nid_postfix() == 3);
    *__error_nid_postfix() = 0;
    Require(setpgid_nid_postfix(123456, 0) == -1);
    Require(*__error_nid_postfix() == 3);
    *__error_nid_postfix() = 0;
    Require(setpgrp_nid_postfix(0, 0) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgrp_nid_postfix(self, 0) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgrp_nid_postfix(0, self) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgrp_nid_postfix(self, self) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgrp_nid_postfix(0, 123456) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgrp_nid_postfix(self, 123456) == -1);
    Require(*__error_nid_postfix() == 1);
    *__error_nid_postfix() = 0;
    Require(setpgrp_nid_postfix(-1, -1) == -1);
    Require(*__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(setpgrp_nid_postfix(0, -1) == -1);
    Require(*__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(setpgrp_nid_postfix(self, -1) == -1);
    Require(*__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(setpgrp_nid_postfix(-1, 0) == -1);
    Require(*__error_nid_postfix() == 3);
    *__error_nid_postfix() = 0;
    Require(setpgrp_nid_postfix(123456, 0) == -1);
    Require(*__error_nid_postfix() == 3);
    const int parent = getppid_nid_postfix();
    Require(parent > 0 && parent != getpid_nid_postfix());
    Require(getppid_nid_postfix() == parent);
#ifdef _WIN32
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(parent));
    Require(process != nullptr);
    CloseHandle(process);
#else
    Require(parent == static_cast<int>(::getppid()));
#endif
    return 0;
}

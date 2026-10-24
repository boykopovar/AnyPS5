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
int APS5_VABI getpgrp_nid_postfix(void);
int APS5_VABI getpgid_nid_postfix(int);
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
    Require(getpgrp_nid_postfix() == self);
    Require(*__error_nid_postfix() == 0);
    Require(getpgid_nid_postfix(0) == self);
    Require(*__error_nid_postfix() == 0);
    Require(getpgid_nid_postfix(self) == self);
    Require(*__error_nid_postfix() == 0);
    *__error_nid_postfix() = 0;
    Require(getpgid_nid_postfix(123456) == -1);
    Require(*__error_nid_postfix() == 3);
    *__error_nid_postfix() = 0;
    Require(getpgid_nid_postfix(-1) == -1);
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

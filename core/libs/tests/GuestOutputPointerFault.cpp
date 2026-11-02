#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#ifdef __linux__
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "output pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr int GuestEfault = 14;
static constexpr int KernelEfault = static_cast<int>(0x8002000Eu);

static bool Efault(int result) {
    return result == -1 && *__error_nid_postfix() == GuestEfault;
}

#ifdef __linux__
static void RequireNonEfaultFailureThrows() {
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        sock_filter filter[] = {
            BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct seccomp_data, nr)),
            BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_process_vm_readv, 0, 1),
            BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
            BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
        };
        sock_fprog program{static_cast<unsigned short>(sizeof(filter) / sizeof(filter[0])), filter};
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0 || prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program) != 0) _exit(2);
        char source[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        char destination[8] = {};
        try {
            GuestCopyIn_nid_no_patch(destination, source, sizeof(source));
        } catch (const std::runtime_error&) {
            _exit(destination[0] == 0 ? 0 : 3);
        }
        _exit(1);
    }
    int status = 0;
    Require(waitpid(child, &status, 0) == child);
    Require(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
#endif

int main() {
    alignas(8) unsigned char valid[512]{};
    alignas(8) unsigned char zeroTimer[32]{};
    auto* timeValid = reinterpret_cast<KernelTimeval*>(valid);
    auto* zoneValid = reinterpret_cast<KernelTimezone*>(valid + 32);
    auto* timerValid = reinterpret_cast<GuestItimerval*>(valid);
    auto* zeroTimerValue = reinterpret_cast<const GuestItimerval*>(zeroTimer);
    for (const std::uintptr_t address : {std::uintptr_t{1}, ~std::uintptr_t{0xfff}}) {
        void* bad = reinterpret_cast<void*>(address);
        auto* badTime = reinterpret_cast<KernelTimeval*>(address);
        auto* badZone = reinterpret_cast<KernelTimezone*>(address);
        auto* badTimer = reinterpret_cast<GuestItimerval*>(address);
        *__error_nid_postfix() = 0;
        *__error_nid_postfix() = 0;
        *__error_nid_postfix() = 0;
        Require(Efault(gettimeofday_nid_postfix(badTime, nullptr)));
        *__error_nid_postfix() = 0;
        Require(Efault(gettimeofday_nid_postfix(timeValid, badZone)));
        *__error_nid_postfix() = 0;
        Require(Efault(getitimer_nid_postfix(0, badTimer)));
        *__error_nid_postfix() = 0;
        Require(Efault(setitimer_nid_postfix(0, zeroTimerValue, badTimer)));
        *__error_nid_postfix() = 0;
        Require(Efault(setitimer_nid_postfix(0, badTimer, timerValid)));
        *__error_nid_postfix() = 0;
        Require(Efault(getitimer_nid_postfix(1, badTimer)));
        *__error_nid_postfix() = 0;
        Require(Efault(setitimer_nid_postfix(1, zeroTimerValue, badTimer)));
        Require(sceKernelGettimeofday(badTime) == KernelEfault);
        Require(sceKernelGettimezone(badZone) == KernelEfault);
    }
    Require(gettimeofday_nid_postfix(timeValid, zoneValid) == 0);
    Require(getitimer_nid_postfix(0, timerValid) == 0);
    Require(setitimer_nid_postfix(0, zeroTimerValue, timerValid) == 0);
    Require(getitimer_nid_postfix(1, timerValid) == 0);
    Require(setitimer_nid_postfix(1, zeroTimerValue, timerValid) == 0);
    GuestItimerval armed{};
    armed.value.tv_sec = 3600;
    *__error_nid_postfix() = 0;
    Require(Efault(setitimer_nid_postfix(0, &armed, reinterpret_cast<GuestItimerval*>(~std::uintptr_t{0xfff}))));
    GuestItimerval current{};
    Require(getitimer_nid_postfix(0, &current) == 0 && current.value.tv_sec > 0);
    Require(setitimer_nid_postfix(0, zeroTimerValue, nullptr) == 0);
    Require(sceKernelGettimeofday(timeValid) == 0);
    Require(sceKernelGettimezone(zoneValid) == 0);
#ifdef __linux__
    RequireNonEfaultFailureThrows();
#endif
}

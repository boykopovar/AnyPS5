#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "sched_rr_get_interval check failed at line %d\n", line);
    std::abort();
}
#define Require(condition) Check((condition), __LINE__)

static bool FailsWith(int result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

int main() {
    constexpr int GUEST_ESRCH = 3;
    constexpr int GUEST_EFAULT = 14;
    const int self = getpid_nid_postfix();

#ifdef _WIN32
    auto* guard = static_cast<KernelTimespec*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    Require(guard != nullptr);
#else
    auto* guard = static_cast<KernelTimespec*>(mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(guard != MAP_FAILED);
#endif

    for (const int pid : {0, self}) {
        KernelTimespec interval{0, 0};
        *__error_nid_postfix() = 0;
        Require(sched_rr_get_interval_nid_postfix(pid, &interval) == 0);
        Require(interval.tv_sec == 0);
        Require(interval.tv_nsec == 94000000);

        for (auto* invalidPtr : {static_cast<KernelTimespec*>(nullptr), guard, reinterpret_cast<KernelTimespec*>(0x10)}) {
            *__error_nid_postfix() = 0;
            Require(FailsWith(sched_rr_get_interval_nid_postfix(pid, invalidPtr), GUEST_EFAULT));
        }
    }

    for (const int pid : {self + 1, -1, 99999}) {
        KernelTimespec interval{0, 0};
        *__error_nid_postfix() = 0;
        Require(FailsWith(sched_rr_get_interval_nid_postfix(pid, &interval), GUEST_ESRCH));

        *__error_nid_postfix() = 0;
        Require(FailsWith(sched_rr_get_interval_nid_postfix(pid, nullptr), GUEST_ESRCH));
    }

#ifdef _WIN32
    VirtualFree(guard, 0, MEM_RELEASE);
#else
    munmap(guard, 4096);
#endif

    std::printf("ALL SCHED_RR_GET_INTERVAL TESTS PASSED\n");
    return 0;
}

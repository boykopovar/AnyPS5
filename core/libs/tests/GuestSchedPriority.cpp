#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <ctime>
extern "C" int* APS5_VABI __error_nid_postfix();
extern "C" {
int APS5_VABI sched_get_priority_max_nid_postfix(int policy);
int APS5_VABI sched_get_priority_min_nid_postfix(int policy);
int APS5_VABI getpriority_nid_postfix(int which, int who);
int APS5_VABI setpriority_nid_postfix(int which, int who, int niceval);
struct GuestRtPrio {
    std::uint16_t type;
    std::uint16_t prio;
};
int APS5_VABI rtprio_thread_nid_postfix(int function, int lwpid, GuestRtPrio* priority);
int APS5_VABI pthread_setconcurrency_nid_postfix(int newLevel);
int APS5_VABI pthread_getconcurrency_nid_postfix(void);
struct GuestTimespec {
    std::int64_t tv_sec;
    std::int64_t tv_nsec;
};
int APS5_VABI sched_rr_get_interval_nid_postfix(int pid, GuestTimespec* interval);
int APS5_VABI getpid_nid_postfix(void);
int* APS5_VABI __error_nid_postfix();
}
static constexpr int PrioProcess = 0;
static constexpr int PrioGroup = 1;
static constexpr int PrioUser = 2;
static void Require(bool value) { if (!value) std::abort(); }
static bool Rejects(int policy) {
    *__error_nid_postfix() = 0;
    const bool max = sched_get_priority_max_nid_postfix(policy) == -1 && *__error_nid_postfix() == 22;
    *__error_nid_postfix() = 0;
    return max && sched_get_priority_min_nid_postfix(policy) == -1 && *__error_nid_postfix() == 22;
}
static void RequireNice(int which, int who) {
    *__error_nid_postfix() = 13;
    Require(getpriority_nid_postfix(which, who) == 0 && *__error_nid_postfix() == 13);
}
static void RequireError(int which, int who, int error) {
    *__error_nid_postfix() = 0;
    Require(getpriority_nid_postfix(which, who) == -1 && *__error_nid_postfix() == error);
}
static void RequireSet(int which, int who, int nice) {
    *__error_nid_postfix() = 13;
    Require(setpriority_nid_postfix(which, who, nice) == 0 && *__error_nid_postfix() == 13);
}
static void RequireSetError(int which, int who, int nice, int error) {
    *__error_nid_postfix() = 0;
    Require(setpriority_nid_postfix(which, who, nice) == -1 && *__error_nid_postfix() == error);
}
int main() {
    Require(Rejects(-1) && Rejects(2) && Rejects(5));
    const int self = getpid_nid_postfix();
    GuestTimespec interval{};
    *__error_nid_postfix() = 13;
    Require(sched_rr_get_interval_nid_postfix(0, &interval) == 0 && interval.tv_sec == 0 && interval.tv_nsec == 94000000 && *__error_nid_postfix() == 13);
    interval.tv_sec = -1;
    interval.tv_nsec = -1;
    Require(sched_rr_get_interval_nid_postfix(0, &interval) == 0 && interval.tv_sec == 0 && interval.tv_nsec == 94000000);
    interval = {};
    Require(sched_rr_get_interval_nid_postfix(self, &interval) == 0 && interval.tv_sec == 0 && interval.tv_nsec == 94000000);
    *__error_nid_postfix() = 0;
    Require(sched_rr_get_interval_nid_postfix(-1, &interval) == -1 && *__error_nid_postfix() == 3);
    *__error_nid_postfix() = 0;
    Require(sched_rr_get_interval_nid_postfix(self + 1, &interval) == -1 && *__error_nid_postfix() == 3);
    *__error_nid_postfix() = 0;
    Require(sched_rr_get_interval_nid_postfix(0, nullptr) == -1 && *__error_nid_postfix() == 14);
    for (int policy : {1, 3}) {
        Require(sched_get_priority_max_nid_postfix(policy) == 767);
        Require(sched_get_priority_min_nid_postfix(policy) == 256);
    }
    Require(Rejects(0) && Rejects(4));
    RequireNice(PrioProcess, 0);
    RequireNice(PrioProcess, self);
    RequireNice(PrioGroup, 0);
    RequireNice(PrioGroup, self);
    RequireNice(PrioUser, 0);
    RequireError(PrioProcess, self + 1, 3);
    RequireError(PrioProcess, -1, 3);
    RequireError(PrioGroup, self + 1, 3);
    RequireError(PrioUser, 1000, 3);
    RequireError(3, 0, 22);
    RequireError(-1, self, 22);
    RequireSet(PrioProcess, 0, 0);
    RequireSet(PrioProcess, self, 5);
    RequireSet(PrioProcess, self, -20);
    RequireSet(PrioGroup, 0, 20);
    RequireSet(PrioUser, 0, -1);
    RequireSetError(PrioProcess, self + 1, 0, 3);
    RequireSetError(PrioProcess, -1, 0, 3);
    RequireSetError(PrioGroup, self + 1, 0, 3);
    RequireSetError(PrioUser, 1000, 0, 3);
    RequireSetError(3, 0, 0, 22);
    RequireSetError(-1, self, 0, 22);
    Require(setpriority_nid_postfix(PrioProcess, 0, 10) == 0 && getpriority_nid_postfix(PrioProcess, 0) == 10);
    Require(setpriority_nid_postfix(PrioProcess, 0, 99) == 0 && getpriority_nid_postfix(PrioProcess, 0) == 20);
    Require(setpriority_nid_postfix(PrioProcess, 0, -99) == 0 && getpriority_nid_postfix(PrioProcess, 0) == -20);
    Require(setpriority_nid_postfix(PrioProcess, 0, 0) == 0 && getpriority_nid_postfix(PrioProcess, 0) == 0);
    GuestRtPrio rtprio{7, 7};
    bool rtprioThrew = false;
    try { rtprio_thread_nid_postfix(0, 0, &rtprio); } catch (const std::exception&) { rtprioThrew = true; }
    Require(rtprioThrew);
    rtprioThrew = false;
    try { rtprio_thread_nid_postfix(1, 0, &rtprio); } catch (const std::exception&) { rtprioThrew = true; }
    Require(rtprioThrew);
    *__error_nid_postfix() = 0;
    Require(rtprio_thread_nid_postfix(2, 0, &rtprio) == -1 && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(rtprio_thread_nid_postfix(0, 0, nullptr) == -1 && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(rtprio_thread_nid_postfix(0, self + 1, &rtprio) == -1 && *__error_nid_postfix() == 3);
    Require(pthread_getconcurrency_nid_postfix() == 0);
    *__error_nid_postfix() = 13;
    Require(pthread_setconcurrency_nid_postfix(4) == 0 && pthread_getconcurrency_nid_postfix() == 4 && *__error_nid_postfix() == 13);
    *__error_nid_postfix() = 13;
    Require(pthread_setconcurrency_nid_postfix(-1) == 22 && *__error_nid_postfix() == 13);
    Require(pthread_setconcurrency_nid_postfix(0) == 0 && pthread_getconcurrency_nid_postfix() == 0);
}

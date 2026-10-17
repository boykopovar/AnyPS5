#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <climits>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <typeinfo>

struct GuestItimerval {
    KernelTimeval interval;
    KernelTimeval value;
};
static_assert(sizeof(GuestItimerval) == 32 && offsetof(GuestItimerval, value) == 16);
struct GuestSignalSet {
    std::uint32_t bits[4];
};
struct GuestTimespec {
    std::int64_t seconds;
    std::int64_t nanoseconds;
};

using Handler = void (APS5_VABI *)(int);
using TerminateHandler = void (*)();
extern "C" {
int APS5_VABI setitimer_nid_postfix(int, const GuestItimerval*, GuestItimerval*);
int APS5_VABI getitimer_nid_postfix(int, GuestItimerval*);
unsigned int APS5_VABI GuestAlarm_nid_no_patch(unsigned int);
Handler APS5_VABI signal_nid_postfix(int, Handler);
int APS5_VABI _sigprocmask_nid_postfix(int, const GuestSignalSet*, GuestSignalSet*);
int APS5_VABI sigtimedwait_nid_postfix(const GuestSignalSet*, void*, const GuestTimespec*);
int APS5_VABI sigpending_nid_postfix(GuestSignalSet*);
int* APS5_VABI __error_nid_postfix();
void* APS5_VABI __cxa_current_primary_exception_nid_postfix();
std::type_info* APS5_VABI __cxa_current_exception_type_nid_postfix();
TerminateHandler APS5_VABI _ZSt13set_terminatePFvvE_nid_postfix(TerminateHandler);
}

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static std::int64_t Micros(const KernelTimeval& time) {
    Require(time.tv_sec >= 0 && time.tv_usec >= 0 && time.tv_usec < 1000000, "timeval is not normalized");
    return time.tv_sec * 1000000 + time.tv_usec;
}

static GuestItimerval Read(int which) {
    GuestItimerval current{{11, 22}, {33, 44}};
    *__error_nid_postfix() = 12345;
    Require(setitimer_nid_postfix(which, nullptr, &current) == 0, "reading the timer failed");
    Require(*__error_nid_postfix() == 12345, "reading the timer changed errno");
    return current;
}

static void RequireFailure(int which, const GuestItimerval* value, int error, const char* message) {
    GuestItimerval previous{{11, 22}, {33, 44}};
    const auto saved = previous;
    *__error_nid_postfix() = 0;
    Require(setitimer_nid_postfix(which, value, &previous) == -1 && *__error_nid_postfix() == error, message);
    Require(std::memcmp(&previous, &saved, sizeof(previous)) == 0, "a failed setitimer wrote the previous value");
}

volatile std::sig_atomic_t delivered = 0;
static void APS5_VABI OnAlarm(int signal) {
    delivered = signal;
}

static bool AlarmPending() {
    GuestSignalSet pending{{0, 0, 0, 0}};
    Require(sigpending_nid_postfix(&pending) == 0, "sigpending failed");
    return (pending.bits[0] & (1u << 13)) != 0;
}

static const GuestItimerval once{{0, 0}, {0, 20000}};

static void ExpireOnce() {
    Require(setitimer_nid_postfix(0, &once, nullptr) == 0, "arming the blocked timer failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
}

static void OnTerminate() {
    const auto* type = __cxa_current_exception_type_nid_postfix();
    const auto* error = static_cast<const std::runtime_error*>(__cxa_current_primary_exception_nid_postfix());
    if (type != nullptr && error != nullptr && std::strcmp(type->name(), typeid(std::runtime_error).name()) == 0
        && std::strstr(error->what(), "it_interval 0.250000 s") != nullptr && delivered == 0) {
        std::printf("expiry threw: %s\n", error->what());
        std::fflush(stdout);
        std::_Exit(0);
    }
    std::fprintf(stderr, "unexpected termination at expiry\n");
    std::_Exit(1);
}

static int UnblockPendingDefault() {
    const GuestSignalSet alarmSet{{1u << 13, 0, 0, 0}};
    Require(_sigprocmask_nid_postfix(1, &alarmSet, nullptr) == 0, "blocking SIGALRM failed");
    ExpireOnce();
    Require(AlarmPending(), "a blocked expiry with the default disposition was not left pending");
    Require(_sigprocmask_nid_postfix(2, &alarmSet, nullptr) == 0, "unblocking SIGALRM failed");
    std::fprintf(stderr, "unblocking a pending SIGALRM with the default disposition did not terminate\n");
    return 1;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "pending-default") == 0) return UnblockPendingDefault();
    const auto ignore = reinterpret_cast<Handler>(std::uintptr_t{1});
    Require(signal_nid_postfix(14, ignore) == nullptr, "SIGALRM was not initially defaulted");
    const GuestItimerval valid{{0, 0}, {30, 0}};
    for (int invalid : {-1, 3, INT_MIN, INT_MAX}) {
        RequireFailure(invalid, &valid, 22, "an unknown timer did not report EINVAL");
        RequireFailure(invalid, nullptr, 22, "an unknown timer read did not report EINVAL");
        *__error_nid_postfix() = 0;
        Require(setitimer_nid_postfix(invalid, nullptr, nullptr) == -1 && *__error_nid_postfix() == 22, "EINVAL is not checked before EFAULT");
    }
    for (int which : {0, 1, 2}) {
        *__error_nid_postfix() = 0;
        Require(setitimer_nid_postfix(which, nullptr, nullptr) == -1 && *__error_nid_postfix() == 14, "a read into null did not report EFAULT");
        for (const GuestItimerval bad : {GuestItimerval{{0, 0}, {-1, 0}}, GuestItimerval{{0, 0}, {0, -1}},
                 GuestItimerval{{0, 0}, {0, 1000000}}, GuestItimerval{{0, 0}, {INT_MAX / 2 + 1, 0}},
                 GuestItimerval{{-1, 0}, {1, 0}}, GuestItimerval{{0, 1000000}, {1, 0}}, GuestItimerval{{INT_MAX / 2 + 1, 0}, {1, 0}}})
            RequireFailure(which, &bad, 22, "an invalid timeval did not report EINVAL");
        const GuestItimerval disarm{{-5, 7}, {0, 0}};
        GuestItimerval previous{{11, 22}, {33, 44}};
        Require(setitimer_nid_postfix(which, &disarm, &previous) == 0, "disarming with an invalid interval failed");
        Require(Micros(previous.interval) == 0 && Micros(previous.value) == 0, "an idle timer reported a value");
        Require(Micros(Read(which).interval) == 0, "disarming kept the interval");
    }
    for (int cpu : {1, 2}) {
        GuestItimerval previous{{11, 22}, {33, 44}};
        const auto saved = previous;
        bool threw = false;
        try {
            setitimer_nid_postfix(cpu, &valid, &previous);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw && std::memcmp(&previous, &saved, sizeof(previous)) == 0, "a CPU timer was armed");
        Require(Micros(Read(cpu).value) == 0, "a CPU timer kept a value");
    }
    bool threw = false;
    const GuestItimerval tiny{{0, 999}, {1, 0}};
    try {
        setitimer_nid_postfix(0, &tiny, nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && Micros(Read(0).value) == 0, "an interval below the clock tick was armed");

    GuestItimerval previous{};
    Require(setitimer_nid_postfix(0, &valid, &previous) == 0 && Micros(previous.value) == 0, "arming ITIMER_REAL failed");
    const auto first = Micros(Read(0).value);
    Require(first > 29000000 && first <= 30000000, "the armed value is not visible");
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const auto second = Micros(Read(0).value);
    Require(second > 0 && second < first, "the remaining time does not count down");
    const GuestItimerval repeating{{2, 500000}, {20, 0}};
    Require(setitimer_nid_postfix(0, &repeating, &previous) == 0, "re-arming ITIMER_REAL failed");
    Require(Micros(previous.interval) == 0 && Micros(previous.value) > 0 && Micros(previous.value) <= second, "re-arming did not return the previous value");
    GuestItimerval queried{{11, 22}, {33, 44}};
    Require(getitimer_nid_postfix(0, &queried) == 0, "getitimer failed");
    Require(Micros(queried.interval) == 2500000 && Micros(queried.value) > 19000000 && Micros(queried.value) <= 20000000,
        "getitimer did not report the setitimer interval");
    const GuestItimerval zero{{0, 0}, {0, 0}};
    Require(setitimer_nid_postfix(0, &zero, &previous) == 0, "disarming ITIMER_REAL failed");
    Require(Micros(previous.interval) == 2500000 && Micros(previous.value) > 19000000 && Micros(previous.value) <= 20000000,
        "disarming did not return the previous value");
    const auto idle = Read(0);
    Require(Micros(idle.interval) == 0 && Micros(idle.value) == 0, "the disarmed timer is still set");

    const GuestItimerval periodic{{0, 300000}, {0, 100000}};
    Require(setitimer_nid_postfix(0, &periodic, nullptr) == 0, "arming a periodic timer failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const auto rearmed = Read(0);
    Require(Micros(rearmed.interval) == 300000 && Micros(rearmed.value) > 0 && Micros(rearmed.value) <= 300000,
        "the periodic timer was not re-armed by its interval");
    Require(GuestAlarm_nid_no_patch(5) == 1, "alarm did not report the periodic timer");
    const auto alarmed = Read(0);
    Require(Micros(alarmed.interval) == 0 && Micros(alarmed.value) > 4000000 && Micros(alarmed.value) <= 5000000, "alarm did not replace the timer");
    Require(setitimer_nid_postfix(0, &zero, &previous) == 0 && Micros(previous.value) > 0, "disarming the alarm failed");
    Require(GuestAlarm_nid_no_patch(0) == 0, "setitimer did not cancel the alarm");

    const GuestSignalSet alarmSet{{1u << 13, 0, 0, 0}};
    const GuestTimespec poll{0, 0};
    const GuestTimespec patience{2, 0};
    GuestSignalSet unblocked{};
    Require(_sigprocmask_nid_postfix(1, &alarmSet, &unblocked) == 0, "blocking SIGALRM failed");
    ExpireOnce();
    Require(!AlarmPending(), "an ignored expiry was left pending");
    Require(signal_nid_postfix(14, nullptr) == ignore, "SIGALRM was not defaulted");
    Require(setitimer_nid_postfix(0, &once, nullptr) == 0, "arming the waited timer failed");
    Require(sigtimedwait_nid_postfix(&alarmSet, nullptr, &patience) == 14, "sigtimedwait did not take the blocked expiry");
    Require(!AlarmPending(), "the taken expiry is still pending");
    const GuestItimerval frequent{{0, 20000}, {0, 20000}};
    Require(setitimer_nid_postfix(0, &frequent, nullptr) == 0, "arming the frequent timer failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Require(setitimer_nid_postfix(0, &zero, nullptr) == 0, "disarming the frequent timer failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(sigtimedwait_nid_postfix(&alarmSet, nullptr, &poll) == 14, "the repeated expiries were not left pending");
    *__error_nid_postfix() = 0;
    Require(sigtimedwait_nid_postfix(&alarmSet, nullptr, &poll) == -1 && *__error_nid_postfix() == 35,
        "the repeated expiries were not merged into one pending signal");
    Require(signal_nid_postfix(14, OnAlarm) == nullptr, "SIGALRM handler was not installed while blocked");
    ExpireOnce();
    Require(delivered == 0 && AlarmPending(), "a blocked expiry with a handler was not left pending");
    Require(_sigprocmask_nid_postfix(3, &unblocked, nullptr) == 0, "unblocking SIGALRM failed");
    Require(delivered == 14, "unblocking SIGALRM did not run its handler");
    Require(_sigprocmask_nid_postfix(1, &alarmSet, nullptr) == 0 && !AlarmPending(), "the delivered expiry is still pending");
    Require(_sigprocmask_nid_postfix(3, &unblocked, nullptr) == 0, "restoring the signal mask failed");
    delivered = 0;
    Require(signal_nid_postfix(14, ignore) == OnAlarm, "SIGALRM was not ignored again");

    Require(signal_nid_postfix(14, OnAlarm) == ignore, "SIGALRM handler was not installed");
    _ZSt13set_terminatePFvvE_nid_postfix(OnTerminate);
    const GuestItimerval handled{{0, 250000}, {0, 50000}};
    Require(setitimer_nid_postfix(0, &handled, nullptr) == 0, "arming the handled timer failed");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::fprintf(stderr, "the expiry with a handler neither delivered nor threw\n");
    return 1;
}

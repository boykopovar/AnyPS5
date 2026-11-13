#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <source_location>
#include <thread>
#include <vector>

namespace {

}

namespace {

constexpr int Efault = 14;
constexpr int Einval = 22;
constexpr int Eagain = 35;
constexpr int ClockRealtime = 0;
constexpr int ClockMonotonic = 4;
constexpr int ClockUptime = 5;
constexpr int TimerAbstime = 1;
constexpr std::int64_t Millisecond = 1000000;
constexpr std::int64_t Second = 1000000000;

const GuestSigevent None{0, 0, 0, {}};

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::exit(1);
}

int Error() {
    return *__error_nid_postfix();
}

std::int64_t Nanoseconds(const KernelTimespec& time) {
    return time.tv_sec * Second + time.tv_nsec;
}

int Create(int clock = ClockMonotonic) {
    int id = -1;
    Require(ktimer_create_nid_postfix(clock, &None, &id) == 0);
    return id;
}

GuestItimerspec Get(int id) {
    GuestItimerspec value{{-1, -1}, {-1, -1}};
    Require(ktimer_gettime_nid_postfix(id, &value) == 0);
    return value;
}

void Sleep(std::int64_t nanoseconds) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
}

void Creation() {
    int id = -1;
    const GuestSigevent badNotify{2, 0, 0, {}};
    const GuestSigevent noSignal{1, 0, 0, {}};
    const GuestSigevent badSignal{4, 129, 0, {}};
    Require(ktimer_create_nid_postfix(ClockUptime, &None, &id) == -1 && Error() == Einval && id == -1);
    Require(ktimer_create_nid_postfix(-1, &None, &id) == -1 && Error() == Einval);
    Require(ktimer_create_nid_postfix(ClockMonotonic, &badNotify, &id) == -1 && Error() == Einval);
    Require(ktimer_create_nid_postfix(ClockMonotonic, &noSignal, &id) == -1 && Error() == Einval);
    Require(ktimer_create_nid_postfix(ClockMonotonic, &badSignal, &id) == -1 && Error() == Einval);
    Require(ktimer_create_nid_postfix(ClockMonotonic, &None, nullptr) == -1 && Error() == Efault);

    std::vector<int> ids;
    for (int expected = 3; expected < 32; ++expected) {
        ids.push_back(Create(expected % 2 == 0 ? ClockRealtime : ClockMonotonic));
        Require(ids.back() == expected);
    }
    Require(ktimer_create_nid_postfix(ClockMonotonic, &None, &id) == -1 && Error() == Eagain);
    Require(ktimer_delete_nid_postfix(7) == 0);
    Require(Create() == 7);
    for (const int each : ids) Require(ktimer_delete_nid_postfix(each) == 0);
}

void InvalidIds() {
    GuestItimerspec value{};
    const int id = Create();
    Require(ktimer_delete_nid_postfix(id) == 0);
    for (const int bad : {-1, 0, 2, id, 32}) {
        Require(ktimer_delete_nid_postfix(bad) == -1 && Error() == Einval);
        Require(ktimer_settime_nid_postfix(bad, 0, &value, nullptr) == -1 && Error() == Einval);
        Require(ktimer_gettime_nid_postfix(bad, &value) == -1 && Error() == Einval);
        Require(ktimer_gettime_nid_postfix(bad, nullptr) == -1 && Error() == Einval);
        Require(ktimer_getoverrun_nid_postfix(bad) == -1 && Error() == Einval);
    }
    Require(ktimer_settime_nid_postfix(id, 0, nullptr, nullptr) == -1 && Error() == Efault);
    const int valid = Create();
    Require(ktimer_gettime_nid_postfix(valid, nullptr) == -1 && Error() == Efault);
    Require(ktimer_delete_nid_postfix(valid) == 0);
}

void Values() {
    const int id = Create();
    GuestItimerspec disarmed = Get(id);
    Require(Nanoseconds(disarmed.it_value) == 0 && Nanoseconds(disarmed.it_interval) == 0);
    for (const GuestItimerspec bad : {GuestItimerspec{{0, 0}, {-1, 0}}, GuestItimerspec{{0, 0}, {0, -1}}, GuestItimerspec{{0, 0}, {0, Second}},
                                 GuestItimerspec{{-1, 0}, {1, 0}}, GuestItimerspec{{0, Second}, {1, 0}}})
        Require(ktimer_settime_nid_postfix(id, 0, &bad, nullptr) == -1 && Error() == Einval);
    const GuestItimerspec valid{{0, 0}, {1, 0}};
    for (const int flags : {2, 4, 0x7ffffffe}) {
        Require(ktimer_settime_nid_postfix(id, flags, &valid, nullptr) == 0);
        const auto relative = Get(id);
        Require(Nanoseconds(relative.it_value) > 0 && Nanoseconds(relative.it_value) <= Second);
    }
    KernelTimespec now{};
    Require(clock_gettime_nid_postfix(ClockMonotonic, &now) == 0);
    const GuestItimerspec future{{0, 0}, {now.tv_sec + 5, now.tv_nsec}};
    Require(ktimer_settime_nid_postfix(id, -1, &future, nullptr) == 0);
    const auto absolute = Get(id);
    Require(Nanoseconds(absolute.it_value) > 4 * Second && Nanoseconds(absolute.it_value) <= 5 * Second);

    const GuestItimerspec clear{{-1, 0}, {0, 0}};
    Require(ktimer_settime_nid_postfix(id, 0, &clear, nullptr) == 0);
    disarmed = Get(id);
    Require(Nanoseconds(disarmed.it_value) == 0 && Nanoseconds(disarmed.it_interval) == 0);

    const GuestItimerspec small{{0, 1}, {10, 0}};
    Require(ktimer_settime_nid_postfix(id, 0, &small, nullptr) == 0);
    GuestItimerspec old{};
    const GuestItimerspec later{{0, 0}, {20, 0}};
    Require(ktimer_settime_nid_postfix(id, 0, &later, &old) == 0);
    Require(Nanoseconds(old.it_interval) == Millisecond);
    Require(Nanoseconds(old.it_value) > 9 * Second && Nanoseconds(old.it_value) <= 10 * Second);
    const GuestItimerspec tiny{{0, 0}, {0, 1}};
    Require(ktimer_settime_nid_postfix(id, 0, &tiny, &old) == 0);
    Require(Nanoseconds(old.it_value) > 19 * Second && Nanoseconds(old.it_value) <= 20 * Second);
    const GuestItimerspec off{};
    Require(ktimer_settime_nid_postfix(id, 0, &off, &old) == 0);
    Require(Nanoseconds(old.it_interval) == 0 && Nanoseconds(old.it_value) <= Millisecond);
    Require(ktimer_delete_nid_postfix(id) == 0);
}

void Expiry() {
    const int id = Create();
    const GuestItimerspec once{{0, 0}, {0, 30 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &once, nullptr) == 0);
    const auto running = Get(id);
    Require(Nanoseconds(running.it_value) > 0 && Nanoseconds(running.it_value) <= 30 * Millisecond);
    Sleep(60 * Millisecond);
    const auto expired = Get(id);
    Require(Nanoseconds(expired.it_value) == 0 && Nanoseconds(expired.it_interval) == 0);
    Require(ktimer_getoverrun_nid_postfix(id) == 0);

    const GuestItimerspec periodic{{0, 10 * Millisecond}, {0, 10 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &periodic, nullptr) == 0);
    Sleep(55 * Millisecond);
    const auto period = Get(id);
    Require(Nanoseconds(period.it_interval) == 10 * Millisecond);
    Require(Nanoseconds(period.it_value) > 0 && Nanoseconds(period.it_value) <= 10 * Millisecond);
    Require(ktimer_getoverrun_nid_postfix(id) == 0);
    Require(ktimer_delete_nid_postfix(id) == 0);
}

void Absolute() {
    for (const int clock : {ClockRealtime, ClockMonotonic}) {
        const int id = Create(clock);
        KernelTimespec now{};
        Require(clock_gettime_nid_postfix(clock, &now) == 0);
        const GuestItimerspec future{{0, 0}, {now.tv_sec + 5, now.tv_nsec}};
        Require(ktimer_settime_nid_postfix(id, TimerAbstime, &future, nullptr) == 0);
        const auto remaining = Get(id);
        Require(Nanoseconds(remaining.it_value) > 4 * Second && Nanoseconds(remaining.it_value) <= 5 * Second);

        const GuestItimerspec past{{0, 0}, {now.tv_sec - 1, now.tv_nsec}};
        Require(ktimer_settime_nid_postfix(id, TimerAbstime, &past, nullptr) == 0);
        Require(Nanoseconds(Get(id).it_value) == 0);
        const GuestItimerspec pastPeriodic{{1, 0}, {now.tv_sec - 10, now.tv_nsec}};
        Require(ktimer_settime_nid_postfix(id, TimerAbstime, &pastPeriodic, nullptr) == 0);
        const auto moved = Get(id);
        Require(Nanoseconds(moved.it_value) > 0 && Nanoseconds(moved.it_value) <= Second);
        Require(Nanoseconds(moved.it_interval) == Second);
        Require(ktimer_delete_nid_postfix(id) == 0);
    }
}

}

static void SettimeGuestBuffers() {
    const int id = Create();
    auto* const badValue = reinterpret_cast<GuestItimerspec*>(1);
    Require(ktimer_settime_nid_postfix(id, 0, badValue, nullptr) == -1 && Error() == Efault);
    Require(ktimer_settime_nid_postfix(-1, 0, badValue, nullptr) == -1 && Error() == Efault);
    Require(Nanoseconds(Get(id).it_value) == 0);
    const GuestItimerspec next{{3, 0}, {20, 0}};
    Require(ktimer_settime_nid_postfix(id, 0, &next, badValue) == -1 && Error() == Efault);
    const auto active = Get(id);
    Require(Nanoseconds(active.it_interval) == 3 * Second && Nanoseconds(active.it_value) > 0);
    Require(ktimer_delete_nid_postfix(id) == 0);
}

namespace {

constexpr int Sigalrm = 14;
constexpr int Sigusr1 = 30;
constexpr int Sigusr2 = 31;
constexpr int SigBlock = 1;
constexpr int SigUnblock = 2;
constexpr std::int32_t SiTimer = 0x10003;
constexpr int SigevSignal = 1;
constexpr int SigevThreadId = 4;

struct Siginfo {
    std::int32_t signo;
    std::int32_t error;
    std::int32_t code;
    std::int32_t pid;
    std::uint32_t uid;
    std::int32_t status;
    std::uint64_t address;
    std::int64_t value;
    std::int32_t timerid;
    std::int32_t overrun;
    std::int32_t spare[8];
};
static_assert(sizeof(Siginfo) == 80);

GuestSignalSet Only(int signal) {
    GuestSignalSet set{{0, 0, 0, 0}};
    set.bits[(signal - 1) / 32] = 1u << ((signal - 1) % 32);
    return set;
}

void Mask(int how, int signal) {
    const GuestSignalSet set = Only(signal);
    Require(sigprocmask_nid_postfix(how, &set, nullptr) == 0);
}

int Take(int signal, std::int64_t timeout, Siginfo& info) {
    const GuestSignalSet set = Only(signal);
    const KernelTimespec wait{timeout / Second, timeout % Second};
    info = {};
    return sigtimedwait_nid_postfix(&set, &info, &wait);
}

bool Pending(int signal) {
    GuestSignalSet pending{{0, 0, 0, 0}};
    Require(sigpending_nid_postfix(&pending) == 0);
    return (pending.bits[(signal - 1) / 32] & Only(signal).bits[(signal - 1) / 32]) != 0;
}

void RequireTimerInfo(const Siginfo& info, int signal, int id, std::int64_t value) {
    Require(info.signo == signal && info.code == SiTimer && info.pid == 0 && info.error == 0);
    Require(info.value == value && info.timerid == id);
}

int CreateSignal(int notify, int signal, std::int64_t value, std::int64_t thread = 0) {
    const GuestSigevent event{notify, signal, value, {thread}};
    int id = -1;
    Require(ktimer_create_nid_postfix(ClockMonotonic, &event, &id) == 0);
    return id;
}

std::atomic<int> handled{0};

void APS5_VABI OnUsr1(int signal) {
    if (signal == Sigusr1) handled.fetch_add(1);
}

void SignalPeriodic() {
    constexpr std::int64_t Value = 0x123456789abcdefLL;
    Mask(SigBlock, Sigusr1);
    const int id = CreateSignal(SigevSignal, Sigusr1, Value);
    Require(ktimer_getoverrun_nid_postfix(id) == 0);
    const GuestItimerspec periodic{{0, 20 * Millisecond}, {0, 20 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &periodic, nullptr) == 0);
    for (int expiry = 0; expiry < 3; ++expiry) {
        Siginfo info{};
        Require(Take(Sigusr1, Second, info) == Sigusr1);
        RequireTimerInfo(info, Sigusr1, id, Value);
        Require(ktimer_getoverrun_nid_postfix(id) == info.overrun);
    }
    Require(Nanoseconds(Get(id).it_interval) == 20 * Millisecond);

    const GuestItimerspec off{};
    Require(ktimer_settime_nid_postfix(id, 0, &off, nullptr) == 0);
    Siginfo info{};
    Take(Sigusr1, 0, info);
    Require(!Pending(Sigusr1));
    Require(signal_nid_postfix(Sigusr1, OnUsr1) == nullptr);
    const GuestItimerspec once{{0, 0}, {0, 10 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &once, nullptr) == 0);
    Sleep(60 * Millisecond);
    Require(Pending(Sigusr1) && handled.load() == 0);
    Mask(SigUnblock, Sigusr1);
    Require(handled.load() == 1 && !Pending(Sigusr1));
    Mask(SigBlock, Sigusr1);
    Require(Nanoseconds(Get(id).it_value) == 0);
    Require(ktimer_delete_nid_postfix(id) == 0);
    signal_nid_postfix(Sigusr1, nullptr);
}

void DefaultEvent() {
    Mask(SigBlock, Sigalrm);
    int id = -1;
    Require(ktimer_create_nid_postfix(ClockMonotonic, nullptr, &id) == 0);
    const GuestItimerspec once{{0, 0}, {0, 10 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &once, nullptr) == 0);
    Siginfo info{};
    Require(Take(Sigalrm, Second, info) == Sigalrm);
    RequireTimerInfo(info, Sigalrm, id, id);
    Require(info.overrun == 0 && ktimer_getoverrun_nid_postfix(id) == 0);
    Require(ktimer_delete_nid_postfix(id) == 0);
}

void Overrun() {
    Mask(SigBlock, Sigusr1);
    const int id = CreateSignal(SigevSignal, Sigusr1, 7);
    const GuestItimerspec periodic{{0, 10 * Millisecond}, {0, 10 * Millisecond}};
    const auto start = std::chrono::steady_clock::now();
    Require(ktimer_settime_nid_postfix(id, 0, &periodic, nullptr) == 0);
    Sleep(125 * Millisecond);
    Require(Pending(Sigusr1) && ktimer_getoverrun_nid_postfix(id) == 0);
    Siginfo info{};
    Require(Take(Sigusr1, 0, info) == Sigusr1);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    RequireTimerInfo(info, Sigusr1, id, 7);
    Require(info.overrun >= 5 && info.overrun <= elapsed / 10);
    Require(ktimer_getoverrun_nid_postfix(id) == info.overrun);
    Require(Take(Sigusr1, Second, info) == Sigusr1);
    RequireTimerInfo(info, Sigusr1, id, 7);
    Require(ktimer_getoverrun_nid_postfix(id) == info.overrun);
    Require(ktimer_delete_nid_postfix(id) == 0);
    Require(ktimer_getoverrun_nid_postfix(id) == -1 && Error() == Einval);
}

void DeleteStops() {
    Mask(SigBlock, Sigusr1);
    const int id = CreateSignal(SigevSignal, Sigusr1, 9);
    const GuestItimerspec periodic{{0, 10 * Millisecond}, {0, 10 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &periodic, nullptr) == 0);
    Siginfo info{};
    Require(Take(Sigusr1, Second, info) == Sigusr1);
    Sleep(30 * Millisecond);
    Require(Pending(Sigusr1));
    Require(ktimer_delete_nid_postfix(id) == 0);
    Require(!Pending(Sigusr1));
    Sleep(50 * Millisecond);
    Require(Take(Sigusr1, 0, info) == -1 && Error() == Eagain);
}

void ThreadTarget() {
    constexpr std::int64_t Value = -5;
    std::atomic<int> target{0};
    std::atomic<bool> ready{false};
    Siginfo received{};
    int result = 0;
    std::thread waiter([&] {
        target.store(pthread_getthreadid_np_nid_postfix());
        Mask(SigBlock, Sigusr2);
        ready.store(true);
        result = Take(Sigusr2, 2 * Second, received);
    });
    while (!ready.load()) Sleep(Millisecond);
    Mask(SigUnblock, Sigusr2);
    const int id = CreateSignal(SigevThreadId, Sigusr2, Value, target.load());
    const GuestItimerspec once{{0, 0}, {0, 20 * Millisecond}};
    Require(ktimer_settime_nid_postfix(id, 0, &once, nullptr) == 0);
    waiter.join();
    Require(result == Sigusr2);
    RequireTimerInfo(received, Sigusr2, id, Value);
    Require(ktimer_delete_nid_postfix(id) == 0);

    const int missing = CreateSignal(SigevThreadId, Sigusr2, Value, 0x7ffffff0);
    const GuestItimerspec periodic{{0, 10 * Millisecond}, {0, 10 * Millisecond}};
    Require(ktimer_settime_nid_postfix(missing, 0, &periodic, nullptr) == 0);
    Sleep(50 * Millisecond);
    const auto disarmed = Get(missing);
    Require(Nanoseconds(disarmed.it_value) == 0 && Nanoseconds(disarmed.it_interval) == 0);
    Require(!Pending(Sigusr2));
    Require(ktimer_delete_nid_postfix(missing) == 0);
}

void SignalCreation() {
    int id = -1;
    for (const int notify : {SigevSignal, SigevThreadId})
        for (const int signal : {0, -1, 129}) {
            const GuestSigevent event{notify, signal, 0, {}};
            Require(ktimer_create_nid_postfix(ClockMonotonic, &event, &id) == -1 && Error() == Einval && id == -1);
        }
    for (const int notify : {-1, 3, 5}) {
        const GuestSigevent event{notify, Sigusr1, 0, {}};
        Require(ktimer_create_nid_postfix(ClockMonotonic, &event, &id) == -1 && Error() == Einval && id == -1);
    }
    Require(ktimer_create_nid_postfix(ClockMonotonic, reinterpret_cast<const GuestSigevent*>(1), &id) == -1 && Error() == Efault && id == -1);
    Require(ktimer_create_nid_postfix(ClockUptime, nullptr, &id) == -1 && Error() == Einval && id == -1);
}

}

int main() {
    Creation();
    InvalidIds();
    Values();
    Expiry();
    Absolute();
    SettimeGuestBuffers();
    SignalCreation();
    SignalPeriodic();
    DefaultEvent();
    Overrun();
    DeleteStops();
    ThreadTarget();
    std::puts("guest_kernel_sys_ktimer passed");
    return 0;
}

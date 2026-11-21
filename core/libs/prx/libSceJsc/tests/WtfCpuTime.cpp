#include "SceTypes.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using Jsc::WtfAbi::OptionalCPUTime;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF CPUTime check failed at line %d\n", line);
        std::abort();
    }
}

double MonotonicSeconds() {
    KernelTimespec ts{};
    Check(clock_gettime_nid_postfix(4, &ts) == 0, __LINE__);
    return static_cast<double>(ts.tv_sec) + ts.tv_nsec / 1.0e9;
}

GuestResourceUsage Usage() {
    GuestResourceUsage usage{};
    Check(getrusage_nid_postfix(0, &usage) == 0, __LINE__);
    return usage;
}

double Seconds(const KernelTimeval& value) {
    return static_cast<double>(value.tv_sec) + static_cast<double>(value.tv_usec) / 1000 / 1000;
}

volatile std::uint64_t sink;

double ChargedSeconds() {
    const GuestResourceUsage usage = Usage();
    return Seconds(usage.ru_utime) + Seconds(usage.ru_stime);
}

void BurnCpuPast(double charged) {
    const auto start = std::chrono::steady_clock::now();
    const auto minimum = start + std::chrono::milliseconds(50);
    const auto deadline = start + std::chrono::seconds(10);
    std::uint64_t value = 1;
    while (std::chrono::steady_clock::now() < minimum || ChargedSeconds() <= charged) {
        Check(std::chrono::steady_clock::now() < deadline, __LINE__);
        for (int i = 0; i < 10000; ++i) value = value * 6364136223846793005ull + 1442695040888963407ull;
    }
    sink = value;
}

void TestLayout() {
    Check(offsetof(OptionalCPUTime, hasValue) == 0, __LINE__);
    Check(offsetof(OptionalCPUTime, value) == 8, __LINE__);
    Check(sizeof(OptionalCPUTime) == 32, __LINE__);
}

void TestMatchesKernel() {
    const double monotonicBefore = MonotonicSeconds();
    const GuestResourceUsage before = Usage();
    const OptionalCPUTime time = _ZN3WTF7CPUTime3getEv();
    const GuestResourceUsage after = Usage();
    const double monotonicAfter = MonotonicSeconds();
    Check(time.hasValue, __LINE__);
    Check(time.value.cpuTime >= monotonicBefore, __LINE__);
    Check(time.value.cpuTime <= monotonicAfter, __LINE__);
    Check(time.value.userTime >= Seconds(before.ru_utime), __LINE__);
    Check(time.value.userTime <= Seconds(after.ru_utime), __LINE__);
    Check(time.value.systemTime >= Seconds(before.ru_stime), __LINE__);
    Check(time.value.systemTime <= Seconds(after.ru_stime), __LINE__);
}

void TestAdvances() {
    const OptionalCPUTime first = _ZN3WTF7CPUTime3getEv();
    BurnCpuPast(first.value.userTime + first.value.systemTime);
    const OptionalCPUTime second = _ZN3WTF7CPUTime3getEv();
    Check(first.hasValue && second.hasValue, __LINE__);
    Check(second.value.cpuTime - first.value.cpuTime >= 0.05, __LINE__);
    Check(second.value.userTime + second.value.systemTime > first.value.userTime + first.value.systemTime, __LINE__);
    Check(second.value.userTime >= first.value.userTime, __LINE__);
    Check(second.value.systemTime >= first.value.systemTime, __LINE__);
}

} // namespace

int main() {
    TestLayout();
    TestAdvances();
    TestMatchesKernel();
    return 0;
}

#include "SceTypes.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "Testing.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using Jsc::WtfAbi::OptionalCPUTime;

double MonotonicSeconds() {
    KernelTimespec ts{};
    Require(clock_gettime_nid_postfix(4, &ts) == 0);
    return static_cast<double>(ts.tv_sec) + ts.tv_nsec / 1.0e9;
}

GuestResourceUsage Usage() {
    GuestResourceUsage usage{};
    Require(getrusage_nid_postfix(0, &usage) == 0);
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
        Require(std::chrono::steady_clock::now() < deadline);
        for (int i = 0; i < 10000; ++i) value = value * 6364136223846793005ull + 1442695040888963407ull;
    }
    sink = value;
}

void TestLayout() {
    Require(offsetof(OptionalCPUTime, hasValue) == 0);
    Require(offsetof(OptionalCPUTime, value) == 8);
    Require(sizeof(OptionalCPUTime) == 32);
}

void TestMatchesKernel() {
    const double monotonicBefore = MonotonicSeconds();
    const GuestResourceUsage before = Usage();
    const OptionalCPUTime time = _ZN3WTF7CPUTime3getEv();
    const GuestResourceUsage after = Usage();
    const double monotonicAfter = MonotonicSeconds();
    Require(time.hasValue);
    Require(time.value.cpuTime >= monotonicBefore);
    Require(time.value.cpuTime <= monotonicAfter);
    Require(time.value.userTime >= Seconds(before.ru_utime));
    Require(time.value.userTime <= Seconds(after.ru_utime));
    Require(time.value.systemTime >= Seconds(before.ru_stime));
    Require(time.value.systemTime <= Seconds(after.ru_stime));
}

void TestAdvances() {
    const OptionalCPUTime first = _ZN3WTF7CPUTime3getEv();
    BurnCpuPast(first.value.userTime + first.value.systemTime);
    const OptionalCPUTime second = _ZN3WTF7CPUTime3getEv();
    Require(first.hasValue && second.hasValue);
    Require(second.value.cpuTime - first.value.cpuTime >= 0.05);
    Require(second.value.userTime + second.value.systemTime > first.value.userTime + first.value.systemTime);
    Require(second.value.userTime >= first.value.userTime);
    Require(second.value.systemTime >= first.value.systemTime);
}

} // namespace

int main() {
    TestLayout();
    TestAdvances();
    TestMatchesKernel();
    return 0;
}

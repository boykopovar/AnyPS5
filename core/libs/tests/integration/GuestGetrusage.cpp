#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

struct Timeval {
    std::int64_t tv_sec;
    std::int64_t tv_usec;
};

struct ResourceUsage {
    Timeval ru_utime;
    Timeval ru_stime;
    std::int64_t rest[14];
};

extern "C" int APS5_VABI getrusage_nid_postfix(int who, ResourceUsage* usage);

namespace {

using Testing::Case;
using Testing::Require;

constexpr int usageSelf = 0;
constexpr int usageThread = 1;

struct Sample {
    bool valid = false;
    std::int64_t micros = 0;
};

Sample CpuMicros(int who) {
    ResourceUsage usage{};
    if (getrusage_nid_postfix(who, &usage) != 0) return {};
    return {true, (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000 + usage.ru_utime.tv_usec + usage.ru_stime.tv_usec};
}

std::int64_t RequireCpuMicros(int who) {
    const Sample sample = CpuMicros(who);
    Require(sample.valid, "getrusage(" + std::to_string(who) + ") succeeds");
    return sample.micros;
}

bool SpinUntil(int who, std::int64_t micros) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    volatile std::uint64_t sink = 0;
    for (;;) {
        const Sample sample = CpuMicros(who);
        if (!sample.valid) return false;
        if (sample.micros >= micros) return true;
        for (int i = 0; i < 100000; ++i) sink = sink + i;
        if (std::chrono::steady_clock::now() >= deadline) return false;
    }
}

void RequireSpin(int who, std::int64_t micros) {
    Require(SpinUntil(who, micros), "cpu time of " + std::to_string(who) + " reaches " + std::to_string(micros) + " us within 10 s");
}

struct WorkerUsage {
    std::int64_t fresh = 0;
    std::int64_t thread = 0;
    std::int64_t process = 0;
    bool valid = false;
};

WorkerUsage SpinInNewThread(std::int64_t spinMicros) {
    WorkerUsage result;
    std::thread worker([&result, spinMicros] {
        const Sample fresh = CpuMicros(usageThread);
        if (!fresh.valid || !SpinUntil(usageThread, fresh.micros + spinMicros)) return;
        const Sample process = CpuMicros(usageSelf);
        const Sample thread = CpuMicros(usageThread);
        result = {fresh.micros, thread.micros, process.micros, process.valid && thread.valid};
    });
    worker.join();
    Require(result.valid, "worker thread measured its cpu time");
    return result;
}

const Case mainThread{"Getrusage_ThreadAfterSpinningMainThread_ReportsMainThreadCpuTime", [] {
    const std::int64_t processBefore = RequireCpuMicros(usageSelf);
    const std::int64_t threadBefore = RequireCpuMicros(usageThread);
    RequireSpin(usageSelf, processBefore + 400000);
    const std::int64_t spent = RequireCpuMicros(usageThread) - threadBefore;
    Require(spent >= 200000, "main thread cpu time grew by at least 200 ms, got " + std::to_string(spent) + " us");
}};

const Case freshThread{"Getrusage_ThreadInNewThread_StartsBelow100Ms", [] {
    const WorkerUsage usage = SpinInNewThread(100000);
    Require(usage.fresh < 100000, "fresh thread cpu time below 100 ms, got " + std::to_string(usage.fresh) + " us");
}};

const Case wholeProcess{"Getrusage_Self_IncludesEveryThread", [] {
    const std::int64_t processBefore = RequireCpuMicros(usageSelf);
    RequireSpin(usageSelf, processBefore + 400000);
    const WorkerUsage usage = SpinInNewThread(100000);
    Require(usage.process >= usage.thread + 300000, "process time " + std::to_string(usage.process) +
            " us includes main thread work beyond worker time " + std::to_string(usage.thread) + " us");
    const std::int64_t processAfter = RequireCpuMicros(usageSelf);
    Require(processAfter >= processBefore + 500000, "process cpu time grew by at least 500 ms, got " +
            std::to_string(processAfter - processBefore) + " us");
}};

} // namespace

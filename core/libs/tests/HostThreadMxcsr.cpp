#include "prx/libc/include/FailureReport.hpp"
#include "prx/libc/include/HostThread.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace {

using ReportFn = void (*)() noexcept;

void Require(bool value) {
    if (!value) std::abort();
}

std::atomic<int> reportHits[9]{};

template <int Index>
void TestReport() noexcept {
    reportHits[Index].fetch_add(1, std::memory_order_relaxed);
}

constexpr ReportFn TestReports[9] = {
    &TestReport<0>, &TestReport<1>, &TestReport<2>,
    &TestReport<3>, &TestReport<4>, &TestReport<5>,
    &TestReport<6>, &TestReport<7>, &TestReport<8>,
};

struct FailureReportBarrier {
    std::atomic<int> arrived{0};
    std::atomic<int> round{0};
    std::atomic<bool> stop{false};
    std::atomic<bool> failed{false};
};

int ProbeFreeFailureReportSlots() {
#ifdef _WIN32
    return 4;
#else
    int count = 0;
    for (; count < 8; ++count) {
        try {
            LibcRegisterFailureReport_nid_no_patch(TestReports[count]);
        } catch (const std::runtime_error&) {
            break;
        }
    }
    for (int i = 0; i < count; ++i) {
        LibcUnregisterFailureReport_nid_no_patch(TestReports[i]);
    }
    return count;
#endif
}

void RegisterSharedReportWorker(FailureReportBarrier* state) {
    int nextRound = 1;
    while (true) {
        while (state->round.load(std::memory_order_acquire) < nextRound) {
            if (state->stop.load(std::memory_order_acquire)) return;
        }
        if (state->stop.load(std::memory_order_acquire)) return;
        try {
            LibcRegisterFailureReport_nid_no_patch(TestReports[0]);
        } catch (...) {
            state->failed.store(true, std::memory_order_release);
        }
        state->arrived.fetch_add(1, std::memory_order_release);
        ++nextRound;
    }
}

void TestFailureReportSingleThreaded(int freeSlots) {
#ifndef _WIN32
    bool threwInvalid = false;
    try {
        LibcRegisterFailureReport_nid_no_patch(nullptr);
    } catch (const std::invalid_argument&) {
        threwInvalid = true;
    }
    Require(threwInvalid);
#endif

    LibcRegisterFailureReport_nid_no_patch(TestReports[0]);
    LibcRegisterFailureReport_nid_no_patch(TestReports[0]);
    for (int i = 1; i < freeSlots; ++i) {
        LibcRegisterFailureReport_nid_no_patch(TestReports[i]);
    }

    LibcUnregisterFailureReport_nid_no_patch(nullptr);

#ifndef _WIN32
    bool threwFull = false;
    try {
        LibcRegisterFailureReport_nid_no_patch(TestReports[freeSlots]);
    } catch (const std::runtime_error&) {
        threwFull = true;
    }
    Require(threwFull);
#endif

    LibcUnregisterFailureReport_nid_no_patch(TestReports[0]);
    LibcUnregisterFailureReport_nid_no_patch(TestReports[0]);
    LibcRegisterFailureReport_nid_no_patch(TestReports[freeSlots]);

    for (int i = 1; i <= freeSlots; ++i) {
        LibcUnregisterFailureReport_nid_no_patch(TestReports[i]);
    }
    LibcUnregisterFailureReport_nid_no_patch(TestReports[freeSlots]);
}

void TestFailureReportConcurrentDedup(int freeSlots) {
    static constexpr int WorkerCount = 4;
    static constexpr int Rounds = 128;

    FailureReportBarrier state;
    std::thread workers[WorkerCount];
    for (int i = 0; i < WorkerCount; ++i) {
        workers[i] = std::thread(RegisterSharedReportWorker, &state);
    }

    for (int r = 1; r <= Rounds; ++r) {
        state.arrived.store(0, std::memory_order_release);
        state.round.store(r, std::memory_order_release);
        while (state.arrived.load(std::memory_order_acquire) < WorkerCount) {}
        Require(!state.failed.load(std::memory_order_acquire));

        bool threw = false;
        try {
            for (int i = 1; i < freeSlots; ++i) {
                LibcRegisterFailureReport_nid_no_patch(TestReports[i]);
            }
        } catch (const std::runtime_error&) {
            threw = true;
        }
        for (int i = 0; i < freeSlots; ++i) {
            LibcUnregisterFailureReport_nid_no_patch(TestReports[i]);
        }
        Require(!threw);
    }

    state.stop.store(true, std::memory_order_release);
    state.round.store(Rounds + 1, std::memory_order_release);
    for (int i = 0; i < WorkerCount; ++i) {
        workers[i].join();
    }
}

}

int main() {
    const auto initial = _mm_getcsr();
    _mm_setcsr(0x9fc0u);
    std::atomic<std::uint32_t> threadWorker{0};
    std::atomic<std::uint32_t> jthreadWorker{0};
    auto thread = Libc::StartHostThread([&] { threadWorker.store(_mm_getcsr(), std::memory_order_relaxed); });
    thread.join();
    auto jthread = Libc::StartHostJThread([&](std::stop_token) { jthreadWorker.store(_mm_getcsr(), std::memory_order_relaxed); });
    jthread.join();
    const auto caller = _mm_getcsr();
    _mm_setcsr(initial);
    if (caller != 0x9fc0u || threadWorker.load(std::memory_order_relaxed) != 0x1f80u || jthreadWorker.load(std::memory_order_relaxed) != 0x1f80u)
        throw std::runtime_error("a host worker inherited guest MXCSR");

    const int freeSlots = ProbeFreeFailureReportSlots();
    Require(freeSlots >= 2);
    TestFailureReportSingleThreaded(freeSlots);
    TestFailureReportConcurrentDedup(freeSlots);
}

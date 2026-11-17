#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "Testing.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <thread>

extern "C" void CpuTimeClockForTest_nid_no_patch(int (*query)(int, KernelTimespec*));

namespace {

int g_queries = 0;
int g_lastClockId = -1;
KernelTimespec g_reported{};

int ClockReporting(int clockId, KernelTimespec* time) {
    ++g_queries;
    g_lastClockId = clockId;
    *time = clockId == 14 ? g_reported : KernelTimespec{99, 0};
    return 0;
}

int ClockFailing(int clockId, KernelTimespec*) {
    ++g_queries;
    g_lastClockId = clockId;
    return -1;
}

double ForCurrentThread() {
    return _ZN3WTF7CPUTime16forCurrentThreadEv().m_value;
}

void TestReportedTime() {
    CpuTimeClockForTest_nid_no_patch(ClockReporting);
    g_queries = 0;
    g_reported = KernelTimespec{3, 250000000};
    Require(ForCurrentThread() == 3.25);
    Require(g_lastClockId == 14);
    Require(g_queries == 1);

    g_reported = KernelTimespec{0, 1};
    Require(ForCurrentThread() == 1.0 / 1000 / 1000 / 1000);

    g_reported = KernelTimespec{0, 123456789};
    Require(ForCurrentThread() == 123456789.0 / 1000 / 1000 / 1000);

    g_reported = KernelTimespec{7, 0};
    Require(ForCurrentThread() == 7.0);
    Require(g_queries == 4);
}

void TestFailingClock() {
    CpuTimeClockForTest_nid_no_patch(ClockFailing);
    g_queries = 0;
    bool threw = false;
    try {
        ForCurrentThread();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(g_lastClockId == 14);
    Require(g_queries == 1);
}

bool BurnUntilAdvanced(double start, double amount) {
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    volatile unsigned work = 0;
    while (ForCurrentThread() <= start + amount) {
        for (int i = 0; i < 10000; ++i)
            work = work + static_cast<unsigned>(i);
        if (std::chrono::steady_clock::now() > limit)
            return false;
    }
    return true;
}

void TestGuestClock() {
    CpuTimeClockForTest_nid_no_patch(nullptr);
    g_queries = 0;
    const double first = ForCurrentThread();
    Require(first >= 0.0);
    Require(BurnUntilAdvanced(first, 0.0));
    Require(ForCurrentThread() > first);
    Require(g_queries == 0);
}

void TestGuestClockCountsOnlyThisThread() {
    CpuTimeClockForTest_nid_no_patch(nullptr);
    std::atomic<bool> workerBurned{false};
    const auto wallStart = std::chrono::steady_clock::now();
    const double before = ForCurrentThread();
    std::thread worker([&workerBurned] { workerBurned = BurnUntilAdvanced(ForCurrentThread(), 0.1); });
    worker.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const double after = ForCurrentThread();
    const std::chrono::duration<double> wall = std::chrono::steady_clock::now() - wallStart;
    Require(workerBurned);
    Require(wall.count() >= 0.15);
    Require(after >= before);
    Require(after - before < 0.03);
    Require(g_queries == 0);
}

} // namespace

int main() {
    TestReportedTime();
    TestFailingClock();
    TestGuestClock();
    TestGuestClockCountsOnlyThisThread();
    return 0;
}

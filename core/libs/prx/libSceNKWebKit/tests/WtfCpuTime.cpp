#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

extern "C" void CpuTimeClockForTest_nid_no_patch(int (*query)(int, KernelTimespec*));

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF::CPUTime check failed at line %u\n", location.line());
        std::abort();
    }
}

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

void TestGuestClock() {
    CpuTimeClockForTest_nid_no_patch(nullptr);
    g_queries = 0;
    const double first = ForCurrentThread();
    const double second = ForCurrentThread();
    Require(first >= 0.0);
    Require(second >= first);
    Require(g_queries == 0);
}

} // namespace

int main() {
    TestReportedTime();
    TestFailingClock();
    TestGuestClock();
    return 0;
}

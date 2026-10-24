#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
Pthread APS5_VABI scePthreadSelf(void);
int APS5_VABI scePthreadGetthreadid(void);
int APS5_VABI scePthreadGetschedparam(Pthread thread, int* policy, KernelSchedParam* param);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
struct GuestRtPrio {
    std::uint16_t type;
    std::uint16_t prio;
};
int APS5_VABI rtprio_thread_nid_postfix(int function, int lwpid, GuestRtPrio* priority);
int* APS5_VABI __error_nid_postfix();
}

static constexpr int SCE_OK = 0;
static constexpr int GUEST_SCHED_FIFO = 1;
static constexpr int GUEST_SCHED_RR = 3;
static constexpr int EXPLICIT_SCHED = 0;
static constexpr int RTP_LOOKUP = 0;
static constexpr int RTP_SET = 1;
static constexpr std::uint16_t RTP_PRIO_REALTIME = 2;
static constexpr std::uint16_t RTP_PRIO_FIFO = 10;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EFAULT = 14;

static void Require(bool value) { if (!value) std::abort(); }

struct Report {
    int policy;
    int priority;
    std::atomic<int> tid{0};
    GuestRtPrio lookup{7, 7};
    int lookupResult = -1;
    std::atomic<bool> release{false};
};

static void* APS5_VABI Entry(void* arg) {
    auto* report = static_cast<Report*>(arg);
    report->lookupResult = rtprio_thread_nid_postfix(RTP_LOOKUP, 0, &report->lookup);
    report->tid.store(scePthreadGetthreadid(), std::memory_order_release);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!report->release.load(std::memory_order_acquire)) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
    return nullptr;
}

static int LookupError(int function, int lwpid, GuestRtPrio* priority) {
    *__error_nid_postfix() = 0;
    Require(rtprio_thread_nid_postfix(function, lwpid, priority) == -1);
    return *__error_nid_postfix();
}

static void CheckThread(int policy, int priority, std::uint16_t expectedType) {
    Report report{policy, priority};
    PthreadAttr attr = nullptr;
    KernelSchedParam param{priority};
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrSetinheritsched(&attr, EXPLICIT_SCHED) == SCE_OK);
    Require(scePthreadAttrSetschedpolicy(&attr, policy) == SCE_OK);
    Require(scePthreadAttrSetschedparam(&attr, &param) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, &attr, Entry, &report, "rtprio") == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (report.tid.load(std::memory_order_acquire) == 0) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
    Require(report.lookupResult == 0 && report.lookup.type == expectedType && report.lookup.prio == priority);
    GuestRtPrio fromCreator{7, 7};
    Require(rtprio_thread_nid_postfix(RTP_LOOKUP, report.tid.load(), &fromCreator) == 0);
    Require(fromCreator.type == expectedType && fromCreator.prio == priority);
    bool threw = false;
    try { rtprio_thread_nid_postfix(RTP_SET, report.tid.load(), &fromCreator); } catch (const std::exception&) { threw = true; }
    Require(threw);
    report.release.store(true, std::memory_order_release);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(LookupError(RTP_LOOKUP, report.tid.load(), &fromCreator) == GUEST_ESRCH);
}

int main() {
    int policy = -1;
    KernelSchedParam param{};
    Require(scePthreadGetschedparam(scePthreadSelf(), &policy, &param) == SCE_OK);
    Require(policy == GUEST_SCHED_FIFO || policy == GUEST_SCHED_RR);
    GuestRtPrio self{7, 7};
    Require(rtprio_thread_nid_postfix(RTP_LOOKUP, 0, &self) == 0);
    Require(self.type == (policy == GUEST_SCHED_FIFO ? RTP_PRIO_FIFO : RTP_PRIO_REALTIME) && self.prio == param.sched_priority);
    GuestRtPrio byId{7, 7};
    Require(rtprio_thread_nid_postfix(RTP_LOOKUP, scePthreadGetthreadid(), &byId) == 0);
    Require(byId.type == self.type && byId.prio == self.prio);
    CheckThread(GUEST_SCHED_FIFO, 512, RTP_PRIO_FIFO);
    CheckThread(GUEST_SCHED_RR, 600, RTP_PRIO_REALTIME);
    CheckThread(GUEST_SCHED_FIFO, 256, RTP_PRIO_FIFO);
    Require(LookupError(2, 0, &self) == GUEST_ESRCH);
    Require(LookupError(RTP_LOOKUP, 0, nullptr) == GUEST_EFAULT);
    Require(LookupError(RTP_LOOKUP, 1, &self) == GUEST_ESRCH);
    Require(LookupError(RTP_LOOKUP, 999999, &self) == GUEST_ESRCH);
    bool threw = false;
    try { rtprio_thread_nid_postfix(RTP_SET, 0, &self); } catch (const std::exception&) { threw = true; }
    Require(threw);
}

#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>

static constexpr int SCE_OK = 0;
static constexpr int GUEST_SCHED_FIFO = 1;
static constexpr int GUEST_SCHED_RR = 3;
static constexpr int EXPLICIT_SCHED = 0;
static constexpr int RTP_LOOKUP = 0;
static constexpr int RTP_SET = 1;
static constexpr std::uint16_t RTP_PRIO_REALTIME = 2;
static constexpr std::uint16_t RTP_PRIO_NORMAL = 3;
static constexpr std::uint16_t RTP_PRIO_FIFO = 10;
static constexpr std::uint16_t RTP_PRIO_IDLE = 4;
static constexpr std::uint16_t RTP_PRIO_IDLE_FIFO = 12;
static constexpr int GUEST_ESRCH = 3;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_EFAULT = 14;

static void Require(bool value) { if (!value) std::abort(); }

struct Report {
    int policy;
    int priority;
    std::atomic<int> tid{0};
    GuestRtPrio lookup{7, 7};
    int lookupResult = -1;
    std::atomic<bool> checkUpdated{false};
    GuestRtPrio updatedLookup{7, 7};
    int updatedLookupResult = -1;
    int updatedSchedResult = -1;
    int updatedSchedPolicy = -1;
    int updatedSchedPriority = -1;
};

static void* APS5_VABI Entry(void* arg) {
    auto* report = static_cast<Report*>(arg);
    report->lookupResult = rtprio_thread_nid_postfix(RTP_LOOKUP, 0, &report->lookup);
    report->tid.store(scePthreadGetthreadid(), std::memory_order_release);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!report->checkUpdated.load(std::memory_order_acquire)) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
    report->updatedLookupResult = rtprio_thread_nid_postfix(RTP_LOOKUP, 0, &report->updatedLookup);
    KernelSchedParam updatedParam{};
    int updatedPolicy = -1;
    report->updatedSchedResult = scePthreadGetschedparam(scePthreadSelf(), &updatedPolicy, &updatedParam);
    report->updatedSchedPolicy = updatedPolicy;
    report->updatedSchedPriority = updatedParam.sched_priority;
    return nullptr;
}

static void VerifySelfSched(int expectedPolicy, int expectedPriority) {
    const std::uint16_t expectedType = expectedPolicy == GUEST_SCHED_FIFO ? RTP_PRIO_FIFO : RTP_PRIO_REALTIME;
    GuestRtPrio byZero{7, 7};
    *__error_nid_postfix() = 1234;
    Require(rtprio_thread_nid_postfix(RTP_LOOKUP, 0, &byZero) == 0);
    Require(*__error_nid_postfix() == 1234);
    Require(byZero.type == expectedType && byZero.prio == expectedPriority);
    GuestRtPrio byTid{7, 7};
    Require(rtprio_thread_nid_postfix(RTP_LOOKUP, scePthreadGetthreadid(), &byTid) == 0);
    Require(byTid.type == expectedType && byTid.prio == expectedPriority);
    int policy = -1;
    KernelSchedParam param{};
    Require(scePthreadGetschedparam(scePthreadSelf(), &policy, &param) == SCE_OK);
    Require(policy == expectedPolicy && param.sched_priority == expectedPriority);
    policy = -1;
    param.sched_priority = -1;
    Require(pthread_getschedparam_nid_postfix(scePthreadSelf(), &policy, &param) == 0);
    Require(policy == expectedPolicy && param.sched_priority == expectedPriority);
    int prio = -1;
    Require(scePthreadGetprio(scePthreadSelf(), &prio) == SCE_OK);
    Require(prio == expectedPriority);
    Require(pthread_getprio_nid_postfix(scePthreadSelf()) == expectedPriority);
    Require(sched_getscheduler_nid_postfix(0) == expectedPolicy);
    param.sched_priority = -1;
    Require(sched_getparam_nid_postfix(0, &param) == 0);
    Require(param.sched_priority == expectedPriority);
}

static int LookupError(int function, int lwpid, GuestRtPrio* priority) {
    *__error_nid_postfix() = 0;
    Require(rtprio_thread_nid_postfix(function, lwpid, priority) == -1);
    return *__error_nid_postfix();
}

static std::string SetFailure(int lwpid, GuestRtPrio request) {
    try {
        rtprio_thread_nid_postfix(RTP_SET, lwpid, &request);
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
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
    GuestRtPrio outOfRange{expectedType, 0};
    Require(LookupError(RTP_SET, report.tid.load(), &outOfRange) == GUEST_EINVAL);
    GuestRtPrio normal{RTP_PRIO_NORMAL, 31};
    Require(LookupError(RTP_SET, report.tid.load(), &normal) == GUEST_EINVAL);
    const std::uint16_t updatedType = expectedType == RTP_PRIO_FIFO ? RTP_PRIO_REALTIME : RTP_PRIO_FIFO;
    const int updatedPolicy = updatedType == RTP_PRIO_FIFO ? GUEST_SCHED_FIFO : GUEST_SCHED_RR;
    const std::uint16_t updatedPrio = priority == 700 ? 300 : 700;
    GuestRtPrio update{updatedType, updatedPrio};
    *__error_nid_postfix() = 4321;
    Require(rtprio_thread_nid_postfix(RTP_SET, report.tid.load(), &update) == 0);
    Require(*__error_nid_postfix() == 4321);
    Require(update.type == updatedType && update.prio == updatedPrio);
    Require(rtprio_thread_nid_postfix(RTP_LOOKUP, report.tid.load(), &fromCreator) == 0);
    Require(fromCreator.type == updatedType && fromCreator.prio == updatedPrio);
    int creatorPolicy = -1;
    KernelSchedParam creatorParam{};
    Require(scePthreadGetschedparam(thread, &creatorPolicy, &creatorParam) == SCE_OK);
    Require(creatorPolicy == updatedPolicy && creatorParam.sched_priority == updatedPrio);
    report.checkUpdated.store(true, std::memory_order_release);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(report.updatedLookupResult == 0 && report.updatedLookup.type == updatedType && report.updatedLookup.prio == updatedPrio);
    Require(report.updatedSchedResult == SCE_OK && report.updatedSchedPolicy == updatedPolicy && report.updatedSchedPriority == updatedPrio);
    Require(LookupError(RTP_LOOKUP, report.tid.load(), &fromCreator) == GUEST_ESRCH);
    Require(LookupError(RTP_SET, report.tid.load(), &update) == GUEST_ESRCH);
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
    Require(LookupError(5, 0, &self) == GUEST_EINVAL);
    for (const std::uint16_t prio : {0, 31}) {
        GuestRtPrio normal{RTP_PRIO_NORMAL, prio};
        Require(LookupError(RTP_SET, 0, &normal) == GUEST_EINVAL);
        Require(normal.type == RTP_PRIO_NORMAL && normal.prio == prio);
    }
    Require(LookupError(RTP_LOOKUP, 0, nullptr) == GUEST_EFAULT);
    auto* unmapped = reinterpret_cast<GuestRtPrio*>(std::uintptr_t{0x10});
    Require(LookupError(RTP_LOOKUP, 0, unmapped) == GUEST_EFAULT);
    Require(LookupError(RTP_SET, 0, unmapped) == GUEST_EFAULT);
    Require(LookupError(RTP_SET, 999999, unmapped) == GUEST_ESRCH);
    void* page = nullptr;
    Require(sceKernelMapFlexibleMemory(&page, 0x4000, 3, 0) == 0 && page != nullptr);
    auto* readOnly = static_cast<GuestRtPrio*>(page);
    *readOnly = {7, 7};
    Require(sceKernelMprotect(page, 0x4000, 1) == 0);
    Require(LookupError(RTP_LOOKUP, 0, readOnly) == GUEST_EFAULT);
    Require(readOnly->type == 7 && readOnly->prio == 7);
    Require(sceKernelMprotect(page, 0x4000, 3) == 0);
    *readOnly = {RTP_PRIO_FIFO, 256};
    Require(sceKernelMprotect(page, 0x4000, 1) == 0);
    Require(scePthreadGetschedparam(scePthreadSelf(), &policy, &param) == SCE_OK);
    const std::uint16_t originalType = policy == GUEST_SCHED_FIFO ? RTP_PRIO_FIFO : RTP_PRIO_REALTIME;
    const auto originalPrio = static_cast<std::uint16_t>(param.sched_priority);
    *__error_nid_postfix() = 1234;
    Require(rtprio_thread_nid_postfix(RTP_SET, 0, readOnly) == 0);
    Require(*__error_nid_postfix() == 1234);
    Require(readOnly->type == RTP_PRIO_FIFO && readOnly->prio == 256);
    VerifySelfSched(GUEST_SCHED_FIFO, 256);
    Require(LookupError(RTP_LOOKUP, 1, &self) == GUEST_ESRCH);
    Require(LookupError(RTP_LOOKUP, 999999, &self) == GUEST_ESRCH);
    for (const std::uint16_t type : {RTP_PRIO_REALTIME, RTP_PRIO_FIFO}) {
        const int expectedPolicy = type == RTP_PRIO_FIFO ? GUEST_SCHED_FIFO : GUEST_SCHED_RR;
        for (const std::uint16_t prio : {256, 512, 767}) {
            GuestRtPrio request{type, prio};
            *__error_nid_postfix() = 5678;
            Require(rtprio_thread_nid_postfix(RTP_SET, 0, &request) == 0);
            Require(*__error_nid_postfix() == 5678);
            VerifySelfSched(expectedPolicy, prio);
        }
        for (const std::uint16_t prio : {255, 768, 895, 896, 959, 960}) {
            GuestRtPrio request{type, prio};
            Require(LookupError(RTP_SET, 0, &request) == GUEST_EINVAL);
        }
    }
    GuestRtPrio byTidSet{RTP_PRIO_REALTIME, 612};
    Require(rtprio_thread_nid_postfix(RTP_SET, scePthreadGetthreadid(), &byTidSet) == 0);
    VerifySelfSched(GUEST_SCHED_RR, 612);
    GuestRtPrio restore{originalType, originalPrio};
    Require(rtprio_thread_nid_postfix(RTP_SET, 0, &restore) == 0);
    VerifySelfSched(policy, originalPrio);
    for (const std::uint16_t type : {RTP_PRIO_IDLE, RTP_PRIO_IDLE_FIFO}) {
        Require(SetFailure(0, {type, 960}) == "rtprio_thread: an idle-class set not implemented");
        for (const std::uint16_t prio : {959, 961}) {
            GuestRtPrio request{type, prio};
            Require(LookupError(RTP_SET, 0, &request) == GUEST_EINVAL);
        }
    }
    for (const std::uint16_t prio : {615, 700, 895}) {
        GuestRtPrio request{RTP_PRIO_NORMAL, prio};
        Require(LookupError(RTP_SET, 0, &request) == GUEST_EINVAL);
    }
    Require(SetFailure(0, {RTP_PRIO_NORMAL | 8, 700}) == "rtprio_thread: a normal-class set with the FIFO bit not implemented");
    for (const std::uint16_t type : {std::uint16_t{RTP_PRIO_NORMAL}, std::uint16_t{RTP_PRIO_NORMAL | 8}}) {
        for (const std::uint16_t prio : {614, 896}) {
            GuestRtPrio request{type, prio};
            Require(LookupError(RTP_SET, 0, &request) == GUEST_EINVAL);
        }
    }
    for (const std::uint16_t type : {0, 1, 5, 9, 13}) {
        GuestRtPrio request{type, 512};
        Require(LookupError(RTP_SET, 0, &request) == GUEST_EINVAL);
    }
    Require(LookupError(RTP_LOOKUP, 999999, nullptr) == GUEST_ESRCH);
    Require(LookupError(RTP_SET, 999999, nullptr) == GUEST_ESRCH);
    for (const std::uint16_t type : {RTP_PRIO_REALTIME, RTP_PRIO_IDLE, RTP_PRIO_FIFO, RTP_PRIO_IDLE_FIFO}) {
        for (const std::uint16_t prio : {0, 15, 31, 32, 999}) {
            GuestRtPrio request{type, prio};
            Require(LookupError(RTP_SET, 0, &request) == GUEST_EINVAL);
            Require(request.type == type && request.prio == prio);
        }
    }
    Require(LookupError(RTP_SET, 999999, &self) == GUEST_ESRCH);
}

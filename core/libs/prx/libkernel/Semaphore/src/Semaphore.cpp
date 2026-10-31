#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include <stdexcept>
#include <string>
#include <utility>

KernelSemaPrivate::KernelSemaPrivate(std::int32_t initCount, std::int32_t maxCount, std::string name, bool isFifo)
 : name(std::move(name)), tokenCount(initCount), initCount(initCount), maxCount(maxCount), isFifo(isFifo) {
}

static bool TraceSync() {
 static const bool enabled = std::getenv("APS5_TRACE_SYNC") != nullptr;
 return enabled;
}

extern "C" {

int APS5_VABI sceKernelCreateSema(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt) {
 (void)opt;
 if (sem == nullptr) {
  APS5_INVALID_ARG_EX;
 }
 if ((attr & ~0x103u) != 0 || (attr & 3u) == 3u || name == nullptr || init < 0 || max <= 0 || init > max) {
  return SCE_KERNEL_ERROR_EINVAL;
 }
 *sem = new KernelSemaPrivate(init, max, std::string(name), (attr & 3u) == 1u);
 (*sem)->priorityQueue = (attr & 3u) == 2u;
 if (TraceSync()) std::fprintf(stderr, "[sema] create %p '%s' attr=0x%x init=%d max=%d\n", static_cast<void*>(*sem), name, attr, init, max);
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelPollSema(KernelSema sem, int need) {
 if (sem == nullptr) {
  return SCE_KERNEL_ERROR_ESRCH;
 }
 if (need <= 0 || need > sem->maxCount) {
  return SCE_KERNEL_ERROR_EINVAL;
 }

 std::lock_guard<std::mutex> lock(sem->mutex);
 if (sem->tokenCount < need) {
  return SCE_KERNEL_ERROR_EBUSY;
 }
 sem->tokenCount -= need;
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelSignalSema(KernelSema sem, int count) {
 if (sem == nullptr) {
  return SCE_KERNEL_ERROR_ESRCH;
 }
 if (count <= 0) {
  return SCE_KERNEL_ERROR_EINVAL;
 }

 std::lock_guard<std::mutex> lock(sem->mutex);
 if (count > sem->maxCount - sem->tokenCount) {
  return SCE_KERNEL_ERROR_EINVAL;
 }
 const auto before = sem->tokenCount;
 sem->tokenCount += count;
 if (TraceSync()) std::fprintf(stderr, "[sema] signal %p '%s' count=%d have=%d->%d\n", static_cast<void*>(sem), sem->name.c_str(), count, before, sem->tokenCount);
 for (auto it = sem->queue.begin(); it != sem->queue.end();) {
  KernelSemaWaiter* waiter = *it;
  if (waiter->need > sem->tokenCount) {
   ++it;
   continue;
  }
  sem->tokenCount -= waiter->need;
  waiter->released = true;
  it = sem->queue.erase(it);
 }
 sem->condition.NotifyAll();
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelWaitSema(KernelSema sem, int need, KernelUseconds* time) {
 if (sem == nullptr) {
  return SCE_KERNEL_ERROR_ESRCH;
 }
 if (need <= 0) {
  APS5_INVALID_ARG_EX;
 }

 std::unique_lock<std::mutex> lock(sem->mutex);
 const void* caller = __builtin_return_address(0);
 if (TraceSync()) std::fprintf(stderr, "[sema] wait begin %p '%s' need=%d have=%d timeout=%lld caller=%p\n", static_cast<void*>(sem), sem->name.c_str(), need, sem->tokenCount, time ? static_cast<long long>(*time) : -1LL, caller);
 const auto traceSyncWaitEnd = [&](int result) {
  if (TraceSync()) std::fprintf(stderr, "[sema] wait end %p '%s' result=0x%x have=%d caller=%p\n", static_cast<void*>(sem), sem->name.c_str(), result, sem->tokenCount, caller);
 };
 if (sem->deleted) {
  traceSyncWaitEnd(SCE_KERNEL_ERROR_EACCES);
  return SCE_KERNEL_ERROR_EACCES;
 }
 if (sem->tokenCount >= need) {
  sem->tokenCount -= need;
  traceSyncWaitEnd(KERNEL_SEMA_OK);
  return KERNEL_SEMA_OK;
 }
 const std::uint64_t generation = sem->cancelGeneration;
 KernelSemaWaiter waiter{need};
 if (sem->priorityQueue && scePthreadGetprio(scePthreadSelf(), &waiter.priority) == 0) {
  const auto lower = std::find_if(sem->queue.begin(), sem->queue.end(), [&](const KernelSemaWaiter* queued) { return queued->priority > waiter.priority; });
  sem->queue.insert(lower, &waiter);
 } else {
  sem->queue.push_back(&waiter);
 }
 ++sem->waiterCount;
 struct WaiterGuard {
  KernelSemaPrivate* sem;
  KernelSemaWaiter* waiter;
  ~WaiterGuard() {
   --sem->waiterCount;
   if (!waiter->released) {
    sem->queue.remove(waiter);
   }
   sem->condition.NotifyAll();
  }
 } waiterGuard{sem, &waiter};

 const auto waitStart = std::chrono::steady_clock::now();
 const auto traceWait = [&](bool timedOut) {
  KernelTraceWait_nid_postfix("sema", caller, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart).count()), timedOut);
 };
 const auto released = [&] { return waiter.released || sem->deleted || sem->cancelGeneration != generation; };
 bool acquired = true;
 if (time == nullptr) {
  sem->condition.Wait(lock, released);
 } else {
  const std::uint64_t deadline = TimedWait::DeadlineNanos(*time);
  acquired = sem->condition.WaitUntil(lock, deadline, released);
  *time = static_cast<KernelUseconds>(TimedWait::RemainingMicros(deadline));
 }
 traceWait(!acquired);
 if (waiter.released) {
  traceSyncWaitEnd(KERNEL_SEMA_OK);
  return KERNEL_SEMA_OK;
 }
 if (sem->deleted) {
  traceSyncWaitEnd(SCE_KERNEL_ERROR_EACCES);
  return SCE_KERNEL_ERROR_EACCES;
 }
 if (sem->cancelGeneration != generation) {
  traceSyncWaitEnd(SCE_KERNEL_ERROR_ECANCELED);
  return SCE_KERNEL_ERROR_ECANCELED;
 }
 traceSyncWaitEnd(SCE_KERNEL_ERROR_ETIMEDOUT);
 return SCE_KERNEL_ERROR_ETIMEDOUT;
}

int APS5_VABI sceKernelCancelSema(KernelSema sem, int count, int* threads) {
 if (sem == nullptr) {
  return SCE_KERNEL_ERROR_ESRCH;
 }

 std::lock_guard<std::mutex> lock(sem->mutex);
 if (count > sem->maxCount) {
  return SCE_KERNEL_ERROR_EINVAL;
 }
 if (threads != nullptr) {
  *threads = static_cast<int>(sem->queue.size());
 }
 sem->tokenCount = count < 0 ? sem->initCount : count;
 sem->queue.clear();
 ++sem->cancelGeneration;
 sem->condition.NotifyAll();
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelDeleteSema(KernelSema sem) {
 if (sem == nullptr) {
  return SCE_KERNEL_ERROR_ESRCH;
 }

 std::unique_lock<std::mutex> lock(sem->mutex);
 sem->deleted = true;
 sem->condition.NotifyAll();
 sem->condition.Wait(lock, [&] { return sem->waiterCount == 0; });
 lock.unlock();
 delete sem;
 return KERNEL_SEMA_OK;
}

}

#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/General.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

KernelSemaPrivate::KernelSemaPrivate(std::int32_t initCount, std::int32_t maxCount, std::string name, bool isFifo)
 : name(std::move(name)), tokenCount(initCount), initCount(initCount), maxCount(maxCount), isFifo(isFifo) {
}

static bool TraceSync() {
 static const bool enabled = std::getenv("APS5_TRACE_SYNC") != nullptr;
 return enabled;
}

static std::mutex liveSemasMutex;
static std::unordered_map<KernelSema, std::shared_ptr<KernelSemaPrivate>> liveSemas;
static std::uintptr_t lastSemaHandle = 0;

static std::shared_ptr<KernelSemaPrivate> FindSema(KernelSema handle) {
 std::lock_guard<std::mutex> lock(liveSemasMutex);
 const auto found = liveSemas.find(handle);
 return found == liveSemas.end() ? nullptr : found->second;
}

extern "C" {

KernelSemaPrivate* KernelSemaObjectForTests_nid_no_patch(KernelSema handle) {
 return FindSema(handle).get();
}

int APS5_VABI sceKernelCreateSema(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt) {
 (void)opt;
 if (sem == nullptr) {
  APS5_INVALID_ARG_EX;
 }
 if ((attr & ~0x103u) != 0 || (attr & 3u) == 3u || name == nullptr || init < 0 || max <= 0 || init > max) {
  return SCE_KERNEL_ERROR_EINVAL;
 }
 if ((attr & 0x100u) != 0) {
  NotImplemented_nid_no_patch("sceKernelCreateSema with attribute bit 0x100");
 }

 auto created = std::make_shared<KernelSemaPrivate>(init, max, std::string(name), attr == 1);
 KernelSema handle = nullptr;
 {
  std::lock_guard<std::mutex> lock(liveSemasMutex);
  handle = reinterpret_cast<KernelSema>(++lastSemaHandle);
  liveSemas.emplace(handle, std::move(created));
 }
 *sem = handle;
 if (TraceSync()) std::fprintf(stderr, "[sema] create %p '%s' attr=0x%x init=%d max=%d\n", static_cast<void*>(*sem), name, attr, init, max);
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelPollSema(KernelSema handle, int need) {
 const auto sem = FindSema(handle);
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

int APS5_VABI sceKernelSignalSema(KernelSema handle, int count) {
 const auto sem = FindSema(handle);
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
 if (TraceSync()) std::fprintf(stderr, "[sema] signal %p '%s' count=%d have=%d->%d\n", static_cast<void*>(handle), sem->name.c_str(), count, before, sem->tokenCount);
 sem->condition.NotifyAll();
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelWaitSema(KernelSema handle, int need, KernelUseconds* time) {
 const auto sem = FindSema(handle);
 if (sem == nullptr) {
  return SCE_KERNEL_ERROR_ESRCH;
 }
 if (need <= 0) {
  APS5_INVALID_ARG_EX;
 }

 std::unique_lock<std::mutex> lock(sem->mutex);
 const std::uint64_t generation = sem->cancelGeneration;
 ++sem->waiterCount;
 ++sem->cancelableCount;
 struct WaiterGuard {
  KernelSemaPrivate* sem;
  std::uint64_t generation;
  ~WaiterGuard() {
   --sem->waiterCount;
   if (sem->cancelGeneration == generation) {
    --sem->cancelableCount;
   }
   sem->condition.NotifyAll();
  }
 } waiterGuard{sem.get(), generation};

 const auto waitStart = std::chrono::steady_clock::now();
 const void* caller = __builtin_return_address(0);
 if (TraceSync()) std::fprintf(stderr, "[sema] wait begin %p '%s' need=%d have=%d timeout=%lld caller=%p\n", static_cast<void*>(handle), sem->name.c_str(), need, sem->tokenCount, time ? static_cast<long long>(*time) : -1LL, caller);
 const auto traceSyncWaitEnd = [&](int result) {
  if (TraceSync()) std::fprintf(stderr, "[sema] wait end %p '%s' result=0x%x have=%d caller=%p\n", static_cast<void*>(handle), sem->name.c_str(), result, sem->tokenCount, caller);
 };
 const auto traceWait = [&](bool timedOut) {
  KernelTraceWait_nid_postfix("sema", caller, static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart).count()), timedOut);
 };
 const auto released = [&] { return sem->tokenCount >= need || sem->deleted || sem->cancelGeneration != generation; };
 if (time == nullptr) {
  sem->condition.Wait(lock, released);
  traceWait(false);
  if (sem->deleted) {
   traceSyncWaitEnd(SCE_KERNEL_ERROR_EACCES);
   return SCE_KERNEL_ERROR_EACCES;
  }
  if (sem->cancelGeneration != generation) {
   traceSyncWaitEnd(SCE_KERNEL_ERROR_ECANCELED);
   return SCE_KERNEL_ERROR_ECANCELED;
  }
  sem->tokenCount -= need;
  traceSyncWaitEnd(KERNEL_SEMA_OK);
  return KERNEL_SEMA_OK;
 }

 const std::uint64_t deadline = TimedWait::DeadlineNanos(*time);
 const bool acquired = sem->condition.WaitUntil(lock, deadline, released);
 *time = static_cast<KernelUseconds>(TimedWait::RemainingMicros(deadline));
 traceWait(!acquired);
 if (sem->deleted) {
  traceSyncWaitEnd(SCE_KERNEL_ERROR_EACCES);
  return SCE_KERNEL_ERROR_EACCES;
 }
 if (sem->cancelGeneration != generation) {
  traceSyncWaitEnd(SCE_KERNEL_ERROR_ECANCELED);
  return SCE_KERNEL_ERROR_ECANCELED;
 }
 if (!acquired) {
  traceSyncWaitEnd(SCE_KERNEL_ERROR_ETIMEDOUT);
  return SCE_KERNEL_ERROR_ETIMEDOUT;
 }
 sem->tokenCount -= need;
 traceSyncWaitEnd(KERNEL_SEMA_OK);
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelCancelSema(KernelSema handle, int count, int* threads) {
 const auto sem = FindSema(handle);
 if (sem == nullptr) {
  return SCE_KERNEL_ERROR_ESRCH;
 }

 std::lock_guard<std::mutex> lock(sem->mutex);
 if (count > sem->maxCount) {
  return SCE_KERNEL_ERROR_EINVAL;
 }
 if (threads != nullptr) {
  *threads = sem->cancelableCount;
 }
 sem->tokenCount = count < 0 ? sem->initCount : count;
 sem->cancelableCount = 0;
 ++sem->cancelGeneration;
 sem->condition.NotifyAll();
 return KERNEL_SEMA_OK;
}

int APS5_VABI sceKernelDeleteSema(KernelSema handle) {
 std::shared_ptr<KernelSemaPrivate> sem;
 {
  std::lock_guard<std::mutex> lock(liveSemasMutex);
  const auto found = liveSemas.find(handle);
  if (found == liveSemas.end()) {
   return SCE_KERNEL_ERROR_ESRCH;
  }
  sem = std::move(found->second);
  liveSemas.erase(found);
 }

 std::unique_lock<std::mutex> lock(sem->mutex);
 sem->deleted = true;
 sem->condition.NotifyAll();
 sem->condition.Wait(lock, [&] { return sem->waiterCount == 0; });
 return KERNEL_SEMA_OK;
}

}

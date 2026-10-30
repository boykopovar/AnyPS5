#pragma once

#include "prx/libSceUlt/SceUltTypes.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceUltInitialize();
int APS5_VABI sceUltFinalize();
int APS5_VABI sceUltMutexOptParamInitialize(UltMutexOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltSemaphoreOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltQueueOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltQueueDataResourcePoolOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltWaitingQueueResourcePoolOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltUlthreadOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltConditionVariableOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI _sceUltConditionVariableOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltMutexCreate(void* mutex, const char* name, void* waitingQueueResourcePool, const UltMutexOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltMutexDestroy(void* mutex);
int APS5_VABI sceUltMutexLock(void* mutex);
int APS5_VABI sceUltMutexTryLock(void* mutex);
int APS5_VABI sceUltMutexUnlock(void* mutex);
int APS5_VABI sceUltConditionVariableCreate(void* condition, const char* name, void* mutex, const void* optParam, std::uint32_t buildVersion);
int APS5_VABI _sceUltConditionVariableCreate(void* condition, const char* name, void* mutex, const void* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltConditionVariableDestroy(void* condition);
int APS5_VABI sceUltConditionVariableSignal(void* condition);
int APS5_VABI sceUltConditionVariableWait(void* condition);
std::uint64_t APS5_VABI sceUltWaitingQueueResourcePoolGetWorkAreaSize(std::uint32_t numThreads, std::uint32_t numSyncObjects);
int APS5_VABI sceUltWaitingQueueResourcePoolCreate(void* pool, const char* name, std::uint32_t numThreads, std::uint32_t numSyncObjects, void* workArea, const void* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltWaitingQueueResourcePoolDestroy(void* pool);
std::uint64_t APS5_VABI sceUltQueueDataResourcePoolGetWorkAreaSize(std::uint32_t numData, std::uint64_t dataSize, std::uint32_t numQueueObject);
int APS5_VABI sceUltQueueDataResourcePoolCreate(void* pool, const char* name, std::uint32_t numData, std::uint64_t dataSize, std::uint32_t numQueueObject, void* waitingQueueResourcePool, void* workArea, const void* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltQueueDataResourcePoolDestroy(void* pool);
int APS5_VABI sceUltQueueCreate(void* queue, const char* name, std::uint64_t dataSize, void* waitingQueueResourcePool, void* queueDataResourcePool, const void* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltQueuePush(void* queue, const void* data);
int APS5_VABI sceUltQueueTryPush(void* queue, const void* data);
int APS5_VABI sceUltQueuePop(void* queue, void* data);
int APS5_VABI sceUltQueueTryPop(void* queue, void* data);
int APS5_VABI sceUltQueueDestroy(void* queue);
int APS5_VABI sceUltSemaphoreCreate(void* semaphore, const char* name, std::uint32_t numInitialResource, void* waitingQueueResourcePool, const void* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltSemaphoreAcquire(void* semaphore, std::int32_t numResource);
int APS5_VABI sceUltSemaphoreTryAcquire(void* semaphore, std::int32_t numResource);
int APS5_VABI sceUltSemaphoreRelease(void* semaphore, std::int32_t numResource);
int APS5_VABI sceUltSemaphoreDestroy(void* semaphore);
int APS5_VABI sceUltUlthreadRuntimeOptParamInitialize(UltUlthreadRuntimeOptParam* optParam, std::uint32_t buildVersion);
std::uint64_t APS5_VABI sceUltUlthreadRuntimeGetWorkAreaSize(std::uint32_t maxNumUlthread, std::uint32_t numWorkerThread);
int APS5_VABI sceUltUlthreadRuntimeCreate(void* runtime, const char* name, std::uint32_t maxNumUlthread, std::uint32_t numWorkerThread, void* workArea, const void* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltUlthreadCreate(void* ulthread, const char* name, UltUlthreadEntry entry, std::uint64_t arg, void* context, std::uint64_t sizeContext, void* runtime, const void* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltUlthreadJoin(void* ulthread, std::int32_t* status);
int APS5_VABI sceUltUlthreadTryJoin(void* ulthread, std::int32_t* status);
int APS5_VABI sceUltUlthreadGetSelf(void** ulthread);
int APS5_VABI sceUltUlthreadYield();
int APS5_VABI sceUltUlthreadExit(std::int32_t status);
int APS5_VABI sceUltUlthreadRuntimeDestroy(void* runtime);
int APS5_VABI _sceUltWaitingQueueResourcePoolCreate(void* pool, const char* name, std::uint32_t numThreads, std::uint32_t numSyncObjects, void* workArea, const void* optParam);
int APS5_VABI _sceUltSemaphoreCreate(void* semaphore, const char* name, std::uint32_t numInitialResource, void* waitingQueueResourcePool, const void* optParam);
int APS5_VABI _sceUltMutexOptParamInitialize(UltMutexOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI _sceUltMutexCreate(void* mutex, const char* name, void* waitingQueueResourcePool, const UltMutexOptParam* optParam);
int APS5_VABI _sceUltQueueDataResourcePoolCreate(void* pool, const char* name, std::uint32_t numData, std::uint64_t dataSize, std::uint32_t numQueueObject, void* waitingQueueResourcePool, void* workArea, const void* optParam);
int APS5_VABI _sceUltQueueCreate(void* queue, const char* name, std::uint64_t dataSize, void* waitingQueueResourcePool, void* queueDataResourcePool, const void* optParam);
int APS5_VABI _sceUltUlthreadRuntimeOptParamInitialize(UltUlthreadRuntimeOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI _sceUltUlthreadRuntimeCreate(void* runtime, const char* name, std::uint32_t maxNumUlthread, std::uint32_t numWorkerThread, void* workArea, const void* optParam);
int APS5_VABI _sceUltUlthreadCreate(void* ulthread, const char* name, UltUlthreadEntry entry, std::uint64_t arg, void* context, std::uint64_t sizeContext, void* runtime, const void* optParam);
int APS5_VABI sceUltConditionVariableSignalAll(void* conditionVariable);
int APS5_VABI sceUltGetConditionVariableInfo(void* object, UltConditionVariableInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity);
int APS5_VABI sceUltGetMutexInfo(void* object, UltMutexInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity);
int APS5_VABI sceUltGetQueueDataResourcePoolInfo(void* object, UltQueueDataResourcePoolInfo* info);
int APS5_VABI sceUltGetQueueInfo(void* object, UltQueueInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity);
int APS5_VABI sceUltGetReaderWriterLockInfo(void* object, UltReaderWriterLockInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity);
int APS5_VABI sceUltGetSemaphoreInfo(void* object, UltSemaphoreInfo* info, UltWaitingThreadInfo* waitingThreads, std::uint32_t capacity);
int APS5_VABI sceUltGetUlthreadInfo(void* object, UltUlthreadInfo* info);
int APS5_VABI sceUltGetUlthreadRuntimeInfo(void* runtime, UltUlthreadRuntimeInfo* info, void** ulthreads, std::uint32_t ulthreadCapacity, Pthread* workerThreads, std::uint32_t workerCapacity);
int APS5_VABI sceUltGetWaitingQueueResourcePoolInfo(void* object, UltWaitingQueueResourcePoolInfo* info);
int APS5_VABI sceUltAbstractThreadFactoryCreateThread();
int APS5_VABI sceUltAbstractThreadFactoryGetNumMaxThreads();
int APS5_VABI sceUltAbstractThreadFactoryJoinThread();
int APS5_VABI sceUltKernelThreadFactoryFinalize();
int APS5_VABI sceUltKernelThreadFactoryGetFactoryPointer();
int APS5_VABI sceUltKernelThreadFactoryInitialize();
int APS5_VABI sceUltModuleFinalize();
int APS5_VABI sceUltModuleInitialize();
int APS5_VABI sceUltUlthreadFactoryFinalize();
int APS5_VABI sceUltUlthreadFactoryGetFactoryPointer();
int APS5_VABI sceUltUlthreadFactoryGetWorkAreaSize();
int APS5_VABI sceUltUlthreadFactoryInitialize();
int APS5_VABI sceUltReaderWriterLockOptParamInitialize(UltOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltReaderWriterLockCreate(void* rwlock, const char* name, void* waitingQueueResourcePool, const UltOptParam* optParam, std::uint32_t buildVersion);
int APS5_VABI sceUltReaderWriterLockDestroy(void* rwlock);
int APS5_VABI sceUltReaderWriterLockLockRead(void* rwlock);
int APS5_VABI sceUltReaderWriterLockTryLockRead(void* rwlock);
int APS5_VABI sceUltReaderWriterLockUnlockRead(void* rwlock);
int APS5_VABI sceUltReaderWriterLockLockWrite(void* rwlock);
int APS5_VABI sceUltReaderWriterLockTryLockWrite(void* rwlock);
int APS5_VABI sceUltReaderWriterLockUnlockWrite(void* rwlock);

}

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceUltFinalize(void) {
 return 0;
}

int sceUltInitialize(void) {
 return 0;
}

int sceUltMutexCreate(void* mutex, const char* name, void* waiting_queue_resource_pool, const UltMutexOptParam* opt_param, uint32_t build_version) {
 (void)mutex;
 (void)name;
 (void)waiting_queue_resource_pool;
 (void)opt_param;
 (void)build_version;
 return 0;
}

int sceUltMutexLock(void* mutex) {
 (void)mutex;
 return 0;
}

int sceUltMutexOptParamInitialize(UltMutexOptParam* opt_param, uint32_t build_version) {
 (void)opt_param;
 (void)build_version;
 return 0;
}

int sceUltMutexUnlock(void* mutex) {
 (void)mutex;
 return 0;
}

int sceUltQueueCreate(void* queue, const char* name, uint64_t data_size, void* waiting_queue_resource_pool, void* queue_data_resource_pool, const void* opt_param, uint32_t build_version) {
 (void)queue;
 (void)name;
 (void)data_size;
 (void)waiting_queue_resource_pool;
 (void)queue_data_resource_pool;
 (void)opt_param;
 (void)build_version;
 return 0;
}

int sceUltQueueDataResourcePoolCreate(void* pool, const char* name, uint32_t num_data, uint64_t data_size, uint32_t num_queue_object, void* waiting_queue_resource_pool, void* work_area, const void* opt_param, uint32_t build_version) {
 (void)pool;
 (void)name;
 (void)num_data;
 (void)data_size;
 (void)num_queue_object;
 (void)waiting_queue_resource_pool;
 (void)work_area;
 (void)opt_param;
 (void)build_version;
 return 0;
}

uint64_t sceUltQueueDataResourcePoolGetWorkAreaSize(uint32_t num_data, uint64_t data_size, uint32_t num_queue_object) {
 (void)num_data;
 (void)data_size;
 (void)num_queue_object;
 return 0;
}

int sceUltQueuePush(void* queue, const void* data) {
 (void)queue;
 (void)data;
 return 0;
}

int sceUltQueueTryPop(void* queue, void* data) {
 (void)queue;
 (void)data;
 return 0;
}

int sceUltSemaphoreAcquire(void* semaphore, int32_t num_resource) {
 (void)semaphore;
 (void)num_resource;
 return 0;
}

int sceUltSemaphoreCreate(void* semaphore, const char* name, int32_t num_initial_resource, void* waiting_queue_resource_pool, const void* opt_param, uint32_t build_version) {
 (void)semaphore;
 (void)name;
 (void)num_initial_resource;
 (void)waiting_queue_resource_pool;
 (void)opt_param;
 (void)build_version;
 return 0;
}

int sceUltSemaphoreDestroy(void* semaphore) {
 (void)semaphore;
 return 0;
}

int sceUltSemaphoreRelease(void* semaphore, int32_t num_resource) {
 (void)semaphore;
 (void)num_resource;
 return 0;
}

int sceUltSemaphoreTryAcquire(void* semaphore, int32_t num_resource) {
 (void)semaphore;
 (void)num_resource;
 return 0;
}

int sceUltUlthreadCreate(void* ulthread, const char* name, UltUlthreadEntry entry, uint64_t arg, void* context, uint64_t size_context, void* runtime, const void* opt_param, uint32_t build_version) {
 (void)ulthread;
 (void)name;
 (void)entry;
 (void)arg;
 (void)context;
 (void)size_context;
 (void)runtime;
 (void)opt_param;
 (void)build_version;
 return 0;
}

int sceUltUlthreadJoin(void* ulthread, int32_t* status) {
 (void)ulthread;
 (void)status;
 return 0;
}

int sceUltUlthreadRuntimeCreate(void* runtime, const char* name, uint32_t max_num_ulthread, uint32_t num_worker_thread, void* work_area, const void* opt_param, uint32_t build_version) {
 (void)runtime;
 (void)name;
 (void)max_num_ulthread;
 (void)num_worker_thread;
 (void)work_area;
 (void)opt_param;
 (void)build_version;
 return 0;
}

uint64_t sceUltUlthreadRuntimeGetWorkAreaSize(uint32_t max_num_ulthread, uint32_t num_worker_thread) {
 (void)max_num_ulthread;
 (void)num_worker_thread;
 return 0;
}

int sceUltUlthreadRuntimeOptParamInitialize(UltUlthreadRuntimeOptParam* opt_param, uint32_t build_version) {
 (void)opt_param;
 (void)build_version;
 return 0;
}

int sceUltWaitingQueueResourcePoolCreate(void* pool, const char* name, uint32_t num_threads, uint32_t num_sync_objects, void* work_area, const void* opt_param, uint32_t build_version) {
 (void)pool;
 (void)name;
 (void)num_threads;
 (void)num_sync_objects;
 (void)work_area;
 (void)opt_param;
 (void)build_version;
 return 0;
}

uint64_t sceUltWaitingQueueResourcePoolGetWorkAreaSize(uint32_t num_threads, uint32_t num_sync_objects) {
 (void)num_threads;
 (void)num_sync_objects;
 return 0;
}

}

#include "prx/libc/include/Api.hpp"

#include "prx/libc/include/GuestErrno.hpp"

#include "prx/libkernel/File/include/File.hpp"

#include "prx/libkernel/Pthread/include/Atfork.hpp"

#include "prx/libkernel/Pthread/include/Cancel.hpp"

#include "prx/libkernel/Pthread/include/Cond.hpp"

#include "prx/libkernel/Pthread/include/Mutex.hpp"

#include "prx/libkernel/Pthread/include/Rwlock.hpp"

#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"

#include "prx/libkernel/Semaphore/include/Semaphore.hpp"

#include "prx/libkernel/Time/include/Time.hpp"

#include "prx/libkernel/include/Api.hpp"

#include "prx/libkernel/include/LibKernelSysProviders.hpp"



extern "C" {

int APS5_VABI __Ux86_64_setcontext_nid_no_patch(void) {
    return __Ux86_64_setcontext();
}













































































































































































































































































































int APS5_VABI sceKernelAddFileEvent_nid_no_patch(KernelEqueue eq, int fd, int watch, void* udata) {
    return sceKernelAddFileEvent(eq, fd, watch, udata);
}

int APS5_VABI sceKernelAddHRTimerEvent_nid_no_patch(KernelEqueue eq, int id, const KernelTimespec* ts, void* udata) {
    return sceKernelAddHRTimerEvent(eq, id, ts, udata);
}

int APS5_VABI sceKernelAddReadEvent_nid_no_patch(KernelEqueue eq, int fd, std::size_t size, void* udata) {
    return sceKernelAddReadEvent(eq, fd, size, udata);
}

int APS5_VABI sceKernelAddTimerEvent_nid_no_patch(KernelEqueue eq, int id, KernelUseconds usec, void* udata) {
    return sceKernelAddTimerEvent(eq, id, usec, udata);
}

int APS5_VABI sceKernelAddUserEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelAddUserEvent(eq, id);
}

int APS5_VABI sceKernelAddUserEventEdge_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelAddUserEventEdge(eq, id);
}

int APS5_VABI sceKernelAddWriteEvent_nid_no_patch(KernelEqueue eq, int fd, std::size_t size, void* udata) {
    return sceKernelAddWriteEvent(eq, fd, size, udata);
}

int APS5_VABI sceKernelAddAmprEvent_nid_no_patch(KernelEqueue eq, int id, void* udata) {
    return sceKernelAddAmprEvent(eq, id, udata);
}

int APS5_VABI sceKernelDeleteAmprEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelDeleteAmprEvent(eq, id);
}

int APS5_VABI sceKernelAddAmprSystemEvent_nid_no_patch(KernelEqueue eq, int id, void* udata) {
    return sceKernelAddAmprSystemEvent(eq, id, udata);
}

int APS5_VABI sceKernelDeleteAmprSystemEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelDeleteAmprSystemEvent(eq, id);
}

int APS5_VABI sceKernelAioCancelRequest_nid_no_patch(int32_t id, int32_t* state) {
    return sceKernelAioCancelRequest(id, state);
}

int APS5_VABI sceKernelAioCancelRequests_nid_no_patch(int32_t* id, int32_t num, int32_t* state) {
    return sceKernelAioCancelRequests(id, num, state);
}

int APS5_VABI sceKernelAioDeleteRequest_nid_no_patch(int32_t id, int32_t* ret) {
    return sceKernelAioDeleteRequest(id, ret);
}

int APS5_VABI sceKernelAioDeleteRequests_nid_no_patch(int32_t* id, int32_t num, int32_t* ret) {
    return sceKernelAioDeleteRequests(id, num, ret);
}

int APS5_VABI sceKernelAioInitializeImpl_nid_no_patch(void* param, int32_t size) {
    return sceKernelAioInitializeImpl(param, size);
}

void APS5_VABI sceKernelAioInitializeParam_nid_no_patch(void* param) {
    sceKernelAioInitializeParam(param);
}

int APS5_VABI sceKernelAioPollRequest_nid_no_patch(int32_t id, int32_t* state) {
    return sceKernelAioPollRequest(id, state);
}

int APS5_VABI sceKernelAioPollRequests_nid_no_patch(int32_t* id, int32_t num, int32_t* state) {
    return sceKernelAioPollRequests(id, num, state);
}

int APS5_VABI sceKernelAioSetParam_nid_no_patch(KernelAioSchedulingParam* param, int32_t schedulingWindowSize, int32_t delayedCountLimit, uint32_t enableSplit, uint32_t splitSize, uint32_t splitChunkSize) {
    return sceKernelAioSetParam(param, schedulingWindowSize, delayedCountLimit, enableSplit, splitSize, splitChunkSize);
}

int APS5_VABI sceKernelAioSubmitReadCommands_nid_no_patch(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitReadCommands(req, size, prio, id);
}

int APS5_VABI sceKernelAioSubmitReadCommandsMultiple_nid_no_patch(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitReadCommandsMultiple(req, size, prio, id);
}

int APS5_VABI sceKernelAioSubmitWriteCommands_nid_no_patch(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitWriteCommands(req, size, prio, id);
}

int APS5_VABI sceKernelAioSubmitWriteCommandsMultiple_nid_no_patch(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    return sceKernelAioSubmitWriteCommandsMultiple(req, size, prio, id);
}

int APS5_VABI sceKernelAioWaitRequest_nid_no_patch(int32_t id, int32_t* state, uint32_t* usec) {
    return sceKernelAioWaitRequest(id, state, usec);
}

int APS5_VABI sceKernelAioWaitRequests_nid_no_patch(int32_t* id, int32_t num, int32_t* state, uint32_t mode, uint32_t* usec) {
    return sceKernelAioWaitRequests(id, num, state, mode, usec);
}

int APS5_VABI sceKernelAllocateDirectMemory_nid_no_patch(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physAddrOut) {
    return sceKernelAllocateDirectMemory(searchStart, searchEnd, len, alignment, memoryType, physAddrOut);
}

int APS5_VABI sceKernelAllocateDirectMemoryForApp_nid_no_patch(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physAddrOut) {
    return sceKernelAllocateDirectMemoryForApp(searchStart, searchEnd, len, alignment, memoryType, physAddrOut);
}

int APS5_VABI sceKernelAllocateMainDirectMemory_nid_no_patch(size_t len, size_t alignment, int memoryType, int64_t* physAddrOut) {
    return sceKernelAllocateMainDirectMemory(len, alignment, memoryType, physAddrOut);
}

int APS5_VABI sceKernelAvailableDirectMemorySize_nid_no_patch(int64_t searchStart, int64_t searchEnd, size_t alignment, int64_t* physAddrOut, size_t* sizeOut) {
    return sceKernelAvailableDirectMemorySize(searchStart, searchEnd, alignment, physAddrOut, sizeOut);
}

int APS5_VABI sceKernelAvailableFlexibleMemorySize_nid_no_patch(size_t* size) {
    return sceKernelAvailableFlexibleMemorySize(size);
}

int APS5_VABI sceKernelBatchMap_nid_no_patch(KernelBatchMapEntry* entries, int numEntries, int* numEntriesOut) {
    return sceKernelBatchMap(entries, numEntries, numEntriesOut);
}

int APS5_VABI sceKernelBatchMap2_nid_no_patch(KernelBatchMapEntry* entries, int numEntries, int* numEntriesOut, int flags) {
    return sceKernelBatchMap2(entries, numEntries, numEntriesOut, flags);
}

int APS5_VABI sceKernelCancelEventFlag_nid_no_patch(KernelEventFlag ef, std::uint64_t setPattern, int* numWaitThreads) {
    return sceKernelCancelEventFlag(ef, setPattern, numWaitThreads);
}

int APS5_VABI sceKernelCancelSema_nid_no_patch(KernelSema sem, int count, int* threads) {
    return sceKernelCancelSema(sem, count, threads);
}

int APS5_VABI sceKernelCheckReachability_nid_no_patch(const char* path) {
    return sceKernelCheckReachability(path);
}

int APS5_VABI sceKernelCheckedReleaseDirectMemory_nid_no_patch(int64_t start, size_t len) {
    return sceKernelCheckedReleaseDirectMemory(start, len);
}


int APS5_VABI sceKernelClearEventFlag_nid_no_patch(KernelEventFlag ef, uint64_t bitPattern) {
    return sceKernelClearEventFlag(ef, bitPattern);
}

int APS5_VABI sceKernelClockGetres_nid_no_patch(KernelClockid clockId, KernelTimespec* tp) {
    return sceKernelClockGetres(clockId, tp);
}

int APS5_VABI sceKernelClockGettime_nid_no_patch(KernelClockid clockId, KernelTimespec* tp) {
    return sceKernelClockGettime(clockId, tp);
}

int APS5_VABI sceKernelClose_nid_no_patch(int d) {
    return sceKernelClose(d);
}

int APS5_VABI sceKernelCloseEventFlag_nid_no_patch(KernelEventFlag ef) {
    return sceKernelCloseEventFlag(ef);
}

int APS5_VABI sceKernelConfiguredFlexibleMemorySize_nid_no_patch(size_t* size) {
    return sceKernelConfiguredFlexibleMemorySize(size);
}

int APS5_VABI sceKernelConvertLocaltimeToUtc_nid_no_patch(int64_t localTime, int64_t reserved, int64_t* utcTime, KernelTimesec* timezone, int32_t* dstSeconds) {
    return sceKernelConvertLocaltimeToUtc(localTime, reserved, utcTime, timezone, dstSeconds);
}

int APS5_VABI sceKernelConvertUtcToLocaltime_nid_no_patch(int64_t utcTime, int64_t* localTime, KernelTimesec* st, int32_t* dstSec) {
    return sceKernelConvertUtcToLocaltime(utcTime, localTime, st, dstSec);
}

int APS5_VABI sceKernelCreateEqueue_nid_no_patch(KernelEqueue* eq, const char* name) {
    return sceKernelCreateEqueue(eq, name);
}

int APS5_VABI sceKernelCreateEventFlag_nid_no_patch(KernelEventFlag* ef, const char* name, std::uint32_t attr, std::uint64_t initPattern, const void* param) {
    return sceKernelCreateEventFlag(ef, name, attr, initPattern, param);
}

int APS5_VABI sceKernelCreateSema_nid_no_patch(KernelSema* sem, const char* name, uint32_t attr, int init, int max, void* opt) {
    return sceKernelCreateSema(sem, name, attr, init, max, opt);
}

int APS5_VABI sceKernelDebugOutText_nid_no_patch(int channel, const char* text) {
    return sceKernelDebugOutText(channel, text);
}

void APS5_VABI sceKernelDebugRaiseException_nid_no_patch(int c1, int c2) {
    sceKernelDebugRaiseException(c1, c2);
}

void APS5_VABI sceKernelDebugRaiseExceptionOnReleaseMode_nid_no_patch(int c1, int c2) {
    sceKernelDebugRaiseExceptionOnReleaseMode(c1, c2);
}

int APS5_VABI sceKernelDebugWriteCppExceptionInfo_nid_no_patch(const void* exception, uint64_t unknown, const char* typeName, const char* what) {
    return sceKernelDebugWriteCppExceptionInfo(exception, unknown, typeName, what);
}

int APS5_VABI sceKernelDeleteEqueue_nid_no_patch(KernelEqueue eq) {
    return sceKernelDeleteEqueue(eq);
}

int APS5_VABI sceKernelDeleteEventFlag_nid_no_patch(KernelEventFlag ef) {
    return sceKernelDeleteEventFlag(ef);
}

int APS5_VABI sceKernelDeleteFileEvent_nid_no_patch(KernelEqueue eq, int fd) {
    return sceKernelDeleteFileEvent(eq, fd);
}

int APS5_VABI sceKernelDeleteHRTimerEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelDeleteHRTimerEvent(eq, id);
}

int APS5_VABI sceKernelDeleteReadEvent_nid_no_patch(KernelEqueue eq, int fd) {
    return sceKernelDeleteReadEvent(eq, fd);
}

int APS5_VABI sceKernelDeleteSema_nid_no_patch(KernelSema sem) {
    return sceKernelDeleteSema(sem);
}

int APS5_VABI sceKernelDeleteTimerEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelDeleteTimerEvent(eq, id);
}

int APS5_VABI sceKernelDeleteUserEvent_nid_no_patch(KernelEqueue eq, int id) {
    return sceKernelDeleteUserEvent(eq, id);
}

int APS5_VABI sceKernelDeleteWriteEvent_nid_no_patch(KernelEqueue eq, int fd) {
    return sceKernelDeleteWriteEvent(eq, fd);
}

int APS5_VABI sceKernelDirectMemoryQuery_nid_no_patch(int64_t offset, int flags, void* info, size_t infoSize) {
    return sceKernelDirectMemoryQuery(offset, flags, info, infoSize);
}

int APS5_VABI sceKernelDlsym_nid_no_patch(KernelModule handle, const char* symbol, void** addr) {
    return sceKernelDlsym(handle, symbol, addr);
}

int APS5_VABI sceKernelEnableDmemAliasing_nid_no_patch(void) {
    return sceKernelEnableDmemAliasing();
}

std::int32_t APS5_VABI sceKernelError_nid_no_patch(std::int32_t posixError) {
    return sceKernelError(posixError);
}

int APS5_VABI sceKernelFchmod_nid_no_patch(int d, std::uint16_t mode) {
    return sceKernelFchmod(d, mode);
}

int APS5_VABI sceKernelFcntl_nid_no_patch(int d, int command, std::uint64_t v0, std::uint64_t v1) {
    return sceKernelFcntl(d, command, v0, v1);
}

int APS5_VABI sceKernelFstat_nid_no_patch(int d, FileStat* sb) {
    return sceKernelFstat(d, sb);
}

int APS5_VABI sceKernelFsync_nid_no_patch(int fd) {
    return sceKernelFsync(fd);
}

int APS5_VABI sceKernelFdatasync_nid_no_patch(int fd) {
    return sceKernelFdatasync(fd);
}

int APS5_VABI sceKernelFtruncate_nid_no_patch(int d, int64_t length) {
    return sceKernelFtruncate(d, length);
}

int APS5_VABI sceKernelFutimes_nid_no_patch(int d, const KernelTimeval* times) {
    return sceKernelFutimes(d, times);
}


int APS5_VABI sceKernelGetAppInfo_nid_no_patch(int pid, void* info) {
    return sceKernelGetAppInfo(pid, info);
}

std::int64_t APS5_VABI sceKernelGetCpuFrequency_nid_no_patch(void) {
    return sceKernelGetCpuFrequency();
}

int APS5_VABI sceKernelGetCpuTemperature_nid_no_patch(int* outCelsius) {
    return sceKernelGetCpuTemperature(outCelsius);
}

int APS5_VABI sceKernelGetCpumode_nid_no_patch(void) {
    return sceKernelGetCpumode();
}

int APS5_VABI sceKernelGetCurrentCpu_nid_no_patch(void) {
    return sceKernelGetCurrentCpu();
}

size_t APS5_VABI sceKernelGetDirectMemorySize_nid_no_patch(void) {
    return sceKernelGetDirectMemorySize();
}

int APS5_VABI sceKernelGetDirectMemoryType_nid_no_patch(int64_t offset, int* memoryType, int64_t* start, int64_t* end) {
    return sceKernelGetDirectMemoryType(offset, memoryType, start, end);
}

intptr_t APS5_VABI sceKernelGetEventData_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventData(ev);
}

int APS5_VABI sceKernelGetEventError_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventError(ev);
}

intptr_t APS5_VABI sceKernelGetEventFflags_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventFflags(ev);
}

int APS5_VABI sceKernelGetEventFilter_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventFilter(ev);
}

uintptr_t APS5_VABI sceKernelGetEventId_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventId(ev);
}

void* APS5_VABI sceKernelGetEventUserData_nid_no_patch(const KernelEvent* ev) {
    return sceKernelGetEventUserData(ev);
}

KernelModule APS5_VABI sceKernelGetExecutableModuleHandle_nid_no_patch(void) {
    return sceKernelGetExecutableModuleHandle();
}

const char* APS5_VABI sceKernelGetFsSandboxRandomWord_nid_no_patch(void) {
    return sceKernelGetFsSandboxRandomWord();
}

std::uint64_t APS5_VABI sceKernelGetGPI_nid_no_patch(void) {
    return sceKernelGetGPI();
}


int APS5_VABI sceKernelGetMainSocId_nid_no_patch(void) {
    return sceKernelGetMainSocId();
}

int APS5_VABI sceKernelGetModuleInfo_nid_no_patch(KernelModule handle, ModuleInfo* info) {
    return sceKernelGetModuleInfo(handle, info);
}

int APS5_VABI sceKernelGetModuleInfoForUnwind_nid_no_patch(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
    return sceKernelGetModuleInfoForUnwind(addr, flags, info);
}

int APS5_VABI sceKernelGetModuleInfoFromAddr_nid_no_patch(std::uint64_t address, int flags, ModuleInfoEx* info) {
    return sceKernelGetModuleInfoFromAddr(address, flags, info);
}

int APS5_VABI sceKernelGetModuleList_nid_no_patch(KernelModule* handles, std::size_t count, std::size_t* actual) {
    return sceKernelGetModuleList(handles, count, actual);
}

int APS5_VABI sceKernelGetPageTableStats_nid_no_patch(int* cpuTotal, int* cpuAvailable, int* gpuTotal, int* gpuAvailable) {
    return sceKernelGetPageTableStats(cpuTotal, cpuAvailable, gpuTotal, gpuAvailable);
}

void* APS5_VABI sceKernelGetProcParam_nid_no_patch(void) {
    return sceKernelGetProcParam();
}

int APS5_VABI sceKernelGetProcessAbiType_nid_no_patch(int pid) {
    return sceKernelGetProcessAbiType(pid);
}

std::uint64_t APS5_VABI sceKernelGetProcessTime_nid_no_patch(void) {
    return sceKernelGetProcessTime();
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounter_nid_no_patch(void) {
    return sceKernelGetProcessTimeCounter();
}

std::uint64_t APS5_VABI sceKernelGetProcessTimeCounterFrequency_nid_no_patch(void) {
    return sceKernelGetProcessTimeCounterFrequency();
}


int APS5_VABI sceKernelGetPrtAperture_nid_no_patch(int index, void** addr, size_t* len) {
    return sceKernelGetPrtAperture(index, addr, len);
}

int APS5_VABI sceKernelGetResidentFmemCount_nid_no_patch(int pid, uint64_t* resident, uint64_t* mapped) {
    return sceKernelGetResidentFmemCount(pid, resident, mapped);
}

MallocReplace* APS5_VABI sceKernelGetSanitizerMallocReplaceExternal_nid_no_patch(void) {
    return sceKernelGetSanitizerMallocReplaceExternal();
}

NewReplace* APS5_VABI sceKernelGetSanitizerNewReplaceExternal_nid_no_patch(void) {
    return sceKernelGetSanitizerNewReplaceExternal();
}

int APS5_VABI sceKernelGetSocPowerConsumption_nid_no_patch(void* consumption) {
    return sceKernelGetSocPowerConsumption(consumption);
}

int APS5_VABI sceKernelGetSocSensorTemperature_nid_no_patch(int sensorId, int* outCelsius) {
    return sceKernelGetSocSensorTemperature(sensorId, outCelsius);
}


int APS5_VABI sceKernelGetTraceMemoryStats_nid_no_patch(std::uint64_t* first, std::uint64_t* second) {
    return sceKernelGetTraceMemoryStats(first, second);
}

std::uint64_t APS5_VABI sceKernelGetTscFrequency_nid_no_patch(void) {
    return sceKernelGetTscFrequency();
}

int APS5_VABI sceKernelGetdents_nid_no_patch(int fd, char* buf, int nbytes) {
    return sceKernelGetdents(fd, buf, nbytes);
}

int APS5_VABI sceKernelGetdirentries_nid_no_patch(int fd, char* buf, int nbytes, int64_t* basep) {
    return sceKernelGetdirentries(fd, buf, nbytes, basep);
}

int APS5_VABI sceKernelGettimeofday_nid_no_patch(KernelTimeval* tp) {
    return sceKernelGettimeofday(tp);
}

int APS5_VABI sceKernelGettimezone_nid_no_patch(KernelTimezone* tz) {
    return sceKernelGettimezone(tz);
}

int APS5_VABI sceKernelInstallExceptionHandler_nid_no_patch(int signum, void* handler) {
    return sceKernelInstallExceptionHandler(signum, handler);
}

int APS5_VABI sceKernelPrintBacktraceWithModuleInfo_nid_no_patch(void) {
    return sceKernelPrintBacktraceWithModuleInfo();
}

int APS5_VABI sceKernelInternalMemoryGetAvailableSize_nid_no_patch(std::size_t* size) {
    return sceKernelInternalMemoryGetAvailableSize(size);
}


int APS5_VABI sceKernelIsAddressSanitizerEnabled_nid_no_patch(void) {
    return sceKernelIsAddressSanitizerEnabled();
}

int APS5_VABI sceKernelIsThreadSanitizerEnabled_nid_no_patch(void) {
    return sceKernelIsThreadSanitizerEnabled();
}

int APS5_VABI sceKernelIsStack_nid_no_patch(void* addr, void** start, void** end) {
    return sceKernelIsStack(addr, start, end);
}

KernelModule APS5_VABI sceKernelLoadStartModule_nid_no_patch(const char* moduleFileName, size_t args, const void* argp, uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
    return sceKernelLoadStartModule(moduleFileName, args, argp, flags, opt, res);
}

std::int64_t APS5_VABI sceKernelLseek_nid_no_patch(int d, std::int64_t offset, int whence) {
    return sceKernelLseek(d, offset, whence);
}

int APS5_VABI sceKernelMapDirectMemory_nid_no_patch(void** addr, size_t len, int prot, int flags, int64_t directMemoryStart, size_t alignment) {
    return sceKernelMapDirectMemory(addr, len, prot, flags, directMemoryStart, alignment);
}

int APS5_VABI sceKernelMapDirectMemory2_nid_no_patch(void** addr, size_t len, int type, int prot, int flags, int64_t directMemoryStart, size_t alignment) {
    return sceKernelMapDirectMemory2(addr, len, type, prot, flags, directMemoryStart, alignment);
}

int APS5_VABI sceKernelMapFlexibleMemory_nid_no_patch(void** addrInOut, size_t len, int prot, int flags) {
    return sceKernelMapFlexibleMemory(addrInOut, len, prot, flags);
}

int APS5_VABI sceKernelMapNamedDirectMemory_nid_no_patch(void** addr, size_t len, int prot, int flags, int64_t directMemoryStart, size_t alignment, const char* name) {
    return sceKernelMapNamedDirectMemory(addr, len, prot, flags, directMemoryStart, alignment, name);
}

int32_t APS5_VABI sceKernelMapNamedFlexibleMemory_nid_no_patch(void** addrInOut, size_t len, int prot, int flags, const char* name) {
    return sceKernelMapNamedFlexibleMemory(addrInOut, len, prot, flags, name);
}

int32_t APS5_VABI sceKernelMapNamedSystemFlexibleMemory_nid_no_patch(void** addrInOut, size_t len, int prot, int flags, const char* name) {
    return sceKernelMapNamedSystemFlexibleMemory(addrInOut, len, prot, flags, name);
}

int APS5_VABI sceKernelMemoryPoolBatch_nid_no_patch(const KernelMemoryPoolBatchEntry* entries, int numEntries, int* numEntriesOut, int flags) {
    return sceKernelMemoryPoolBatch(entries, numEntries, numEntriesOut, flags);
}

int APS5_VABI sceKernelMemoryPoolCommit_nid_no_patch(void* addr, size_t len, int type, int prot, int flags) {
    return sceKernelMemoryPoolCommit(addr, len, type, prot, flags);
}

int APS5_VABI sceKernelMemoryPoolDecommit_nid_no_patch(void* addr, size_t len, int flags) {
    return sceKernelMemoryPoolDecommit(addr, len, flags);
}

int APS5_VABI sceKernelMemoryPoolExpand_nid_no_patch(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int64_t* physAddrOut) {
    return sceKernelMemoryPoolExpand(searchStart, searchEnd, len, alignment, physAddrOut);
}

int APS5_VABI sceKernelMemoryPoolGetBlockStats_nid_no_patch(KernelMemoryPoolBlockStats* output, size_t outputSize) {
    return sceKernelMemoryPoolGetBlockStats(output, outputSize);
}

int APS5_VABI sceKernelMemoryPoolReserve_nid_no_patch(void* addrIn, size_t len, size_t alignment, int flags, void** addrOut) {
    return sceKernelMemoryPoolReserve(addrIn, len, alignment, flags, addrOut);
}

int APS5_VABI sceKernelMkdir_nid_no_patch(const char* path, uint16_t mode) {
    return sceKernelMkdir(path, mode);
}


int APS5_VABI sceKernelMmap_nid_no_patch(void* addr, size_t len, int prot, int flags, int fd, int64_t offset, void** res) {
    return sceKernelMmap(addr, len, prot, flags, fd, offset, res);
}

int APS5_VABI sceKernelMprotect_nid_no_patch(const void* addr, size_t len, int prot) {
    return sceKernelMprotect(addr, len, prot);
}

int APS5_VABI sceKernelMtypeprotect_nid_no_patch(const void* addr, size_t len, int type, int prot) {
    return sceKernelMtypeprotect(addr, len, type, prot);
}



int APS5_VABI sceKernelMunmap_nid_no_patch(uint64_t vaddr, size_t len) {
    return sceKernelMunmap(vaddr, len);
}

int APS5_VABI sceKernelNanosleep_nid_no_patch(const KernelTimespec* rqtp, KernelTimespec* rmtp) {
    return sceKernelNanosleep(rqtp, rmtp);
}

int APS5_VABI sceKernelOpen_nid_no_patch(const char* path, int flags, std::uint16_t mode) {
    return sceKernelOpen(path, flags, mode);
}

int APS5_VABI sceKernelOpenEventFlag_nid_no_patch(KernelEventFlag* ef, const char* name) {
    return sceKernelOpenEventFlag(ef, name);
}

int APS5_VABI sceKernelPollEventFlag_nid_no_patch(KernelEventFlag ef, uint64_t bitPattern, uint32_t waitMode, uint64_t* resultPat) {
    return sceKernelPollEventFlag(ef, bitPattern, waitMode, resultPat);
}

int APS5_VABI sceKernelPollSema_nid_no_patch(KernelSema sem, int need) {
    return sceKernelPollSema(sem, need);
}

int64_t APS5_VABI sceKernelPread_nid_no_patch(int d, void* buf, size_t nbytes, int64_t offset) {
    return sceKernelPread(d, buf, nbytes, offset);
}

int64_t APS5_VABI sceKernelPreadv_nid_no_patch(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return sceKernelPreadv(d, iov, iovcnt, offset);
}

int APS5_VABI sceKernelProtectDirectMemoryForPID_nid_no_patch(int64_t offset, size_t len, int prot, int pid) {
    return sceKernelProtectDirectMemoryForPID(offset, len, prot, pid);
}

int64_t APS5_VABI sceKernelPwrite_nid_no_patch(int d, const void* buf, size_t nbytes, int64_t offset) {
    return sceKernelPwrite(d, buf, nbytes, offset);
}

int64_t APS5_VABI sceKernelPwritev_nid_no_patch(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return sceKernelPwritev(d, iov, iovcnt, offset);
}

int APS5_VABI sceKernelQueryMemoryProtection_nid_no_patch(void* addr, void** start, void** end, int* prot) {
    return sceKernelQueryMemoryProtection(addr, start, end, prot);
}

int APS5_VABI sceKernelRaiseException_nid_no_patch(Pthread thread, int signum) {
    return sceKernelRaiseException(thread, signum);
}

int APS5_VABI sceKernelRdup_nid_no_patch(int pid, int fd) {
    return sceKernelRdup(pid, fd);
}

std::int64_t APS5_VABI sceKernelRead_nid_no_patch(int d, void* buf, std::size_t nbytes) {
    return sceKernelRead(d, buf, nbytes);
}

uint64_t APS5_VABI sceKernelReadTsc_nid_no_patch(void) {
    return sceKernelReadTsc();
}

int64_t APS5_VABI sceKernelReadv_nid_no_patch(int d, const KernelIovec* iov, int iovcnt) {
    return sceKernelReadv(d, iov, iovcnt);
}

int APS5_VABI sceKernelReleaseDirectMemory_nid_no_patch(int64_t start, size_t len) {
    return sceKernelReleaseDirectMemory(start, len);
}

int APS5_VABI sceKernelReleaseFlexibleMemory_nid_no_patch(void* addr, size_t len) {
    return sceKernelReleaseFlexibleMemory(addr, len);
}

int APS5_VABI sceKernelRemoveExceptionHandler_nid_no_patch(int signum) {
    return sceKernelRemoveExceptionHandler(signum);
}

int APS5_VABI sceKernelRename_nid_no_patch(const char* from, const char* to) {
    return sceKernelRename(from, to);
}

int APS5_VABI sceKernelReserveVirtualRange_nid_no_patch(void** addr, size_t len, int flags, size_t alignment) {
    return sceKernelReserveVirtualRange(addr, len, flags, alignment);
}

int APS5_VABI sceKernelRmdir_nid_no_patch(const char* path) {
    return sceKernelRmdir(path);
}

int APS5_VABI sceKernelSendNotificationRequest_nid_no_patch(int device, const void* request, std::size_t size, int flags) {
    return sceKernelSendNotificationRequest(device, request, size, flags);
}

int APS5_VABI sceKernelSetEventFlag_nid_no_patch(KernelEventFlag ef, std::uint64_t bitPattern) {
    return sceKernelSetEventFlag(ef, bitPattern);
}

void APS5_VABI sceKernelSetGPO_nid_no_patch(std::uint32_t bits) {
    sceKernelSetGPO(bits);
}

int APS5_VABI sceKernelSetPrtAperture_nid_no_patch(int index, void* addr, size_t len) {
    return sceKernelSetPrtAperture(index, addr, len);
}

int APS5_VABI sceKernelSetVirtualRangeName_nid_no_patch(const void* addr, uint64_t len, const char* name) {
    return sceKernelSetVirtualRangeName(addr, len, name);
}

int APS5_VABI sceKernelSignalSema_nid_no_patch(KernelSema sem, int count) {
    return sceKernelSignalSema(sem, count);
}

unsigned int APS5_VABI sceKernelSleep_nid_no_patch(unsigned int seconds) {
    return sceKernelSleep(seconds);
}

int APS5_VABI sceKernelStat_nid_no_patch(const char* path, FileStat* sb) {
    return sceKernelStat(path, sb);
}

int APS5_VABI sceKernelStopUnloadModule_nid_no_patch(KernelModule handle, size_t args, const void* argp, uint32_t flags, const KernelUnloadModuleOpt* opt, int* res) {
    return sceKernelStopUnloadModule(handle, args, argp, flags, opt, res);
}

void APS5_VABI sceKernelSync_nid_no_patch(void) {
    sceKernelSync();
}

int APS5_VABI sceKernelTriggerUserEvent_nid_no_patch(KernelEqueue eq, int id, void* udata) {
    return sceKernelTriggerUserEvent(eq, id, udata);
}


int APS5_VABI sceKernelUnlink_nid_no_patch(const char* path) {
    return sceKernelUnlink(path);
}



int APS5_VABI sceKernelUuidCreate_nid_no_patch(std::uint32_t* uuid) {
    return sceKernelUuidCreate(uuid);
}

int APS5_VABI sceKernelVirtualQuery_nid_no_patch(const void* addr, int flags, VirtualQueryInfo* info, uint64_t infoSize) {
    return sceKernelVirtualQuery(addr, flags, info, infoSize);
}

int APS5_VABI sceKernelWaitEqueue_nid_no_patch(KernelEqueue eq, KernelEvent* ev, int num, int* out, const KernelUseconds* timo) {
    return sceKernelWaitEqueue(eq, ev, num, out, timo);
}

int APS5_VABI sceKernelWaitEventFlag_nid_no_patch(KernelEventFlag ef, uint64_t bitPattern, uint32_t waitMode, uint64_t* resultPat, KernelUseconds* timeout) {
    return sceKernelWaitEventFlag(ef, bitPattern, waitMode, resultPat, timeout);
}

int APS5_VABI sceKernelWaitSema_nid_no_patch(KernelSema sem, int need, KernelUseconds* time) {
    return sceKernelWaitSema(sem, need, time);
}

std::int64_t APS5_VABI sceKernelWrite_nid_no_patch(int d, const void* buf, std::size_t nbytes) {
    return sceKernelWrite(d, buf, nbytes);
}

int64_t APS5_VABI sceKernelWritev_nid_no_patch(int d, const KernelIovec* iov, int iovcnt) {
    return sceKernelWritev(d, iov, iovcnt);
}

int APS5_VABI sceKernelWriteModifyMtypeProtectCommand_nid_no_patch(void* dst, std::uint64_t address, std::uint64_t size, std::uint64_t type, std::uint64_t protection, std::uint64_t mask, std::uint64_t* outSize) {
    return sceKernelWriteModifyMtypeProtectCommand(dst, address, size, type, protection, mask, outSize);
}

int APS5_VABI sceKernelWriteRemapIntoPrtCommand_nid_no_patch(std::uint32_t* dst, std::uint64_t address, std::uint64_t source, std::uint64_t size, int protection, int opcode, std::uint64_t* outSize) {
    return sceKernelWriteRemapIntoPrtCommand(dst, address, source, size, protection, opcode, outSize);
}

int APS5_VABI scePthreadAttrDestroy_nid_no_patch(PthreadAttr* attr) {
    return scePthreadAttrDestroy(attr);
}

int APS5_VABI scePthreadAttrGet_nid_no_patch(Pthread thread, PthreadAttr* attr) {
    return scePthreadAttrGet(thread, attr);
}

int APS5_VABI scePthreadAttrGetaffinity_nid_no_patch(const PthreadAttr* attr, KernelCpumask* mask) {
    return scePthreadAttrGetaffinity(attr, mask);
}

int APS5_VABI scePthreadAttrGetdetachstate_nid_no_patch(const PthreadAttr* attr, int* state) {
    return scePthreadAttrGetdetachstate(attr, state);
}

int APS5_VABI scePthreadAttrGetguardsize_nid_no_patch(const PthreadAttr* attr, size_t* guardSize) {
    return scePthreadAttrGetguardsize(attr, guardSize);
}

int APS5_VABI scePthreadAttrGetinheritsched_nid_no_patch(const PthreadAttr* attr, int* inheritSched) {
    return scePthreadAttrGetinheritsched(attr, inheritSched);
}

int APS5_VABI scePthreadAttrGetschedparam_nid_no_patch(const PthreadAttr* attr, KernelSchedParam* param) {
    return scePthreadAttrGetschedparam(attr, param);
}

int APS5_VABI scePthreadAttrGetschedpolicy_nid_no_patch(const PthreadAttr* attr, int* policy) {
    return scePthreadAttrGetschedpolicy(attr, policy);
}

int APS5_VABI scePthreadAttrGetsolosched_nid_no_patch(const PthreadAttr* attr, int* solosched) {
    return scePthreadAttrGetsolosched(attr, solosched);
}

int APS5_VABI scePthreadAttrGetstack_nid_no_patch(const PthreadAttr* attr, void** stackaddr, std::size_t* stacksize) {
    return scePthreadAttrGetstack(attr, stackaddr, stacksize);
}

int APS5_VABI scePthreadAttrGetstackaddr_nid_no_patch(const PthreadAttr* attr, void** stackAddr) {
    return scePthreadAttrGetstackaddr(attr, stackAddr);
}

int APS5_VABI scePthreadAttrGetstacksize_nid_no_patch(const PthreadAttr* attr, size_t* stackSize) {
    return scePthreadAttrGetstacksize(attr, stackSize);
}

int APS5_VABI scePthreadAttrInit_nid_no_patch(PthreadAttr* attr) {
    return scePthreadAttrInit(attr);
}

int APS5_VABI scePthreadAttrSetaffinity_nid_no_patch(PthreadAttr* attr, KernelCpumask mask) {
    return scePthreadAttrSetaffinity(attr, mask);
}

int APS5_VABI scePthreadAttrSetdetachstate_nid_no_patch(PthreadAttr* attr, int detachstate) {
    return scePthreadAttrSetdetachstate(attr, detachstate);
}

int APS5_VABI scePthreadAttrSetguardsize_nid_no_patch(PthreadAttr* attr, size_t guardSize) {
    return scePthreadAttrSetguardsize(attr, guardSize);
}

int APS5_VABI scePthreadAttrSetinheritsched_nid_no_patch(PthreadAttr* attr, int inheritSched) {
    return scePthreadAttrSetinheritsched(attr, inheritSched);
}

int APS5_VABI scePthreadAttrSetschedparam_nid_no_patch(PthreadAttr* attr, const KernelSchedParam* param) {
    return scePthreadAttrSetschedparam(attr, param);
}

int APS5_VABI scePthreadAttrSetschedpolicy_nid_no_patch(PthreadAttr* attr, int policy) {
    return scePthreadAttrSetschedpolicy(attr, policy);
}

int APS5_VABI scePthreadAttrSetstack_nid_no_patch(PthreadAttr* attr, void* addr, size_t size) {
    return scePthreadAttrSetstack(attr, addr, size);
}

int APS5_VABI scePthreadAttrSetstackaddr_nid_no_patch(PthreadAttr* attr, void* addr) {
    return scePthreadAttrSetstackaddr(attr, addr);
}

int APS5_VABI scePthreadAttrSetstacksize_nid_no_patch(PthreadAttr* attr, std::size_t stacksize) {
    return scePthreadAttrSetstacksize(attr, stacksize);
}

int APS5_VABI scePthreadAttrSetsolosched_nid_no_patch(PthreadAttr* attr, int solosched) {
    return scePthreadAttrSetsolosched(attr, solosched);
}

int APS5_VABI scePthreadCancel_nid_no_patch(Pthread thread) {
    return scePthreadCancel(thread);
}

int APS5_VABI scePthreadCondBroadcast_nid_no_patch(PthreadCond* cond) {
    return scePthreadCondBroadcast(cond);
}

int APS5_VABI scePthreadCondDestroy_nid_no_patch(PthreadCond* cond) {
    return scePthreadCondDestroy(cond);
}

int APS5_VABI scePthreadCondInit_nid_no_patch(PthreadCond* cond, const PthreadCondattr* attr, const char* name) {
    return scePthreadCondInit(cond, attr, name);
}

int APS5_VABI scePthreadCondSignal_nid_no_patch(PthreadCond* cond) {
    return scePthreadCondSignal(cond);
}

int APS5_VABI scePthreadCondSignalto_nid_no_patch(PthreadCond* cond, Pthread thread) {
    return scePthreadCondSignalto(cond, thread);
}

int APS5_VABI scePthreadCondTimedwait_nid_no_patch(PthreadCond* cond, PthreadMutex* mutex, KernelUseconds usec) {
    return scePthreadCondTimedwait(cond, mutex, usec);
}

int APS5_VABI scePthreadCondWait_nid_no_patch(PthreadCond* cond, PthreadMutex* mutex) {
    return scePthreadCondWait(cond, mutex);
}

int APS5_VABI scePthreadCondattrDestroy_nid_no_patch(PthreadCondattr* attr) {
    return scePthreadCondattrDestroy(attr);
}

int APS5_VABI scePthreadCondattrGetpshared_nid_no_patch(const PthreadCondattr* attr, int* pshared) {
    return scePthreadCondattrGetpshared(attr, pshared);
}

int APS5_VABI scePthreadCondattrInit_nid_no_patch(PthreadCondattr* attr) {
    return scePthreadCondattrInit(attr);
}

int APS5_VABI scePthreadCondattrSetclock_nid_no_patch(PthreadCondattr* attr, KernelClockid clockId) {
    return scePthreadCondattrSetclock(attr, clockId);
}

int APS5_VABI scePthreadCondattrSetpshared_nid_no_patch(PthreadCondattr* attr, int pshared) {
    return scePthreadCondattrSetpshared(attr, pshared);
}

int APS5_VABI scePthreadCreate_nid_no_patch(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    return scePthreadCreate(thread, attr, entry, arg, name);
}

int APS5_VABI scePthreadDetach_nid_no_patch(Pthread thread) {
    return scePthreadDetach(thread);
}

int APS5_VABI scePthreadEqual_nid_no_patch(Pthread thread1, Pthread thread2) {
    return scePthreadEqual(thread1, thread2);
}

void APS5_VABI scePthreadExit_nid_no_patch(void* retval) {
    scePthreadExit(retval);
}

int APS5_VABI scePthreadGetaffinity_nid_no_patch(Pthread thread, KernelCpumask* mask) {
    return scePthreadGetaffinity(thread, mask);
}

int APS5_VABI scePthreadGetname_nid_no_patch(Pthread thread, char* name) {
    return scePthreadGetname(thread, name);
}

int APS5_VABI scePthreadGetprio_nid_no_patch(Pthread thread, int* prio) {
    return scePthreadGetprio(thread, prio);
}

int APS5_VABI scePthreadGetschedparam_nid_no_patch(Pthread thread, int* policy, KernelSchedParam* param) {
    return scePthreadGetschedparam(thread, policy, param);
}

void* APS5_VABI scePthreadGetspecific_nid_no_patch(PthreadKey key) {
    return scePthreadGetspecific(key);
}

int APS5_VABI scePthreadGetthreadid_nid_no_patch(void) {
    return scePthreadGetthreadid();
}

int APS5_VABI scePthreadJoin_nid_no_patch(Pthread thread, void** retval) {
    return scePthreadJoin(thread, retval);
}

int APS5_VABI scePthreadKeyCreate_nid_no_patch(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    return scePthreadKeyCreate(key, destructor);
}

int APS5_VABI scePthreadKeyDelete_nid_no_patch(PthreadKey key) {
    return scePthreadKeyDelete(key);
}

int APS5_VABI scePthreadMutexDestroy_nid_no_patch(PthreadMutex* mutex) {
    return scePthreadMutexDestroy(mutex);
}

int APS5_VABI scePthreadMutexGetprioceiling_nid_no_patch(PthreadMutex* mutex, int* prioceiling) {
    return scePthreadMutexGetprioceiling(mutex, prioceiling);
}

int APS5_VABI scePthreadMutexGetspinloops_nid_no_patch(PthreadMutex* mutex, int* count) {
    return scePthreadMutexGetspinloops(mutex, count);
}

int APS5_VABI scePthreadMutexGetyieldloops_nid_no_patch(PthreadMutex* mutex, int* count) {
    return scePthreadMutexGetyieldloops(mutex, count);
}

int APS5_VABI scePthreadMutexInit_nid_no_patch(PthreadMutex* mutex, const PthreadMutexattr* attr, const char* name) {
    return scePthreadMutexInit(mutex, attr, name);
}

int APS5_VABI scePthreadMutexLock_nid_no_patch(PthreadMutex* mutex) {
    return scePthreadMutexLock(mutex);
}

int APS5_VABI scePthreadMutexSetprioceiling_nid_no_patch(PthreadMutex* mutex, int ceiling, int* oldCeiling) {
    return scePthreadMutexSetprioceiling(mutex, ceiling, oldCeiling);
}

int APS5_VABI scePthreadMutexSetspinloops_nid_no_patch(PthreadMutex* mutex, int count) {
    return scePthreadMutexSetspinloops(mutex, count);
}

int APS5_VABI scePthreadMutexSetyieldloops_nid_no_patch(PthreadMutex* mutex, int count) {
    return scePthreadMutexSetyieldloops(mutex, count);
}

int APS5_VABI scePthreadMutexTimedlock_nid_no_patch(PthreadMutex* mutex, KernelUseconds usec) {
    return scePthreadMutexTimedlock(mutex, usec);
}

int APS5_VABI scePthreadMutexTrylock_nid_no_patch(PthreadMutex* mutex) {
    return scePthreadMutexTrylock(mutex);
}

int APS5_VABI scePthreadMutexUnlock_nid_no_patch(PthreadMutex* mutex) {
    return scePthreadMutexUnlock(mutex);
}

int APS5_VABI scePthreadMutexattrDestroy_nid_no_patch(PthreadMutexattr* attr) {
    return scePthreadMutexattrDestroy(attr);
}

int APS5_VABI scePthreadMutexattrGetkind_nid_no_patch(PthreadMutexattr attr) {
    return scePthreadMutexattrGetkind(attr);
}

int APS5_VABI scePthreadMutexattrGetprioceiling_nid_no_patch(const PthreadMutexattr* attr, int* prioceiling) {
    return scePthreadMutexattrGetprioceiling(attr, prioceiling);
}

int APS5_VABI scePthreadMutexattrGetprotocol_nid_no_patch(const PthreadMutexattr* attr, int* protocol) {
    return scePthreadMutexattrGetprotocol(attr, protocol);
}

int APS5_VABI scePthreadMutexattrGetpshared_nid_no_patch(const PthreadMutexattr* attr, int* pshared) {
    return scePthreadMutexattrGetpshared(attr, pshared);
}

int APS5_VABI scePthreadMutexattrGettype_nid_no_patch(const PthreadMutexattr* attr, int* type) {
    return scePthreadMutexattrGettype(attr, type);
}

int APS5_VABI scePthreadMutexattrInit_nid_no_patch(PthreadMutexattr* attr) {
    return scePthreadMutexattrInit(attr);
}

int APS5_VABI scePthreadMutexattrSetkind_nid_no_patch(PthreadMutexattr* attr, int kind) {
    return scePthreadMutexattrSetkind(attr, kind);
}

int APS5_VABI scePthreadMutexattrSetprioceiling_nid_no_patch(PthreadMutexattr* attr, int prioceiling) {
    return scePthreadMutexattrSetprioceiling(attr, prioceiling);
}

int APS5_VABI scePthreadMutexattrSetprotocol_nid_no_patch(PthreadMutexattr* attr, int protocol) {
    return scePthreadMutexattrSetprotocol(attr, protocol);
}

int APS5_VABI scePthreadMutexattrSetpshared_nid_no_patch(PthreadMutexattr* attr, int pshared) {
    return scePthreadMutexattrSetpshared(attr, pshared);
}

int APS5_VABI scePthreadMutexattrSettype_nid_no_patch(PthreadMutexattr* attr, int type) {
    return scePthreadMutexattrSettype(attr, type);
}

int APS5_VABI scePthreadOnce_nid_no_patch(int32_t* once, void (APS5_VABI* init)(void)) {
    return scePthreadOnce(once, init);
}

int APS5_VABI scePthreadRename_nid_no_patch(Pthread thread, const char* name) {
    return scePthreadRename(thread, name);
}

int APS5_VABI scePthreadRwlockDestroy_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockDestroy(rwlock);
}

int APS5_VABI scePthreadRwlockInit_nid_no_patch(PthreadRwlock* rwlock, const PthreadRwlockattr* attr, const char* name) {
    return scePthreadRwlockInit(rwlock, attr, name);
}

int APS5_VABI scePthreadRwlockRdlock_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockRdlock(rwlock);
}

int APS5_VABI scePthreadRwlockTimedrdlock_nid_no_patch(PthreadRwlock* rwlock, KernelUseconds usec) {
    return scePthreadRwlockTimedrdlock(rwlock, usec);
}

int APS5_VABI scePthreadRwlockTimedwrlock_nid_no_patch(PthreadRwlock* rwlock, KernelUseconds usec) {
    return scePthreadRwlockTimedwrlock(rwlock, usec);
}

int APS5_VABI scePthreadRwlockTryrdlock_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockTryrdlock(rwlock);
}

int APS5_VABI scePthreadRwlockTrywrlock_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockTrywrlock(rwlock);
}

int APS5_VABI scePthreadRwlockUnlock_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockUnlock(rwlock);
}

int APS5_VABI scePthreadRwlockWrlock_nid_no_patch(PthreadRwlock* rwlock) {
    return scePthreadRwlockWrlock(rwlock);
}

int APS5_VABI scePthreadRwlockattrDestroy_nid_no_patch(PthreadRwlockattr* attr) {
    return scePthreadRwlockattrDestroy(attr);
}

int APS5_VABI scePthreadRwlockattrGetpshared_nid_no_patch(const PthreadRwlockattr* attr, int* pshared) {
    return scePthreadRwlockattrGetpshared(attr, pshared);
}

int APS5_VABI scePthreadRwlockattrInit_nid_no_patch(PthreadRwlockattr* attr) {
    return scePthreadRwlockattrInit(attr);
}

int APS5_VABI scePthreadRwlockattrSetpshared_nid_no_patch(PthreadRwlockattr* attr, int pshared) {
    return scePthreadRwlockattrSetpshared(attr, pshared);
}

int APS5_VABI scePthreadRwlockattrGettype_nid_no_patch(const PthreadRwlockattr* attr, int* type) {
    return scePthreadRwlockattrGettype(attr, type);
}

int APS5_VABI scePthreadRwlockattrSettype_nid_no_patch(PthreadRwlockattr* attr, int type) {
    return scePthreadRwlockattrSettype(attr, type);
}

Pthread APS5_VABI scePthreadSelf_nid_no_patch(void) {
    return scePthreadSelf();
}

int APS5_VABI scePthreadSemInit_nid_no_patch(PthreadSem* sem, int flag, unsigned int value, const char* name) {
    return scePthreadSemInit(sem, flag, value, name);
}

int APS5_VABI scePthreadSemDestroy_nid_no_patch(PthreadSem* sem) {
    return scePthreadSemDestroy(sem);
}

int APS5_VABI scePthreadSemTimedwait_nid_no_patch(PthreadSem* sem, KernelUseconds usec) {
    return scePthreadSemTimedwait(sem, usec);
}

int APS5_VABI scePthreadSemGetvalue_nid_no_patch(PthreadSem* sem, int* value) {
    return scePthreadSemGetvalue(sem, value);
}

int APS5_VABI scePthreadSemPost_nid_no_patch(PthreadSem* sem) {
    return scePthreadSemPost(sem);
}

int APS5_VABI scePthreadSemTrywait_nid_no_patch(PthreadSem* sem) {
    return scePthreadSemTrywait(sem);
}

int APS5_VABI scePthreadSemWait_nid_no_patch(PthreadSem* sem) {
    return scePthreadSemWait(sem);
}

int APS5_VABI scePthreadSetaffinity_nid_no_patch(Pthread thread, KernelCpumask mask) {
    return scePthreadSetaffinity(thread, mask);
}

int APS5_VABI scePthreadSetcancelstate_nid_no_patch(int state, int* oldState) {
    return scePthreadSetcancelstate(state, oldState);
}

int APS5_VABI scePthreadSetcanceltype_nid_no_patch(int type, int* oldType) {
    return scePthreadSetcanceltype(type, oldType);
}

int APS5_VABI scePthreadSetprio_nid_no_patch(Pthread thread, int prio) {
    return scePthreadSetprio(thread, prio);
}

int APS5_VABI scePthreadSetschedparam_nid_no_patch(Pthread thread, int policy, const KernelSchedParam* param) {
    return scePthreadSetschedparam(thread, policy, param);
}

int APS5_VABI scePthreadSetspecific_nid_no_patch(PthreadKey key, void* value) {
    return scePthreadSetspecific(key, value);
}

void APS5_VABI scePthreadTestcancel_nid_no_patch(void) {
    scePthreadTestcancel();
}

void APS5_VABI scePthreadYield_nid_no_patch(void) {
    scePthreadYield();
}


































































int APS5_VABI sceKernelGetSystemExVersion_nid_no_patch(char* first, char* second) {
    return sceKernelGetSystemExVersion(first, second);
}

int APS5_VABI sysarch_nid_no_patch(int operation, void* parameters) {
    return sysarch(operation, parameters);
}























int APS5_VABI sceKernelAprResolveFilepathsToIds_nid_no_patch(const char** paths, uint32_t count, uint32_t* ids, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsToIds(paths, count, ids, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsAndFileSizes_nid_no_patch(const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsToIdsAndFileSizes(paths, count, ids, sizes, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIds_nid_no_patch(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsWithPrefixToIds(prefix, paths, count, ids, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes_nid_no_patch(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, uint32_t* errorIndex) {
    return sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizes(prefix, paths, count, ids, sizes, errorIndex);
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsForEach_nid_no_patch(const char** paths, uint32_t count, uint32_t* ids, int* results) {
    return sceKernelAprResolveFilepathsToIdsForEach(paths, count, ids, results);
}

int APS5_VABI sceKernelAprResolveFilepathsToIdsAndFileSizesForEach_nid_no_patch(const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, int* results) {
    return sceKernelAprResolveFilepathsToIdsAndFileSizesForEach(paths, count, ids, sizes, results);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsForEach_nid_no_patch(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, int* results) {
    return sceKernelAprResolveFilepathsWithPrefixToIdsForEach(prefix, paths, count, ids, results);
}

int APS5_VABI sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizesForEach_nid_no_patch(const char* prefix, const char** paths, uint32_t count, uint32_t* ids, uint64_t* sizes, int* results) {
    return sceKernelAprResolveFilepathsWithPrefixToIdsAndFileSizesForEach(prefix, paths, count, ids, sizes, results);
}

int APS5_VABI sceKernelAprGetFileSize_nid_no_patch(uint32_t id, uint64_t* size) {
    return sceKernelAprGetFileSize(id, size);
}

int APS5_VABI sceKernelAprGetFileStat_nid_no_patch(uint32_t id, FileStat* stat) {
    return sceKernelAprGetFileStat(id, stat);
}

int APS5_VABI sceKernelAprSubmitCommandBuffer_nid_no_patch(const Apr::CommandBufferObject* buffer, uint32_t priority) {
    return sceKernelAprSubmitCommandBuffer(buffer, priority);
}

int APS5_VABI sceKernelAprSubmitCommandBufferAndGetResult_nid_no_patch(const Apr::CommandBufferObject* buffer, uint32_t priority, uint32_t* result, uint32_t* id) {
    return sceKernelAprSubmitCommandBufferAndGetResult(buffer, priority, result, id);
}

int APS5_VABI sceKernelAprSubmitCommandBufferAndGetId_nid_no_patch(const Apr::CommandBufferObject* buffer, uint32_t priority, uint32_t* id) {
    return sceKernelAprSubmitCommandBufferAndGetId(buffer, priority, id);
}

int APS5_VABI sceKernelAprWaitCommandBuffer_nid_no_patch(uint32_t id) {
    return sceKernelAprWaitCommandBuffer(id);
}





int APS5_VABI sceKernelJitCreateAliasOfSharedMemory_nid_no_patch(int fd, int maxProt, int* fdOut) {
    return sceKernelJitCreateAliasOfSharedMemory(fd, maxProt, fdOut);
}

int APS5_VABI sceKernelJitCreateSharedMemory_nid_no_patch(const char* name, std::size_t len, int maxProt, int* fdOut) {
    return sceKernelJitCreateSharedMemory(name, len, maxProt, fdOut);
}

int APS5_VABI sceKernelJitMapSharedMemory_nid_no_patch(int fd, int prot, void** startOut) {
    return sceKernelJitMapSharedMemory(fd, prot, startOut);
}


int APS5_VABI sceKernelGetOpenPsId_nid_no_patch(void* openPsIdOutput) {
    return sceKernelGetOpenPsId(openPsIdOutput);
}

int APS5_VABI sceCoredumpRegisterCoredumpHandler_nid_no_patch(uint64_t handler, size_t stackSize, uint64_t context) {
    return sceCoredumpRegisterCoredumpHandler(handler, stackSize, context);
}

int APS5_VABI sceCoredumpUnregisterCoredumpHandler_nid_no_patch(void) {
    return sceCoredumpUnregisterCoredumpHandler();
}

int64_t APS5_VABI sceCoredumpWriteUserData_nid_no_patch(const void* data, size_t size) {
    return sceCoredumpWriteUserData(data, size);
}

int APS5_VABI sceCoredumpAttachUserFile_nid_no_patch(std::uint32_t userValue, const char* path) {
    return sceCoredumpAttachUserFile(userValue, path);
}

int APS5_VABI sceCoredumpAttachMemoryRegionAsUserFile_nid_no_patch(std::uint32_t userValue, const void* mem, std::int32_t size, const char* name) {
    return sceCoredumpAttachMemoryRegionAsUserFile(userValue, mem, size, name);
}

int APS5_VABI sceCoredumpAttachMemoryRegion_nid_no_patch(std::uint32_t userValue, const void* mem, std::int32_t size) {
    return sceCoredumpAttachMemoryRegion(userValue, mem, size);
}

int APS5_VABI sceCoredumpAttachUserMemoryFile_nid_no_patch(std::uint32_t userValue, const void* mem, std::int32_t size) {
    return sceCoredumpAttachUserMemoryFile(userValue, mem, size);
}

void APS5_VABI sceCoredumpDebugTextOut_nid_no_patch(const char* str, int len) {
    sceCoredumpDebugTextOut(str, len);
}

int APS5_VABI sceCoredumpGetStopInfoCpu_nid_no_patch(void* info, size_t size) {
    return sceCoredumpGetStopInfoCpu(info, size);
}

int APS5_VABI sceCoredumpGetStopInfoGpu_Agc_nid_no_patch(void* info, size_t size) {
    return sceCoredumpGetStopInfoGpu_Agc(info, size);
}

int APS5_VABI sceCoredumpGetThreadContextInfo_nid_no_patch(void* threadContextInfo, size_t threadContextInfoSize) {
    return sceCoredumpGetThreadContextInfo(threadContextInfo, threadContextInfoSize);
}

int APS5_VABI sceCoredumpConfigDumpMode_nid_no_patch(int mode) {
    return sceCoredumpConfigDumpMode(mode);
}

int APS5_VABI sceCoredumpSetUserDataType_nid_no_patch(std::int32_t type) {
    return sceCoredumpSetUserDataType(type);
}

int APS5_VABI sceKernelGetModuleInfo2_nid_no_patch(KernelModule handle, ModuleInfo* info) {
    return sceKernelGetModuleInfo2(handle, info);
}

int APS5_VABI sceKernelGetModuleList2_nid_no_patch(KernelModule* handles, std::size_t count, std::size_t* actual) {
    return sceKernelGetModuleList2(handles, count, actual);
}




}

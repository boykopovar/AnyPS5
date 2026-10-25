#pragma once

#include "SceTypes.hpp"

struct NpAllocator {
    void* (APS5_VABI *allocate)(std::size_t size, void* context);
    void* (APS5_VABI *reallocate)(void* pointer, std::size_t size, void* context);
    void (APS5_VABI *release)(void* pointer, void* context);
    void* context;
};

struct NpAllocatorEx {
    NpAllocator allocator;
    int outOfMemoryError;
};

struct NpCallout {
    NpCallout* next;
    void (APS5_VABI *handler)(void* arg);
    std::uint64_t deadline;
    void* arg;
    std::uint64_t reserved;
};

struct NpCalloutContext {
    Pthread thread;
    PthreadMutex mutex;
    PthreadCond cond;
    int initialized;
    int stopping;
    NpCallout* pending;
};

struct NpHeap {
    void* mspace;
};

struct NpHeapStat {
    std::uint64_t maxSystemSize;
    std::uint64_t maxInuseSize;
    std::uint64_t currentInuseSize;
};

struct NpIdFull {
    NpOnlineId handle;
    std::uint8_t opt[8];
    std::uint8_t reserved[8];
};

struct NpKernelMemory {
    std::uint64_t size;
    std::uint64_t reserved;
    void* address;
};

struct NpMemoryHeap {
    NpKernelMemory memory;
    NpHeap heap;
};

struct NpTitleIdParts {
    char id[9];
    char reserved;
    std::uint8_t index;
};

extern "C" {

int APS5_VABI sceNpCmpOnlineId(const NpOnlineId* onlineId1, const NpOnlineId* onlineId2);
int APS5_VABI sceNpCmpNpIdInOrder(const NpIdFull* npId1, const NpIdFull* npId2, std::int32_t* order);
int APS5_VABI sceNpGetPlatformType(const NpIdFull* npId);
int APS5_VABI sceNpIntIsValidOnlineId(const NpOnlineId* onlineId);
int APS5_VABI sceNpMutexInit(PthreadMutex* mutex, const char* name, std::uint64_t flags);
int APS5_VABI sceNpMutexDestroy(PthreadMutex* mutex);
int APS5_VABI sceNpMutexLock(PthreadMutex* mutex);
int APS5_VABI sceNpMutexTryLock(PthreadMutex* mutex);
int APS5_VABI sceNpMutexUnlock(PthreadMutex* mutex);
int APS5_VABI sceNpLwMutexInit(PthreadMutex* mutex, const char* name, std::uint64_t flags);
int APS5_VABI sceNpLwMutexDestroy(PthreadMutex* mutex);
int APS5_VABI sceNpLwMutexLock(PthreadMutex* mutex);
int APS5_VABI sceNpLwMutexTryLock(PthreadMutex* mutex);
int APS5_VABI sceNpLwMutexUnlock(PthreadMutex* mutex);
int APS5_VABI sceNpCondInit(PthreadCond* cond, const char* name, std::uint64_t);
int APS5_VABI sceNpCondDestroy(PthreadCond* cond);
int APS5_VABI sceNpCondSignal(PthreadCond* cond);
int APS5_VABI sceNpCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, std::uint32_t usec);
int APS5_VABI sceNpJoinThread(Pthread thread, void** retval);
int APS5_VABI sceNpGetSystemClockUsec(std::int64_t* usec);
void APS5_VABI sceNpGetSdkVersion(char* version);
int APS5_VABI sceNpCloseEventFlag(void);
int APS5_VABI sceNpCloseSema(void);
int APS5_VABI sceNpGetNavSdkVersion(void);
int APS5_VABI sceNpIntGetPlatformType(void);
int APS5_VABI sceNpIntSetPlatformType(void);
int APS5_VABI sceNpJsonParse(void);
int APS5_VABI sceNpJsonParseBuf(void);
int APS5_VABI sceNpJsonParseBufInit(void);
int APS5_VABI sceNpJsonParseEx(void);
int APS5_VABI sceNpJsonParseExInit(void);
int APS5_VABI sceNpJsonParseInit(void);
int APS5_VABI sceNpOpenSema(void);
int APS5_VABI sceNpServerErrorJsonGetErrorCode(void);
int APS5_VABI sceNpServerErrorJsonMultiGetErrorCode(void);
int APS5_VABI sceNpServerErrorJsonParse(void);
int APS5_VABI sceNpServerErrorJsonParseInit(void);
int APS5_VABI sceNpServerErrorJsonParseMultiInit(void);
int APS5_VABI sceNpSetPlatformType(void);
int APS5_VABI sceNpUtilCanonicalizeNpIdForPs4(void);
int APS5_VABI sceNpUtilCanonicalizeNpIdForPsp2(void);
int APS5_VABI sceNpUtilGetDateSetAuto(void);
int APS5_VABI sceNpUtilGetDbgCommerce(void);
int APS5_VABI sceNpUtilGetEnv(void);
int APS5_VABI sceNpUtilGetFakeDisplayNameMode(void);
int APS5_VABI sceNpUtilGetFakeRateLimit(void);
int APS5_VABI sceNpUtilGetIgnoreNpTitleId(void);
int APS5_VABI sceNpUtilGetNpDebug(void);
int APS5_VABI sceNpUtilGetNpTestPatch(void);
int APS5_VABI sceNpUtilGetShareTitleCheck(void);
int APS5_VABI sceNpUtilGetSystemLanguage(void);
int APS5_VABI sceNpUtilGetTrcNotify(void);
int APS5_VABI sceNpUtilGetWebApi2FakeRateLimit(void);
int APS5_VABI sceNpUtilGetWebApi2FakeRateLimitTarget(void);
int APS5_VABI sceNpUtilGetWebTraceSetting(void);
int APS5_VABI sceNpUtilJidToNpId(void);
int APS5_VABI sceNpAtomicCas32(std::int32_t* value, std::int32_t expected, std::int32_t desired);
int APS5_VABI sceNpAtomicInc32(std::int32_t* value);
int APS5_VABI sceNpAtomicDec32(std::int32_t* value);
int APS5_VABI sceNpCondSignalAll(PthreadCond* cond);
int APS5_VABI sceNpCondSignalTo(PthreadCond* cond, Pthread thread);
int APS5_VABI sceNpCondWait(PthreadCond* cond, PthreadMutex* mutex);
int APS5_VABI sceNpLwCondInit(PthreadCond* cond, const char* name);
int APS5_VABI sceNpLwCondDestroy(PthreadCond* cond);
int APS5_VABI sceNpLwCondSignal(PthreadCond* cond);
int APS5_VABI sceNpLwCondSignalAll(PthreadCond* cond);
int APS5_VABI sceNpLwCondSignalTo(PthreadCond* cond, Pthread thread);
int APS5_VABI sceNpLwCondWait(PthreadCond* cond, PthreadMutex* mutex);
int APS5_VABI sceNpCreateEventFlag(KernelEventFlag* flag, const char* name, std::uint32_t attr, std::uint64_t pattern);
int APS5_VABI sceNpDeleteEventFlag(KernelEventFlag flag);
int APS5_VABI sceNpOpenEventFlag(KernelEventFlag* flag, const char* name);
int APS5_VABI sceNpSetEventFlag(KernelEventFlag flag, std::uint64_t pattern);
int APS5_VABI sceNpClearEventFlag(KernelEventFlag flag, std::uint64_t pattern);
int APS5_VABI sceNpPollEventFlag(KernelEventFlag flag, std::uint64_t pattern, std::uint32_t mode, std::uint64_t* result);
int APS5_VABI sceNpCancelEventFlag(KernelEventFlag flag, std::uint64_t pattern, int* waiting);
int APS5_VABI sceNpCreateSema(KernelSema* sema, std::uint32_t flags, int initial, int maximum, const char* name);
int APS5_VABI sceNpDeleteSema(KernelSema sema);
int APS5_VABI sceNpPollSema(KernelSema sema);
int APS5_VABI sceNpSignalSema(KernelSema sema);
int APS5_VABI sceNpCreateThread(Pthread* thread, PthreadEntry entry, void* arg, int priority, std::size_t stackSize, KernelCpumask affinity, const char* name);
int APS5_VABI sceNpThreadGetId(Pthread* thread);
int APS5_VABI sceNpGetProcessId(void);
int APS5_VABI sceNpDbgDumpText(const char* text, int length);
int APS5_VABI sceNpIpc2ClientInit(void);
int APS5_VABI sceNpIpc2ClientTerm(void);
int APS5_VABI sceNpArchInit(void);
void APS5_VABI sceNpArchTerm(void);
int APS5_VABI sceNpHeapInit(NpHeap* heap, void* base, std::size_t size, const char* name);
void APS5_VABI sceNpHeapDestroy(NpHeap* heap);
void APS5_VABI sceNpHeapGetAllocator(NpHeap* heap, NpAllocator* allocator);
int APS5_VABI sceNpHeapGetStat(const NpHeap* heap, NpHeapStat* stat);
void APS5_VABI sceNpHeapShowStat(const NpHeap* heap);
int APS5_VABI sceNpMemoryHeapInit(NpMemoryHeap* heap, std::uint64_t size, const char* name);
void APS5_VABI sceNpMemoryHeapDestroy(NpMemoryHeap* heap);
void APS5_VABI sceNpMemoryHeapGetAllocator(NpMemoryHeap* heap, NpAllocator* allocator);
void APS5_VABI sceNpMemoryHeapGetAllocatorEx(NpMemoryHeap* heap, int outOfMemoryError, NpAllocatorEx* allocator);
void APS5_VABI sceNpGlobalHeapGetAllocator(NpAllocator* allocator);
void APS5_VABI sceNpGlobalHeapGetAllocatorEx(NpAllocatorEx* allocator);
NpAllocator* APS5_VABI sceNpGlobalHeapGetAllocatorPtr(void);
NpAllocatorEx* APS5_VABI sceNpGlobalHeapGetAllocatorExPtr(void);
int APS5_VABI sceNpAllocateKernelMemoryNoAlignment(std::uint64_t size, const char* name, void** address, NpKernelMemory* memory);
int APS5_VABI sceNpAllocateKernelMemoryWithAlignment(std::uint64_t size, const char* name, void** address, NpKernelMemory* memory);
int APS5_VABI sceNpFreeKernelMemory(NpKernelMemory* memory);
void APS5_VABI sceNpGetSdkVersionUInt(std::uint32_t* version);
int APS5_VABI sceNpRtcConvertToPosixTime(const RtcTick* tick, std::int64_t* seconds);
int APS5_VABI sceNpRtcFormatRFC3339(char* text, std::size_t size, const RtcTick* utc, int timeZoneMinutes);
int APS5_VABI sceNpUserGetUserIdList(int* users, std::size_t capacity, int* count);
int APS5_VABI sceNpRtcParseRFC3339(RtcTick* utc, const char* text);
int APS5_VABI sceNpCalloutInitCtx(NpCalloutContext* context, const char* name, std::size_t stackSize, int priority, KernelCpumask affinity);
void APS5_VABI sceNpCalloutTermCtx(NpCalloutContext* context);
int APS5_VABI sceNpCalloutStartOnCtx64(NpCalloutContext* context, NpCallout* callout, std::uint64_t usec, void (APS5_VABI *handler)(void* arg), void* arg);
int APS5_VABI sceNpCalloutStartOnCtx(NpCalloutContext* context, NpCallout* callout, std::uint32_t usec, void (APS5_VABI *handler)(void* arg), void* arg);
int APS5_VABI sceNpCalloutStopOnCtx(NpCalloutContext* context, NpCallout* callout, int* wasPending);
std::uint32_t APS5_VABI sceNpGetRandom(void);
int APS5_VABI sceNpUtilCmpAccountId(std::uint64_t left, std::uint64_t right);
int APS5_VABI sceNpCmpNpId(const void* npId1, const void* npId2);
bool APS5_VABI sceNpIntIsOnlineIdString(const char* text);
int APS5_VABI sceNpUtilGetNpLanguageCode(int language, char* code);
int APS5_VABI sceNpUtilGetNpLanguageCode2(int language, char* tag);
const char* APS5_VABI sceNpUtilGetNpLanguageCodeStr(int language);
const char* APS5_VABI sceNpUtilGetNpLanguageCode2Str(int language);
std::uint32_t APS5_VABI sceNpDbgAssignDebugId(void);
int APS5_VABI sceNpUtilBuildTitleId(const NpTitleIdParts* parts, char* titleId, std::size_t size);
void APS5_VABI sceNpDbgDumpBinary(const void* data, std::size_t size);
int APS5_VABI sceNpBase64GetDecodeSize(const char* src, std::size_t length);
int APS5_VABI sceNpBase64Decoder(const char* src, std::size_t length, void* dst, std::size_t capacity);
int APS5_VABI sceNpBase64Encoder(const void* src, char* dst, std::size_t length);
int APS5_VABI sceNpBase64UrlGetDecodeSize(const char* src, std::size_t length);
int APS5_VABI sceNpBase64UrlDecoder(const char* src, std::size_t length, void* dst, std::size_t capacity);
int APS5_VABI sceNpBase64UrlEncoder(const void* src, char* dst, std::size_t length);
int APS5_VABI sceNpHexToInt(int c);
int APS5_VABI sceNpIntToHex(std::uint32_t value, bool uppercase);
int APS5_VABI sceNpUInt32ToStr(char* buf, std::size_t size, std::uint32_t value, std::uint32_t radix, std::uint32_t flags, std::size_t* length);
int APS5_VABI sceNpUInt64ToStr(char* buf, std::size_t size, std::uint64_t value, std::uint32_t radix, std::uint32_t flags, std::size_t* length);
int APS5_VABI sceNpInt32ToStr(char* buf, std::size_t size, std::int32_t value, std::uint32_t radix, std::uint32_t flags, std::size_t* length);
int APS5_VABI sceNpInt64ToStr(char* buf, std::size_t size, std::int64_t value, std::uint32_t radix, std::uint32_t flags, std::size_t* length);
int APS5_VABI sceNpStrToInt32(const char* str, std::int32_t* value, int base, std::size_t* parsedLength);
int APS5_VABI sceNpStrToInt64(const char* str, std::int64_t* value, int base, std::size_t* parsedLength);
int APS5_VABI sceNpStrToUInt32(const char* str, std::uint32_t* value, int base, std::size_t* parsedLength);
int APS5_VABI sceNpStrToUInt64(const char* str, std::uint64_t* value, int base, std::size_t* parsedLength);
int APS5_VABI sceNpStrBuildHex(char* dst, std::size_t size, const void* src, std::size_t length, bool uppercase);
int APS5_VABI sceNpStrParseHex(const char* hex, void* out, std::size_t capacity);
int APS5_VABI sceNpStrnParseHex(const char* hex, std::size_t length, void* out, std::size_t capacity);
int APS5_VABI sceNpStrcpyToBuf(char* dst, std::size_t size, const char* src);
int APS5_VABI sceNpStrncpyToBuf(char* dst, std::size_t size, const char* src, std::size_t maxLength);
int APS5_VABI sceNpUtilJsonEscape(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength);
int APS5_VABI sceNpUtilJsonUnescape(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength, std::size_t* dstLength);
int APS5_VABI sceNpUtilJsonGetOneChar(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength, std::size_t* dstLength, std::size_t* consumed);
int APS5_VABI sceNpUtilGetNthChar(std::size_t index, const char* text, const char** character);
int APS5_VABI sceNpUtilHttpUrlEncode(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength, std::size_t* dstLength);

[[noreturn]] void APS5_VABI sceNpPanic(void);

}

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

struct NpJsonRule {
    std::uint64_t kind;
    const char* key;
    std::uint64_t descend;
};

struct NpJsonRuleSet {
    const NpJsonRule* const* levels;
    std::uint64_t count;
};

using NpJsonCallback = int(APS5_VABI*)(void* context, std::int32_t event, std::uint32_t depth, std::int32_t element,
    const char* key, const void* value, std::int32_t arrayIndex, void* user);
using NpJsonExCallback = int(APS5_VABI*)(void* context, std::int32_t event, std::uint32_t depth, std::int32_t element,
    const char* key, const void* value, std::int32_t arrayIndex, std::uint32_t offset, void* user);

struct NpKernelMemory {
    std::uint64_t size;
    std::uint64_t reserved;
    void* address;
};

struct NpMemoryHeap {
    NpKernelMemory memory;
    NpHeap heap;
};

struct NpServerErrorJsonResult {
    std::uint8_t found;
    std::int32_t code;
};

struct NpTitleIdParts {
    char id[9];
    char reserved;
    std::uint8_t index;
};

using NpTime = RtcTick;
struct NpMemoryStreamReader {
    const void* const* vtable;
    const void* data;
    std::size_t size;
    std::size_t position;
};

struct NpMemoryStreamWriter {
    const void* const* vtable;
    void* data;
    std::size_t size;
    std::size_t position;
};

struct NpStreamReader {
    const void* const* vtable;
};

struct NpStreamWriter {
    const void* const* vtable;
};

struct NpStreamCtx {
    const void* const* vtable;
};

struct NpRefObject {
    const void* const* vtable;
    std::int32_t refCount;
};

struct NpCommunicationId {
    char data[9];
    char term;
    std::uint8_t num;
    char dummy;
};

struct NpCommIdObject {
    const void* const* vtable;
    NpCommunicationId commId;
};

struct NpTitleIdObject {
    const void* const* vtable;
    NpTitleId titleId;
};

extern "C" {

int APS5_VABI sceNpGetSandboxMode();
int APS5_VABI sceNpUtilGetSandboxMode();

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
int APS5_VABI sceNpCloseSema(void);
int APS5_VABI sceNpGetNavSdkVersion(void);
int APS5_VABI sceNpIntGetPlatformType(void);
int APS5_VABI sceNpIntSetPlatformType(void);
int APS5_VABI sceNpJsonParse(void* guestContext, const char* data, int length);
int APS5_VABI sceNpJsonParseBuf(void* guestContext, const char* data, int length);
int APS5_VABI sceNpJsonParseBufInit(void* guestContext, const NpJsonRuleSet* rules, char* key,
    std::size_t keySize, char* value, std::size_t valueSize, NpJsonCallback callback, void* user);
int APS5_VABI sceNpJsonParseEx(void* guestContext, const char* data, int length);
int APS5_VABI sceNpJsonParseExInit(void* guestContext, const NpJsonRuleSet* rules, char* key,
    std::size_t keySize, char* value, std::size_t valueSize, NpJsonExCallback callback, void* user);
int APS5_VABI sceNpJsonParseInit(void* guestContext, const NpJsonRuleSet* rules, NpJsonCallback callback,
    void* user);
int APS5_VABI sceNpOpenSema(void);
int APS5_VABI sceNpServerErrorJsonGetErrorCode(std::int32_t httpStatus, const char* json, std::size_t length);
int APS5_VABI sceNpServerErrorJsonMultiGetErrorCode(std::int32_t httpStatus, const char* json, std::size_t length);
int APS5_VABI sceNpServerErrorJsonParse(void* guestContext, const char* data, int length);
int APS5_VABI sceNpServerErrorJsonParseInit(void* guestContext, NpServerErrorJsonResult* result);
int APS5_VABI sceNpServerErrorJsonParseMultiInit(void* guestContext, NpServerErrorJsonResult* result);
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
int APS5_VABI sceNpCloseEventFlag(KernelEventFlag flag);
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
void* APS5_VABI _ZN3sce2np6ObjectnwEmR14SceNpAllocator(std::size_t size, NpAllocator* allocator);
void* APS5_VABI _ZN3sce2np6ObjectnaEmR14SceNpAllocator(std::size_t size, NpAllocator* allocator);
void APS5_VABI _ZN3sce2np6ObjectdlEPvR14SceNpAllocator(void* object, NpAllocator* allocator);
void APS5_VABI _ZN3sce2np6ObjectdaEPvR14SceNpAllocator(void* object, NpAllocator* allocator);
void* APS5_VABI _ZN3sce2np6ObjectnwEmR16SceNpAllocatorEx(std::size_t size, NpAllocatorEx* allocator);
void* APS5_VABI _ZN3sce2np6ObjectnaEmR16SceNpAllocatorEx(std::size_t size, NpAllocatorEx* allocator);
void APS5_VABI _ZN3sce2np6ObjectdlEPvR16SceNpAllocatorEx(void* object, NpAllocatorEx* allocator);
void APS5_VABI _ZN3sce2np6ObjectdaEPvR16SceNpAllocatorEx(void* object, NpAllocatorEx* allocator);
void APS5_VABI _ZN3sce2np6ObjectdlEPv(void* object);
void APS5_VABI _ZN3sce2np6ObjectdaEPv(void* object);
int APS5_VABI sceNpAllocateKernelMemoryNoAlignment(std::uint64_t size, const char* name, void** address, NpKernelMemory* memory);
int APS5_VABI sceNpAllocateKernelMemoryWithAlignment(std::uint64_t size, const char* name, void** address, NpKernelMemory* memory);
int APS5_VABI sceNpFreeKernelMemory(NpKernelMemory* memory);
void APS5_VABI sceNpGetSdkVersionUInt(std::uint32_t* version);
int APS5_VABI sceNpRtcConvertToPosixTime(const RtcTick* tick, std::int64_t* seconds);
int APS5_VABI _ZN3sce2np4Time12GetUserClockEPS1_(NpTime* time);
int APS5_VABI _ZN3sce2np4Time15GetNetworkClockEPS1_(NpTime* time);
int APS5_VABI _ZN3sce2np4Time20GetDebugNetworkClockEPS1_(NpTime* time);
int APS5_VABI _ZN3sce2np4Time11SetDateTimeERKNS0_8DateTimeE(NpTime* self, const RtcDateTime* dateTime);
int APS5_VABI _ZN3sce2np4Time11GetDateTimeEPNS0_8DateTimeE(const NpTime* self, RtcDateTime* dateTime);
int APS5_VABI _ZNK3sce2np4Time18ConvertToPosixTimeEPl(const NpTime* self, std::int64_t* seconds);
NpTime* APS5_VABI _ZN3sce2np4Time15AddMicroSecondsEl(NpTime* self, std::int64_t microseconds);
NpTime* APS5_VABI _ZN3sce2np4Time10AddSecondsEl(NpTime* self, std::int64_t seconds);
NpTime* APS5_VABI _ZN3sce2np4Time10AddMinutesEl(NpTime* self, std::int64_t minutes);
NpTime* APS5_VABI _ZN3sce2np4Time8AddHoursEl(NpTime* self, std::int64_t hours);
NpTime* APS5_VABI _ZN3sce2np4Time7AddDaysEl(NpTime* self, std::int64_t days);
bool APS5_VABI _ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_lPl(const NpTime* self, const NpTime* since, std::int64_t timeout, std::int64_t* remaining);
bool APS5_VABI _ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_jPj(const NpTime* self, const NpTime* since, std::uint32_t timeout, std::uint32_t* remaining);
bool APS5_VABI _ZNK3sce2np4Time22IsTimedoutSinceSecondsERKS1_lPl(const NpTime* self, const NpTime* since, std::int64_t timeout, std::int64_t* remaining);
int APS5_VABI _ZNK3sce2np4Time13FormatRFC3339EPcmi(const NpTime* self, char* text, std::size_t size, int timeZoneMinutes);
NpTime* APS5_VABI _ZN3sce2np4TimeplERKS1_(NpTime* self, const NpTime* other);
NpTime* APS5_VABI _ZN3sce2np4TimeplERK10SceRtcTick(NpTime* self, const RtcTick* tick);
bool APS5_VABI _ZN3sce2npeqERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npeqERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npeqERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs);
bool APS5_VABI _ZN3sce2npneERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npneERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npneERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs);
bool APS5_VABI _ZN3sce2npleERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npleERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npleERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs);
bool APS5_VABI _ZN3sce2npgeERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npgeERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npgeERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs);
bool APS5_VABI _ZN3sce2npltERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npltERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npltERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs);
bool APS5_VABI _ZN3sce2npgtERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npgtERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs);
bool APS5_VABI _ZN3sce2npgtERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs);
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

int APS5_VABI _ZN3sce2np10CancelableC2Ev(void);
int APS5_VABI sceNpUtilXmlEscape(void);
int APS5_VABI _ZN3sce2np4CondD0Ev(void);
int APS5_VABI _sceNpAllocatorExRealloc(void);
int APS5_VABI _ZN3sce2np8WorkItem6CancelEi(void);
int APS5_VABI _ZN3sce2np9WorkQueue11GetItemByIdEi(void);
int APS5_VABI _ZN3sce2np14JsonDocBuilderC2ERKNS0_9JsonValueE(void);
int APS5_VABI _ZN3sce2np7RingBuf4dtorEv(void);
int APS5_VABI _ZThn16_N3sce2np9HttpTransD0Ev(void);
int APS5_VABI _ZN3sce2np10MemoryFileD0Ev(void);
int APS5_VABI _ZN3sce2np10EventQueueC2EP16SceNpAllocatorEx(void);
int APS5_VABI _ZN3sce2np4Cond4ctorEv(void);
int APS5_VABI _ZN3sce2np16StreamReadBufferD1Ev(void);
int APS5_VABI _ZNK3sce2np9EventFlag6IsInitEv(void);
int APS5_VABI _ZN3sce2np10CancelLockC2Ev(void);
int APS5_VABI _ZN3sce2np13NpTitleSecretC2ERK16SceNpTitleSecret(void);
int APS5_VABI _ZN3sce2np8WorkItem10SetRunningEv(void);
int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient21CreateRequestForAsyncEPiiPKvm(void);
int APS5_VABI _ZN3sce2np18HttpConnectionPool13InvalidateAllEv(void);
int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient21DeleteRequestForAsyncEii(void);
int APS5_VABI _ZN3sce2np15CancelableScopeD1Ev(void);
int APS5_VABI _ZN3sce2np8JsonFileD2Ev(void);
int APS5_VABI _ZN3sce2np9JsonArray12AddItemArrayEPPS1_(void);
int APS5_VABI _ZN3sce2np14CalloutContextD0Ev(void);
int APS5_VABI _ZN3sce2np9Semaphore4WaitEj(void);
int APS5_VABI _ZN3sce2np13RingBufMemory6ExpandEm(void);
int APS5_VABI _ZThn8_N3sce2np10MemoryFile4ReadEPNS0_6HandleEPvmPm(void);
int APS5_VABI _ZN3sce2np7RingBuf15CheckoutForReadEPm(void);
int APS5_VABI _ZN3sce2np9EventFlag4OpenEPKc(void);
int APS5_VABI _ZN3sce2np13NpTitleSecret5ClearEv(void);
int APS5_VABI _ZN3sce2np10EventQueue7EnqueueEPKvmj(void);
int APS5_VABI _ZN3sce2np12HttpTemplateD1Ev(void);
int APS5_VABI _ZN3sce2np9WorkQueueD2Ev(void);
int APS5_VABI _ZThn16_N3sce2np9HttpTrans5WriteEPNS0_6HandleEPKvmPm(void);
int APS5_VABI _ZN3sce2np10EventQueue4ctorEv(void);
int APS5_VABI _ZN3sce2np20BufferedStreamReaderD1Ev(void);
int APS5_VABI _ZN3sce2np3ipc10IpmiClient7ConnectEPKvm(void);
int APS5_VABI _ZN3sce2np4Cond4InitEPKcPNS0_5MutexE(void);
int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient10DisconnectEv(void);
int APS5_VABI _ZN3sce2np3ipc10IpmiClient11IsConnectedEv(void);
int APS5_VABI _ZN3sce2np12HttpTemplate19SetAuthInfoCallbackEPFii15SceHttpAuthTypePKcPcS5_iPPhPmPiPvESA_(void);
int APS5_VABI sceNpUtilParseJid(void);
int APS5_VABI _ZN3sce2np9Semaphore6CreateEiiPKc(void);
int APS5_VABI sceNpUtilXmlUnescape(void);
int APS5_VABI sceNpXmlParse(void);
int APS5_VABI _ZN3sce2np9WorkQueue15GetFinishedItemENS0_14WorkItemStatusE(void);
int APS5_VABI _ZN3sce2np9EventFlag4PollEmjPm(void);
int APS5_VABI _ZN3sce2np9EventFlagD0Ev(void);
int APS5_VABI _ZN3sce2np9LocalFile6RemoveEPKc(void);
int APS5_VABI _ZN3sce2np14JsonDocBuilder12BuildBufSizeEv(void);
int APS5_VABI _ZN3sce2np10MemoryFile8TruncateEl(void);
int APS5_VABI _ZN3sce2np9HttpTrans24GetResponseContentLengthEPNS0_6HandleEPbPm(void);
int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient13CreateRequestEPiiPKvm(void);
int APS5_VABI _ZN3sce2np8SelectorD2Ev(void);
int APS5_VABI _ZN3sce2np10CancelableD1Ev(void);
int APS5_VABI _ZN3sce2np7HttpUriD1Ev(void);
int APS5_VABI _ZN3sce2np15CancelableScopeD0Ev(void);
int APS5_VABI _ZN3sce2np10Cancelable7DestroyEv(void);
int APS5_VABI _ZThn8_N3sce2np9LocalFileD1Ev(void);
int APS5_VABI _ZNK3sce2np10JsonString6GetStrEPcm(void);
int APS5_VABI _ZN3sce2np10Cancelable10LockCancelEPKciS3_(void);
int APS5_VABI _ZN3sce2np8WorkItem10SetPendingEv(void);
int APS5_VABI _ZN3sce2np7Callout4StopEv(void);
int APS5_VABI _ZN3sce2np7RingBuf14CheckinForReadEm(void);
int APS5_VABI _ZN3sce2np6ThreadD1Ev(void);
int APS5_VABI _ZN3sce2npeqERK12SceNpTitleIdRKNS0_9NpTitleIdE(void);
int APS5_VABI _ZN3sce2np9EventFlag3SetEm(void);
int APS5_VABI _ZNK3sce2np7RingBuf6IsFullEv(void);
int APS5_VABI _ZN3sce2npneERKNS0_13NpTitleSecretERK16SceNpTitleSecret(void);
int APS5_VABI _ZN3sce2np9LocalFileD0Ev(void);
int APS5_VABI _ZN3sce2np16StreamReadBufferD2Ev(void);
int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient11TermServiceEi(void);
int APS5_VABI _ZN3sce2np6HandleD1Ev(void);
int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient17UnregisterServiceEi(void);
int APS5_VABI _ZN3sce2np9WorkQueue4ctorEv(void);
int APS5_VABI _ZN3sce2np6Thread4InitEPKcimm(void);
int APS5_VABI _ZN3sce2np7Callout10IsTimedoutEv(void);
int APS5_VABI _ZN3sce2np7Callout11CalloutFuncEPv(void);
int APS5_VABI _ZThn8_N3sce2np6HandleD1Ev(void);
int APS5_VABI _ZN3sce2np6Handle4InitEv(void);
int APS5_VABI _ZThn16_N3sce2np10MemoryFileD1Ev(void);
int APS5_VABI _sceNpAllocatorExFree(void);
int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient7ConnectEPKvm(void);
int APS5_VABI _ZN3sce2np7RingBufC1Ev(void);
int APS5_VABI _ZN3sce2np10MemoryFileD1Ev(void);
int APS5_VABI _ZN3sce2np9HttpTrans21GetResponseStatusCodeEPNS0_6HandleEPi(void);
int APS5_VABI _ZN3sce2np13NpTitleSecretC2ERKS1_(void);
int APS5_VABI _ZN3sce2np14CalloutContextD1Ev(void);
int APS5_VABI _ZN3sce2np5Mutex7DestroyEv(void);
int APS5_VABI _ZN3sce2np14JsonDocBuilderC1ERKNS0_9JsonValueE(void);
int APS5_VABI _ZN3sce2np9HttpTransD2Ev(void);
int APS5_VABI _ZN3sce2np12HttpTemplateC2Ev(void);
int APS5_VABI _ZN3sce2np9EventFlag6CreateEPKcj(void);
int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient15RegisterServiceEPKNS1_17ServiceClientInfoE(void);
int APS5_VABI _ZN3sce2np8WorkItemD0Ev(void);
int APS5_VABI _ZN3sce2np9SemaphoreD2Ev(void);
int APS5_VABI _ZN3sce2np9EventFlagD2Ev(void);
int APS5_VABI _sceNpAllocatorMalloc(void);
int APS5_VABI _ZN3sce2np13RingBufMemory6IsInitEv(void);
int APS5_VABI _ZN3sce2np7RingBufD0Ev(void);
int APS5_VABI _sceNpHeapMalloc(void);
int APS5_VABI _ZN3sce2np12HttpTemplate7DestroyEv(void);
int APS5_VABI _ZN3sce2np15CancelableScope5BeginEPNS0_6HandleEPKciS5_(void);
int APS5_VABI _ZNK3sce2np10EventQueue6IsInitEv(void);
int APS5_VABI _ZN3sce2np8JsonBool5ClearEv(void);
int APS5_VABI _ZN3sce2np9LocalFile8TruncateEl(void);
int APS5_VABI _ZNK3sce2np10JsonNumber6GetNumEPcm(void);
int APS5_VABI _ZN3sce2np13RingBufMemory7DestroyEv(void);
int APS5_VABI _ZN3sce2np9LocalFileC1Ev(void);
int APS5_VABI _ZN3sce2np7RingBufD2Ev(void);
int APS5_VABI _ZN3sce2np4PathD2Ev(void);
int APS5_VABI _ZN3sce2np4CondD1Ev(void);
int APS5_VABI _ZN3sce2np9EventFlag7DestroyEv(void);
int APS5_VABI _ZN3sce2np9HttpTransC1EP16SceNpAllocatorEx(void);
int APS5_VABI _ZN3sce2np6Thread4dtorEv(void);
int APS5_VABI _ZN3sce2np12WorkerThreadD0Ev(void);
int APS5_VABI _ZN3sce2np13RingBufMemoryD0Ev(void);
int APS5_VABI _ZN3sce2np9LocalFile5CloseEv(void);
int APS5_VABI _ZNK3sce2np10JsonString9GetLengthEv(void);
int APS5_VABI _ZN3sce2np10EventQueueD0Ev(void);
int APS5_VABI _ZN3sce2np8JsonNull5ClearEv(void);
int APS5_VABI _ZN3sce2np3ipc17ServiceIpmiClient12AbortRequestEii(void);
int APS5_VABI _ZNK3sce2np5Mutex6IsInitEv(void);

void APS5_VABI _ZN3sce2np18MemoryStreamReaderC1EPKvm(NpMemoryStreamReader* self, const void* data, std::size_t size);
void APS5_VABI _ZN3sce2np18MemoryStreamReaderC2EPKvm(NpMemoryStreamReader* self, const void* data, std::size_t size);
void APS5_VABI _ZN3sce2np18MemoryStreamReaderD1Ev(NpMemoryStreamReader* self);
void APS5_VABI _ZN3sce2np18MemoryStreamReaderD2Ev(NpMemoryStreamReader* self);
void APS5_VABI _ZN3sce2np18MemoryStreamReaderD0Ev(NpMemoryStreamReader* self);
int APS5_VABI _ZN3sce2np18MemoryStreamReader4ReadEPNS0_6HandleEPvmPm(NpMemoryStreamReader* self, void* handle, void* out, std::size_t size, std::size_t* read);
void APS5_VABI _ZN3sce2np18MemoryStreamWriterC1EPvm(NpMemoryStreamWriter* self, void* data, std::size_t size);
void APS5_VABI _ZN3sce2np18MemoryStreamWriterC2EPvm(NpMemoryStreamWriter* self, void* data, std::size_t size);
void APS5_VABI _ZN3sce2np18MemoryStreamWriterD1Ev(NpMemoryStreamWriter* self);
void APS5_VABI _ZN3sce2np18MemoryStreamWriterD2Ev(NpMemoryStreamWriter* self);
void APS5_VABI _ZN3sce2np18MemoryStreamWriterD0Ev(NpMemoryStreamWriter* self);
int APS5_VABI _ZN3sce2np18MemoryStreamWriter5WriteEPNS0_6HandleEPKvmPm(NpMemoryStreamWriter* self, void* handle, const void* data, std::size_t size, std::size_t* written);
void APS5_VABI _ZN3sce2np9NpTitleIdC1Ev(NpTitleIdObject* self);
void APS5_VABI _ZN3sce2np9NpTitleIdC2Ev(NpTitleIdObject* self);
void APS5_VABI _ZN3sce2np9NpTitleIdC1ERKS1_(NpTitleIdObject* self, const NpTitleIdObject* other);
void APS5_VABI _ZN3sce2np9NpTitleIdC2ERKS1_(NpTitleIdObject* self, const NpTitleIdObject* other);
void APS5_VABI _ZN3sce2np9NpTitleIdC1ERK12SceNpTitleId(NpTitleIdObject* self, const NpTitleId* titleId);
void APS5_VABI _ZN3sce2np9NpTitleIdC2ERK12SceNpTitleId(NpTitleIdObject* self, const NpTitleId* titleId);
void APS5_VABI _ZN3sce2np9NpTitleIdD1Ev(NpTitleIdObject* self);
void APS5_VABI _ZN3sce2np9NpTitleIdD2Ev(NpTitleIdObject* self);
void APS5_VABI _ZN3sce2np9NpTitleIdD0Ev(NpTitleIdObject* self);
NpTitleIdObject* APS5_VABI _ZN3sce2np9NpTitleIdaSERKS1_(NpTitleIdObject* self, const NpTitleIdObject* other);
void APS5_VABI _ZN3sce2np9NpTitleId5ClearEv(NpTitleIdObject* self);
bool APS5_VABI _ZNK3sce2np9NpTitleId7IsEmptyEv(const NpTitleIdObject* self);
bool APS5_VABI _ZNK3sce2np9NpTitleId6EqualsEPKc(const NpTitleIdObject* self, const char* titleId);
bool APS5_VABI _ZNK3sce2np9NpTitleId6EqualsERKS1_(const NpTitleIdObject* self, const NpTitleIdObject* other);
bool APS5_VABI _ZNK3sce2np9NpTitleId6EqualsERK12SceNpTitleId(const NpTitleIdObject* self, const NpTitleId* titleId);
int APS5_VABI _ZN3sce2np9NpTitleId20ConstructFromTitleIdEPS1_PKc(NpTitleIdObject* out, const char* text);
int APS5_VABI _ZN3sce2np9NpTitleId20ConstructFromTitleIdEPS1_PKcm(NpTitleIdObject* out, const char* text, std::size_t length);
int APS5_VABI _ZN3sce2np9NpTitleId5ParseEPS1_PKc(NpTitleIdObject* out, const char* text);
int APS5_VABI _ZN3sce2np9NpTitleId5ParseEPS1_PKcm(NpTitleIdObject* out, const char* text, std::size_t length);
int APS5_VABI _ZN3sce2np9NpTitleId5BuildERKS1_Pcm(const NpTitleIdObject* titleId, char* text, std::size_t size);
bool APS5_VABI _ZN3sce2npeqERKNS0_9NpTitleIdES3_(const NpTitleIdObject* lhs, const NpTitleIdObject* rhs);
bool APS5_VABI _ZN3sce2npeqERKNS0_9NpTitleIdERK12SceNpTitleId(const NpTitleIdObject* lhs, const NpTitleId* rhs);
bool APS5_VABI _ZN3sce2npneERKNS0_9NpTitleIdES3_(const NpTitleIdObject* lhs, const NpTitleIdObject* rhs);
bool APS5_VABI _ZN3sce2npneERK12SceNpTitleIdRKNS0_9NpTitleIdE(const NpTitleId* lhs, const NpTitleIdObject* rhs);
bool APS5_VABI _ZN3sce2npneERKNS0_9NpTitleIdERK12SceNpTitleId(const NpTitleIdObject* lhs, const NpTitleId* rhs);

int APS5_VABI _ZN3sce2np12StreamReader4ReadEPNS0_6HandleEPNS0_9StreamCtxEPvmPm(NpStreamReader* self, void* handle, NpStreamCtx* ctx, void* out, std::size_t size, std::size_t* read);
int APS5_VABI _ZN3sce2np12StreamReader7ReadAllEPNS0_6HandleEPvmPm(NpStreamReader* self, void* handle, void* out, std::size_t size, std::size_t* read);
int APS5_VABI _ZN3sce2np12StreamReader7ReadAllEPNS0_6HandleEPNS0_9StreamCtxEPvmPm(NpStreamReader* self, void* handle, NpStreamCtx* ctx, void* out, std::size_t size, std::size_t* read);
int APS5_VABI _ZN3sce2np12StreamReader7SkipAllEPNS0_6HandleEPl(NpStreamReader* self, void* handle, std::int64_t* skipped);
int APS5_VABI _ZN3sce2np12StreamReader7SkipAllEPNS0_6HandleEPNS0_9StreamCtxEPl(NpStreamReader* self, void* handle, NpStreamCtx* ctx, std::int64_t* skipped);
int APS5_VABI _ZN3sce2np12StreamReader8ReadDataEPNS0_6HandleEPvmPm(NpStreamReader* self, void* handle, void* out, std::size_t size, std::size_t* read);
int APS5_VABI _ZN3sce2np12StreamReader8ReadDataEPNS0_6HandleEPNS0_9StreamCtxEPvmPm(NpStreamReader* self, void* handle, NpStreamCtx* ctx, void* out, std::size_t size, std::size_t* read);
int APS5_VABI _ZN3sce2np12StreamReader8SkipDataEPNS0_6HandleElPl(NpStreamReader* self, void* handle, std::int64_t size, std::int64_t* skipped);
int APS5_VABI _ZN3sce2np12StreamReader8SkipDataEPNS0_6HandleEPNS0_9StreamCtxElPl(NpStreamReader* self, void* handle, NpStreamCtx* ctx, std::int64_t size, std::int64_t* skipped);
int APS5_VABI _ZN3sce2np12StreamWriter15WriteFilledDataEPNS0_6HandleEcl(NpStreamWriter* self, void* handle, char fill, std::int64_t size);
int APS5_VABI _ZN3sce2np12StreamWriter15WriteFilledDataEPNS0_6HandleEPNS0_9StreamCtxEcl(NpStreamWriter* self, void* handle, NpStreamCtx* ctx, char fill, std::int64_t size);
int APS5_VABI _ZN3sce2np12StreamWriter5WriteEPNS0_6HandleEPNS0_9StreamCtxEPKvmPm(NpStreamWriter* self, void* handle, NpStreamCtx* ctx, const void* data, std::size_t size, std::size_t* written);
int APS5_VABI _ZN3sce2np12StreamWriter9WriteDataEPNS0_6HandleEPKvmPm(NpStreamWriter* self, void* handle, const void* data, std::size_t size, std::size_t* written);
int APS5_VABI _ZN3sce2np12StreamWriter9WriteDataEPNS0_6HandleEPNS0_9StreamCtxEPKvmPm(NpStreamWriter* self, void* handle, NpStreamCtx* ctx, const void* data, std::size_t size, std::size_t* written);
void APS5_VABI _ZN3sce2np9RefObjectC1Ev(NpRefObject* self);
void APS5_VABI _ZN3sce2np9RefObjectC2Ev(NpRefObject* self);
void APS5_VABI _ZN3sce2np9RefObjectD1Ev(NpRefObject* self);
void APS5_VABI _ZN3sce2np9RefObjectD2Ev(NpRefObject* self);
void APS5_VABI _ZN3sce2np9RefObjectD0Ev(NpRefObject* self);
int APS5_VABI _ZN3sce2np9RefObject6AddRefEv(NpRefObject* self);
int APS5_VABI _ZN3sce2np9RefObject7ReleaseEv(NpRefObject* self);
int APS5_VABI sceNpUtilParseTitleId(NpCommunicationId* parts, const char* titleId);
void APS5_VABI _ZN3sce2np8NpCommIdC1Ev(NpCommIdObject* self);
void APS5_VABI _ZN3sce2np8NpCommIdC2Ev(NpCommIdObject* self);
void APS5_VABI _ZN3sce2np8NpCommIdC1ERKS1_(NpCommIdObject* self, const NpCommIdObject* other);
void APS5_VABI _ZN3sce2np8NpCommIdC2ERKS1_(NpCommIdObject* self, const NpCommIdObject* other);
void APS5_VABI _ZN3sce2np8NpCommIdC1ERK20SceNpCommunicationId(NpCommIdObject* self, const NpCommunicationId* commId);
void APS5_VABI _ZN3sce2np8NpCommIdC2ERK20SceNpCommunicationId(NpCommIdObject* self, const NpCommunicationId* commId);
void APS5_VABI _ZN3sce2np8NpCommIdD1Ev(NpCommIdObject* self);
void APS5_VABI _ZN3sce2np8NpCommIdD2Ev(NpCommIdObject* self);
void APS5_VABI _ZN3sce2np8NpCommIdD0Ev(NpCommIdObject* self);
void APS5_VABI _ZN3sce2np8NpCommId5ClearEv(NpCommIdObject* self);
bool APS5_VABI _ZNK3sce2np8NpCommId7IsEmptyEv(const NpCommIdObject* self);
bool APS5_VABI _ZNK3sce2np8NpCommId6EqualsEPKc(const NpCommIdObject* self, const char* commId);
bool APS5_VABI _ZNK3sce2np8NpCommId6EqualsERKS1_(const NpCommIdObject* self, const NpCommIdObject* other);
bool APS5_VABI _ZNK3sce2np8NpCommId6EqualsERK20SceNpCommunicationId(const NpCommIdObject* self, const NpCommunicationId* commId);
int APS5_VABI _ZN3sce2np8NpCommId5ParseEPS1_PKc(NpCommIdObject* out, const char* text);
int APS5_VABI _ZN3sce2np8NpCommId5ParseEPS1_PKcm(NpCommIdObject* out, const char* text, std::size_t length);
int APS5_VABI _ZN3sce2np8NpCommId5BuildERKS1_Pcm(const NpCommIdObject* commId, char* text, std::size_t size);
bool APS5_VABI _ZN3sce2npeqERKNS0_8NpCommIdES3_(const NpCommIdObject* lhs, const NpCommIdObject* rhs);
bool APS5_VABI _ZN3sce2npeqERK20SceNpCommunicationIdRKNS0_8NpCommIdE(const NpCommunicationId* lhs, const NpCommIdObject* rhs);
bool APS5_VABI _ZN3sce2npeqERKNS0_8NpCommIdERK20SceNpCommunicationId(const NpCommIdObject* lhs, const NpCommunicationId* rhs);
bool APS5_VABI _ZN3sce2npneERKNS0_8NpCommIdES3_(const NpCommIdObject* lhs, const NpCommIdObject* rhs);
bool APS5_VABI _ZN3sce2npneERK20SceNpCommunicationIdRKNS0_8NpCommIdE(const NpCommunicationId* lhs, const NpCommIdObject* rhs);
bool APS5_VABI _ZN3sce2npneERKNS0_8NpCommIdERK20SceNpCommunicationId(const NpCommIdObject* lhs, const NpCommunicationId* rhs);
[[noreturn]] void APS5_VABI sceNpPanic(void);

}

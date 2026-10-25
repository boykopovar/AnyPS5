#include "prx/libSceSaveData/SaveDataCore.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNpCommon/include/Api.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libSceRtc/include/Rtc.hpp"
#include <algorithm>
#include <atomic>
#include <iterator>
#include <mutex>
#include <random>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/MallocManagedSize.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Pthread/include/Cond.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libkernel/Semaphore/include/Semaphore.hpp"

static_assert(sizeof(NpCallout) == 0x28);

static_assert(sizeof(NpCalloutContext) == 0x28);

static_assert(sizeof(NpKernelMemory) == 0x18);

static_assert(sizeof(NpMemoryHeap) == 0x20);

namespace {

constexpr int NP_ERROR_NULL_ARGUMENT = static_cast<int>(0x80550003);
constexpr int NP_ERROR_IPC_ALREADY_INITIALIZED = static_cast<int>(0x80559D00);
constexpr int NP_ERROR_IPC_NOT_INITIALIZED = static_cast<int>(0x80559D01);
constexpr int NP_ERROR_ONLINE_ID_MISMATCH = static_cast<int>(0x80550609);
constexpr int NP_ERROR_INVALID_NP_ID = static_cast<int>(0x80550605);
constexpr std::size_t NP_ID_OPT_OFFSET = 0x14;
constexpr std::size_t NP_ID_PLATFORM_OFFSET = 0x18;
constexpr std::size_t NP_ID_TERMINATOR_FLAG_OFFSET = 0x1C;

std::uint32_t NpIdWord(const std::uint8_t* npId, std::size_t offset) {
    std::uint32_t value;
    std::memcpy(&value, npId + offset, sizeof(value));
    return value;
}
constexpr std::size_t NP_ONLINE_ID_MAX = 16;
constexpr std::size_t NP_ONLINE_ID_MIN = 3;
constexpr int NP_ERROR_INVALID_ARGUMENT_UTIL = static_cast<int>(0x80550601);
constexpr std::size_t NP_LANGUAGE_CODE_SIZE = 6;
constexpr std::size_t NP_LANGUAGE_CODE2_SIZE = 0x24;

struct NpLanguageCodes {
    const char* code;
    const char* tag;
};

constexpr NpLanguageCodes NP_LANGUAGE_CODES[] {
    {"ja", "ja-JP"}, {"en", "en-US"}, {"fr", "fr-FR"}, {"es", "es-ES"}, {"de", "de-DE"}, {"it", "it-IT"},
    {"nl", "nl-NL"}, {"pt", "pt-PT"}, {"ru", "ru-RU"}, {"ko", "ko-KR"}, {"zh-TW", "zh-Hant"}, {"zh-CN", "zh-Hans"},
    {"fi", "fi-FI"}, {"sv", "sv-SE"}, {"da", "da-DK"}, {"no", "no-NO"}, {"pl", "pl-PL"}, {"pt-BR", "pt-BR"},
    {"en-GB", "en-GB"}, {"tr", "tr-TR"}, {"es-MX", "es-419"}, {"ar", "ar-AE"}, {"fr-CA", "fr-CA"}, {"cs", "cs-CZ"},
    {"hu", "hu-HU"}, {"el", "el-GR"}, {"ro", "ro-RO"}, {"th", "th-TH"}, {"vi", "vi-VN"}, {"id", "id-ID"},
    {"uk", "uk-UA"},
};

const NpLanguageCodes* FindLanguageCodes(int language) {
    if (language < 0 || static_cast<std::size_t>(language) >= std::size(NP_LANGUAGE_CODES)) return nullptr;
    return &NP_LANGUAGE_CODES[language];
}

void CopyText(char* out, const char* text, std::size_t size) {
    std::strncpy(out, text, size - 1);
    out[size - 1] = '\0';
}

std::atomic<std::uint32_t> g_debugIds {0};

constexpr int NP_UTIL_ERROR_BUFFER_TOO_SMALL = static_cast<int>(0x80550602);
constexpr int NP_UTIL_ERROR_OUT_OF_RANGE = static_cast<int>(0x8055060A);
constexpr std::size_t NP_TITLE_ID_SIZE = 13;
constexpr std::uint8_t NP_TITLE_ID_INDEX_MAX = 99;
constexpr std::size_t NP_DUMP_LINE_BYTES = 16;

bool IsAsciiLetter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool IsOnlineIdText(const char* text) {
    const std::size_t length = std::strlen(text);
    if (length < NP_ONLINE_ID_MIN || length > NP_ONLINE_ID_MAX || !IsAsciiLetter(text[0])) return false;
    return std::all_of(text + 1, text + length, [](char c) {
        return IsAsciiLetter(c) || (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

constexpr int NP_ERROR_BUFFER_TOO_SMALL = static_cast<int>(0x80558008);
constexpr std::size_t NP_SDK_VERSION_SIZE = 8;
constexpr std::size_t NP_RFC3339_BUFFER_MIN = 0x80;
constexpr std::size_t NP_USER_ID_LIST_SIZE = 20;
constexpr int USER_ID_INVALID = -1;
constexpr KernelClockid NP_SYSTEM_CLOCK = 4;

static_assert(sizeof(SceKernelSystemSwVersion) == 0x28);

std::uint32_t SystemVersion() {
    SceKernelSystemSwVersion version {};
    version.size = sizeof(version);
    sceKernelGetSystemSwVersion_nid_postfix(&version);

    return version.version;
}

int BcdPair(std::uint32_t value, int shift) {
    return static_cast<int>((value >> (shift + 4)) & 0xF) * 10 + static_cast<int>((value >> shift) & 0xF);
}

constexpr int NP_ERROR_OUT_OF_MEMORY = static_cast<int>(0x80550005);
constexpr int NP_ERROR_NULL_OBJECT = static_cast<int>(0x80558005);
constexpr int NP_ERROR_HEAP_CREATE = static_cast<int>(0x8055800A);
constexpr std::uint64_t NP_KERNEL_MEMORY_ALIGNMENT = 0x4000;
constexpr int NP_KERNEL_MEMORY_PROTECTION = 3;
constexpr std::size_t NP_GLOBAL_HEAP_SIZE = 0x80000;
constexpr char NP_GLOBAL_HEAP_NAME[] = "NpCommonGlobalHeap";
constexpr std::size_t NP_IPC_CLIENT_HEAP_SIZE = 0x10000;
constexpr char NP_IPC_CLIENT_HEAP_NAME[] = "NpCommonIpcClientHeap";

void* APS5_VABI HeapAllocate(std::size_t size, void* context) {
    auto* heap = static_cast<NpHeap*>(context);
    return heap->mspace != nullptr ? sceLibcMspaceMalloc_nid_postfix(heap->mspace, size) : nullptr;
}

void* APS5_VABI HeapReallocate(void* pointer, std::size_t size, void* context) {
    auto* heap = static_cast<NpHeap*>(context);
    return heap->mspace != nullptr ? sceLibcMspaceRealloc_nid_postfix(heap->mspace, pointer, size) : nullptr;
}

void APS5_VABI HeapRelease(void* pointer, void* context) {
    auto* heap = static_cast<NpHeap*>(context);
    if (heap->mspace != nullptr && pointer != nullptr) sceLibcMspaceFree_nid_postfix(heap->mspace, pointer);
}

void* APS5_VABI MemoryHeapAllocate(std::size_t size, void* context) {
    return HeapAllocate(size, &static_cast<NpMemoryHeap*>(context)->heap);
}

void* APS5_VABI MemoryHeapReallocate(void* pointer, std::size_t size, void* context) {
    return HeapReallocate(pointer, size, &static_cast<NpMemoryHeap*>(context)->heap);
}

void APS5_VABI MemoryHeapRelease(void* pointer, void* context) {
    HeapRelease(pointer, &static_cast<NpMemoryHeap*>(context)->heap);
}

std::uint64_t AlignKernelMemory(std::uint64_t size) {
    const std::uint64_t remainder = size & (NP_KERNEL_MEMORY_ALIGNMENT - 1);
    return remainder == 0 ? size : size + NP_KERNEL_MEMORY_ALIGNMENT - remainder;
}

int MapKernelMemory(NpKernelMemory* memory, std::uint64_t size, const char* name) {
    memory->reserved = 0;
    memory->size = size;
    return sceKernelMapNamedFlexibleMemory(&memory->address, size, NP_KERNEL_MEMORY_PROTECTION, 0, name);
}

void UnmapKernelMemory(NpKernelMemory* memory) {
    if (memory->address != nullptr) sceKernelMunmap(reinterpret_cast<std::uint64_t>(memory->address), memory->size);
    memory->address = nullptr;
}

int InitHeap(NpHeap* heap, void* base, std::size_t size, const char* name) {
    heap->mspace = sceLibcMspaceCreate_nid_postfix(name, base, size, 0);
    return heap->mspace != nullptr ? 0 : NP_ERROR_HEAP_CREATE;
}

void DestroyHeap(NpHeap* heap) {
    if (heap->mspace == nullptr) return;
    sceLibcMspaceDestroy_nid_postfix(heap->mspace);

    heap->mspace = nullptr;
}

int InitMemoryHeap(NpMemoryHeap* heap, std::uint64_t size, const char* name) {
    if (heap == nullptr) return NP_ERROR_NULL_OBJECT;
    int result = MapKernelMemory(&heap->memory, AlignKernelMemory(size), name);
    if (result < 0) return result;
    result = InitHeap(&heap->heap, heap->memory.address, heap->memory.size, name);
    if (result >= 0) return result;
    UnmapKernelMemory(&heap->memory);
    *heap = {};
    return result;
}

void DestroyMemoryHeap(NpMemoryHeap* heap) {
    DestroyHeap(&heap->heap);
    UnmapKernelMemory(&heap->memory);
}

NpAllocator MemoryHeapAllocator(NpMemoryHeap* heap) {
    return {MemoryHeapAllocate, MemoryHeapReallocate, MemoryHeapRelease, heap};
}

constexpr int NP_ERROR_NOT_INITIALIZED = static_cast<int>(0x80558001);
constexpr int NP_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80558002);
constexpr int NP_ERROR_ALREADY_QUEUED = static_cast<int>(0x80558006);
constexpr std::uint64_t NP_CALLOUT_MAX_WAIT = 0xFFFFFFFF;

std::mutex g_randomMutex;
std::mt19937 g_random {std::random_device {}()};

NpMemoryHeap g_globalHeap {};
NpAllocator g_globalAllocator {};
NpAllocatorEx g_globalAllocatorEx {};
int g_archReferences = 0;
std::mutex g_archMutex;
std::mutex g_ipcClientMutex;
bool g_ipcClientInitialized = false;
NpMemoryHeap g_ipcClientHeap {};

constexpr int KERNEL_ERROR_EBUSY = static_cast<int>(0x80020010);
constexpr int KERNEL_ERROR_ETIMEDOUT = static_cast<int>(0x8002003C);
constexpr int NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80558009);
constexpr int NP_ERROR_TIMEOUT = static_cast<int>(0x8055800B);
constexpr int NP_ERROR_EVENT_FLAG_BUSY = static_cast<int>(0x8055800C);
constexpr int NP_ERROR_SEMA_BUSY = static_cast<int>(0x8055800D);
constexpr int NP_ERROR_MUTEX_BUSY = static_cast<int>(0x8055800F);
constexpr std::uint32_t NP_MUTEX_FLAG_RECURSIVE = 1;
constexpr int PTHREAD_MUTEX_TYPE_RECURSIVE = 2;
constexpr std::uint32_t NP_SEMA_FLAG_PRIORITY = 1;
constexpr std::uint32_t KERNEL_SEMA_ATTR_FIFO = 1;
constexpr std::uint32_t KERNEL_SEMA_ATTR_PRIORITY_SHIFT = 8;
constexpr int NP_THREAD_EXPLICIT_SCHED = 0;
constexpr int NP_THREAD_SCHED_POLICY = 1;

int NpResult(int result) {
    return result < 0 ? result : 0;
}

int NpBusyResult(int result, int busy) {
    return result == KERNEL_ERROR_EBUSY ? busy : NpResult(result);
}

int InitMutex(PthreadMutex* mutex, const char* name, std::uint32_t flags) {
    PthreadMutexattr attr {};
    int result = scePthreadMutexattrInit(&attr);
    if (result < 0) return result;
    if ((flags & NP_MUTEX_FLAG_RECURSIVE) != 0) result = scePthreadMutexattrSettype(&attr, PTHREAD_MUTEX_TYPE_RECURSIVE);
    if (result >= 0) result = scePthreadMutexInit(mutex, &attr, name);
    scePthreadMutexattrDestroy(&attr);

    return NpResult(result);
}

int InitCond(PthreadCond* cond, const char* name) {
    PthreadCondattr attr {};
    const int result = scePthreadCondattrInit(&attr);
    if (result < 0) return result;
    const int created = scePthreadCondInit(cond, &attr, name);
    scePthreadCondattrDestroy(&attr);

    return created;
}

}

extern "C" {

int APS5_VABI sceNpAtomicCas32(std::int32_t* value, std::int32_t expected, std::int32_t desired) {
    std::atomic_ref<std::int32_t>(*value).compare_exchange_strong(expected, desired);
    return expected;
}

int APS5_VABI sceNpAtomicInc32(std::int32_t* value) {
    return std::atomic_ref<std::int32_t>(*value).fetch_add(1);
}

int APS5_VABI sceNpAtomicDec32(std::int32_t* value) {
    return std::atomic_ref<std::int32_t>(*value).fetch_sub(1);
}

int APS5_VABI sceNpCondSignalAll(PthreadCond* cond) {
    return scePthreadCondBroadcast(cond);
}

int APS5_VABI sceNpCondSignalTo(PthreadCond* cond, Pthread thread) {
    return scePthreadCondSignalto(cond, thread);
}

int APS5_VABI sceNpCondWait(PthreadCond* cond, PthreadMutex* mutex) {
    return scePthreadCondWait(cond, mutex);
}

int APS5_VABI sceNpLwCondInit(PthreadCond* cond, const char* name) {
    return InitCond(cond, name);
}

int APS5_VABI sceNpLwCondDestroy(PthreadCond* cond) {
    return scePthreadCondDestroy(cond);
}

int APS5_VABI sceNpLwCondSignal(PthreadCond* cond) {
    return scePthreadCondSignal(cond);
}

int APS5_VABI sceNpLwCondSignalAll(PthreadCond* cond) {
    return scePthreadCondBroadcast(cond);
}

int APS5_VABI sceNpLwCondSignalTo(PthreadCond* cond, Pthread thread) {
    return scePthreadCondSignalto(cond, thread);
}

int APS5_VABI sceNpLwCondWait(PthreadCond* cond, PthreadMutex* mutex) {
    return scePthreadCondWait(cond, mutex);
}

int APS5_VABI sceNpCreateEventFlag(KernelEventFlag* flag, const char* name, std::uint32_t attr, std::uint64_t pattern) {
    return NpResult(sceKernelCreateEventFlag(flag, name, attr, pattern, nullptr));
}

int APS5_VABI sceNpDeleteEventFlag(KernelEventFlag flag) {
    return NpResult(sceKernelDeleteEventFlag(flag));
}

int APS5_VABI sceNpOpenEventFlag(KernelEventFlag* flag, const char* name) {
    return NpResult(sceKernelOpenEventFlag(flag, name));
}

int APS5_VABI sceNpCloseEventFlag(KernelEventFlag flag) {
    return NpResult(sceKernelCloseEventFlag(flag));
}

int APS5_VABI sceNpSetEventFlag(KernelEventFlag flag, std::uint64_t pattern) {
    return NpResult(sceKernelSetEventFlag(flag, pattern));
}

int APS5_VABI sceNpClearEventFlag(KernelEventFlag flag, std::uint64_t pattern) {
    return NpResult(sceKernelClearEventFlag(flag, pattern));
}

int APS5_VABI sceNpPollEventFlag(KernelEventFlag flag, std::uint64_t pattern, std::uint32_t mode, std::uint64_t* result) {
    return NpBusyResult(sceKernelPollEventFlag(flag, pattern, mode, result), NP_ERROR_EVENT_FLAG_BUSY);
}

int APS5_VABI sceNpCancelEventFlag(KernelEventFlag flag, std::uint64_t pattern, int* waiting) {
    return NpResult(sceKernelCancelEventFlag(flag, pattern, waiting));
}

int APS5_VABI sceNpCreateSema(KernelSema* sema, std::uint32_t flags, int initial, int maximum, const char* name) {
    const std::uint32_t attr = KERNEL_SEMA_ATTR_FIFO + ((flags & NP_SEMA_FLAG_PRIORITY) << KERNEL_SEMA_ATTR_PRIORITY_SHIFT);
    return NpResult(sceKernelCreateSema(sema, name, attr, initial, maximum, nullptr));
}

int APS5_VABI sceNpDeleteSema(KernelSema sema) {
    return NpResult(sceKernelDeleteSema(sema));
}

int APS5_VABI sceNpPollSema(KernelSema sema) {
    return NpBusyResult(sceKernelPollSema(sema, 1), NP_ERROR_SEMA_BUSY);
}

int APS5_VABI sceNpSignalSema(KernelSema sema) {
    return NpResult(sceKernelSignalSema(sema, 1));
}

int APS5_VABI sceNpCreateThread(Pthread* thread, PthreadEntry entry, void* arg, int priority, std::size_t stackSize, KernelCpumask affinity, const char* name) {
    PthreadAttr attr {};
    int result = scePthreadAttrInit(&attr);
    if (result < 0) return result;
    result = scePthreadAttrSetstacksize(&attr, stackSize);
    if (result < 0) {
        scePthreadAttrDestroy(&attr);

        return result;
    }
    if (priority != 0) {
        result = scePthreadAttrSetinheritsched(&attr, NP_THREAD_EXPLICIT_SCHED);
        if (result >= 0) result = scePthreadAttrSetschedpolicy(&attr, NP_THREAD_SCHED_POLICY);
        if (result >= 0) {
            KernelSchedParam param {};
            param.sched_priority = priority;
            result = scePthreadAttrSetschedparam(&attr, &param);
        }
    }
    if (result >= 0 && affinity != 0) result = scePthreadAttrSetaffinity(&attr, affinity);
    if (result >= 0) result = scePthreadCreate(thread, &attr, entry, arg, name);
    scePthreadAttrDestroy(&attr);

    return result;
}

int APS5_VABI sceNpThreadGetId(Pthread* thread) {
    if (thread == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    *thread = scePthreadSelf();
    return 0;
}

int APS5_VABI sceNpGetProcessId(void) {
    return getpid_nid_postfix();
}

[[noreturn]] void APS5_VABI sceNpPanic(void) {
    std::abort();
}

int APS5_VABI sceNpDbgDumpText(const char* text, int length) {
    return std::printf("%.*s", length, text);
}

int APS5_VABI sceNpIpc2ClientInit(void) {
    std::lock_guard lock(g_ipcClientMutex);
    if (g_ipcClientInitialized) return NP_ERROR_IPC_ALREADY_INITIALIZED;
    const int result = InitMemoryHeap(&g_ipcClientHeap, NP_IPC_CLIENT_HEAP_SIZE, NP_IPC_CLIENT_HEAP_NAME);
    if (result < 0) return result;
    g_ipcClientInitialized = true;
    return 0;
}

int APS5_VABI sceNpIpc2ClientTerm(void) {
    std::lock_guard lock(g_ipcClientMutex);
    if (!g_ipcClientInitialized) return NP_ERROR_IPC_NOT_INITIALIZED;
    DestroyMemoryHeap(&g_ipcClientHeap);
    g_ipcClientInitialized = false;
    return 0;
}

int APS5_VABI sceNpArchInit(void) {
    std::lock_guard lock(g_archMutex);
    if (g_archReferences != 0) {
        ++g_archReferences;
        return 0;
    }
    const int result = InitMemoryHeap(&g_globalHeap, NP_GLOBAL_HEAP_SIZE, NP_GLOBAL_HEAP_NAME);
    if (result < 0) return result;
    g_archReferences = 1;
    return 0;
}

void APS5_VABI sceNpArchTerm(void) {
    std::lock_guard lock(g_archMutex);
    if (g_archReferences == 0) return;
    if (--g_archReferences == 0) DestroyMemoryHeap(&g_globalHeap);
}

int APS5_VABI sceNpHeapInit(NpHeap* heap, void* base, std::size_t size, const char* name) {
    return InitHeap(heap, base, size, name);
}

void APS5_VABI sceNpHeapDestroy(NpHeap* heap) {
    DestroyHeap(heap);
}

void APS5_VABI sceNpHeapGetAllocator(NpHeap* heap, NpAllocator* allocator) {
    *allocator = {HeapAllocate, HeapReallocate, HeapRelease, heap};
}

int APS5_VABI sceNpHeapGetStat(const NpHeap* heap, NpHeapStat* stat) {
    MallocManagedSize stats {};
    stats.size = sizeof(stats);
    stats.version = 1;
    const int result = sceLibcMspaceMallocStats_nid_postfix(heap->mspace, &stats);
    if (result == 0) *stat = {stats.maxSystemSize, stats.maxInuseSize, stats.currentInuseSize};
    return result;
}

void APS5_VABI sceNpHeapShowStat(const NpHeap* heap) {
    NpHeapStat stat {};
    if (sceNpHeapGetStat(heap, &stat) != 0) {
        std::printf("[NpCommon] heap statistics unavailable\n");
        return;
    }
    std::printf("[NpCommon] heap: system %llu bytes, peak in use %llu bytes, in use %llu bytes\n",
        static_cast<unsigned long long>(stat.maxSystemSize), static_cast<unsigned long long>(stat.maxInuseSize),
        static_cast<unsigned long long>(stat.currentInuseSize));
}

int APS5_VABI sceNpMemoryHeapInit(NpMemoryHeap* heap, std::uint64_t size, const char* name) {
    return InitMemoryHeap(heap, size, name);
}

void APS5_VABI sceNpMemoryHeapDestroy(NpMemoryHeap* heap) {
    DestroyMemoryHeap(heap);
}

void APS5_VABI sceNpMemoryHeapGetAllocator(NpMemoryHeap* heap, NpAllocator* allocator) {
    *allocator = MemoryHeapAllocator(heap);
}

void APS5_VABI sceNpMemoryHeapGetAllocatorEx(NpMemoryHeap* heap, int outOfMemoryError, NpAllocatorEx* allocator) {
    *allocator = {MemoryHeapAllocator(heap), outOfMemoryError};
}

void APS5_VABI sceNpGlobalHeapGetAllocator(NpAllocator* allocator) {
    *allocator = MemoryHeapAllocator(&g_globalHeap);
}

void APS5_VABI sceNpGlobalHeapGetAllocatorEx(NpAllocatorEx* allocator) {
    *allocator = {MemoryHeapAllocator(&g_globalHeap), NP_ERROR_OUT_OF_MEMORY};
}

NpAllocator* APS5_VABI sceNpGlobalHeapGetAllocatorPtr(void) {
    g_globalAllocator = MemoryHeapAllocator(&g_globalHeap);
    return &g_globalAllocator;
}

NpAllocatorEx* APS5_VABI sceNpGlobalHeapGetAllocatorExPtr(void) {
    g_globalAllocatorEx = {MemoryHeapAllocator(&g_globalHeap), NP_ERROR_OUT_OF_MEMORY};
    return &g_globalAllocatorEx;
}

int APS5_VABI sceNpAllocateKernelMemoryNoAlignment(std::uint64_t size, const char* name, void** address, NpKernelMemory* memory) {
    if (memory == nullptr) return NP_ERROR_NULL_OBJECT;
    const int result = MapKernelMemory(memory, size, name);
    if (result >= 0 && address != nullptr) *address = memory->address;
    return result;
}

int APS5_VABI sceNpAllocateKernelMemoryWithAlignment(std::uint64_t size, const char* name, void** address, NpKernelMemory* memory) {
    return sceNpAllocateKernelMemoryNoAlignment(AlignKernelMemory(size), name, address, memory);
}

int APS5_VABI sceNpFreeKernelMemory(NpKernelMemory* memory) {
    if (memory == nullptr) return NP_ERROR_NULL_OBJECT;
    UnmapKernelMemory(memory);
    return 0;
}

void APS5_VABI sceNpGetSdkVersionUInt(std::uint32_t* version) {
    const std::uint32_t value = SystemVersion();
    if (version != nullptr) *version = value;
}

int APS5_VABI sceNpRtcConvertToPosixTime(const RtcTick* tick, std::int64_t* seconds) {
    if (tick->tick == 0) return NP_ERROR_INVALID_ARGUMENT;
    RtcDateTime time {};
    const int result = sceRtcSetTick(&time, tick);
    if (result < 0) return result;
    return NpResult(sceRtcGetTime_t(&time, seconds));
}

int APS5_VABI sceNpRtcFormatRFC3339(char* text, std::size_t size, const RtcTick* utc, int timeZoneMinutes) {
    if (size < NP_RFC3339_BUFFER_MIN) return NP_ERROR_BUFFER_TOO_SMALL;
    const int result = sceRtcFormatRFC3339(text, utc, timeZoneMinutes);
    if (result < 0) return result;
    text[size - 1] = 0;
    return 0;
}

int APS5_VABI sceNpUserGetUserIdList(int* users, std::size_t capacity, int* count) {
    if (capacity < NP_USER_ID_LIST_SIZE) return NP_ERROR_INVALID_ARGUMENT;
    int registered[NP_USER_ID_LIST_SIZE];
    std::fill(std::begin(registered), std::end(registered), USER_ID_INVALID);
    static_assert(sizeof(UserServiceRegisteredUserIdList) <= sizeof(registered));
    const int result = sceUserServiceGetRegisteredUserIdList(reinterpret_cast<UserServiceRegisteredUserIdList*>(registered));
    if (result < 0) return result;
    int found = 0;
    for (const int user : registered)
        if (user != USER_ID_INVALID) users[found++] = user;
    *count = found;
    return 0;
}

int APS5_VABI sceNpRtcParseRFC3339(RtcTick* utc, const char* text) {
    return sceRtcParseRFC3339(utc, text);
}

static std::uint64_t ClockUsec() {
    std::int64_t usec = 0;
    sceNpGetSystemClockUsec(&usec);

    return static_cast<std::uint64_t>(usec);
}

static void* APS5_VABI CalloutThread(void* arg) {
    auto* context = static_cast<NpCalloutContext*>(arg);
    sceNpMutexLock(&context->mutex);

    for (;;) {
        const std::uint64_t now = ClockUsec();
        NpCallout* head = context->pending;
        if (head != nullptr && head->deadline <= now) {
            context->pending = head->next;
            sceNpMutexUnlock(&context->mutex);

            head->handler(head->arg);
            sceNpMutexLock(&context->mutex);

            continue;
        }
        if (context->stopping > 0) break;
        const std::uint64_t wait = head != nullptr ? std::min(head->deadline - now, NP_CALLOUT_MAX_WAIT) : 0;
        sceNpCondTimedwait(&context->cond, &context->mutex, static_cast<std::uint32_t>(wait));

        if (context->stopping > 0) break;
    }
    sceNpMutexUnlock(&context->mutex);

    return nullptr;
}

int APS5_VABI sceNpCalloutInitCtx(NpCalloutContext* context, const char* name, std::size_t stackSize, int priority,
    KernelCpumask affinity) {
    if (context->initialized > 0) return NP_ERROR_ALREADY_INITIALIZED;
    context->stopping = 0;
    context->pending = nullptr;
    int result = sceNpMutexInit(&context->mutex, name, 1);
    if (result < 0) return result;
    result = sceNpCondInit(&context->cond, name, 0);
    if (result >= 0) {
        result = sceNpCreateThread(&context->thread, CalloutThread, context, priority, stackSize, affinity, name);
        if (result >= 0) {
            context->initialized = 1;
            return 0;
        }
        sceNpCondDestroy(&context->cond);

    }
    sceNpMutexDestroy(&context->mutex);

    return result;
}

void APS5_VABI sceNpCalloutTermCtx(NpCalloutContext* context) {
    if (context->initialized == 0) return;
    sceNpMutexLock(&context->mutex);

    context->stopping = 1;
    sceNpCondSignal(&context->cond);
    sceNpMutexUnlock(&context->mutex);
    sceNpJoinThread(context->thread, nullptr);
    sceNpCondDestroy(&context->cond);
    sceNpMutexDestroy(&context->mutex);

    context->initialized = 0;
}

int APS5_VABI sceNpCalloutStartOnCtx64(NpCalloutContext* context, NpCallout* callout, std::uint64_t usec,
    void (APS5_VABI *handler)(void* arg), void* arg) {
    if (context->initialized == 0) return NP_ERROR_NOT_INITIALIZED;
    const std::uint64_t deadline = ClockUsec() + usec;
    sceNpMutexLock(&context->mutex);

    for (NpCallout* entry = context->pending; entry != nullptr; entry = entry->next) {
        if (entry == callout) {
            sceNpMutexUnlock(&context->mutex);

            return NP_ERROR_ALREADY_QUEUED;
        }
    }
    *callout = {};
    callout->deadline = deadline;
    callout->handler = handler;
    callout->arg = arg;
    NpCallout** link = &context->pending;
    while (*link != nullptr && callout->deadline > (*link)->deadline) link = &(*link)->next;
    callout->next = *link;
    *link = callout;
    sceNpCondSignal(&context->cond);
    sceNpMutexUnlock(&context->mutex);

    return 0;
}

int APS5_VABI sceNpCalloutStartOnCtx(NpCalloutContext* context, NpCallout* callout, std::uint32_t usec,
    void (APS5_VABI *handler)(void* arg), void* arg) {
    return sceNpCalloutStartOnCtx64(context, callout, usec, handler, arg);
}

int APS5_VABI sceNpCalloutStopOnCtx(NpCalloutContext* context, NpCallout* callout, int* wasPending) {
    if (context->initialized == 0) return NP_ERROR_NOT_INITIALIZED;
    sceNpMutexLock(&context->mutex);

    int removed = 0;
    for (NpCallout** link = &context->pending; *link != nullptr; link = &(*link)->next) {
        if (*link != callout) continue;
        *link = callout->next;
        removed = 1;
        break;
    }
    if (wasPending != nullptr) *wasPending = removed;
    sceNpMutexUnlock(&context->mutex);

    return 0;
}

std::uint32_t APS5_VABI sceNpGetRandom(void) {
    std::lock_guard lock(g_randomMutex);
    return static_cast<std::uint32_t>(g_random());
}

int APS5_VABI sceNpUtilCmpAccountId(std::uint64_t left, std::uint64_t right) {
    return left != right ? 1 : 0;
}

int APS5_VABI sceNpCmpNpId(const void* npId1, const void* npId2) {
    if (!npId1 || !npId2) return NP_ERROR_NULL_ARGUMENT;
    const auto* left = static_cast<const std::uint8_t*>(npId1);
    const auto* right = static_cast<const std::uint8_t*>(npId2);
    if (left[NP_ID_TERMINATOR_FLAG_OFFSET] != 1 || right[NP_ID_TERMINATOR_FLAG_OFFSET] != 1) return NP_ERROR_INVALID_NP_ID;
    if (std::strncmp(static_cast<const char*>(npId1), static_cast<const char*>(npId2), NP_ONLINE_ID_MAX) != 0) return NP_ERROR_ONLINE_ID_MISMATCH;
    if (NpIdWord(left, NP_ID_OPT_OFFSET) != NpIdWord(right, NP_ID_OPT_OFFSET)) return NP_ERROR_ONLINE_ID_MISMATCH;
    if (NpIdWord(left, NP_ID_PLATFORM_OFFSET) == NpIdWord(right, NP_ID_PLATFORM_OFFSET)) return 0;
    if (left[NP_ID_PLATFORM_OFFSET] == 0 || right[NP_ID_PLATFORM_OFFSET] == 0) return 0;
    return NP_ERROR_ONLINE_ID_MISMATCH;
}

bool APS5_VABI sceNpIntIsOnlineIdString(const char* text) {
    return text != nullptr && IsOnlineIdText(text);
}

int APS5_VABI sceNpUtilGetNpLanguageCode(int language, char* code) {
    const NpLanguageCodes* codes = FindLanguageCodes(language);
    if (codes == nullptr || code == nullptr) return NP_ERROR_INVALID_ARGUMENT_UTIL;
    CopyText(code, codes->code, NP_LANGUAGE_CODE_SIZE);
    return 0;
}

int APS5_VABI sceNpUtilGetNpLanguageCode2(int language, char* tag) {
    const NpLanguageCodes* codes = FindLanguageCodes(language);
    if (codes == nullptr || tag == nullptr) return NP_ERROR_INVALID_ARGUMENT_UTIL;
    CopyText(tag, codes->tag, NP_LANGUAGE_CODE2_SIZE);
    return 0;
}

const char* APS5_VABI sceNpUtilGetNpLanguageCodeStr(int language) {
    const NpLanguageCodes* codes = FindLanguageCodes(language);
    return codes != nullptr ? codes->code : nullptr;
}

const char* APS5_VABI sceNpUtilGetNpLanguageCode2Str(int language) {
    const NpLanguageCodes* codes = FindLanguageCodes(language);
    return codes != nullptr ? codes->tag : nullptr;
}

std::uint32_t APS5_VABI sceNpDbgAssignDebugId(void) {
    std::uint32_t id = g_debugIds.fetch_add(1) + 1;
    if (id == 0) id = g_debugIds.fetch_add(1) + 1;
    return id;
}

int APS5_VABI sceNpUtilBuildTitleId(const NpTitleIdParts* parts, char* titleId, std::size_t size) {
    if (parts == nullptr || titleId == nullptr) return NP_ERROR_INVALID_ARGUMENT_UTIL;
    if (size < NP_TITLE_ID_SIZE) return NP_UTIL_ERROR_BUFFER_TOO_SMALL;
    if (parts->index > NP_TITLE_ID_INDEX_MAX) return NP_UTIL_ERROR_OUT_OF_RANGE;
    std::memcpy(titleId, parts->id, sizeof(parts->id));
    titleId[9] = '_';
    titleId[10] = static_cast<char>('0' + parts->index / 10);
    titleId[11] = static_cast<char>('0' + parts->index % 10);
    titleId[12] = '\0';
    return 0;
}

void APS5_VABI sceNpDbgDumpBinary(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t line = 0; line < size; line += NP_DUMP_LINE_BYTES) {
        const std::size_t count = std::min(NP_DUMP_LINE_BYTES, size - line);
        for (std::size_t i = 0; i < NP_DUMP_LINE_BYTES; ++i) {
            if (i < count) {
                std::printf("%02x", bytes[line + i]);
            } else {
                std::printf("  ");
            }
            if ((i & 1) != 0) std::putchar(' ');
        }
        std::putchar(' ');
        for (std::size_t i = 0; i < count; ++i) {
            const std::uint8_t c = bytes[line + i];
            std::putchar(c >= 0x20 && c <= 0x7E ? c : '.');
        }
        std::putchar('\n');
    }
    std::putchar('\n');
}

}

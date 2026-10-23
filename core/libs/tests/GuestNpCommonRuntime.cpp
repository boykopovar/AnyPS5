#include "SceTypes.hpp"

#include <atomic>
#include <chrono>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

extern "C" {
int APS5_VABI sceNpAtomicCas32(std::int32_t* value, std::int32_t expected, std::int32_t desired);
int APS5_VABI sceNpAtomicInc32(std::int32_t* value);
int APS5_VABI sceNpAtomicDec32(std::int32_t* value);
int APS5_VABI sceNpCreateSema(KernelSema* sema, std::uint32_t flags, int initial, int maximum, const char* name);
int APS5_VABI sceNpDeleteSema(KernelSema sema);
int APS5_VABI sceNpPollSema(KernelSema sema);
int APS5_VABI sceNpSignalSema(KernelSema sema);
int APS5_VABI sceNpCreateEventFlag(KernelEventFlag* flag, const char* name, std::uint32_t attr, std::uint64_t pattern);
int APS5_VABI sceNpDeleteEventFlag(KernelEventFlag flag);
int APS5_VABI sceNpSetEventFlag(KernelEventFlag flag, std::uint64_t pattern);
int APS5_VABI sceNpPollEventFlag(KernelEventFlag flag, std::uint64_t pattern, std::uint32_t mode, std::uint64_t* result);
int APS5_VABI sceNpCreateThread(Pthread* thread, PthreadEntry entry, void* arg, int priority, std::size_t stackSize, KernelCpumask affinity, const char* name);
int APS5_VABI sceNpJoinThread(Pthread thread, void** value);
int APS5_VABI sceNpThreadGetId(Pthread* thread);

struct NpHeap {
    void* mspace;
};

struct NpHeapStat {
    std::uint64_t maxSystemSize;
    std::uint64_t maxInuseSize;
    std::uint64_t currentInuseSize;
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

int APS5_VABI sceNpHeapInit(NpHeap* heap, void* base, std::size_t size, const char* name);
void APS5_VABI sceNpHeapDestroy(NpHeap* heap);
void APS5_VABI sceNpHeapGetAllocator(NpHeap* heap, NpAllocator* allocator);
int APS5_VABI sceNpHeapGetStat(const NpHeap* heap, NpHeapStat* stat);
int APS5_VABI sceNpMemoryHeapInit(NpMemoryHeap* heap, std::uint64_t size, const char* name);
void APS5_VABI sceNpMemoryHeapDestroy(NpMemoryHeap* heap);
void APS5_VABI sceNpMemoryHeapGetAllocatorEx(NpMemoryHeap* heap, int outOfMemoryError, NpAllocatorEx* allocator);
int APS5_VABI sceNpArchInit(void);
int APS5_VABI sceNpIpc2ClientInit(void);
int APS5_VABI sceNpIpc2ClientTerm(void);
void APS5_VABI sceNpArchTerm(void);
NpAllocatorEx* APS5_VABI sceNpGlobalHeapGetAllocatorExPtr(void);
int APS5_VABI sceNpAllocateKernelMemoryWithAlignment(std::uint64_t size, const char* name, void** address, NpKernelMemory* memory);
int APS5_VABI sceNpFreeKernelMemory(NpKernelMemory* memory);
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

int APS5_VABI sceNpCalloutInitCtx(NpCalloutContext* context, const char* name, std::size_t stackSize, int priority, KernelCpumask affinity);
void APS5_VABI sceNpCalloutTermCtx(NpCalloutContext* context);
int APS5_VABI sceNpCalloutStartOnCtx(NpCalloutContext* context, NpCallout* callout, std::uint32_t usec, void (APS5_VABI *handler)(void* arg), void* arg);
int APS5_VABI sceNpCalloutStopOnCtx(NpCalloutContext* context, NpCallout* callout, int* wasPending);
int APS5_VABI sceNpUtilCmpAccountId(std::uint64_t left, std::uint64_t right);
int APS5_VABI sceNpCmpNpId(const void* npId1, const void* npId2);
bool APS5_VABI sceNpIntIsOnlineIdString(const char* text);
int APS5_VABI sceNpUtilGetNpLanguageCode(int language, char* code);
int APS5_VABI sceNpUtilGetNpLanguageCode2(int language, char* tag);
const char* APS5_VABI sceNpUtilGetNpLanguageCodeStr(int language);
const char* APS5_VABI sceNpUtilGetNpLanguageCode2Str(int language);
std::uint32_t APS5_VABI sceNpDbgAssignDebugId(void);
int APS5_VABI sceNpUtilJsonEscape(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength);
int APS5_VABI sceNpUtilJsonUnescape(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength, std::size_t* dstLength);
int APS5_VABI sceNpUtilJsonGetOneChar(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength, std::size_t* dstLength, std::size_t* consumed);
int APS5_VABI sceNpUtilGetNthChar(std::size_t index, const char* text, const char** character);
int APS5_VABI sceNpUtilHttpUrlEncode(char* dst, std::size_t dstSize, const char* src, std::size_t srcLength, std::size_t* dstLength);
struct NpTitleIdParts {
    char id[9];
    char reserved;
    std::uint8_t index;
};

int APS5_VABI sceNpUtilBuildTitleId(const NpTitleIdParts* parts, char* titleId, std::size_t size);
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
}

namespace {

constexpr int NpIpcAlreadyInitialized = static_cast<int>(0x80559D00);
constexpr int NpIpcNotInitialized = static_cast<int>(0x80559D01);
constexpr int NpOutOfMemory = static_cast<int>(0x80550005);
constexpr int NpNullObject = static_cast<int>(0x80558005);
constexpr int NpNotInitialized = static_cast<int>(0x80558001);
constexpr int NpAlreadyInitialized = static_cast<int>(0x80558002);
constexpr int NpAlreadyQueued = static_cast<int>(0x80558006);
constexpr int NpUtilInvalid = static_cast<int>(0x80550601);
constexpr int NpUtilBufferTooSmall = static_cast<int>(0x80550602);
constexpr int NpUtilOutOfRange = static_cast<int>(0x8055060A);
constexpr int NpUtilInvalidCharacter = static_cast<int>(0x80550608);
constexpr int NpInvalidFormat = static_cast<int>(0x80558003);
constexpr int NpInvalidCharacter = static_cast<int>(0x80558004);
constexpr int NpBufferTooSmall = static_cast<int>(0x80558008);
constexpr int NpOutOfRange = static_cast<int>(0x8055800E);
constexpr int NpInvalidArgument = static_cast<int>(0x80558009);
constexpr int NpEventFlagBusy = static_cast<int>(0x8055800C);
constexpr int NpSemaBusy = static_cast<int>(0x8055800D);
constexpr std::uint32_t EventFlagWaitAnd = 1;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

void* APS5_VABI ReturnArgument(void* arg) {
    Pthread self {};
    Require(sceNpThreadGetId(&self) == 0 && self != nullptr, "a created thread can read its id");
    return arg;
}

void TestAtomics() {
    std::int32_t value = 5;
    Require(sceNpAtomicInc32(&value) == 5 && value == 6, "increment returns the previous value");
    Require(sceNpAtomicDec32(&value) == 6 && value == 5, "decrement returns the previous value");
    Require(sceNpAtomicCas32(&value, 4, 9) == 5 && value == 5, "a failed compare-and-swap returns the current value");
    Require(sceNpAtomicCas32(&value, 5, 9) == 5 && value == 9, "a successful compare-and-swap returns the previous value");
}

void TestSemaphoresAndFlags() {
    KernelSema sema {};
    Require(sceNpCreateSema(&sema, 0, 0, 2, "NpSema") == 0, "a semaphore is created");
    Require(sceNpPollSema(sema) == NpSemaBusy, "an empty semaphore reports the busy error");
    Require(sceNpSignalSema(sema) == 0 && sceNpPollSema(sema) == 0, "a signalled semaphore can be taken once");
    Require(sceNpDeleteSema(sema) == 0, "the semaphore is deleted");

    KernelEventFlag flag {};
    Require(sceNpCreateEventFlag(&flag, "NpFlag", 0, 0) == 0, "an event flag is created");
    std::uint64_t result = 0;
    Require(sceNpPollEventFlag(flag, 1, EventFlagWaitAnd, &result) == NpEventFlagBusy, "a clear event flag reports the busy error");
    Require(sceNpSetEventFlag(flag, 1) == 0 && sceNpPollEventFlag(flag, 1, EventFlagWaitAnd, &result) == 0 && result == 1, "a set bit is polled");
    Require(sceNpDeleteEventFlag(flag) == 0, "the event flag is deleted");
}

void TestThreads() {
    Require(sceNpThreadGetId(nullptr) == NpInvalidArgument, "a null thread id output is refused");
    Pthread thread {};
    int marker = 0;
    Require(sceNpCreateThread(&thread, ReturnArgument, &marker, 0, 0x10000, 0, "NpThread") == 0, "a thread is created");
    void* value = nullptr;
    Require(sceNpJoinThread(thread, &value) == 0 && value == &marker, "the thread is joined with its return value");
}

void UseAllocator(const NpAllocator& allocator) {
    auto* block = static_cast<std::uint8_t*>(allocator.allocate(64, allocator.context));
    Require(block != nullptr, "the allocator hands out memory");
    block[63] = 0x5A;
    block = static_cast<std::uint8_t*>(allocator.reallocate(block, 256, allocator.context));
    Require(block != nullptr && block[63] == 0x5A, "reallocation keeps the contents");
    allocator.release(block, allocator.context);
    allocator.release(nullptr, allocator.context);
}

void TestHeaps() {
    NpMemoryHeap memoryHeap {};
    Require(sceNpMemoryHeapInit(nullptr, 0x1000, "np-test") == NpNullObject, "a null memory heap is refused");
    Require(sceNpMemoryHeapInit(&memoryHeap, 0x1001, "np-test") == 0, "a memory heap is created");
    Require(memoryHeap.memory.size == 0x4000 && memoryHeap.memory.address != nullptr && memoryHeap.heap.mspace != nullptr,
        "the memory heap size is rounded up to 16 KiB");
    NpAllocatorEx allocatorEx {};
    sceNpMemoryHeapGetAllocatorEx(&memoryHeap, 0x1234, &allocatorEx);
    Require(allocatorEx.allocator.context == &memoryHeap && allocatorEx.outOfMemoryError == 0x1234,
        "the extended allocator carries the caller's error code");
    UseAllocator(allocatorEx.allocator);
    sceNpMemoryHeapDestroy(&memoryHeap);
    Require(memoryHeap.memory.address == nullptr && memoryHeap.heap.mspace == nullptr, "destroying clears the memory heap");

    NpKernelMemory memory {};
    void* address = nullptr;
    Require(sceNpAllocateKernelMemoryWithAlignment(0x10, "np-test", &address, nullptr) == NpNullObject,
        "kernel memory needs an object");
    Require(sceNpAllocateKernelMemoryWithAlignment(0x10, "np-test", &address, &memory) == 0
            && address == memory.address && memory.size == 0x4000,
        "kernel memory is mapped in 16 KiB units");
    NpHeap heap {};
    Require(sceNpHeapInit(&heap, memory.address, memory.size, "np-test") == 0 && heap.mspace != nullptr, "a heap is created");
    NpAllocator allocator {};
    sceNpHeapGetAllocator(&heap, &allocator);
    Require(allocator.context == &heap, "the heap allocator uses the heap as its context");
    UseAllocator(allocator);
    NpHeapStat stat {};
    Require(sceNpHeapGetStat(&heap, &stat) == 0 && stat.maxInuseSize >= stat.currentInuseSize, "heap statistics are reported");
    sceNpHeapDestroy(&heap);
    Require(heap.mspace == nullptr, "destroying clears the heap");
    Require(sceNpFreeKernelMemory(&memory) == 0 && memory.address == nullptr, "kernel memory is released");
    Require(sceNpFreeKernelMemory(nullptr) == NpNullObject, "freeing needs an object");

    Require(sceNpArchInit() == 0 && sceNpArchInit() == 0, "the library initialises more than once");
    NpAllocatorEx* global = sceNpGlobalHeapGetAllocatorExPtr();
    Require(global->outOfMemoryError == NpOutOfMemory, "the global allocator reports the library's out-of-memory error");
    UseAllocator(global->allocator);
    sceNpArchTerm();
    UseAllocator(sceNpGlobalHeapGetAllocatorExPtr()->allocator);
    sceNpArchTerm();

    Require(sceNpIpc2ClientTerm() == NpIpcNotInitialized, "the ipc client must be started before it stops");
    Require(sceNpIpc2ClientInit() == 0 && sceNpIpc2ClientInit() == NpIpcAlreadyInitialized, "the ipc client starts once");
    Require(sceNpIpc2ClientTerm() == 0 && sceNpIpc2ClientInit() == 0 && sceNpIpc2ClientTerm() == 0, "the ipc client restarts");
}

struct CalloutRecord {
    std::atomic<int> order {0};
    std::atomic<int> first {0};
    std::atomic<int> second {0};
};

CalloutRecord g_callouts;

void APS5_VABI RecordFirst(void*) {
    g_callouts.first = ++g_callouts.order;
}

void APS5_VABI RecordSecond(void*) {
    g_callouts.second = ++g_callouts.order;
}

void APS5_VABI RecordUnexpected(void*) {
    Require(false, "a stopped callout does not run");
}

void TestCallouts() {
    NpCalloutContext context {};
    NpCallout callout {};
    Require(sceNpCalloutStartOnCtx(&context, &callout, 0, RecordFirst, nullptr) == NpNotInitialized, "a context must be initialised first");
    Require(sceNpCalloutInitCtx(&context, "np-callout", 0x4000, 0, 0) == 0, "a callout context starts");
    Require(sceNpCalloutInitCtx(&context, "np-callout", 0x4000, 0, 0) == NpAlreadyInitialized, "a context starts once");

    NpCallout stopped {};
    int wasPending = 0;
    Require(sceNpCalloutStartOnCtx(&context, &stopped, 60000000, RecordUnexpected, nullptr) == 0, "a distant callout is queued");
    Require(sceNpCalloutStartOnCtx(&context, &stopped, 60000000, RecordUnexpected, nullptr) == NpAlreadyQueued,
        "a queued callout cannot be queued again");
    Require(sceNpCalloutStopOnCtx(&context, &stopped, &wasPending) == 0 && wasPending == 1, "a queued callout is removed");
    Require(sceNpCalloutStopOnCtx(&context, &stopped, &wasPending) == 0 && wasPending == 0, "removing twice reports nothing pending");

    NpCallout queuedFirst {};
    NpCallout queuedSecond {};
    Require(sceNpCalloutStartOnCtx(&context, &queuedFirst, 30000000, RecordUnexpected, nullptr) == 0, "an earlier distant callout is queued");
    Require(sceNpCalloutStartOnCtx(&context, &queuedSecond, 60000000, RecordUnexpected, nullptr) == 0, "a later distant callout is queued");
    const std::uint64_t queuedDeadline = queuedFirst.deadline;
    Require(sceNpCalloutStartOnCtx(&context, &queuedFirst, 0, RecordFirst, &context) == NpAlreadyQueued,
        "the earlier callout cannot be queued again");
    Require(queuedFirst.next == &queuedSecond && queuedFirst.deadline == queuedDeadline && queuedFirst.handler == RecordUnexpected &&
        queuedFirst.arg == nullptr, "a rejected start leaves the queued callout unchanged");
    Require(sceNpCalloutStopOnCtx(&context, &queuedSecond, &wasPending) == 0 && wasPending == 1,
        "the later callout is still pending after the rejected start");
    Require(sceNpCalloutStopOnCtx(&context, &queuedFirst, &wasPending) == 0 && wasPending == 1, "the earlier callout is still pending");

    NpCallout later {};
    Require(sceNpCalloutStartOnCtx(&context, &later, 20000, RecordSecond, nullptr) == 0, "a later callout is queued");
    Require(sceNpCalloutStartOnCtx(&context, &callout, 1000, RecordFirst, nullptr) == 0, "an earlier callout is queued");
    for (int i = 0; i < 2000 && g_callouts.second == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(g_callouts.first == 1 && g_callouts.second == 2, "callouts run in deadline order");
    sceNpCalloutTermCtx(&context);
    Require(context.initialized == 0, "terminating clears the context");
    sceNpCalloutTermCtx(&context);
}

bool Equals(const char* actual, const char* expected) {
    return std::strcmp(actual, expected) == 0;
}

void TestIds() {
    Require(sceNpUtilCmpAccountId(7, 7) == 0 && sceNpUtilCmpAccountId(7, 8) == 1, "account ids compare for equality");

    constexpr int NullArgument = static_cast<int>(0x80550003);
    constexpr int InvalidNpId = static_cast<int>(0x80550605);
    constexpr int NotMatch = static_cast<int>(0x80550609);
    auto makeNpId = [](const char* name, std::uint32_t opt, std::uint32_t platform) {
        std::array<std::uint8_t, 0x24> id{};
        std::memcpy(id.data(), name, std::strlen(name));
        std::memcpy(id.data() + 0x14, &opt, sizeof(opt));
        std::memcpy(id.data() + 0x18, &platform, sizeof(platform));
        id[0x1C] = 1;
        return id;
    };
    const auto alice = makeNpId("Alice_01", 5, 0x11223344);
    auto other = alice;
    Require(sceNpCmpNpId(nullptr, alice.data()) == NullArgument && sceNpCmpNpId(alice.data(), nullptr) == NullArgument, "null NpIds are refused");
    Require(sceNpCmpNpId(alice.data(), other.data()) == 0, "identical NpIds match");
    other[0x1C] = 0;
    Require(sceNpCmpNpId(alice.data(), other.data()) == InvalidNpId && sceNpCmpNpId(other.data(), alice.data()) == InvalidNpId, "the flag byte at 0x1C must be 1");
    Require(sceNpCmpNpId(makeNpId("Bob-2", 5, 0x11223344).data(), alice.data()) == NotMatch, "different online ids do not match");
    Require(sceNpCmpNpId(makeNpId("Alice_01", 6, 0x11223344).data(), alice.data()) == NotMatch, "different words at 0x14 do not match");
    Require(sceNpCmpNpId(makeNpId("Alice_01", 5, 0).data(), alice.data()) == 0, "an empty first byte at 0x18 on the first id matches");
    Require(sceNpCmpNpId(alice.data(), makeNpId("Alice_01", 5, 0).data()) == 0, "an empty first byte at 0x18 on the second id matches");
    Require(sceNpCmpNpId(alice.data(), makeNpId("Alice_01", 5, 0x11223355).data()) == NotMatch, "different words at 0x18 do not match");

    Require(sceNpIntIsOnlineIdString("Alice_01") && sceNpIntIsOnlineIdString("Bob-2"), "letters, digits, '_' and '-' are allowed");
    Require(!sceNpIntIsOnlineIdString("ab") && !sceNpIntIsOnlineIdString("abcdefghijklmnopq"), "online ids are 3 to 16 characters");
    Require(!sceNpIntIsOnlineIdString("1abc") && !sceNpIntIsOnlineIdString("ab.c") && !sceNpIntIsOnlineIdString(nullptr),
        "online ids start with a letter and use no other symbols");
}

void TestJsonText() {
    char escaped[64] = {};
    Require(sceNpUtilJsonEscape(escaped, sizeof(escaped), "a\"b/\n\x01", 7) == 0 && Equals(escaped, "a\\\"b\\/\\n\\u0001"),
        "json escaping covers quotes, solidus, newlines and controls");
    std::strcpy(escaped, "x");
    Require(sceNpUtilJsonEscape(escaped, sizeof(escaped), "yz", 2) == 0 && Equals(escaped, "xyz"), "escaping appends to the buffer");
    Require(sceNpUtilJsonEscape(escaped, 6, "a", 1) == NpUtilBufferTooSmall, "escaping needs room for one escape");
    Require(sceNpUtilJsonEscape(escaped, sizeof(escaped), nullptr, 1) == NpUtilInvalid, "escaping needs a source");

    char text[32] = {};
    std::size_t length = 0;
    Require(sceNpUtilJsonUnescape(text, sizeof(text), "a\\nb\\u0041", 10, &length) == 0 && Equals(text, "a\nbA") && length == 5,
        "json unescaping decodes escapes and counts the terminator");
    Require(sceNpUtilJsonUnescape(text, sizeof(text), "\\uD83D\\uDE00", 12, &length) == 0 && Equals(text, "\xF0\x9F\x98\x80")
            && length == 5,
        "surrogate pairs become one UTF-8 character");
    Require(sceNpUtilJsonUnescape(text, sizeof(text), "\\u00G1", 6, &length) == NpUtilBufferTooSmall && Equals(text, "\\u00G1"),
        "a bad unicode escape is copied and reported");
    Require(sceNpUtilJsonUnescape(nullptr, sizeof(text), "abc", 3, &length) == 0 && length == 4, "a null buffer reports the size");
    Require(sceNpUtilJsonUnescape(text, 3, "abc", 3, &length) == NpUtilBufferTooSmall, "unescaping needs room for the terminator");

    char one[4] = {};
    std::size_t consumed = 0;
    Require(sceNpUtilJsonGetOneChar(one, sizeof(one), "\\u00e9x", 7, &length, &consumed) == 0 && length == 2 && consumed == 6
            && std::memcmp(one, "\xC3\xA9", 2) == 0,
        "one escaped character is decoded");
    Require(sceNpUtilJsonGetOneChar(one, sizeof(one), "\xE6\x97\xA5", 3, &length, &consumed) == 0 && length == 3 && consumed == 3,
        "one UTF-8 character is copied");
    Require(sceNpUtilJsonGetOneChar(one, sizeof(one), "\xE6\x97", 2, &length, &consumed) == NpUtilBufferTooSmall && length == 0
            && consumed == 3,
        "a cut UTF-8 character reports its full length");
    Require(sceNpUtilJsonGetOneChar(one, 1, "\xC3\xA9", 2, &length, &consumed) == NpUtilBufferTooSmall && length == 2,
        "a short buffer still reports the decoded size");

    const char* character = nullptr;
    const char* japanese = "\xE6\x97\xA5" "a";
    Require(sceNpUtilGetNthChar(1, japanese, &character) == 0 && character == japanese + 3, "characters are counted, not bytes");
    Require(sceNpUtilGetNthChar(10, "abc", &character) == 0 && *character == '\0', "counting stops at the end");
    Require(sceNpUtilGetNthChar(2, "a\x01b", &character) == NpUtilInvalidCharacter, "control characters are refused");

    char url[32] = {};
    Require(sceNpUtilHttpUrlEncode(url, sizeof(url), "a b=c*", 6, &length) == 0 && Equals(url, "a+b%3Dc*") && length == 9,
        "form encoding keeps the safe set and escapes the rest");
    Require(sceNpUtilHttpUrlEncode(nullptr, 0, "@", 1, &length) == 0 && length == 4, "a null buffer reports the encoded size");
    Require(sceNpUtilHttpUrlEncode(url, 3, "@", 1, &length) == NpUtilBufferTooSmall, "encoding needs room for the terminator");
}

void TestTitleIds() {
    NpTitleIdParts parts {{'A', 'B', 'C', 'D', '1', '2', '3', '4', '5'}, 0, 7};
    char titleId[13] = {};
    Require(sceNpUtilBuildTitleId(&parts, titleId, sizeof(titleId)) == 0 && Equals(titleId, "ABCD12345_07"), "the index is two digits");
    Require(sceNpUtilBuildTitleId(&parts, titleId, 12) == NpUtilBufferTooSmall, "a title id needs 13 bytes");
    parts.index = 100;
    Require(sceNpUtilBuildTitleId(&parts, titleId, sizeof(titleId)) == NpUtilOutOfRange, "the index is at most 99");
}

void TestLanguagesAndDebugIds() {
    char code[6] = {};
    char tag[36] = {};
    Require(sceNpUtilGetNpLanguageCode(1, code) == 0 && Equals(code, "en"), "language 1 is English");
    Require(sceNpUtilGetNpLanguageCode2(20, tag) == 0 && Equals(tag, "es-419"), "language 20 is Latin American Spanish");
    Require(Equals(sceNpUtilGetNpLanguageCodeStr(11), "zh-CN") && Equals(sceNpUtilGetNpLanguageCode2Str(30), "uk-UA"),
        "language strings are returned directly");
    Require(sceNpUtilGetNpLanguageCode(31, code) == NpUtilInvalid && sceNpUtilGetNpLanguageCodeStr(31) == nullptr,
        "unknown languages are refused");
    Require(sceNpUtilGetNpLanguageCode2(0, nullptr) == NpUtilInvalid, "a language code needs a buffer");
    const std::uint32_t first = sceNpDbgAssignDebugId();
    Require(first != 0 && sceNpDbgAssignDebugId() == first + 1, "debug ids count up");
}

void TestBase64() {
    const std::uint8_t payload[] = {1, 2, 3, 4, 5};
    char text[16] = {};
    Require(sceNpBase64Encoder(payload, text, 5) == 8 && Equals(text, "AQIDBAU="), "base64 pads the last group");
    Require(sceNpBase64Encoder(payload, text, 0) == 0, "an empty payload encodes to nothing");
    Require(sceNpBase64GetDecodeSize("AQIDBAU=", 8) == 5 && sceNpBase64GetDecodeSize("AQ==", 4) == 1,
        "the decoded size follows the padding");
    Require(sceNpBase64GetDecodeSize("", 0) == NpInvalidFormat && sceNpBase64GetDecodeSize("AQI", 3) == NpInvalidFormat,
        "base64 needs whole groups");
    Require(sceNpBase64GetDecodeSize("AQ=A", 4) == NpInvalidCharacter && sceNpBase64GetDecodeSize("A=AAAQID", 8) == NpInvalidCharacter,
        "padding is only allowed at the end");
    std::uint8_t bytes[8] = {};
    Require(sceNpBase64Decoder("AQIDBAU=", 8, bytes, sizeof(bytes)) == 5 && std::memcmp(bytes, payload, 5) == 0,
        "base64 decodes back to the payload");
    Require(sceNpBase64Decoder("AQID", 4, bytes, 2) == NpInvalidFormat, "decoding refuses a short buffer");
    Require(sceNpBase64Decoder("AQ?D", 4, bytes, 3) == NpInvalidCharacter, "decoding refuses characters outside the alphabet");

    const std::uint8_t url[] = {0xFB, 0x00};
    std::memset(text, 0, sizeof(text));
    Require(sceNpBase64UrlEncoder(url, text, 1) == 2 && Equals(text, "-w"), "base64url leaves out the padding");
    Require(sceNpBase64UrlEncoder(url, text, 2) == 3 && Equals(text, "-wA"), "base64url writes three characters for two bytes");
    Require(sceNpBase64UrlGetDecodeSize("-wA", 3) == 2 && sceNpBase64UrlGetDecodeSize("-w==", 4) == 1,
        "base64url accepts padded and unpadded tails");
    Require(sceNpBase64UrlGetDecodeSize("", 0) == NpInvalidCharacter && sceNpBase64UrlGetDecodeSize("AAAAA", 5) == NpInvalidCharacter,
        "a single trailing character cannot be decoded");
    Require(sceNpBase64UrlGetDecodeSize("+w==", 4) == NpInvalidCharacter, "base64url refuses the standard alphabet");
    Require(sceNpBase64UrlDecoder("-wA", 3, bytes, 2) == 2 && bytes[0] == 0xFB && bytes[1] == 0, "base64url decodes a short tail");
    Require(sceNpBase64UrlDecoder("-w", 2, bytes, 0) == NpInvalidFormat, "base64url decoding refuses a short buffer");
}

void TestNumbers() {
    Require(sceNpHexToInt('a') == 10 && sceNpHexToInt('F') == 15 && sceNpHexToInt('g') == -1, "hex digits are read");
    Require(sceNpIntToHex(10, false) == 'a' && sceNpIntToHex(15, true) == 'F' && sceNpIntToHex(9, true) == '9', "hex digits are written");

    char text[32] = {};
    std::size_t length = 0;
    Require(sceNpUInt32ToStr(text, sizeof(text), 255, 16, 3, &length) == 0 && Equals(text, "0XFF") && length == 4,
        "the hex prefix follows the case flag");
    Require(sceNpUInt32ToStr(text, sizeof(text), 8, 8, 2, &length) == 0 && Equals(text, "010") && length == 3, "octal gets a zero prefix");
    Require(sceNpUInt32ToStr(text, sizeof(text), 0, 10, 4, &length) == 0 && Equals(text, "0") && length == 1, "zero has no sign");
    Require(sceNpUInt32ToStr(text, 4, 1234, 10, 0, &length) == NpBufferTooSmall, "the terminator must fit");
    Require(sceNpUInt32ToStr(text, sizeof(text), 1, 37, 0, &length) == NpInvalidArgument, "the radix is limited to 36");
    Require(sceNpUInt32ToStr(nullptr, 0, 42, 10, 4, &length) == 0 && length == 3, "a null buffer reports the length");
    Require(sceNpInt32ToStr(text, sizeof(text), -2147483647 - 1, 10, 0, &length) == 0 && Equals(text, "-2147483648"),
        "a negative 32-bit value always has a sign");
    Require(sceNpInt32ToStr(text, 3, -42, 10, 0, &length) == NpBufferTooSmall, "the sign takes buffer space");
    Require(sceNpInt64ToStr(text, sizeof(text), -42, 10, 0, &length) == 0 && Equals(text, "42") && length == 2,
        "a 64-bit value only gets a sign with the sign flag");
    Require(sceNpInt64ToStr(text, sizeof(text), -42, 10, 4, &length) == 0 && Equals(text, "-42"), "the sign flag writes the minus");
    Require(sceNpUInt64ToStr(text, sizeof(text), UINT64_MAX, 16, 3, &length) == 0 && Equals(text, "0XFFFFFFFFFFFFFFFF") && length == 18,
        "the full 64-bit range is formatted");

    std::int32_t value32 = 0;
    std::size_t parsed = 0;
    Require(sceNpStrToInt32("-567abc", &value32, 10, &parsed) == 0 && value32 == -567 && parsed == 4, "parsing stops at the first non-digit");
    Require(sceNpStrToInt32("0x1A", &value32, 0, &parsed) == 0 && value32 == 26, "base zero follows the prefix");
    Require(sceNpStrToInt32("invalid", &value32, 10, &parsed) == NpOutOfRange, "no digits is an error");
    parsed = 0;
    Require(sceNpStrToInt32("3000000000", &value32, 10, &parsed) == NpOutOfRange && parsed == 10,
        "the length is reported before the 32-bit range check");
    std::int64_t value64 = 0;
    Require(sceNpStrToInt64("-9223372036854775808", &value64, 10, nullptr) == 0 && value64 == INT64_MIN, "the 64-bit minimum parses");
    Require(sceNpStrToInt64("99999999999999999999", &value64, 10, nullptr) == NpOutOfRange, "64-bit overflow is an error");
    std::uint32_t unsigned32 = 0;
    Require(sceNpStrToUInt32("4294967295", &unsigned32, 10, nullptr) == 0 && unsigned32 == UINT32_MAX, "the 32-bit maximum parses");
    Require(sceNpStrToUInt32("4294967296", &unsigned32, 10, nullptr) == NpOutOfRange, "32-bit unsigned overflow is an error");
    std::uint64_t unsigned64 = 0;
    Require(sceNpStrToUInt64("0xFFFFFFFFFFFFFFFF", &unsigned64, 0, nullptr) == 0 && unsigned64 == UINT64_MAX, "the 64-bit maximum parses");
    Require(sceNpStrToUInt64("18446744073709551616", &unsigned64, 10, nullptr) == NpOutOfRange, "64-bit unsigned overflow is an error");
}

void TestHexAndCopies() {
    const std::uint8_t data[] = {0x0A, 0xFE};
    char text[8] = {};
    Require(sceNpStrBuildHex(text, 5, data, 2, false) == 0 && Equals(text, "0afe"), "bytes are written as lowercase hex");
    Require(sceNpStrBuildHex(text, 5, data, 2, true) == 0 && Equals(text, "0AFE"), "bytes are written as uppercase hex");
    Require(sceNpStrBuildHex(text, 4, data, 2, false) == NpBufferTooSmall, "hex output needs room for the terminator");
    std::uint8_t bytes[4] = {};
    Require(sceNpStrParseHex("0aFE", bytes, 2) == 0 && bytes[0] == 0x0A && bytes[1] == 0xFE, "hex is read in either case");
    Require(sceNpStrParseHex("0AF", bytes, 2) == NpInvalidArgument, "odd hex is refused");
    Require(sceNpStrParseHex("0AFE", bytes, 1) == NpBufferTooSmall, "hex parsing checks the output size");
    Require(sceNpStrParseHex("0AG1", bytes, 2) == NpInvalidArgument, "non-hex characters are refused");
    Require(sceNpStrnParseHex("DEADBEEF1234", 8, bytes, 4) == 0 && bytes[0] == 0xDE && bytes[3] == 0xEF, "a hex prefix is read");

    char copy[8] = {};
    Require(sceNpStrcpyToBuf(copy, 6, "Hello") == 0 && Equals(copy, "Hello"), "a string that fits is copied");
    Require(sceNpStrcpyToBuf(copy, 5, "Hello") == NpBufferTooSmall, "a string without room for its terminator is refused");
    Require(sceNpStrncpyToBuf(copy, 6, "Hello World", 5) == 0 && Equals(copy, "Hello"), "the length limit sizes the copy");
    Require(sceNpStrncpyToBuf(copy, 5, "Hello World", 5) == NpBufferTooSmall, "the limited string still needs a terminator");
}

}

int main() {
    TestAtomics();
    TestSemaphoresAndFlags();
    TestThreads();
    TestHeaps();
    TestBase64();
    TestNumbers();
    TestHexAndCopies();
    TestCallouts();
    TestIds();
    TestLanguagesAndDebugIds();
    TestJsonText();
    TestTitleIds();
    std::puts("guest np common runtime: PASS");
}

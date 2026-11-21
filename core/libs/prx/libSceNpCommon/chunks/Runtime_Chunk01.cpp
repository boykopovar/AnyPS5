extern "C" {
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNpCommon/include/Api.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libSceRtc/include/Rtc.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <iterator>
#include <mutex>
#include <random>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
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
constexpr int RTC_ERROR_INVALID_POINTER = static_cast<int>(0x80B50002);
constexpr std::size_t NP_ID_OPT_OFFSET = 0x14;
constexpr std::size_t NP_ID_PLATFORM_OFFSET = 0x18;
constexpr std::size_t NP_ID_TERMINATOR_FLAG_OFFSET = 0x1C;

bool GuestTextLength(const char* text, std::size_t maximum, std::size_t& length) {
    char chunk[4096];
    for (std::size_t offset = 0; offset < maximum;) {
        const auto address = reinterpret_cast<std::uintptr_t>(text) + offset;
        const std::size_t count = std::min(sizeof(chunk) - address % sizeof(chunk), maximum - offset);
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(chunk, text + offset, count);
        errno = savedErrno;
        if (!copied) return false;
        if (const void* end = std::memchr(chunk, 0, count)) {
            length = offset + static_cast<std::size_t>(static_cast<const char*>(end) - chunk);
            return true;
        }
        offset += count;
    }
    length = maximum;
    return true;
}

char* DuplicateGuestText(NpAllocatorEx* allocator, const char* text, std::size_t length) {
    auto* copy = static_cast<char*>(_sceNpAllocatorExMalloc(allocator, length + 1));
    if (copy == nullptr) return nullptr;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(copy, text, length);
    errno = savedErrno;
    if (!copied) {
        _sceNpAllocatorExFree(allocator, copy);
        return nullptr;
    }
    copy[length] = '\0';
    return copy;
}

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

int ReadClock(RtcTick* time, int result) {
    if (result >= 0) return 0;
    time->tick = 0;
    return result;
}

void RequireTimeAdd(int result, const char* function) {
    if (result < 0) throw std::invalid_argument(std::string(function) + ": the tick is out of range");
}

constexpr int NP_ERROR_TITLE_ID_INVALID = static_cast<int>(0x80559E11);
constexpr int NP_ERROR_TITLE_ID_BUFFER_TOO_SMALL = static_cast<int>(0x80559E0D);
constexpr std::size_t NP_TITLE_ID_CODE_LENGTH = 9;
constexpr std::size_t NP_TITLE_ID_LENGTH = 12;

void* const NpRefObjectVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np9RefObjectD1Ev), reinterpret_cast<void*>(&_ZN3sce2np9RefObjectD0Ev),
};

void* const NpTitleIdVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np9NpTitleIdD1Ev), reinterpret_cast<void*>(&_ZN3sce2np9NpTitleIdD0Ev),
};

bool SameTitleId(const char* lhs, const char* rhs) {
    return std::strncmp(lhs, rhs, NP_TITLE_ID_LENGTH) == 0;
}

bool IsTitleIdLetter(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool IsTitleIdDigit(char c) {
    return c >= '0' && c <= '9';
}

constexpr std::size_t NP_TITLE_SECRET_SIZE = sizeof(NpTitleSecret);

void* const NpTitleSecretVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np13NpTitleSecretD1Ev), reinterpret_cast<void*>(&_ZN3sce2np13NpTitleSecretD0Ev),
};

bool SameTitleSecret(const NpTitleSecret* lhs, const void* rhs) {
    return std::memcmp(lhs->data, rhs, NP_TITLE_SECRET_SIZE) == 0;
}

int OnlyFailure(int result) {
    return result < 0 ? result : 0;
}

int StoreTitleId(NpTitleIdObject* out, const char* text, std::size_t length, std::size_t expected) {
    if (length != expected) return NP_ERROR_TITLE_ID_INVALID;
    if (!std::all_of(text, text + 4, IsTitleIdLetter) || !std::all_of(text + 4, text + NP_TITLE_ID_CODE_LENGTH, IsTitleIdDigit))
        return NP_ERROR_TITLE_ID_INVALID;
    if (expected == NP_TITLE_ID_LENGTH && (text[9] != '_' || !IsTitleIdDigit(text[10]) || !IsTitleIdDigit(text[11])))
        return NP_ERROR_TITLE_ID_INVALID;
    out->titleId = {};
    std::memcpy(out->titleId.data, text, NP_TITLE_ID_CODE_LENGTH);
    std::memcpy(out->titleId.data + NP_TITLE_ID_CODE_LENGTH, "_00", 3);
    return 0;
}

void* const NpCommIdVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np8NpCommIdD1Ev), reinterpret_cast<void*>(&_ZN3sce2np8NpCommIdD0Ev),
};

bool IsCommIdText(const char* text) {
    return std::all_of(text, text + 4, IsTitleIdLetter) && std::all_of(text + 4, text + NP_TITLE_ID_CODE_LENGTH, IsTitleIdDigit) &&
        text[9] == '_' && IsTitleIdDigit(text[10]) && IsTitleIdDigit(text[11]);
}

bool SameCommId(const NpCommunicationId& lhs, const NpCommunicationId& rhs) {
    return std::strncmp(lhs.data, rhs.data, sizeof(lhs.data)) == 0 && lhs.num == rhs.num;
}
constexpr std::size_t NP_SDK_VERSION_SIZE = 8;
constexpr std::size_t NP_RFC3339_BUFFER_MIN = 0x80;
constexpr std::size_t NP_USER_ID_LIST_SIZE = 20;
constexpr int USER_ID_INVALID = -1;
constexpr KernelClockid NP_SYSTEM_CLOCK = 4;

static_assert(sizeof(SceKernelSystemSwVersion) == 0x28);

template <typename T>
void WriteOptionalGuest(T* output, T value) {
    if (output == nullptr) return;
    const int savedErrno = errno;
    GuestCopyOut_nid_no_patch(output, &value, sizeof(value));
    errno = savedErrno;
}

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

constexpr std::size_t NP_OBJECT_HEADER_SIZE = 16;

constexpr int NP_ERROR_OBJECT_ALREADY_INITIALIZED = static_cast<int>(0x80559E03);
constexpr std::int32_t NP_SEMAPHORE_CREATED = 1;
constexpr std::int32_t NP_SEMAPHORE_OPENED = 2;

void* const NpSemaphoreVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np9SemaphoreD1Ev), reinterpret_cast<void*>(&_ZN3sce2np9SemaphoreD0Ev),
};

void RequireSemaphoreInit(const NpSemaphoreObject* self, const char* message) {
    if (self->state == 0) throw std::invalid_argument(message);
}

void ReleaseSemaphore(NpSemaphoreObject* self) {
    if (self->state == NP_SEMAPHORE_OPENED) NotImplemented_nid_no_patch("sce::np::Semaphore: closing an opened semaphore (sceKernelCloseSema)");
    else if (self->state == NP_SEMAPHORE_CREATED) sceNpDeleteSema(self->sema);
}


void* const NpMutexVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np5MutexD1Ev), reinterpret_cast<void*>(&_ZN3sce2np5MutexD0Ev),
};

void* const NpCondVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np4CondD1Ev), reinterpret_cast<void*>(&_ZN3sce2np4CondD0Ev),
};

void RequireMutexInit(const NpMutexObject* self, const char* function) {
    if (!self->isInit) throw std::invalid_argument(std::string(function) + ": the mutex is not initialized");
}

void RequireMutexResult(int result, const char* function) {
    if (result != 0) throw std::runtime_error(std::string(function) + ": the kernel mutex call failed");
}

constexpr int NP_ERROR_STREAM_BUFFER_TOO_SMALL = static_cast<int>(0x80559E0D);
constexpr std::size_t NP_MEMORY_STREAM_SIZE = 0x20;

void* const NpMemoryStreamReaderVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np18MemoryStreamReaderD1Ev), reinterpret_cast<void*>(&_ZN3sce2np18MemoryStreamReaderD0Ev),
    reinterpret_cast<void*>(&_ZN3sce2np18MemoryStreamReader4ReadEPNS0_6HandleEPvmPm),
};

void* const NpMemoryStreamWriterVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np18MemoryStreamWriterD1Ev), reinterpret_cast<void*>(&_ZN3sce2np18MemoryStreamWriterD0Ev),
    reinterpret_cast<void*>(&_ZN3sce2np18MemoryStreamWriter5WriteEPNS0_6HandleEPKvmPm),
};
constexpr int NP_ERROR_STREAM_SHORT = static_cast<int>(0x80559E10);
constexpr std::size_t NP_STREAM_CHUNK = 0x100;

int StreamRead(NpStreamReader* self, void* handle, void* out, std::size_t size, std::size_t* read) {
    using Read = int(APS5_VABI*)(NpStreamReader*, void*, void*, std::size_t, std::size_t*);
    return reinterpret_cast<Read>(const_cast<void*>(self->vtable[2]))(self, handle, out, size, read);
}

int StreamWrite(NpStreamWriter* self, void* handle, const void* data, std::size_t size, std::size_t* written) {
    using Write = int(APS5_VABI*)(NpStreamWriter*, void*, const void*, std::size_t, std::size_t*);
    return reinterpret_cast<Write>(const_cast<void*>(self->vtable[2]))(self, handle, data, size, written);
}

int StreamNotify(NpStreamCtx* ctx, const void* data, std::size_t size) {
    using Notify = int(APS5_VABI*)(NpStreamCtx*, const void*, std::size_t);
    return ctx == nullptr ? 0 : reinterpret_cast<Notify>(const_cast<void*>(ctx->vtable[2]))(ctx, data, size);
}

int ReadAll(NpStreamReader* self, void* handle, NpStreamCtx* ctx, void* out, std::size_t size, std::size_t* read) {
    auto* cursor = static_cast<std::uint8_t*>(out);
    std::size_t got = 0;
    std::size_t total = 0;
    for (std::size_t remaining = size; remaining != 0; remaining = size - total) {
        int result = StreamRead(self, handle, cursor, remaining, &got);
        if (result < 0) return result;
        result = StreamNotify(ctx, cursor, got);
        if (result < 0) return result;
        if (remaining < got) sceNpPanic();
        if (got == 0) {
            *read = total;
            return 0;
        }
        total += got;
        cursor += got;
    }
    std::uint8_t probe = 0;
    int result = StreamRead(self, handle, &probe, 1, &got);
    if (result < 0) return result;
    result = StreamNotify(ctx, &probe, got);
    if (result < 0) return result;
    if (got != 0) return NP_ERROR_STREAM_BUFFER_TOO_SMALL;
    *read = size;
    return 0;
}

int SkipAll(NpStreamReader* self, void* handle, NpStreamCtx* ctx, std::int64_t* skipped) {
    std::uint8_t buffer[NP_STREAM_CHUNK];
    while (true) {
        std::size_t got = 0;
        int result = StreamRead(self, handle, buffer, sizeof(buffer), &got);
        if (result >= 0 && got > sizeof(buffer)) return NP_ERROR_STREAM_BUFFER_TOO_SMALL;
        if (result >= 0) result = StreamNotify(ctx, buffer, got);
        if (skipped != nullptr) *skipped += static_cast<std::int64_t>(got);
        if (result < 0) return result;
        if (got == 0) return 0;
    }
}

int ReadData(NpStreamReader* self, void* handle, NpStreamCtx* ctx, void* out, std::size_t size, std::size_t* read) {
    auto* bytes = static_cast<std::uint8_t*>(out);
    std::size_t total = 0;
    std::size_t got = 0;
    int result = 0;
    do {
        result = StreamRead(self, handle, bytes + total, size - total, &got);
        if (result >= 0 && got > size - total) result = NP_ERROR_STREAM_BUFFER_TOO_SMALL;
        if (result >= 0) result = StreamNotify(ctx, bytes + total, got);
        if (result < 0) break;
        total += got;
    } while (got != 0 && total != size);
    if (result >= 0) result = total == size ? 0 : NP_ERROR_STREAM_SHORT;
    if (read != nullptr) *read = total;
    return result;
}

int SkipData(NpStreamReader* self, void* handle, NpStreamCtx* ctx, std::int64_t size, std::int64_t* skipped) {
    std::uint8_t buffer[NP_STREAM_CHUNK];
    int result = 0;
    while (size >= 1) {
        std::size_t done = 0;
        result = ReadData(self, handle, ctx, buffer, std::min(static_cast<std::size_t>(size), sizeof(buffer)), &done);
        if (skipped != nullptr) *skipped += static_cast<std::int64_t>(done);
        size -= static_cast<std::int64_t>(done);
        if (result < 0) return result;
    }
    return 0;
}

int WriteFilledData(NpStreamWriter* self, void* handle, NpStreamCtx* ctx, char fill, std::int64_t size) {
    std::uint8_t buffer[NP_STREAM_CHUNK];
    std::memset(buffer, static_cast<std::uint8_t>(fill), sizeof(buffer));
    while (size > 0) {
        std::size_t written = 0;
        const std::size_t chunk = std::min(static_cast<std::size_t>(size), sizeof(buffer));
        int result = StreamWrite(self, handle, buffer, chunk, &written);
        if (result < 0) return result;
        if (written > chunk) return NP_ERROR_STREAM_BUFFER_TOO_SMALL;
        result = StreamNotify(ctx, buffer, written);
        if (result < 0) return result;
        size -= static_cast<std::int64_t>(written);
    }
    return 0;
}

int WriteData(NpStreamWriter* self, void* handle, NpStreamCtx* ctx, const void* data, std::size_t size, std::size_t* written) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::size_t total = 0;
    std::size_t got = 0;
    int result = 0;
    while (total < size) {
        result = StreamWrite(self, handle, bytes + total, size - total, &got);
        if (result >= 0 && got > size - total) result = NP_ERROR_STREAM_BUFFER_TOO_SMALL;
        if (result >= 0) result = StreamNotify(ctx, bytes + total, got);
        if (result < 0) break;
        total += got;
    }
    if (result >= 0) result = 0;
    if (written != nullptr) *written = total;
    return result;
}

constexpr std::int32_t NP_EVENT_FLAG_CREATED = 1;
constexpr std::int32_t NP_EVENT_FLAG_OPENED = 2;

void* const NpEventFlagVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np9EventFlagD1Ev), reinterpret_cast<void*>(&_ZN3sce2np9EventFlagD0Ev),
};

void RequireEventFlagInit(const NpEventFlagObject* self, const char* message) {
    if (self->state == 0) throw std::invalid_argument(message);
}

void* AllocateObject(std::size_t size, NpAllocator* allocator) {
    auto* header = static_cast<std::uint8_t*>(allocator->allocate(size + NP_OBJECT_HEADER_SIZE, allocator->context));
    if (header == nullptr) return nullptr;
    std::memcpy(header, &allocator, sizeof(allocator));
    return header + NP_OBJECT_HEADER_SIZE;
}

void ReleaseObject(void* object) {
    if (object == nullptr) return;
    auto* header = static_cast<std::uint8_t*>(object) - NP_OBJECT_HEADER_SIZE;
    NpAllocator* allocator = nullptr;
    std::memcpy(&allocator, header, sizeof(allocator));
    allocator->release(header, allocator->context);
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
constexpr int KERNEL_ERROR_ECANCELED = static_cast<int>(0x80020055);
constexpr int NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80558009);
constexpr int NP_ERROR_TIMEOUT = static_cast<int>(0x8055800B);
constexpr int NP_ERROR_EVENT_FLAG_BUSY = static_cast<int>(0x8055800C);
constexpr int NP_ERROR_CANCELED = static_cast<int>(0x80558010);
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

bool ClearGuestBytes(void* destination, std::size_t size) {
    constexpr std::array<std::uint8_t, 256> zeros{};
    const int savedErrno = errno;
    bool cleared = true;
    for (std::size_t offset = 0; cleared && offset < size; offset += zeros.size()) {
        cleared = GuestCopyOut_nid_no_patch(static_cast<std::uint8_t*>(destination) + offset, zeros.data(), std::min(zeros.size(), size - offset));
    }
    errno = savedErrno;
    return cleared;
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

int APS5_VABI sceNpWaitEventFlag(KernelEventFlag flag, std::uint64_t pattern, std::uint32_t mode, std::uint64_t* result, KernelUseconds* timeout) {
    const int status = sceKernelWaitEventFlag(flag, pattern, mode, result, timeout);
    if (status == KERNEL_ERROR_ETIMEDOUT) return NP_ERROR_TIMEOUT;
    if (status == KERNEL_ERROR_ECANCELED) return NP_ERROR_CANCELED;
    return NpResult(status);
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

int APS5_VABI sceNpWaitSema(KernelSema sema, KernelUseconds* timeout) {
    const int status = sceKernelWaitSema(sema, 1, timeout);
    return status == KERNEL_ERROR_ETIMEDOUT ? NP_ERROR_TIMEOUT : NpResult(status);
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
    const Pthread self = scePthreadSelf();
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(thread, &self, sizeof(self));
    errno = savedErrno;
    if (!copied) return NP_ERROR_INVALID_ARGUMENT;
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
    NpHeap local {};
    int savedErrno = errno;
    const bool read = GuestCopyIn_nid_no_patch(&local, heap, sizeof(local));
    errno = savedErrno;
    if (!read) return NP_ERROR_INVALID_ARGUMENT;
    MallocManagedSize stats {};
    stats.size = sizeof(stats);
    stats.version = 1;
    const int result = sceLibcMspaceMallocStats_nid_postfix(local.mspace, &stats);
    if (result != 0) return result;
    const NpHeapStat value {stats.maxSystemSize, stats.maxInuseSize, stats.currentInuseSize};
    savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(stat, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return NP_ERROR_INVALID_ARGUMENT;
    return 0;
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

NpAllocator* APS5_VABI _sceNpAllocatorExConvertAllocator(NpAllocatorEx* allocator) {
    return &allocator->allocator;
}

void* APS5_VABI _sceNpAllocatorExMalloc(NpAllocatorEx* allocator, std::size_t size) {
    return allocator->allocator.allocate(size, allocator->allocator.context);
}

void* APS5_VABI _sceNpAllocatorExRealloc(NpAllocatorEx* allocator, void* pointer, std::size_t size) {
    return allocator->allocator.reallocate(pointer, size, allocator->allocator.context);
}

void APS5_VABI _sceNpAllocatorExFree(NpAllocatorEx* allocator, void* pointer) {
    allocator->allocator.release(pointer, allocator->allocator.context);
}

char* APS5_VABI _sceNpAllocatorExStrdup(NpAllocatorEx* allocator, const char* text) {
    std::size_t length = 0;
    if (!GuestTextLength(text, SIZE_MAX - 1, length)) return nullptr;
    return DuplicateGuestText(allocator, text, length);
}

char* APS5_VABI _sceNpAllocatorExStrndup(NpAllocatorEx* allocator, const char* text, std::size_t maxLength) {
    std::size_t length = 0;
    if (!GuestTextLength(text, std::min(maxLength, SIZE_MAX - 1), length)) return nullptr;
    return DuplicateGuestText(allocator, text, length);
}

void* APS5_VABI _ZN3sce2np6ObjectnwEmR14SceNpAllocator(std::size_t size, NpAllocator* allocator) {
    return AllocateObject(size, allocator);
}

void* APS5_VABI _ZN3sce2np6ObjectnaEmR14SceNpAllocator(std::size_t size, NpAllocator* allocator) {
    return AllocateObject(size, allocator);
}

void APS5_VABI _ZN3sce2np6ObjectdlEPvR14SceNpAllocator(void* object, NpAllocator*) {
    ReleaseObject(object);
}

void APS5_VABI _ZN3sce2np6ObjectdaEPvR14SceNpAllocator(void* object, NpAllocator*) {
    ReleaseObject(object);
}

void* APS5_VABI _ZN3sce2np6ObjectnwEmR16SceNpAllocatorEx(std::size_t size, NpAllocatorEx* allocator) {
    return AllocateObject(size, &allocator->allocator);
}

void* APS5_VABI _ZN3sce2np6ObjectnaEmR16SceNpAllocatorEx(std::size_t size, NpAllocatorEx* allocator) {
    return AllocateObject(size, &allocator->allocator);
}

void APS5_VABI _ZN3sce2np6ObjectdlEPvR16SceNpAllocatorEx(void* object, NpAllocatorEx*) {
    ReleaseObject(object);
}

void APS5_VABI _ZN3sce2np6ObjectdaEPvR16SceNpAllocatorEx(void* object, NpAllocatorEx*) {
    ReleaseObject(object);
}

void APS5_VABI _ZN3sce2np6ObjectdlEPv(void* object) {
    ReleaseObject(object);
}

void APS5_VABI _ZN3sce2np6ObjectdaEPv(void* object) {
    ReleaseObject(object);
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
    if (version == nullptr) return;
    const int savedErrno = errno;
    GuestCopyOut_nid_no_patch(version, &value, sizeof(value));
    errno = savedErrno;
}

int APS5_VABI sceNpRtcConvertToPosixTime(const RtcTick* tick, std::int64_t* seconds) {
    if (tick == nullptr || seconds == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    RtcTick localTick{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localTick, tick, sizeof(localTick));
    errno = savedErrno;
    if (!copied || localTick.tick == 0) return NP_ERROR_INVALID_ARGUMENT;
    RtcDateTime time {};
    const int result = sceRtcSetTick(&time, &localTick);
    if (result < 0) return result;
    return NpResult(sceRtcGetTime_t(&time, seconds));
}

int APS5_VABI _ZN3sce2np4Time12GetUserClockEPS1_(NpTime* time) {
    return ReadClock(time, sceRtcGetCurrentTick(time));
}

int APS5_VABI _ZN3sce2np4Time15GetNetworkClockEPS1_(NpTime* time) {
    return ReadClock(time, sceRtcGetCurrentNetworkTick(time));
}

int APS5_VABI _ZN3sce2np4Time20GetDebugNetworkClockEPS1_(NpTime* time) {
    return ReadClock(time, sceRtcGetCurrentDebugNetworkTick(time));
}

int APS5_VABI _ZN3sce2np4Time11SetDateTimeERKNS0_8DateTimeE(NpTime* self, const RtcDateTime* dateTime) {
    const RtcDateTime copy = *dateTime;
    const int result = sceRtcGetTick(&copy, self);
    return result < 0 ? result : 0;
}

int APS5_VABI _ZN3sce2np4Time11GetDateTimeEPNS0_8DateTimeE(const NpTime* self, RtcDateTime* dateTime) {
    RtcDateTime converted {};
    const int result = sceRtcSetTick(&converted, self);
    if (result < 0) return result;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(dateTime, &converted, sizeof(converted));
    errno = savedErrno;
    if (!copied) return NP_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI _ZNK3sce2np4Time18ConvertToPosixTimeEPl(const NpTime* self, std::int64_t* seconds) {
    const int result = sceNpRtcConvertToPosixTime(self, seconds);
    return result < 0 ? result : 0;
}

NpTime* APS5_VABI _ZN3sce2np4Time15AddMicroSecondsEl(NpTime* self, std::int64_t microseconds) {
    RequireTimeAdd(sceRtcTickAddMicroseconds(self, self, microseconds), "sce::np::Time::AddMicroSeconds");
    return self;
}

NpTime* APS5_VABI _ZN3sce2np4Time10AddSecondsEl(NpTime* self, std::int64_t seconds) {
    RequireTimeAdd(sceRtcTickAddSeconds(self, self, seconds), "sce::np::Time::AddSeconds");
    return self;
}

NpTime* APS5_VABI _ZN3sce2np4Time10AddMinutesEl(NpTime* self, std::int64_t minutes) {
    RequireTimeAdd(sceRtcTickAddMinutes(self, self, minutes), "sce::np::Time::AddMinutes");
    return self;
}

NpTime* APS5_VABI _ZN3sce2np4Time8AddHoursEl(NpTime* self, std::int64_t hours) {
    RequireTimeAdd(sceRtcTickAddHours(self, self, static_cast<std::int32_t>(hours)), "sce::np::Time::AddHours");
    return self;
}

NpTime* APS5_VABI _ZN3sce2np4Time7AddDaysEl(NpTime* self, std::int64_t days) {
    RequireTimeAdd(sceRtcTickAddDays(self, self, static_cast<std::int32_t>(days)), "sce::np::Time::AddDays");
    return self;
}

bool APS5_VABI _ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_lPl(const NpTime* self, const NpTime* since, std::int64_t timeout,
    std::int64_t* remaining) {
    const auto limit = static_cast<std::uint64_t>(timeout);
    const std::uint64_t elapsed = self->tick - since->tick;
    const bool timedOut = timeout < 0 || self->tick < since->tick || limit <= elapsed;
    WriteOptionalGuest(remaining, timedOut ? std::int64_t{0} : static_cast<std::int64_t>(limit - elapsed));
    return timedOut;
}

bool APS5_VABI _ZNK3sce2np4Time27IsTimedoutSinceMicroSecondsERKS1_jPj(const NpTime* self, const NpTime* since, std::uint32_t timeout,
    std::uint32_t* remaining) {
    const std::uint64_t elapsed = self->tick - since->tick;
    const bool before = self->tick < since->tick;
    WriteOptionalGuest(remaining, before || elapsed > timeout ? std::uint32_t{0} : timeout - static_cast<std::uint32_t>(elapsed));
    return before || timeout <= elapsed;
}

bool APS5_VABI _ZNK3sce2np4Time22IsTimedoutSinceSecondsERKS1_lPl(const NpTime* self, const NpTime* since, std::int64_t timeout,
    std::int64_t* remaining) {
    bool timedOut = true;
    std::uint64_t left = 0;
    if (timeout >= 0 && self->tick >= since->tick) {
        const std::uint64_t elapsed = self->tick - since->tick;
        const std::uint64_t limit = static_cast<std::uint64_t>(timeout) * 1000000;
        timedOut = limit <= elapsed;
        left = (elapsed <= limit ? limit - elapsed : 0) / 1000000;
    }
    WriteOptionalGuest(remaining, static_cast<std::int64_t>(left));
    return timedOut;
}

}

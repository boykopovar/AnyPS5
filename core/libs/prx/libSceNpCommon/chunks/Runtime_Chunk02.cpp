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


int APS5_VABI _ZNK3sce2np4Time13FormatRFC3339EPcmi(const NpTime* self, char* text, std::size_t size, int timeZoneMinutes) {
    return sceNpRtcFormatRFC3339(text, size, self, timeZoneMinutes);
}

NpTime* APS5_VABI _ZN3sce2np4TimeplERKS1_(NpTime* self, const NpTime* other) {
    RequireTimeAdd(sceRtcTickAddTicks(self, self, static_cast<std::int64_t>(other->tick)), "sce::np::Time::operator+");
    return self;
}

NpTime* APS5_VABI _ZN3sce2np4TimeplERK10SceRtcTick(NpTime* self, const RtcTick* tick) {
    RequireTimeAdd(sceRtcTickAddTicks(self, self, static_cast<std::int64_t>(tick->tick)), "sce::np::Time::operator+");
    return self;
}

bool APS5_VABI _ZN3sce2npeqERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs) {
    return lhs->tick == rhs->tick;
}

bool APS5_VABI _ZN3sce2npeqERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs) {
    return lhs->tick == rhs->tick;
}

bool APS5_VABI _ZN3sce2npeqERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs) {
    return lhs->tick == rhs->tick;
}

bool APS5_VABI _ZN3sce2npneERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs) {
    return lhs->tick != rhs->tick;
}

bool APS5_VABI _ZN3sce2npneERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs) {
    return lhs->tick != rhs->tick;
}

bool APS5_VABI _ZN3sce2npneERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs) {
    return lhs->tick != rhs->tick;
}

bool APS5_VABI _ZN3sce2npleERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs) {
    return lhs->tick <= rhs->tick;
}

bool APS5_VABI _ZN3sce2npleERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs) {
    return lhs->tick <= rhs->tick;
}

bool APS5_VABI _ZN3sce2npleERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs) {
    return lhs->tick <= rhs->tick;
}

bool APS5_VABI _ZN3sce2npgeERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs) {
    return lhs->tick >= rhs->tick;
}

bool APS5_VABI _ZN3sce2npgeERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs) {
    return lhs->tick >= rhs->tick;
}

bool APS5_VABI _ZN3sce2npgeERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs) {
    return lhs->tick >= rhs->tick;
}

bool APS5_VABI _ZN3sce2npltERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs) {
    return lhs->tick < rhs->tick;
}

bool APS5_VABI _ZN3sce2npltERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs) {
    return lhs->tick < rhs->tick;
}

bool APS5_VABI _ZN3sce2npltERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs) {
    return lhs->tick < rhs->tick;
}

bool APS5_VABI _ZN3sce2npgtERKNS0_4TimeES3_(const NpTime* lhs, const NpTime* rhs) {
    return lhs->tick > rhs->tick;
}

bool APS5_VABI _ZN3sce2npgtERK10SceRtcTickRKNS0_4TimeE(const RtcTick* lhs, const NpTime* rhs) {
    return lhs->tick > rhs->tick;
}

bool APS5_VABI _ZN3sce2npgtERKNS0_4TimeERK10SceRtcTick(const NpTime* lhs, const RtcTick* rhs) {
    return lhs->tick > rhs->tick;
}

int APS5_VABI sceNpRtcFormatRFC3339(char* text, std::size_t size, const RtcTick* utc, int timeZoneMinutes) {
    if (size < NP_RFC3339_BUFFER_MIN) return NP_ERROR_BUFFER_TOO_SMALL;
    const int result = sceRtcFormatRFC3339(text, utc, timeZoneMinutes);
    if (result < 0) return result;
    const char terminator = 0;
    const int savedErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(text + size - 1, &terminator, sizeof(terminator));
    errno = savedErrno;
    return written ? 0 : RTC_ERROR_INVALID_POINTER;
}

int APS5_VABI sceNpUserGetUserIdList(int* users, std::size_t capacity, int* count) {
    if (capacity < NP_USER_ID_LIST_SIZE) return NP_ERROR_INVALID_ARGUMENT;
    UserServiceRegisteredUserIdList registered{};
    std::fill(std::begin(registered.user_id), std::end(registered.user_id), USER_ID_INVALID);
    const int result = sceUserServiceGetRegisteredUserIdList(&registered);
    if (result < 0) return result;
    std::array<int, std::size(registered.user_id)> ids{};
    int found = 0;
    for (const int user : registered.user_id)
        if (user != USER_ID_INVALID) ids[found++] = user;
    const int savedErrno = errno;
    const bool copied = (found == 0 || GuestCopyOut_nid_no_patch(users, ids.data(), found * sizeof(int))) &&
        GuestCopyOut_nid_no_patch(count, &found, sizeof(found));
    errno = savedErrno;
    if (!copied) return NP_ERROR_INVALID_ARGUMENT;
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

static thread_local bool calloutContextEnded = false;

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
            if (calloutContextEnded) return nullptr;
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
    if (scePthreadEqual(scePthreadSelf(), context->thread) != 0) {
        sceNpMutexLock(&context->mutex);
        sceNpMutexUnlock(&context->mutex);
        sceNpCondDestroy(&context->cond);
        sceNpMutexDestroy(&context->mutex);
        scePthreadDetach(context->thread);
        context->initialized = 0;
        calloutContextEnded = true;
        return;
    }
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
    NpIdFull leftId {};
    NpIdFull rightId {};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&leftId, npId1, sizeof(leftId)) && GuestCopyIn_nid_no_patch(&rightId, npId2, sizeof(rightId));
    errno = savedErrno;
    if (!copied) return NP_ERROR_NULL_ARGUMENT;
    const auto* left = reinterpret_cast<const std::uint8_t*>(&leftId);
    const auto* right = reinterpret_cast<const std::uint8_t*>(&rightId);
    if (left[NP_ID_TERMINATOR_FLAG_OFFSET] != 1 || right[NP_ID_TERMINATOR_FLAG_OFFSET] != 1) return NP_ERROR_INVALID_NP_ID;
    if (std::strncmp(leftId.handle.data, rightId.handle.data, NP_ONLINE_ID_MAX) != 0) return NP_ERROR_ONLINE_ID_MISMATCH;
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
    char localCode[NP_LANGUAGE_CODE_SIZE]{};
    CopyText(localCode, codes->code, NP_LANGUAGE_CODE_SIZE);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(code, localCode, NP_LANGUAGE_CODE_SIZE);
    errno = savedErrno;
    if (!copied) return NP_ERROR_INVALID_ARGUMENT_UTIL;
    return 0;
}

int APS5_VABI sceNpUtilGetNpLanguageCode2(int language, char* tag) {
    const NpLanguageCodes* codes = FindLanguageCodes(language);
    if (codes == nullptr || tag == nullptr) return NP_ERROR_INVALID_ARGUMENT_UTIL;
    char localTag[NP_LANGUAGE_CODE2_SIZE]{};
    CopyText(localTag, codes->tag, NP_LANGUAGE_CODE2_SIZE);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(tag, localTag, NP_LANGUAGE_CODE2_SIZE);
    errno = savedErrno;
    if (!copied) return NP_ERROR_INVALID_ARGUMENT_UTIL;
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
    NpTitleIdParts localParts{};
    const int savedErrno = errno;
    const bool read = GuestCopyIn_nid_no_patch(&localParts, parts, sizeof(localParts));
    errno = savedErrno;
    if (!read) return NP_ERROR_INVALID_ARGUMENT_UTIL;
    if (localParts.index > NP_TITLE_ID_INDEX_MAX) return NP_UTIL_ERROR_OUT_OF_RANGE;
    char localTitleId[NP_TITLE_ID_SIZE] = {};
    std::memcpy(localTitleId, localParts.id, sizeof(localParts.id));
    localTitleId[9] = '_';
    localTitleId[10] = static_cast<char>('0' + localParts.index / 10);
    localTitleId[11] = static_cast<char>('0' + localParts.index % 10);
    localTitleId[12] = '\0';
    const bool written = GuestCopyOut_nid_no_patch(titleId, localTitleId, sizeof(localTitleId));
    errno = savedErrno;
    if (!written) return NP_ERROR_INVALID_ARGUMENT_UTIL;
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

void APS5_VABI _ZN3sce2np9SemaphoreC1Ev(NpSemaphoreObject* self) {
    self->vtable = &NpSemaphoreVtable[2];
    self->state = 0;
}

void APS5_VABI _ZN3sce2np9SemaphoreC2Ev(NpSemaphoreObject* self) {
    _ZN3sce2np9SemaphoreC1Ev(self);
}

void APS5_VABI _ZN3sce2np9SemaphoreD1Ev(NpSemaphoreObject* self) {
    self->vtable = &NpSemaphoreVtable[2];
    if (self->state == NP_SEMAPHORE_OPENED) NotImplemented_nid_no_patch("sce::np::Semaphore: closing an opened semaphore (sceKernelCloseSema)");
    else if (self->state == NP_SEMAPHORE_CREATED) sceKernelDeleteSema(self->sema);
}

void APS5_VABI _ZN3sce2np9SemaphoreD2Ev(NpSemaphoreObject* self) {
    _ZN3sce2np9SemaphoreD1Ev(self);
}

void APS5_VABI _ZN3sce2np9SemaphoreD0Ev(NpSemaphoreObject* self) {
    self->vtable = &NpSemaphoreVtable[2];
    ReleaseSemaphore(self);
    _ZN3sce2np6ObjectdlEPv(self);
}

bool APS5_VABI _ZNK3sce2np9Semaphore6IsInitEv(const NpSemaphoreObject* self) {
    return self->state != 0;
}

int APS5_VABI _ZN3sce2np9Semaphore6CreateEiiPKc(NpSemaphoreObject* self, int initial, int maximum, const char* name) {
    if (self->state != 0) return NP_ERROR_OBJECT_ALREADY_INITIALIZED;
    const int result = sceNpCreateSema(&self->sema, 0, initial, maximum, name);
    if (result < 0) return result;
    self->state = NP_SEMAPHORE_CREATED;
    return 0;
}

int APS5_VABI _ZN3sce2np9Semaphore4OpenEPKc(NpSemaphoreObject* self, const char*) {
    if (self->state != 0) return NP_ERROR_OBJECT_ALREADY_INITIALIZED;
    NotImplemented_nid_no_patch("sce::np::Semaphore::Open: opening a named semaphore (sceKernelOpenSema)");
    return 0;
}

void APS5_VABI _ZN3sce2np9Semaphore7DestroyEv(NpSemaphoreObject* self) {
    ReleaseSemaphore(self);
    self->state = 0;
}

int APS5_VABI _ZN3sce2np9Semaphore4WaitEj(NpSemaphoreObject* self, std::uint32_t usec) {
    RequireSemaphoreInit(self, "sce::np::Semaphore::Wait: the semaphore is not initialized");
    KernelUseconds timeout = usec;
    return NpResult(sceNpWaitSema(self->sema, usec != 0 ? &timeout : nullptr));
}

int APS5_VABI _ZN3sce2np9Semaphore6SignalEv(NpSemaphoreObject* self) {
    RequireSemaphoreInit(self, "sce::np::Semaphore::Signal: the semaphore is not initialized");
    return NpResult(sceNpSignalSema(self->sema));
}
void APS5_VABI _ZN3sce2np5MutexC1Ev(NpMutexObject* self) {
    self->vtable = &NpMutexVtable[2];
    self->mutex = nullptr;
    self->isInit = false;
}

void APS5_VABI _ZN3sce2np5MutexC2Ev(NpMutexObject* self) {
    _ZN3sce2np5MutexC1Ev(self);
}

void APS5_VABI _ZN3sce2np5Mutex4dtorEv(NpMutexObject* self) {
    if (self->isInit) scePthreadMutexDestroy(&self->mutex);
}

void APS5_VABI _ZN3sce2np5MutexD1Ev(NpMutexObject* self) {
    self->vtable = &NpMutexVtable[2];
    _ZN3sce2np5Mutex4dtorEv(self);
}

void APS5_VABI _ZN3sce2np5MutexD2Ev(NpMutexObject* self) {
    _ZN3sce2np5MutexD1Ev(self);
}

void APS5_VABI _ZN3sce2np5MutexD0Ev(NpMutexObject* self) {
    self->vtable = &NpMutexVtable[2];
    if (self->isInit) sceNpMutexDestroy(&self->mutex);
    _ZN3sce2np6ObjectdlEPv(self);
}

void APS5_VABI _ZN3sce2np5Mutex4ctorEv(NpMutexObject* self) {
    self->isInit = false;
}

int APS5_VABI _ZN3sce2np5Mutex4InitEPKcj(NpMutexObject* self, const char* name, std::uint32_t flags) {
    if (self->isInit) return NP_ERROR_OBJECT_ALREADY_INITIALIZED;
    const int result = sceNpMutexInit(&self->mutex, name, flags & 1);
    if (result < 0) return result;
    self->isInit = true;
    return 0;
}

}

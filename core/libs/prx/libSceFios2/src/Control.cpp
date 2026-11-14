#include "prx/libSceFios2/include/Api.hpp"
#include "Runtime.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {
std::atomic<std::uint8_t> g_decompressorThreads{0};
std::atomic<std::uint8_t> g_decompressorLimit{0};

void SetDecompressorLimit(std::uint8_t count) {
    g_decompressorLimit.store(count, std::memory_order_relaxed);
    g_decompressorThreads.store(count, std::memory_order_relaxed);
}

std::uint8_t DecompressorThreadsFromParams(const SceFiosParams* params) {
    std::array<std::uint8_t, 0x19> head{};
    if (params == nullptr || !GuestCopyIn_nid_no_patch(head.data(), params, sizeof(std::uint32_t))) return 2;
    std::uint32_t first = 0;
    std::memcpy(&first, head.data(), sizeof(first));
    if (((first >> 1) & 0x7fff) < head.size() || !GuestCopyIn_nid_no_patch(head.data(), params, head.size())) return 2;
    return head[0x18] == 0 ? 2 : std::min<std::uint8_t>(head[0x18], 3);
}

struct IOFilter {
    SceFiosIOFilterCallback callback{};
    void* context{};
};

struct OverlayRecord {
    std::uint8_t type{};
    std::uint8_t order{};
    std::uint16_t destinationLength{};
    std::uint16_t sourceLength{};
    std::uint16_t reserved{};
    std::int32_t processId{};
    std::int32_t id{};
    char destination[292]{};
    char source[292]{};
};

static_assert(sizeof(OverlayRecord) == 0x258);

struct CacheFilterConfig {
    std::uint64_t size{};
    std::uint64_t workBufferSize{};
    std::uint64_t blockSize{};
    void* workBuffer{};
    const char* path{};
    std::uint64_t flags{};
    std::uint64_t reserved[3]{};
};

struct RamCacheContext {
    std::uint64_t size{};
    std::uint64_t workBufferSize{};
    std::uint64_t blockSize{};
    void* workBuffer{};
    const char* path{};
    std::uint64_t flags{};
    std::uint64_t reserved[3]{};
};

struct CacheFilterRegistration {
    CacheFilterConfig config;
    RamCacheContext context;
    std::string path;
};

static_assert(sizeof(CacheFilterConfig) == 0x48);
static_assert(sizeof(RamCacheContext) == 0x48);

std::array<IOFilter, 256> g_ioFilters{};
std::mutex g_ioFiltersMutex;
std::vector<CacheFilterRegistration> g_cacheFilters;
std::mutex g_cacheFiltersMutex;
std::vector<OverlayRecord> g_overlays;
std::mutex g_overlaysMutex;
std::int32_t g_lastOverlayId = 0;

std::string NormalizeOverlayPath(const std::string& path) {
    std::string normalized;
    std::size_t start = 0;
    const bool absolute = !path.empty() && path.front() == '/';
    while (start <= path.size()) {
        const std::size_t end = std::min(path.find('/', start), path.size());
        const std::string part = path.substr(start, end - start);
        if (!part.empty() && part != ".") {
            if (!normalized.empty() || absolute) normalized += '/';
            normalized += part;
        }
        start = end + 1;
    }
    return normalized.empty() && absolute ? "/" : normalized;
}

bool OverlayTakesSource(std::uint8_t type, std::int32_t flag, const std::string& source, const std::string& original) {
    if (type == 0) return true;
    if (type == 3 && flag == 1) return true;
    SceFiosStat sourceStat{};
    if (Fios::StatPath(source, sourceStat) != SCE_FIOS_OK) return false;
    if (type != 2) return true;
    SceFiosStat originalStat{};
    if (Fios::StatPath(original, originalStat) != SCE_FIOS_OK) return true;
    if (sourceStat.modificationDate == originalStat.modificationDate) NotImplemented_nid_no_patch(__func__);
    return sourceStat.modificationDate > originalStat.modificationDate;
}

struct PsarcDescriptor {
    std::uint64_t size;
    std::uint64_t workBufferSize;
    std::uint64_t workBuffer;
    std::uint64_t flags;
    std::uint64_t reserved[3];
};

static_assert(sizeof(PsarcDescriptor) == 0x38);

constexpr std::int32_t kFilterNotHandled = static_cast<std::int32_t>(0x8082001Du);
constexpr char kPsarcDescription[] = "description not implemented";
std::mutex g_psarcMutex;
std::int32_t g_psarcUsers = 0;
bool g_psarcRejected = false;

int StartPsarcDearchiver(const PsarcDescriptor* descriptor) {
    if (descriptor == nullptr || descriptor->workBuffer == 0) return SCE_FIOS_ERROR_BAD_PTR;
    if (descriptor->size != sizeof(PsarcDescriptor) || descriptor->workBufferSize == 0) return SCE_FIOS_ERROR_BAD_SIZE;
    if ((descriptor->workBuffer & 0x3f) != 0) return SCE_FIOS_ERROR_BAD_ALIGNMENT;
    if ((descriptor->flags & ~std::uint64_t{1}) != 0) return SCE_FIOS_ERROR_BAD_FLAGS;
    for (const std::uint64_t word : descriptor->reserved)
        if (word != 0) return SCE_FIOS_ERROR_BAD_SIZE;
    const std::lock_guard lock(g_psarcMutex);
    ++g_psarcUsers;
    return SCE_FIOS_OK;
}

bool PsarcDescriptorAccepted(const PsarcDescriptor* descriptor) {
    return descriptor != nullptr && descriptor->reserved[1] <= 7 && (descriptor->reserved[0] & 7) == 0 && descriptor->flags <= 1 &&
           descriptor->size == sizeof(PsarcDescriptor) && descriptor->workBufferSize != 0 && descriptor->workBuffer != 0 &&
           (descriptor->workBuffer & 0x3f) == 0;
}

void ClearIOFilters() {
    const std::lock_guard lock(g_ioFiltersMutex);
    g_ioFilters.fill({});
    const std::lock_guard cacheLock(g_cacheFiltersMutex);
    g_cacheFilters.clear();
}
}

extern "C" {

int APS5_VABI sceFiosArchiveGetDecompressorThreadCount() {
    return g_decompressorThreads.load(std::memory_order_relaxed);
}

int APS5_VABI sceFiosArchiveSetDecompressorThreadCount(std::uint8_t count) {
    const std::uint8_t updated = count == 0 ? 1 : std::min(g_decompressorLimit.load(std::memory_order_relaxed), count);
    g_decompressorThreads.store(updated, std::memory_order_relaxed);
    return updated;
}

SceFiosOp APS5_VABI sceFiosArchiveGetMountBufferSize(const SceFiosOpAttr*, const char*, SceFiosSize*) {
    return 0;
}

SceFiosSize APS5_VABI sceFiosArchiveGetMountBufferSizeSync(const SceFiosOpAttr*, const char*, SceFiosSize*) {
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

SceFiosOp APS5_VABI sceFiosArchiveMount(const SceFiosOpAttr*, SceFiosFH*, const char*, const char*, const void*, std::uint64_t, const void*) {
    return 0;
}

int APS5_VABI sceFiosArchiveMountSync(const SceFiosOpAttr*, SceFiosFH*, const char*, const char*, const void*, std::uint64_t, const void*) {
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

SceFiosOp APS5_VABI sceFiosArchiveMountWithOrder(const SceFiosOpAttr*, SceFiosFH*, const char*, const char*, const void*, std::uint64_t, const void*, std::uint32_t) {
    return 0;
}

int APS5_VABI sceFiosArchiveMountWithOrderSync(const SceFiosOpAttr*, SceFiosFH*, const char*, const char*, const void*, std::uint64_t, const void*, std::uint32_t) {
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

SceFiosOp APS5_VABI sceFiosArchiveUnmount(const SceFiosOpAttr*, SceFiosFH) {
    return 0;
}

int APS5_VABI sceFiosArchiveUnmountSync(const SceFiosOpAttr*, SceFiosFH) {
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

int APS5_VABI sceFiosIOFilterAdd(std::int32_t index, SceFiosIOFilterCallback callback, void* context) {
    if (callback == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    if (index < 0 || index >= 256) return SCE_FIOS_ERROR_BAD_INDEX;
    IOFilter& filter = g_ioFilters[static_cast<std::size_t>(index)];
    {
        const std::lock_guard lock(g_ioFiltersMutex);
        if (filter.callback != nullptr) return SCE_FIOS_ERROR_BAD_INDEX;
    }
    const int started = callback(context, 0, nullptr);
    if (started != SCE_FIOS_OK && started != kFilterNotHandled) return started;
    {
        const std::lock_guard lock(g_ioFiltersMutex);
        if (filter.callback == nullptr) {
            filter.callback = callback;
            filter.context = context;
            return SCE_FIOS_OK;
        }
    }
    callback(context, 1, nullptr);
    return SCE_FIOS_ERROR_BAD_INDEX;
}

int APS5_VABI sceFiosIOFilterPsarcDearchiver(void* descriptor, std::int32_t mode, void* data) {
    const auto* psarc = static_cast<const PsarcDescriptor*>(descriptor);
    if (mode == 0) return StartPsarcDearchiver(psarc);
    {
        const std::lock_guard lock(g_psarcMutex);
        if (g_psarcRejected || !PsarcDescriptorAccepted(psarc)) {
            g_psarcRejected = true;
            return SCE_FIOS_ERROR_BAD_PTR;
        }
        if (mode == 1) {
            --g_psarcUsers;
            return SCE_FIOS_OK;
        }
    }
    if (data == nullptr || (mode != 2 && mode != -1)) return kFilterNotHandled;
    if (mode == 2) return Fios::CopyOut(data, kPsarcDescription, sizeof(kPsarcDescription)) ? SCE_FIOS_OK : SCE_FIOS_ERROR_BAD_PTR;
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosInitialize(const SceFiosParams* params) {
    if (!Fios::Initialize(params)) return SCE_FIOS_ERROR_BAD_PTR;
    SetDecompressorLimit(DecompressorThreadsFromParams(params));
    return SCE_FIOS_OK;
}

void APS5_VABI sceFiosTerminate() {
    Fios::Terminate();
    ClearIOFilters();
    SetDecompressorLimit(0);
}

bool APS5_VABI sceFiosIsInitialized(SceFiosParams* outParams) {
    return Fios::IsInitialized(outParams);
}

void APS5_VABI sceFiosUpdateParameters(const SceFiosParams* params) {
    Fios::UpdateParameters(params);
}

void APS5_VABI sceFiosSetGlobalDefaultOpAttr(const SceFiosOpAttr* attr) {
    Fios::SetGlobalAttr(attr);
}

bool APS5_VABI sceFiosGetGlobalDefaultOpAttr(SceFiosOpAttr* outAttr) {
    SceFiosOpAttr attr{};
    const bool present = Fios::GlobalAttr(attr);
    if (outAttr != nullptr && present) Fios::CopyOut(outAttr, &attr, sizeof(attr));
    return present;
}

void APS5_VABI sceFiosSetThreadDefaultOpAttr(const SceFiosOpAttr* attr) {
    Fios::SetThreadAttr(attr);
}

bool APS5_VABI sceFiosGetThreadDefaultOpAttr(SceFiosOpAttr* outAttr) {
    SceFiosOpAttr attr{};
    const bool present = Fios::ThreadAttr(attr);
    if (outAttr != nullptr && present) Fios::CopyOut(outAttr, &attr, sizeof(attr));
    return present;
}

void APS5_VABI sceFiosGetDefaultOpAttr(SceFiosOpAttr* outAttr) {
    SceFiosOpAttr attr{};
    if (!Fios::ThreadAttr(attr)) Fios::GlobalAttr(attr);
    if (outAttr != nullptr) Fios::CopyOut(outAttr, &attr, sizeof(attr));
}

void APS5_VABI sceFiosSuspend() {
    Fios::Suspend();
}

void APS5_VABI sceFiosResume() {
    Fios::Resume();
}

std::uint32_t APS5_VABI sceFiosGetSuspendCount() {
    return Fios::SuspendCount();
}

bool APS5_VABI sceFiosIsSuspended() {
    return Fios::SuspendCount() != 0;
}

void APS5_VABI sceFiosCancelAllOps() {
    Fios::CancelAll();
}

void APS5_VABI sceFiosShutdownAndCancelOps() {
    Fios::Terminate();
    ClearIOFilters();
    SetDecompressorLimit(0);
}

void APS5_VABI sceFiosCloseAllFiles() {
    Fios::CloseAllFiles();
}

bool APS5_VABI sceFiosIsIdle() {
    return Fios::IsIdle();
}

static std::uint32_t CopyHandles(std::uint32_t (*list)(SceFiosHandle*, std::uint32_t), SceFiosHandle* out, std::uint32_t capacity) {
    const std::uint32_t total = list(nullptr, 0);
    std::vector<SceFiosHandle> handles(std::min(total, capacity));
    const std::uint32_t listed = list(handles.data(), static_cast<std::uint32_t>(handles.size()));
    const std::uint32_t count = std::min(listed, static_cast<std::uint32_t>(handles.size()));
    if (out != nullptr && count != 0) Fios::CopyOut(out, handles.data(), count * sizeof(SceFiosHandle));
    return listed;
}

std::uint32_t APS5_VABI sceFiosGetAllFHs(SceFiosFH* outArray, std::uint32_t arraySize) {
    return CopyHandles(Fios::ListFiles, outArray, arraySize);
}

std::uint32_t APS5_VABI sceFiosGetAllDHs(SceFiosDH* outArray, std::uint32_t arraySize) {
    return CopyHandles(Fios::ListDirectories, outArray, arraySize);
}

std::uint32_t APS5_VABI sceFiosGetAllOps(SceFiosOp* outArray, std::uint32_t arraySize) {
    return CopyHandles(Fios::ListOps, outArray, arraySize);
}

bool APS5_VABI sceFiosIsValidHandle(SceFiosHandle handle) {
    return Fios::IsValidHandle(handle);
}

void APS5_VABI sceFiosOpRescheduleWithPriority(SceFiosOp op, const void*, std::int32_t) {
    if (Fios::OpAttr(op) == nullptr || Fios::OpDone(op)) return;
}

void APS5_VABI sceFiosOpReschedule(SceFiosOp op, const void* scheduler) {
    const SceFiosOpAttr* attr = Fios::OpAttr(op);
    if (attr == nullptr || Fios::OpDone(op)) return;
    sceFiosOpRescheduleWithPriority(op, scheduler, attr->priority);
}

void APS5_VABI sceFiosCacheFlushSync() {}

void APS5_VABI sceFiosCacheFlushFileSync(const char*) {}

void APS5_VABI sceFiosCacheFlushFileRangeSync(const char*, SceFiosOffset, SceFiosSize) {}

std::int64_t APS5_VABI sceFiosCacheContainsFileRangeSync(const char*, std::uint64_t, std::uint64_t) {
    return SCE_FIOS_ERROR_BAD_INDEX;
}

int APS5_VABI sceFiosIOFilterGetInfo(std::int32_t index, void** callback, void** context) {
    if (index < 0 || index >= 256) return SCE_FIOS_ERROR_BAD_INDEX;
    const std::lock_guard lock(g_ioFiltersMutex);
    const IOFilter& filter = g_ioFilters[static_cast<std::size_t>(index)];
    if (filter.callback == nullptr) return SCE_FIOS_ERROR_BAD_INDEX;
    if (callback != nullptr && !Fios::CopyOut(callback, &filter.callback, sizeof(filter.callback))) return SCE_FIOS_ERROR_BAD_PTR;
    if (context != nullptr && !Fios::CopyOut(context, &filter.context, sizeof(filter.context))) return SCE_FIOS_ERROR_BAD_PTR;
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosIOFilterRemove(std::int32_t index) {
    if (index < 0 || index >= 256) return SCE_FIOS_ERROR_BAD_INDEX;
    IOFilter removed{};
    {
        const std::lock_guard lock(g_ioFiltersMutex);
        IOFilter& filter = g_ioFilters[static_cast<std::size_t>(index)];
        if (filter.callback == nullptr) return SCE_FIOS_ERROR_BAD_INDEX;
        removed = filter;
        filter = {};
    }
    removed.callback(removed.context, 1, nullptr);
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosIOFilterCache(const void* config, std::int32_t index, const void* context) {
    if (config == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    CacheFilterRegistration registration{};
    if (!Fios::CopyIn(&registration.config, config, sizeof(registration.config))) return SCE_FIOS_ERROR_BAD_PTR;
    if (registration.config.size != sizeof(CacheFilterConfig) || registration.config.workBufferSize < 0x20000 || registration.config.blockSize == 0 || registration.config.blockSize > registration.config.workBufferSize) return SCE_FIOS_ERROR_BAD_SIZE;
    if (registration.config.workBuffer == nullptr || (reinterpret_cast<std::uintptr_t>(registration.config.workBuffer) & 7) != 0 || registration.config.path == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    if ((registration.config.flags & ~0xe17ull) != 0) return SCE_FIOS_ERROR_BAD_FLAGS;
    if (index != 0) return SCE_FIOS_ERROR_BAD_INDEX;
    if (context == nullptr || !Fios::CopyIn(&registration.context, context, sizeof(registration.context))) return SCE_FIOS_ERROR_BAD_PTR;
    if (registration.context.size != sizeof(RamCacheContext) || registration.context.workBufferSize < 0x20000 || registration.context.blockSize != registration.config.blockSize || registration.context.workBuffer == nullptr || (reinterpret_cast<std::uintptr_t>(registration.context.workBuffer) & 0x3f) != 0 || registration.context.path == nullptr) return SCE_FIOS_ERROR_BAD_SIZE;
    if (const std::int32_t error = Fios::CopyPath(registration.config.path, registration.path)) return error;
    std::string contextPath;
    if (const std::int32_t error = Fios::CopyPath(registration.context.path, contextPath)) return error;
    if (registration.path != contextPath) return SCE_FIOS_ERROR_BAD_OVERLAY;
    {
        const std::lock_guard lock(g_overlaysMutex);
        const auto found = std::find_if(g_overlays.begin(), g_overlays.end(), [&registration](const OverlayRecord& overlay) { return registration.path == overlay.destination; });
        if (found == g_overlays.end()) return SCE_FIOS_ERROR_BAD_OVERLAY;
    }
    std::lock_guard lock(g_cacheFiltersMutex);
    g_cacheFilters.push_back(std::move(registration));
    return SCE_FIOS_OK;
}

SceFiosOp APS5_VABI sceFiosCachePrefetchFH(const SceFiosOpAttr*, SceFiosFH) {
    return 0;
}

SceFiosOp APS5_VABI sceFiosCachePrefetchFHRange(const SceFiosOpAttr*, SceFiosFH, SceFiosOffset, SceFiosSize) {
    return 0;
}

int APS5_VABI sceFiosCachePrefetchFHRangeSync(const SceFiosOpAttr*, SceFiosFH, SceFiosOffset, SceFiosSize) {
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosCachePrefetchFHSync(const SceFiosOpAttr*, SceFiosFH) {
    return SCE_FIOS_OK;
}

SceFiosOp APS5_VABI sceFiosCachePrefetchFile(const SceFiosOpAttr*, const char*) {
    return 0;
}

SceFiosOp APS5_VABI sceFiosCachePrefetchFileRange(const SceFiosOpAttr*, const char*, SceFiosOffset, SceFiosSize) {
    return 0;
}

int APS5_VABI sceFiosCachePrefetchFileRangeSync(const SceFiosOpAttr*, const char*, SceFiosOffset, SceFiosSize) {
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosCachePrefetchFileSync(const SceFiosOpAttr*, const char*) {
    return SCE_FIOS_ERROR_UNIMPLEMENTED;
}

std::int64_t APS5_VABI sceFiosCacheContainsFileSync(const char* path, std::uint64_t* outCached) {
    const std::uint64_t cached = 0;
    if (outCached != nullptr && !Fios::CopyOut(outCached, &cached, sizeof(cached))) return SCE_FIOS_ERROR_BAD_PTR;
    Fios::OpInfo info;
    if (const std::int32_t error = Fios::CopyPath(path, info.path)) return error;
    SceFiosStat stat{};
    if (const std::int32_t error = Fios::StatPath(info.path, stat)) return error;
    if ((stat.statFlags & SCE_FIOS_STAT_DIRECTORY) != 0) return SCE_FIOS_ERROR_NOT_A_FILE;
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosOverlayGetList(std::int32_t* ids, std::uint64_t capacity, std::uint64_t* count) {
    if (count == nullptr || (capacity != 0 && ids == nullptr)) return SCE_FIOS_ERROR_BAD_PTR;
    const std::lock_guard lock(g_overlaysMutex);
    const std::uint64_t total = g_overlays.size();
    if (!Fios::CopyOut(count, &total, sizeof(total))) return SCE_FIOS_ERROR_BAD_PTR;
    const std::uint64_t copied = std::min<std::uint64_t>(capacity, total);
    for (std::uint64_t i = 0; i < copied; ++i) {
        if (!Fios::CopyOut(ids + i, &g_overlays[i].id, sizeof(g_overlays[i].id))) return SCE_FIOS_ERROR_BAD_PTR;
    }
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosOverlayGetInfo(std::int32_t id, void* outInfo) {
    if (outInfo == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    const std::lock_guard lock(g_overlaysMutex);
    const auto found = std::find_if(g_overlays.begin(), g_overlays.end(), [id](const OverlayRecord& overlay) { return overlay.id == id; });
    if (found == g_overlays.end()) return SCE_FIOS_ERROR_BAD_OVERLAY;
    return Fios::CopyOut(outInfo, &*found, sizeof(*found)) ? SCE_FIOS_OK : SCE_FIOS_ERROR_BAD_PTR;
}

int APS5_VABI sceFiosOverlayRemove(std::int32_t id) {
    const std::lock_guard lock(g_overlaysMutex);
    const auto found = std::find_if(g_overlays.begin(), g_overlays.end(), [id](const OverlayRecord& overlay) { return overlay.id == id; });
    if (found == g_overlays.end()) return SCE_FIOS_ERROR_BAD_OVERLAY;
    g_overlays.erase(found);
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosOverlayModify(std::int32_t id, const void* overlay) {
    if (overlay == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    OverlayRecord record{};
    if (!Fios::CopyIn(&record, overlay, sizeof(record))) return SCE_FIOS_ERROR_BAD_PTR;
    const auto destinationEnd = std::find(std::begin(record.destination), std::end(record.destination), '\0');
    const auto sourceEnd = std::find(std::begin(record.source), std::end(record.source), '\0');
    if (destinationEnd == std::begin(record.destination) || sourceEnd == std::begin(record.source)) return SCE_FIOS_ERROR_BAD_PTR;
    if (destinationEnd == std::end(record.destination) || sourceEnd == std::end(record.source)) return SCE_FIOS_ERROR_PATH_TOO_LONG;
    if (record.type > 3) return SCE_FIOS_ERROR_BAD_FLAGS;
    record.destinationLength = static_cast<std::uint16_t>(destinationEnd - std::begin(record.destination));
    record.sourceLength = static_cast<std::uint16_t>(sourceEnd - std::begin(record.source));
    const std::lock_guard lock(g_overlaysMutex);
    const auto found = std::find_if(g_overlays.begin(), g_overlays.end(), [id](const OverlayRecord& item) { return item.id == id; });
    if (found == g_overlays.end()) return SCE_FIOS_ERROR_BAD_OVERLAY;
    record.id = id;
    *found = record;
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosOverlayAdd(const void* overlay, void* context) {
    if (overlay == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    OverlayRecord record{};
    if (!Fios::CopyIn(&record, overlay, sizeof(record))) return SCE_FIOS_ERROR_BAD_PTR;
    const auto destinationEnd = std::find(std::begin(record.destination), std::end(record.destination), '\0');
    const auto sourceEnd = std::find(std::begin(record.source), std::end(record.source), '\0');
    if (destinationEnd == std::begin(record.destination) || sourceEnd == std::begin(record.source)) return SCE_FIOS_ERROR_BAD_PTR;
    if (destinationEnd == std::end(record.destination) || sourceEnd == std::end(record.source)) return SCE_FIOS_ERROR_PATH_TOO_LONG;
    if (record.type > 3) return SCE_FIOS_ERROR_BAD_FLAGS;
    record.destinationLength = static_cast<std::uint16_t>(destinationEnd - std::begin(record.destination));
    record.sourceLength = static_cast<std::uint16_t>(sourceEnd - std::begin(record.source));
    std::int32_t id = 0;
    const std::lock_guard lock(g_overlaysMutex);
    id = ++g_lastOverlayId;
    if (id <= 0) id = g_lastOverlayId = 1;
    record.id = id;
    if (context != nullptr && !Fios::CopyOut(context, &id, sizeof(id))) return SCE_FIOS_ERROR_BAD_PTR;
    g_overlays.push_back(record);
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosOverlayResolveSync(std::int32_t flag, const char* path, char* result, std::size_t capacity) {
    if (path == nullptr || result == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    if (capacity > 0x400) return SCE_FIOS_ERROR_BAD_SIZE;
    std::string input;
    const int copyError = CopyGuestString_nid_no_patch(path, capacity, input);
    if (copyError == 14) return SCE_FIOS_ERROR_BAD_PTR;
    if (copyError != 0) return SCE_FIOS_ERROR_PATH_TOO_LONG;
    if (!Fios::CopyOut(result, input.c_str(), input.size() + 1)) return SCE_FIOS_ERROR_BAD_PTR;
    if (flag != 0 && flag != 1) return SCE_FIOS_ERROR_BAD_RESOLVE_FLAG;
    std::vector<OverlayRecord> overlays;
    {
        const std::lock_guard lock(g_overlaysMutex);
        overlays = g_overlays;
    }
    std::erase_if(overlays, [](const OverlayRecord& overlay) { return overlay.order > 0x7f; });
    std::stable_sort(overlays.begin(), overlays.end(), [](const OverlayRecord& left, const OverlayRecord& right) { return left.order < right.order; });
    const std::string normalized = NormalizeOverlayPath(input);
    for (const OverlayRecord& overlay : overlays) {
        const std::string destination = NormalizeOverlayPath(overlay.destination);
        if (normalized != destination && !(normalized.size() > destination.size() && normalized.compare(0, destination.size(), destination) == 0 && normalized[destination.size()] == '/')) continue;
        const std::string candidate = NormalizeOverlayPath(overlay.source) + normalized.substr(destination.size());
        if (!OverlayTakesSource(overlay.type, flag, candidate, normalized)) continue;
        if (candidate.size() + 1 > capacity) return SCE_FIOS_ERROR_PATH_TOO_LONG;
        if (!Fios::CopyOut(result, candidate.c_str(), candidate.size() + 1)) return SCE_FIOS_ERROR_BAD_PTR;
        break;
    }
    return SCE_FIOS_OK;
}

int APS5_VABI sceFiosResolveSync(const SceFiosOpAttr*, const void* request, void* result) {
    if (request == nullptr || result == nullptr) return SCE_FIOS_ERROR_BAD_PTR;
    std::uint8_t copied[0x410];
    if (!Fios::CopyIn(copied, request, sizeof(copied)) || !Fios::CopyOut(result, copied, sizeof(copied))) return SCE_FIOS_ERROR_BAD_PTR;
    return SCE_FIOS_OK;
}

SceFiosOp APS5_VABI sceFiosResolve(const SceFiosOpAttr* attr, const void* request, void* result) {
    if (!Fios::IsInitialized(nullptr)) return 0;
    Fios::OpInfo info;
    return Fios::Submit(attr, std::move(info), [request, result](Fios::Completion& completion) {
        if (request == nullptr || result == nullptr) {
            completion.error = SCE_FIOS_ERROR_BAD_PTR;
            return;
        }
        std::uint8_t copied[0x410];
        if (!Fios::CopyIn(copied, request, sizeof(copied)) || !Fios::CopyOut(result, copied, sizeof(copied))) completion.error = SCE_FIOS_ERROR_BAD_PTR;
    }, Fios::Async);
}

bool APS5_VABI sceFiosOpIsDone(SceFiosOp op) {
    return Fios::OpDone(op);
}

int APS5_VABI sceFiosOpWait(SceFiosOp op) {
    return Fios::Wait(op);
}

int APS5_VABI sceFiosOpWaitUntil(SceFiosOp op, SceFiosTime deadline) {
    return Fios::WaitUntil(op, deadline);
}

int APS5_VABI sceFiosOpSetBuffer(SceFiosOp op, void* buffer) {
    return Fios::SetOpBuffer(op, buffer);
}

void APS5_VABI sceFiosOpDelete(SceFiosOp op) {
    Fios::DeleteOp(op);
}

int APS5_VABI sceFiosOpSyncWait(SceFiosOp op) {
    Fios::Completion completion;
    if (!Fios::SyncWait(op, completion)) return SCE_FIOS_ERROR_BAD_OP;
    return completion.error != SCE_FIOS_OK ? completion.error : static_cast<std::int32_t>(completion.result);
}

std::int64_t APS5_VABI sceFiosOpSyncWaitForIO(SceFiosOp op) {
    Fios::Completion completion;
    if (!Fios::SyncWait(op, completion)) return SCE_FIOS_ERROR_BAD_OP;
    return completion.error != SCE_FIOS_OK ? completion.error : completion.result;
}

int APS5_VABI sceFiosOpGetError(SceFiosOp op) {
    std::int32_t error = SCE_FIOS_ERROR_BAD_OP;
    Fios::OpError(op, error);
    return error;
}

void APS5_VABI sceFiosOpCancel(SceFiosOp op) {
    Fios::CancelOp(op);
}

bool APS5_VABI sceFiosOpIsCancelled(SceFiosOp op) {
    return Fios::OpIsCancelled(op);
}

const SceFiosOpAttr* APS5_VABI sceFiosOpGetAttr(SceFiosOp op) {
    return Fios::OpAttr(op);
}

const char* APS5_VABI sceFiosOpGetPath(SceFiosOp op) {
    return Fios::OpPath(op);
}

void* APS5_VABI sceFiosOpGetBuffer(SceFiosOp op) {
    return Fios::OpBuffer(op);
}

std::int64_t APS5_VABI sceFiosOpGetOffset(SceFiosOp op) {
    std::int64_t request = 0, actual = 0, offset = 0;
    return Fios::QueryOp(op, request, actual, offset) ? offset : SCE_FIOS_ERROR_BAD_OP;
}

std::int64_t APS5_VABI sceFiosOpGetRequestCount(SceFiosOp op) {
    std::int64_t request = 0, actual = 0, offset = 0;
    return Fios::QueryOp(op, request, actual, offset) ? request : SCE_FIOS_ERROR_BAD_OP;
}

std::int64_t APS5_VABI sceFiosOpGetActualCount(SceFiosOp op) {
    std::int64_t request = 0, actual = 0, offset = 0;
    return Fios::QueryOp(op, request, actual, offset) ? actual : SCE_FIOS_ERROR_BAD_OP;
}

}

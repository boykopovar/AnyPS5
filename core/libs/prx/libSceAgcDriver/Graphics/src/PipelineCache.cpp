#include "prx/libSceAgcDriver/Graphics/include/PipelineCache.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "ShaderCacheDirectory.hpp"
#include "ShaderDiskCache.hpp"
#include "prx/libSceAgcDriver/Graphics/src/MappedCacheFile.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace AgcDriver::Graphics {

namespace {

constexpr std::uint32_t FileMagic = 0x50565041u;
constexpr std::uint32_t FileFormat = 1;

bool profiling() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

constexpr std::size_t CheckedSaveBytes = std::size_t{64} << 20u;
constexpr std::uint64_t LargestDefaultReserveBytes = std::uint64_t{4096} << 20u;

std::atomic<std::uint64_t> mappedSaves{0};
std::atomic<std::uint64_t> mappedWindows{0};
std::atomic<std::uint64_t> postponedSaves{0};

struct HostMemory {
    std::uint64_t available = 0;
    std::uint64_t installed = 0;
};

HostMemory hostMemory() {
#ifdef _WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) return {status.ullAvailPhys, status.ullTotalPhys};
#endif
    return {};
}

std::uint64_t saveReserveBytes(const HostMemory& memory) {
    const char* text = std::getenv("APS5_PIPELINE_CACHE_SAVE_FREE_MB");
    if (text == nullptr || *text == '\0') return std::min(LargestDefaultReserveBytes, memory.installed / 16u);
    return std::strtoull(text, nullptr, 10) << 20u;
}

#ifdef _WIN32
std::optional<std::size_t> mappedSaveFromBytes() {
    const char* text = std::getenv("APS5_PIPELINE_CACHE_MAPPED_SAVE_MB");
    if (text == nullptr || *text == '\0' || std::strcmp(text, "off") == 0) return std::nullopt;
    return static_cast<std::size_t>(std::strtoull(text, nullptr, 10)) << 20u;
}

std::size_t saveWindowBytes() {
    const char* text = std::getenv("APS5_PIPELINE_CACHE_SAVE_WINDOW_KIB");
    if (text == nullptr || *text == '\0') return std::size_t{64} << 20u;
    return static_cast<std::size_t>(std::strtoull(text, nullptr, 10)) << 10u;
}
#endif

std::mutex& pathMutex(const std::filesystem::path& path) {
    struct Registry {
        std::mutex mutex;
        std::map<std::filesystem::path, std::mutex> paths;
    };
    static auto& registry = *new Registry;
    std::lock_guard lock(registry.mutex);
    return registry.paths[path];
}

}

PipelineCache::PipelineCache(const Context& context, const VkPhysicalDeviceProperties& properties, bool loadFromFile) : context(context), properties(properties) {
    static_assert(sizeof(FileHeader) == 24);
    const auto directory = ShaderRecompiler::ShaderCacheDirectory();
    if (!directory.empty()) {
        std::string uuid;
        for (const auto byte : properties.pipelineCacheUUID) {
            char digits[3];
            std::snprintf(digits, sizeof(digits), "%02x", static_cast<unsigned>(byte));
            uuid += digits;
        }
        char version[17];
        std::snprintf(version, sizeof(version), "%016llx", static_cast<unsigned long long>(ShaderRecompiler::ShaderDiskCache::SourceVersion()));
        char name[160];
        std::snprintf(name, sizeof(name), "vk-pipelines-%04x-%04x-%08x-%s.bin", properties.vendorID, properties.deviceID, properties.driverVersion, uuid.c_str());
        path = directory / version / name;
    }
    std::vector<std::byte> initialData;
    if (!path.empty()) {
        std::lock_guard pathLock(pathMutex(path));
        if (loadFromFile) load(initialData);
    }
    VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    info.initialDataSize = initialData.size();
    info.pInitialData = initialData.empty() ? nullptr : initialData.data();
    const auto create = context.Function<PFN_vkCreatePipelineCache>("vkCreatePipelineCache");
    VkResult result = create(context.device, &info, nullptr, &cache);
    if (result != VK_SUCCESS && !initialData.empty()) {
        std::fprintf(stderr, "[pipeline-cache] the driver refused %s (Vulkan result %d); starting empty\n", path.string().c_str(), static_cast<int>(result));
        info.initialDataSize = 0;
        info.pInitialData = nullptr;
        loadedBytes = 0;
        settledBytes = 0;
        settledFile.reset();
        result = create(context.device, &info, nullptr, &cache);
    }
    Check(result, "vkCreatePipelineCache");
    try {
        if (!path.empty()) saver = Libc::StartHostThread([this] { run(); });
    } catch (...) {
        context.Function<PFN_vkDestroyPipelineCache>("vkDestroyPipelineCache")(context.device, cache, nullptr);
        throw;
    }
}

PipelineCache::~PipelineCache() {
    if (saver.joinable()) {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        wake.notify_all();
        saver.join();
        save(true, true);
    }
    context.Function<PFN_vkDestroyPipelineCache>("vkDestroyPipelineCache")(context.device, cache, nullptr);
}

std::span<const std::byte> PipelineCache::File::Payload() const {
    return std::span(contents).subspan(sizeof(FileHeader));
}

PipelineCache::File PipelineCache::readFile() const {
    File file;
    if (path.empty()) {
        file.state = FileState::Missing;
        return file;
    }
    if (!ShaderRecompiler::ReadWholeFile(path, file.contents)) {
        std::error_code error;
#ifdef _WIN32
        const auto native = ShaderRecompiler::NativeCachePath_nid_no_patch(path, error);
        const bool exists = !error && std::filesystem::exists(native, error);
#else
        const bool exists = std::filesystem::exists(path, error);
#endif
        file.state = exists || error ? FileState::Unreadable : FileState::Missing;
        return file;
    }
    file.state = FileState::Invalid;
    if (file.contents.size() < sizeof(FileHeader)) {
        file.invalidReason = "truncated";
        return file;
    }
    std::memcpy(&file.header, file.contents.data(), sizeof(FileHeader));
    if (file.header.magic != FileMagic || file.header.format != FileFormat) {
        file.invalidReason = "another format";
        return file;
    }
    if (file.header.dataBytes != file.contents.size() - sizeof(FileHeader)) {
        file.invalidReason = "truncated";
        return file;
    }
    const auto data = file.Payload();
    if (ShaderRecompiler::HashBytes(data) != file.header.dataHash) {
        file.invalidReason = "checksum mismatch";
        return file;
    }
    struct VulkanHeader {
        std::uint32_t headerSize;
        std::uint32_t headerVersion;
        std::uint32_t vendorID;
        std::uint32_t deviceID;
        std::uint8_t uuid[VK_UUID_SIZE];
    } vulkan{};
    static_assert(sizeof(VulkanHeader) == 32);
    if (data.size() < sizeof(vulkan)) {
        file.invalidReason = "no Vulkan header";
        return file;
    }
    std::memcpy(&vulkan, data.data(), sizeof(vulkan));
    if (vulkan.headerSize < sizeof(vulkan) || vulkan.headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE || vulkan.vendorID != properties.vendorID || vulkan.deviceID != properties.deviceID || std::memcmp(vulkan.uuid, properties.pipelineCacheUUID, VK_UUID_SIZE) != 0) {
        file.invalidReason = "made for another device or driver";
        return file;
    }
    file.state = FileState::Valid;
    return file;
}

std::optional<PipelineCache::FileHeader> PipelineCache::readHeader() const {
    if (path.empty()) return std::nullopt;
#ifdef _WIN32
    std::error_code error;
    const auto native = ShaderRecompiler::NativeCachePath_nid_no_patch(path, error);
    if (error) return std::nullopt;
    const auto file = CreateFileW(native.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;
    struct Close { HANDLE file; ~Close() { CloseHandle(file); } } close{file};
    FileHeader header{};
    DWORD readBytes = 0;
    if (!ReadFile(file, &header, sizeof(header), &readBytes, nullptr) || readBytes != sizeof(header)) return std::nullopt;
#else
    std::ifstream file(path, std::ios::binary);
    FileHeader header{};
    if (!file.read(reinterpret_cast<char*>(&header), sizeof(header))) return std::nullopt;
#endif
    if (header.magic != FileMagic || header.format != FileFormat) return std::nullopt;
    return header;
}

void PipelineCache::load(std::vector<std::byte>& initialData) {
    auto file = readFile();
    if (file.state == FileState::Missing) return;
    if (file.state == FileState::Unreadable) {
        std::fprintf(stderr, "[pipeline-cache] cannot read %s; starting empty\n", path.string().c_str());
        return;
    }
    if (file.state == FileState::Invalid) {
        std::fprintf(stderr, "[pipeline-cache] ignoring %s: %s\n", path.string().c_str(), file.invalidReason);
        return;
    }
    initialData = std::move(file.contents);
    initialData.erase(initialData.begin(), initialData.begin() + sizeof(FileHeader));
    loadedBytes = initialData.size();
    settledBytes = initialData.size();
    settledFile = file.header;
    holdsFile = true;
    std::fprintf(stderr, "[pipeline-cache] loaded %.1f KiB from %s\n", static_cast<double>(initialData.size()) / 1024.0, path.string().c_str());
}

void PipelineCache::save(bool final, bool waitForPipelines) try {
    if (path.empty()) return;
    std::lock_guard pathLock(pathMutex(path));
    const auto getData = context.Function<PFN_vkGetPipelineCacheData>("vkGetPipelineCacheData");
    const auto snapshot = [&] { return waitForPipelines ? synchronization.LockForSnapshot() : synchronization.TryLockForSnapshot(); };
    std::size_t liveBytes = 0;
    {
        auto synchronizationLock = snapshot();
        if (!synchronizationLock.owns_lock()) return;
        if (getData(context.device, cache, &liveBytes, nullptr) != VK_SUCCESS || liveBytes == 0) return;
    }
    const auto onDisk = readHeader();
    if (liveBytes == settledBytes && onDisk == settledFile) return;
    const bool fileUnchanged = holdsFile && settledFile.has_value() && onDisk == settledFile;
    if (!fileUnchanged) holdsFile = false;
    const auto started = std::chrono::steady_clock::now();
    const auto settle = [&](const std::optional<FileHeader>& file) {
        settledBytes = liveBytes;
        settledFile = file;
    };
    auto disk = fileUnchanged ? File{} : readFile();
    if (disk.state == FileState::Unreadable) {
        std::fprintf(stderr, "[pipeline-cache] cannot read %s; not saving over it\n", path.string().c_str());
        return;
    }
    if (disk.state == FileState::Invalid) std::fprintf(stderr, "[pipeline-cache] replacing %s: %s\n", path.string().c_str(), disk.invalidReason);
    std::size_t floor = fileUnchanged ? std::max<std::size_t>(loadedBytes, settledFile->dataBytes) : 0;
    VkPipelineCache merged = VK_NULL_HANDLE;
    bool mergedWithFile = false;
    struct DestroyMerged {
        const Context& context;
        VkPipelineCache& cache;
        void Reset() {
            if (cache == VK_NULL_HANDLE) return;
            context.Function<PFN_vkDestroyPipelineCache>("vkDestroyPipelineCache")(context.device, cache, nullptr);
            cache = VK_NULL_HANDLE;
        }
        ~DestroyMerged() { Reset(); }
    } destroyMerged{context, merged};
    if (disk.state == FileState::Valid) {
        const auto data = disk.Payload();
        VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        info.initialDataSize = data.size();
        info.pInitialData = data.data();
        const auto result = context.Function<PFN_vkCreatePipelineCache>("vkCreatePipelineCache")(context.device, &info, nullptr, &merged);
        if (result == VK_SUCCESS) {
            floor = std::max(loadedBytes, data.size());
            mergedWithFile = true;
        } else {
            merged = VK_NULL_HANDLE;
            std::fprintf(stderr, "[pipeline-cache] replacing %s: the driver refused it (Vulkan result %d)\n", path.string().c_str(), static_cast<int>(result));
        }
    }
    std::vector<std::byte>().swap(disk.contents);
    if (merged != VK_NULL_HANDLE) {
        auto synchronizationLock = snapshot();
        if (!synchronizationLock.owns_lock()) return;
        const auto result = context.Function<PFN_vkMergePipelineCaches>("vkMergePipelineCaches")(context.device, merged, 1, &cache);
        if (result != VK_SUCCESS) {
            std::fprintf(stderr, "[pipeline-cache] cannot merge into %s (Vulkan result %d); not saving over it\n", path.string().c_str(), static_cast<int>(result));
            return;
        }
    }
    const auto source = merged != VK_NULL_HANDLE ? merged : cache;
    std::size_t size = 0;
    std::vector<std::byte> file;
    std::byte* contents = nullptr;
#ifdef _WIN32
    std::unique_ptr<MappedCacheFile> mapped;
#endif
    {
        auto synchronizationLock = source == cache ? snapshot() : PipelineCacheSynchronization::SnapshotLock{};
        if (source == cache && !synchronizationLock.owns_lock()) return;
        if (getData(context.device, source, &size, nullptr) != VK_SUCCESS || size == 0) return;
        if (merged != VK_NULL_HANDLE && size == disk.header.dataBytes) {
            settle(disk.header);
            return;
        }
        const auto affordable = [&](std::uint64_t held) {
            if (final || size < CheckedSaveBytes) return true;
            const auto memory = hostMemory();
            const auto reserve = saveReserveBytes(memory);
            if (reserve == 0 || memory.available == 0 || memory.available >= reserve + held) return true;
            const auto postponed = postponedSaves.fetch_add(1, std::memory_order_relaxed) + 1;
            static std::atomic<std::int64_t> nextReport{0};
            const auto now = static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
            auto due = nextReport.load(std::memory_order_relaxed);
            if (now >= due && nextReport.compare_exchange_strong(due, now + 60, std::memory_order_relaxed)) std::fprintf(stderr, "[pipeline-cache] not saving %.1f KiB now: the host has %.0f MiB of memory free, the save would hold %.0f MiB and %.0f MiB are to stay free (APS5_PIPELINE_CACHE_SAVE_FREE_MB); %llu saves put off so far\n", static_cast<double>(size) / 1024.0, static_cast<double>(memory.available) / 1048576.0, static_cast<double>(held) / 1048576.0, static_cast<double>(reserve) / 1048576.0, static_cast<unsigned long long>(postponed));
            return false;
        };
#ifdef _WIN32
        if (const auto from = mappedSaveFromBytes(); from.has_value() && size >= *from) {
            const auto window = saveWindowBytes();
            if (!affordable(window != 0 ? std::min<std::uint64_t>(size, 4 * static_cast<std::uint64_t>(window)) : size)) return;
            std::error_code error;
            const auto native = ShaderRecompiler::NativeCachePath_nid_no_patch(path, error);
            if (!error) mapped = MappedCacheFile::Create(native, sizeof(FileHeader) + size, window, [](std::function<void()> body) { return Libc::StartHostThread(std::move(body)); });
            if (mapped != nullptr) contents = mapped->Data();
            else std::fprintf(stderr, "[pipeline-cache] cannot map a new file beside %s; saving from a buffer\n", path.string().c_str());
        }
#endif
        if (contents == nullptr) {
            if (!affordable(size)) return;
            file.resize(sizeof(FileHeader) + size);
            contents = file.data();
        }
        const auto result = getData(context.device, source, &size, contents + sizeof(FileHeader));
        if (result != VK_SUCCESS && result != VK_INCOMPLETE) return;
    }
    destroyMerged.Reset();
    if (size < floor) {
        std::fprintf(stderr, "[pipeline-cache] keeping %s: the merged cache has %.1f KiB, less than the %.1f KiB on disk or loaded at start\n", path.string().c_str(), static_cast<double>(size) / 1024.0, static_cast<double>(floor) / 1024.0);
        settle(fileUnchanged ? *settledFile : disk.header);
        return;
    }
    const FileHeader header{FileMagic, FileFormat, size, ShaderRecompiler::HashBytes(std::span<const std::byte>(contents + sizeof(FileHeader), size))};
    std::memcpy(contents, &header, sizeof(header));
    bool throughMapping = false;
#ifdef _WIN32
    if (mapped != nullptr) {
        mappedWindows.fetch_add(mapped->WindowsOpened(), std::memory_order_relaxed);
        if (!mapped->Commit(sizeof(FileHeader) + size)) {
            std::fprintf(stderr, "[pipeline-cache] cannot write %s\n", path.string().c_str());
            return;
        }
        mapped.reset();
        mappedSaves.fetch_add(1, std::memory_order_relaxed);
        throughMapping = true;
    }
#endif
    if (!throughMapping) {
        file.resize(sizeof(FileHeader) + size);
        if (!ShaderRecompiler::WriteFileAtomically(path, file)) {
            std::fprintf(stderr, "[pipeline-cache] cannot write %s\n", path.string().c_str());
            return;
        }
    }
    settle(header);
    if (source == cache) holdsFile = true;
    if (final || profiling()) std::fprintf(stderr, "[pipeline-cache] saved %.1f KiB to %s in %.1f ms%s%s%s\n", static_cast<double>(size) / 1024.0, path.string().c_str(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(), mergedWithFile ? " (merged with the file)" : "", throughMapping ? " (written through a mapped file)" : "", final ? " (teardown)" : "");
} catch (const std::bad_alloc&) {
    std::fputs("[pipeline-cache] save skipped: host memory exhausted\n", stderr);
}

std::uint64_t PipelineCache::MappedSaves() {
    return mappedSaves.load(std::memory_order_relaxed);
}

std::uint64_t PipelineCache::MappedSaveWindows() {
    return mappedWindows.load(std::memory_order_relaxed);
}

std::uint64_t PipelineCache::PostponedSaves() {
    return postponedSaves.load(std::memory_order_relaxed);
}

void PipelineCache::SaveNow() {
    save(false, true);
}

void PipelineCache::SaveUnlessBusy() {
    save(false, false);
}

void PipelineCache::run() {
    std::unique_lock lock(mutex);
    while (!wake.wait_for(lock, std::chrono::seconds(SaveIntervalSeconds), [&] { return stopping; })) {
        lock.unlock();
        save(false, false);
        lock.lock();
    }
}

}

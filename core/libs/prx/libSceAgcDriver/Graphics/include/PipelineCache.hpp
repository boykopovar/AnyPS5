#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINECACHE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINECACHE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace AgcDriver::Graphics {

class PipelineCache {
public:
    static constexpr int SaveIntervalSeconds = 10;

    PipelineCache(const Context& context, const VkPhysicalDeviceProperties& properties, bool loadFromFile = true);
    ~PipelineCache();

    PipelineCache(const PipelineCache&) = delete;
    PipelineCache& operator=(const PipelineCache&) = delete;
    VkPipelineCache Handle() const { return cache; }
    PipelineCacheSynchronization& Synchronization() { return synchronization; }
    void SaveNow();
    void SaveUnlessBusy();

    friend struct PipelineCacheTestAccess;

private:
    struct FileHeader {
        std::uint32_t magic;
        std::uint32_t format;
        std::uint64_t dataBytes;
        std::uint64_t dataHash;
        bool operator==(const FileHeader&) const = default;
    };
    enum class FileState { Missing, Unreadable, Invalid, Valid };
    struct File {
        FileState state = FileState::Missing;
        const char* invalidReason = nullptr;
        FileHeader header{};
        std::vector<std::byte> contents;
        std::span<const std::byte> Payload() const;
    };

    File readFile() const;
    std::optional<FileHeader> readHeader() const;
    void load(std::vector<std::byte>& initialData);
    void save(bool final, bool waitForPipelines);
    void run();

    Context context;
    VkPhysicalDeviceProperties properties{};
    VkPipelineCache cache = VK_NULL_HANDLE;
    PipelineCacheSynchronization synchronization;
    std::filesystem::path path;
    bool holdsFile = false;
    std::size_t loadedBytes = 0;
    std::size_t settledBytes = 0;
    std::optional<FileHeader> settledFile;
    std::mutex mutex;
    std::condition_variable wake;
    bool stopping = false;
    std::thread saver;
};

struct PipelineCacheTestAccess {
    static bool HasHeader(const PipelineCache& cache) {
        return cache.readHeader().has_value();
    }
};

}

#endif

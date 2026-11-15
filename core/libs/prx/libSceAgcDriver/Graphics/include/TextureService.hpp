#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURESERVICE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURESERVICE_HPP

#include "prx/libSceAgcDriver/Graphics/include/BindlessTable.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeviceImageKeys.hpp"
#include "BindlessImageAbi.hpp"
#include "DeviceImageTableAbi.hpp"
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace ShaderRecompiler {
struct RecompileResult;
}

namespace AgcDriver::Graphics {

class Recorder;

using BindlessUse = ShaderRecompiler::BindlessImageAbi::TableUse;
using BindlessTableRegistration = ShaderRecompiler::BindlessImageAbi::TableRegistration;

class BindlessKeyRefusal : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

enum class BindlessRefusalKind : std::uint32_t { Storage, Multisample, ColorCompare, Unmapped, NoMode, Sampler, Interface, Other, Count };

BindlessRefusalKind ClassifyBindlessRefusal(std::string_view reason);

struct BindlessPrepared {
    DeviceImageKeys::Key key{};
    std::optional<DeviceImageValue> settled;
    std::string refusal;
    bool permanent = false;
    std::uint64_t address = 0;
    std::uint64_t bytes = 0;
    std::uint64_t generation = 0;
    std::shared_ptr<void> state;
};

struct BindlessResolution {
    DeviceImageValue value;
    std::shared_ptr<void> owner;
    VkImageView view = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;
    std::shared_ptr<void> samplerOwner;
    VkSampler sampler = VK_NULL_HANDLE;
    std::uint64_t address = 0;
    std::uint64_t bytes = 0;
    std::uint64_t generation = 0;
    bool perUse = false;
    bool dcc = false;
};

class BindlessResolver {
public:
    virtual ~BindlessResolver() = default;
    virtual BindlessPrepared Prepare(const DeviceImageKeys::Key& key) = 0;
    virtual BindlessResolution Commit(const BindlessPrepared& prepared) = 0;
    virtual bool Current(const BindlessResolution& resolution) = 0;
    virtual bool RefreshGeneration(BindlessPrepared& prepared) {
        static_cast<void>(prepared);
        return false;
    }
};

class BindlessMemory {
public:
    virtual ~BindlessMemory() = default;
    virtual bool Read(std::uint64_t address, std::span<std::uint32_t> words) = 0;
    virtual std::uint64_t Collect(std::uint64_t address, std::size_t bytes) = 0;
    virtual bool Unchanged(std::uint64_t address, std::size_t bytes, std::uint64_t generation) = 0;
    virtual std::uint64_t MappingSerial() = 0;
    virtual bool Mapped(std::uint64_t address, std::size_t bytes) = 0;
};

class TextureService {
public:
    static constexpr std::uint64_t PageBytes = 4096u;
    static constexpr std::size_t MaxRejectionDetails = 64u;

    struct Options {
        bool lazy = false;
        bool worker = true;
        std::uint32_t commitChunk = 256u;
        std::uint64_t budgetBytes = 0;
        std::function<std::uint64_t()> budgetSource;
        std::uint64_t idleSubmits = 64u;
    };

    struct Counts {
        std::uint64_t registered = 0;
        std::uint64_t primed = 0;
        std::uint64_t primedRecords = 0;
        std::uint64_t primedKeys = 0;
        std::uint64_t primerNanoseconds = 0;
        std::uint64_t workerPrepared = 0;
        std::uint64_t workerCommitted = 0;
        std::uint64_t created = 0;
        std::uint64_t refused = 0;
        std::array<std::uint64_t, static_cast<std::size_t>(BindlessRefusalKind::Count)> refusals{};
        std::uint64_t keptRefusals = 0;
        std::uint64_t misses = 0;
        std::uint64_t missesResolved = 0;
        std::uint64_t pagesChanged = 0;
        std::uint64_t recordsRead = 0;
        std::uint64_t recordsChanged = 0;
        std::uint64_t revalidated = 0;
        std::uint64_t stale = 0;
        std::uint64_t servedChecked = 0;
        std::uint64_t servedStale = 0;
        std::uint64_t refreshed = 0;
        std::uint64_t dccRefreshed = 0;
        std::uint64_t retired = 0;
        std::uint64_t unmapped = 0;
        std::uint64_t evicted = 0;
        std::uint64_t fullEvictions = 0;
        std::uint64_t serves = 0;
        std::uint64_t serveNanoseconds = 0;
        std::uint64_t presentWaits = 0;
        std::uint64_t presentWaitNanoseconds = 0;
        std::uint64_t queued = 0;
        std::uint64_t entries = 0;
        std::uint64_t imageSlots = 0;
        std::uint64_t samplerSlots = 0;
        std::uint64_t pinnedBytes = 0;
        std::uint64_t budgetBytes = 0;
        std::uint64_t tables = 0;
        std::uint64_t tableHostBytes = 0;
        std::uint64_t entryHostBytes = 0;
        std::uint64_t rejections = 0;
        std::uint64_t rejectionHostBytes = 0;
        std::uint64_t revalidateNanoseconds = 0;
        std::uint64_t revalidateCollectNanoseconds = 0;
        std::uint64_t revalidateCheckNanoseconds = 0;
        std::uint64_t replacePrepareNanoseconds = 0;
        std::uint64_t replaceCommitNanoseconds = 0;
        std::uint64_t publishNanoseconds = 0;
        std::uint64_t earlyNanoseconds = 0;
        std::uint64_t earlyPrepared = 0;
        std::uint64_t earlyUsed = 0;
        std::uint64_t earlyDropped = 0;
    };

    TextureService(BindlessTable& table, std::shared_ptr<BindlessResolver> resolver, std::shared_ptr<BindlessMemory> memory, Options options);
    ~TextureService();
    TextureService(const TextureService&) = delete;
    TextureService& operator=(const TextureService&) = delete;

    static Options OptionsFromEnvironment(std::uint64_t defaultBudgetBytes);

    BindlessTable& Table() { return table; }
    const BindlessTable& Table() const { return table; }
    std::uint64_t Register(const BindlessTableRegistration& registration);
    bool Primed(std::uint64_t id) const;
    std::vector<BindlessTableRegistration> Registrations() const;
    bool WaitPrimed(std::uint64_t id, std::uint32_t milliseconds);
    void Serve(std::span<const std::uint64_t> ids, Recorder& recorder);
    void Publish(Recorder& recorder) { Serve({}, recorder); }
    std::uint64_t PresentSerial(Recorder& recorder);
    void AfterPresentWait(Recorder& recorder, std::uint64_t waitedNanoseconds);
    void PrepareRevalidation();
    void Revalidate(Recorder& recorder);
    std::optional<DeviceImageValue> Find(const DeviceImageKeys::Key& key) const;
    std::shared_ptr<void> Owner(const DeviceImageKeys::Key& key) const;
    std::optional<std::string> Rejection(std::uint32_t fault) const;
    Counts Totals() const;
    std::string TakeFrameLine(std::uint64_t oldPathNanoseconds);

private:
    enum class TableState { Queued, Reading, Prepared, Done };
    struct Tracked {
        std::uint64_t id = 0;
        BindlessTableRegistration registration;
        TableState state = TableState::Queued;
        std::vector<std::uint32_t> words;
        std::vector<std::uint64_t> pageGenerations;
        std::uint64_t oldest = 0;
        std::uint64_t firstPage = 0;
        std::uint64_t span = 0;
        std::vector<BindlessPrepared> pending;
        std::uint64_t lastUsed = 0;
        std::uint64_t evictionEpoch = 0;
        std::uint64_t surfaceEpoch = ~std::uint64_t{0};
        std::vector<DeviceImageKeys::Key> surfaceKeys;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> surfaces;
        std::uint64_t registeredAt = 0;
        std::uint64_t primerNanoseconds = 0;
        bool unmapped = false;
        std::vector<DeviceImageKeys::Key> perUse;
        std::uint64_t perUseEpoch = ~std::uint64_t{0};
    };
    struct Entry {
        DeviceImageValue value;
        BindlessResolution resolution;
        std::uint64_t lastUsed = 0;
    };
    struct SlotRef {
        std::uint32_t slot = 0;
        std::uint32_t references = 0;
    };
    struct OwnerRef {
        std::uint64_t bytes = 0;
        std::uint32_t references = 0;
    };
    struct KeyHash {
        std::size_t operator()(const DeviceImageKeys::Key& key) const noexcept;
    };
    using Lock = std::unique_lock<std::mutex>;

    void workerLoop();
    void readTable(Tracked& table);
    std::vector<DeviceImageKeys::Key> recordKeys(const Tracked& table, std::uint32_t record) const;
    BindlessPrepared prepare(const DeviceImageKeys::Key& key);
    void completeTable(Tracked& table, Recorder& recorder, Lock& lock);
    void commitTable(Tracked& table, Recorder& recorder, Lock& lock, std::size_t limit);
    void commitPrepared(std::vector<BindlessPrepared>& prepared, Recorder& recorder, Lock& lock);
    void install(const DeviceImageKeys::Key& key, BindlessResolution resolution, Recorder& recorder);
    void place(const DeviceImageKeys::Key& key, BindlessResolution resolution, Recorder& recorder, std::uint64_t lastUsed);
    BindlessResolution take(const DeviceImageKeys::Key& key);
    void replace(const DeviceImageKeys::Key& key, Recorder& recorder, Lock& lock, const BindlessPrepared* early = nullptr);
    BindlessResolution settle(const BindlessPrepared& prepared);
    std::uint32_t refuse(const DeviceImageKeys::Key& key, const std::string& reason, bool permanent);
    std::uint32_t acquireImage(VkImageView view, VkImageLayout layout);
    std::uint32_t acquireSampler(VkSampler sampler);
    void releaseResolution(const BindlessResolution& resolution, Recorder& recorder);
    static void discard(const BindlessResolution& resolution, Recorder& recorder);
    void insertValue(const DeviceImageKeys::Key& key, const DeviceImageValue& value, Recorder& recorder);
    bool evictIdle(Recorder& recorder, std::size_t wanted, std::uint64_t idleBefore, std::uint64_t targetBytes);
    std::uint64_t currentBudget();
    void redecode(Tracked& table, Recorder& recorder, Lock& lock);
    void reverify(Tracked& table, Recorder& recorder, Lock& lock);
    void revalidateServed(Tracked& table, Recorder& recorder, Lock& lock);
    void resolveMisses(Recorder& recorder, Lock& lock);
    std::vector<DeviceImageMiss> unresolved(std::span<const DeviceImageMiss> misses) const;
    void refreshPerUse(std::span<const std::uint64_t> served, Recorder& recorder, Lock& lock);
    void checkMapping(Recorder& recorder);
    void retireNow(Recorder& recorder);
    std::size_t queueDepth() const;
    void hostBytes(Counts& counts) const;

    BindlessTable& table;
    std::shared_ptr<BindlessResolver> resolver;
    std::shared_ptr<BindlessMemory> memory;
    Options options;
    mutable std::mutex mutex;
    std::condition_variable changed;
    std::map<std::uint64_t, Tracked> tables;
    std::size_t preparedTables = 0;
    std::map<BindlessTableRegistration, std::uint64_t> ids;
    std::deque<std::uint64_t> queue;
    std::vector<BindlessPrepared> missPending;
    std::unordered_map<DeviceImageKeys::Key, Entry, KeyHash> entries;
    std::map<VkImageView, SlotRef> imageSlots;
    std::map<VkSampler, SlotRef> samplerSlots;
    std::map<const void*, OwnerRef> owners;
    std::set<DeviceImageKeys::Key> retiring;
    std::set<DeviceImageKeys::Key> perUseKeys;
    std::set<DeviceImageKeys::Key> permanentRefusals;
    std::set<std::string> reportedReasons;
    std::vector<std::string> rejections;
    std::map<DeviceImageKeys::Key, std::uint32_t> rejectionIndex;
    std::uint32_t cappedRejection = 0;
    std::uint64_t nextId = 1;
    std::uint64_t evictionEpoch = 0;
    std::uint64_t perUseEpoch = 0;
    std::uint64_t mappingSerial = 0;
    std::uint64_t surfaceEpoch = 0;
    std::uint64_t pinnedBytes = 0;
    std::uint64_t budgetBytes = 0;
    std::uint64_t lastRings = 0;
    std::uint64_t lastDropped = 0;
    std::uint64_t lastRecords = 0;
    std::uint64_t lastSeenClears = 0;
    std::uint64_t frame = 0;
    Counts totals;
    Counts reported;
    std::uint64_t reportedOldPath = 0;
    std::unordered_map<DeviceImageKeys::Key, BindlessPrepared, KeyHash> early;
    std::uint64_t earlyMapping = 0;
    bool stopping = false;
    bool takingMisses = false;
    std::thread worker;
};

std::vector<std::pair<std::uint64_t, std::uint64_t>> BindlessReadableRanges(const ShaderRecompiler::RecompileResult& program);
void PrimeBindlessTables(const Context& context, std::span<const BindlessTableRegistration> tables);
void ServeBindlessTables(const Context& context, Recorder& recorder, std::span<const BindlessTableRegistration> tables);
std::uint64_t BindlessOldPathNanoseconds();
void AddBindlessOldPathNanoseconds(std::uint64_t nanoseconds);
bool BindlessStatsRequested();

}

#endif

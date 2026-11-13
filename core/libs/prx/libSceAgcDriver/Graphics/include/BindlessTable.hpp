#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDLESSTABLE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_BINDLESSTABLE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DescriptorHeap.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeviceImageKeys.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace AgcDriver::Graphics {

class Buffer;
class Recorder;

bool BindlessImagesRequested();

struct BindlessFeatureSupport {
    bool bufferDeviceAddress = false;
    bool nullDescriptor = false;
    bool runtimeDescriptorArray = false;
    bool partiallyBound = false;
    bool sampledNonUniform = false;
    bool storageNonUniform = false;
    bool sampledUpdateAfterBind = false;
    bool storageUpdateAfterBind = false;
    bool updateUnusedWhilePending = false;
};

std::optional<std::string> BindlessUnsupported(const BindlessFeatureSupport& support);
std::uint32_t BindlessRingRecords();

class BindlessTable {
public:
    using Key = DeviceImageKeys::Key;
    static constexpr std::uint32_t DefaultRingCapacity = 65536u;
    static constexpr std::uint32_t MinSeenSlots = 1u << 22u;

    BindlessTable(const Context& context, DescriptorHeap::Capacities capacities, std::uint32_t buckets = DeviceImageKeys::InitialBuckets, std::uint32_t maxBuckets = DeviceImageKeys::MaxBuckets, std::uint32_t ringCapacity = DefaultRingCapacity);
    ~BindlessTable();
    BindlessTable(const BindlessTable&) = delete;
    BindlessTable& operator=(const BindlessTable&) = delete;

    DescriptorHeap& Heap() { return heap; }
    std::uint64_t HeaderAddress() const;
    DeviceImageKeys::Insertion Insert(const Key& key, const DeviceImageValue& value);
    std::optional<DeviceImageValue> Find(const Key& key) const;
    std::uint32_t Retire(const std::function<bool(const Key&, const DeviceImageValue&)>& drop);
    void Publish(Recorder& recorder);
    std::vector<DeviceImageMiss> TakeMisses();
    std::shared_ptr<void> HoldRing();
    void ForgetReportedMisses();
    std::uint32_t RingCapacity() const { return ringCapacity; }
    std::uint32_t SeenSlots() const { return seenSlots; }

    struct Counts {
        std::uint64_t publications = 0;
        std::uint64_t bucketUpdates = 0;
        std::uint64_t uploads = 0;
        std::uint64_t rings = 0;
        std::uint64_t drained = 0;
        std::uint64_t misses = 0;
        std::uint64_t dropped = 0;
        std::uint64_t retired = 0;
        std::uint64_t seenClears = 0;
        std::uint32_t entries = 0;
        std::uint32_t buckets = 0;
    };
    Counts Totals() const;

private:
    struct Rings {
        std::mutex mutex;
        std::vector<std::shared_ptr<Buffer>> free;
        std::vector<DeviceImageMiss> misses;
        std::uint64_t drained = 0;
        std::uint64_t written = 0;
        std::uint64_t dropped = 0;
        std::map<const Buffer*, std::uint32_t> holds;
        std::vector<std::shared_ptr<Buffer>> deferred;
        std::uint32_t capacity = 0;
        bool clearSeen = true;
    };
    static void drain(const std::shared_ptr<Rings>& rings, const std::shared_ptr<Buffer>& ring);
    static void release(const std::shared_ptr<Rings>& rings, const std::shared_ptr<Buffer>& ring);

    Context context;
    DescriptorHeap heap;
    mutable std::mutex mutex;
    DeviceImageKeys keys;
    std::shared_ptr<Buffer> header;
    std::shared_ptr<Buffer> table;
    std::uint32_t tableBuckets = 0;
    std::uint32_t ringCapacity = 0;
    std::uint32_t seenSlots = 0;
    std::shared_ptr<Buffer> seen;
    std::shared_ptr<Buffer> ring;
    std::uint64_t ringBatch = 0;
    std::shared_ptr<Rings> rings;
    Counts totals;
};

}

#endif

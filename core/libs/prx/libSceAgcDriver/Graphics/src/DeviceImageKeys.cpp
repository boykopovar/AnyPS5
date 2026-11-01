#include "prx/libSceAgcDriver/Graphics/include/DeviceImageKeys.hpp"
#include <algorithm>
#include <bit>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace AgcDriver::Graphics {

namespace Abi = ShaderRecompiler::DeviceImageTableAbi;

DeviceImageKeys::DeviceImageKeys(std::uint32_t buckets, std::uint32_t maxBuckets) : maxBuckets(maxBuckets) {
    if (!std::has_single_bit(buckets) || buckets < Abi::ProbeSteps || maxBuckets < buckets || !std::has_single_bit(maxBuckets)) throw std::invalid_argument("device image keys: the bucket counts must be powers of two of at least " + std::to_string(Abi::ProbeSteps) + " buckets, the maximum not below the start");
    words.assign(static_cast<std::size_t>(buckets) * Abi::BucketWords, 0u);
}

std::optional<std::uint32_t> DeviceImageKeys::place(std::vector<std::uint32_t>& target, const Key& key, bool& found) const {
    const auto mask = static_cast<std::uint32_t>(target.size() / Abi::BucketWords) - 1u;
    const auto home = Abi::Hash(key) & mask;
    found = false;
    for (std::uint32_t step = 0; step < Abi::ProbeSteps; ++step) {
        const auto bucket = (home + step) & mask;
        const auto* at = target.data() + static_cast<std::size_t>(bucket) * Abi::BucketWords;
        if (at[Abi::StateWord] != Abi::LiveState) return bucket;
        if (std::equal(key.begin(), key.end(), at)) {
            found = true;
            return bucket;
        }
    }
    return std::nullopt;
}

std::optional<DeviceImageValue> DeviceImageKeys::Find(const Key& key) const {
    const auto mask = Buckets() - 1u;
    const auto home = Abi::Hash(key) & mask;
    for (std::uint32_t step = 0; step < Abi::ProbeSteps; ++step) {
        const auto* at = words.data() + static_cast<std::size_t>((home + step) & mask) * Abi::BucketWords;
        if (at[Abi::StateWord] != Abi::LiveState) return std::nullopt;
        if (std::equal(key.begin(), key.end(), at)) return DeviceImageValue{at[Abi::SlotWord], at[Abi::ModeWord], at[Abi::FaultWord], at[Abi::SamplerSlotWord]};
    }
    return std::nullopt;
}

std::uint32_t DeviceImageKeys::Retain(const std::function<bool(const Key&)>& keep) {
    std::vector<std::uint32_t> kept(words.size(), 0u);
    std::uint32_t dropped = 0;
    std::uint32_t live = 0;
    for (std::uint32_t bucket = 0; bucket < Buckets(); ++bucket) {
        const auto* at = words.data() + static_cast<std::size_t>(bucket) * Abi::BucketWords;
        if (at[Abi::StateWord] != Abi::LiveState) continue;
        Key key{};
        std::copy_n(at, Abi::KeyWords, key.begin());
        if (!keep(key)) {
            ++dropped;
            continue;
        }
        bool found = false;
        const auto destination = place(kept, key, found);
        if (!destination.has_value()) throw std::runtime_error("device image keys: a retained key found no bucket in a table of the same size");
        std::copy_n(at, Abi::BucketWords, kept.begin() + static_cast<std::ptrdiff_t>(*destination) * Abi::BucketWords);
        ++live;
    }
    if (dropped == 0u) return 0u;
    words = std::move(kept);
    entries = live;
    rebuilt = true;
    changed.clear();
    return dropped;
}

bool DeviceImageKeys::grow() {
    for (auto buckets = Buckets() * 2u; buckets != 0u && buckets <= maxBuckets; buckets *= 2u) {
        std::vector<std::uint32_t> moved(static_cast<std::size_t>(buckets) * Abi::BucketWords, 0u);
        bool complete = true;
        for (std::uint32_t bucket = 0; bucket < Buckets() && complete; ++bucket) {
            const auto* at = words.data() + static_cast<std::size_t>(bucket) * Abi::BucketWords;
            if (at[Abi::StateWord] != Abi::LiveState) continue;
            Key key{};
            std::copy_n(at, Abi::KeyWords, key.begin());
            bool found = false;
            const auto destination = place(moved, key, found);
            complete = destination.has_value();
            if (complete) std::copy_n(at, Abi::BucketWords, moved.begin() + static_cast<std::ptrdiff_t>(*destination) * Abi::BucketWords);
        }
        if (!complete) continue;
        words = std::move(moved);
        rebuilt = true;
        changed.clear();
        return true;
    }
    return false;
}

DeviceImageKeys::Insertion DeviceImageKeys::Insert(const Key& key, const DeviceImageValue& value) {
    const auto store = [&](std::uint32_t bucket) {
        auto* at = words.data() + static_cast<std::size_t>(bucket) * Abi::BucketWords;
        at[Abi::SlotWord] = value.slot;
        at[Abi::ModeWord] = value.mode;
        at[Abi::FaultWord] = value.fault;
        at[Abi::SamplerSlotWord] = value.samplerSlot;
        changed.push_back(bucket);
    };
    bool found = false;
    if (const auto bucket = place(words, key, found); bucket.has_value() && found) {
        store(*bucket);
        return Insertion::Updated;
    }
    if ((entries + 1u) * 2u > Buckets() && !grow()) return Insertion::Full;
    for (;;) {
        if (const auto bucket = place(words, key, found); bucket.has_value()) {
            auto* at = words.data() + static_cast<std::size_t>(*bucket) * Abi::BucketWords;
            std::copy(key.begin(), key.end(), at);
            at[Abi::StateWord] = Abi::LiveState;
            ++entries;
            store(*bucket);
            return Insertion::Inserted;
        }
        if (!grow()) return Insertion::Full;
    }
}

void DeviceImageKeys::Clear() {
    std::fill(words.begin(), words.end(), 0u);
    entries = 0;
    rebuilt = true;
    changed.clear();
}

std::vector<std::uint32_t> DeviceImageKeys::TakeChanged() {
    std::ranges::sort(changed);
    changed.erase(std::unique(changed.begin(), changed.end()), changed.end());
    rebuilt = false;
    return std::exchange(changed, {});
}

DeviceImageRing DecodeDeviceImageRing(std::span<const std::uint32_t> ring, std::uint32_t capacity) {
    if (ring.size() < Abi::RingRecordsWord + static_cast<std::size_t>(capacity) * Abi::RecordWords) throw std::invalid_argument("device image ring: the buffer is smaller than its capacity");
    DeviceImageRing result;
    result.written = ring[Abi::RingCountWord];
    const auto records = std::min<std::uint64_t>(result.written, capacity);
    result.dropped = result.written - records;
    result.misses.reserve(static_cast<std::size_t>(records));
    for (std::uint64_t record = 0; record < records; ++record) {
        const auto* at = ring.data() + Abi::RingRecordsWord + record * Abi::RecordWords;
        DeviceImageMiss miss;
        std::copy_n(at, Abi::KeyWords, miss.key.begin());
        miss.pc = at[Abi::RecordPcWord];
        result.misses.push_back(miss);
    }
    return result;
}

std::vector<DeviceImageMiss> DistinctDeviceImageMisses(std::span<const DeviceImageMiss> misses) {
    std::vector<DeviceImageMiss> distinct;
    std::set<DeviceImageKeys::Key> seen;
    for (const auto& miss : misses) {
        if (seen.insert(miss.key).second) distinct.push_back(miss);
    }
    return distinct;
}

}

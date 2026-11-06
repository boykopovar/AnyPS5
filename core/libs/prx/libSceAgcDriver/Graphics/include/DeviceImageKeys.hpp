#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEVICEIMAGEKEYS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEVICEIMAGEKEYS_HPP

#include "DeviceImageTableAbi.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace AgcDriver::Graphics {

struct DeviceImageValue {
    std::uint32_t slot = 0;
    std::uint32_t mode = 0;
    std::uint32_t fault = 0;
    std::uint32_t samplerSlot = 0;

    bool operator==(const DeviceImageValue& other) const = default;
};

class DeviceImageKeys {
public:
    using Key = ShaderRecompiler::DeviceImageTableAbi::Key;
    static constexpr std::uint32_t InitialBuckets = 4096u;
    static constexpr std::uint32_t MaxBuckets = 1u << 17u;

    explicit DeviceImageKeys(std::uint32_t buckets = InitialBuckets, std::uint32_t maxBuckets = MaxBuckets);

    std::optional<DeviceImageValue> Find(const Key& key) const;
    enum class Insertion { Inserted, Updated, Full };
    Insertion Insert(const Key& key, const DeviceImageValue& value);
    void Clear();
    std::uint32_t Retain(const std::function<bool(const Key&)>& keep);

    std::uint32_t Buckets() const { return static_cast<std::uint32_t>(words.size() / ShaderRecompiler::DeviceImageTableAbi::BucketWords); }
    std::uint32_t Entries() const { return entries; }
    std::span<const std::uint32_t> Words() const { return words; }
    bool Rebuilt() const { return rebuilt; }
    std::vector<std::uint32_t> TakeChanged();

    template <typename TVisit>
    void ForEach(TVisit&& visit) const {
        namespace Abi = ShaderRecompiler::DeviceImageTableAbi;
        for (std::uint32_t bucket = 0; bucket < Buckets(); ++bucket) {
            const auto* at = words.data() + static_cast<std::size_t>(bucket) * Abi::BucketWords;
            if (at[Abi::StateWord] != Abi::LiveState) continue;
            Key key{};
            for (std::uint32_t word = 0; word < Abi::KeyWords; ++word) key[word] = at[word];
            visit(key, DeviceImageValue{at[Abi::SlotWord], at[Abi::ModeWord], at[Abi::FaultWord], at[Abi::SamplerSlotWord]});
        }
    }

private:
    std::optional<std::uint32_t> place(std::vector<std::uint32_t>& target, const Key& key, bool& found) const;
    bool grow();

    std::vector<std::uint32_t> words;
    std::uint32_t maxBuckets;
    std::uint32_t entries = 0;
    bool rebuilt = true;
    std::vector<std::uint32_t> changed;
};

struct DeviceImageMiss {
    DeviceImageKeys::Key key{};
    std::uint32_t pc = 0;
};

struct DeviceImageRing {
    std::vector<DeviceImageMiss> misses;
    std::uint64_t written = 0;
    std::uint64_t dropped = 0;
};

DeviceImageRing DecodeDeviceImageRing(std::span<const std::uint32_t> ring, std::uint32_t capacity);
std::vector<DeviceImageMiss> DistinctDeviceImageMisses(std::span<const DeviceImageMiss> misses);

}

#endif

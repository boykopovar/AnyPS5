#ifndef CORE_SHADER_RECOMPILER_RESULTMEMOADMISSION_HPP
#define CORE_SHADER_RECOMPILER_RESULTMEMOADMISSION_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace ShaderRecompiler::Detail {

inline std::uint64_t ResultMemoIndex(std::uint64_t variant, std::uint64_t hash) {
    return (variant * 0x9e3779b97f4a7c15ull) ^ hash;
}

class ResultMemoAdmission {
public:
    bool Observe(std::uint64_t variant, std::uint64_t hash) {
        const Key key{variant, hash};
        auto bucket = find(key);
        if (buckets[bucket] != 0) return true;
        if (size == entries.size()) {
            erase(entries[next]);
            bucket = find(key);
        } else {
            ++size;
        }
        entries[next] = key;
        buckets[bucket] = static_cast<std::uint16_t>(next + 1);
        next = (next + 1) % entries.size();
        return false;
    }

private:
    struct Key {
        std::uint64_t variant;
        std::uint64_t hash;
        bool operator==(const Key&) const = default;
    };
    static constexpr std::size_t bucketMask = 511;
    static std::size_t homeBucket(const Key& key) {
        const auto hash = ResultMemoIndex(key.variant, key.hash);
        return (hash ^ (hash >> 33u)) & bucketMask;
    }
    std::size_t find(const Key& key) const {
        auto bucket = homeBucket(key);
        while (buckets[bucket] != 0 && entries[buckets[bucket] - 1] != key) bucket = (bucket + 1) & bucketMask;
        return bucket;
    }
    void erase(const Key& key) {
        auto hole = find(key);
        auto scan = (hole + 1) & bucketMask;
        while (buckets[scan] != 0) {
            const auto home = homeBucket(entries[buckets[scan] - 1]);
            if (((scan - home) & bucketMask) >= ((scan - hole) & bucketMask)) {
                buckets[hole] = buckets[scan];
                hole = scan;
            }
            scan = (scan + 1) & bucketMask;
        }
        buckets[hole] = 0;
    }
    std::array<Key, 256> entries{};
    std::array<std::uint16_t, bucketMask + 1> buckets{};
    std::size_t next = 0;
    std::size_t size = 0;
};

}

#endif

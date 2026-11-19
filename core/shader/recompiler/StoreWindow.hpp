#ifndef CORE_SHADER_RECOMPILER_STOREWINDOW_HPP
#define CORE_SHADER_RECOMPILER_STOREWINDOW_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ShaderRecompiler {

inline constexpr std::uint32_t StoreTableUnbounded = 0xffffffffu;
inline constexpr std::uint32_t StoreTableMaxKeys = 4096u;
inline constexpr std::uint32_t StoreTableMaxListedKeys = 64u;

enum class StoreTableFallback : std::uint32_t { None, Unmatched, TooMany, NotBuffer, Unevaluated };

enum class StoreTableMiss : std::uint32_t { None, NoStores, NotHandle, NotTableRead, SplitRead, NotAffine, NoSource, UnboundedKey, AddressStore, GcnShader, Count };

inline constexpr std::array<const char*, static_cast<std::size_t>(StoreTableMiss::Count)> StoreTableMissNames{"none", "no stores", "not a V# handle", "V# not a table read", "V# dwords split", "key not affine", "table not evaluable", "unbounded key", "address store", "GCN shader"};

inline const char* StoreTableMissName(StoreTableMiss miss) {
    const auto index = static_cast<std::size_t>(miss);
    return index < StoreTableMissNames.size() ? StoreTableMissNames[index] : "unknown";
}

struct StoreTableSnapshot {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::uint32_t keys = 0;
    std::uint32_t offset = 0;
    std::uint32_t stride = 0;
    std::uint32_t maxIndex = StoreTableUnbounded;
    std::uint32_t maxOffset = StoreTableUnbounded;
    std::uint32_t maxSoffset = StoreTableUnbounded;
    std::uint32_t pc = 0;
    std::array<std::uint32_t, 4> descriptor{};
    std::vector<std::uint32_t> keyList;
    bool address = false;
    bool direct = false;
    bool scalar = false;

    bool operator==(const StoreTableSnapshot& other) const = default;
};

struct StoreTablesSnapshot {
    std::vector<StoreTableSnapshot> tables;
    std::uint64_t shader = 0;
    std::uint64_t wantedKeys = 0;
    std::uint32_t missPc = 0;
    StoreTableFallback fallback = StoreTableFallback::Unmatched;
    StoreTableMiss miss = StoreTableMiss::None;

    bool operator==(const StoreTablesSnapshot& other) const = default;
};

}

#endif

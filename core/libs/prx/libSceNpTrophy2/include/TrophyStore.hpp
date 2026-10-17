#ifndef CORE_LIBS_PRX_LIBSCENPTROPHY2_INCLUDE_TROPHYSTORE_HPP
#define CORE_LIBS_PRX_LIBSCENPTROPHY2_INCLUDE_TROPHYSTORE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct NpTrophy2Unlock {
    std::string titleId;
    std::int32_t userId = 0;
    std::uint32_t serviceLabel = 0;
    std::int32_t trophyId = 0;
    std::uint64_t tick = 0;
};

using NpTrophy2UnlockListener = void (*)(void* context, const NpTrophy2Unlock& unlock);

struct NpTrophy2EventProperty {
    std::string path;
    std::optional<std::uint64_t> value;
    std::string text;
};

struct NpTrophy2Event {
    std::string name;
    std::vector<NpTrophy2EventProperty> properties;
};

namespace NpTrophy2 {

inline constexpr char TrophyRootGuest[] = "/_trophy";
inline constexpr std::uint64_t UnixEpochTick = 62135596800000000ull;

std::uint64_t CurrentTick();

class TrophyStore {
public:
    explicit TrophyStore(std::filesystem::path directory);

    bool Unlock(std::int32_t userId, std::uint32_t serviceLabel, std::int32_t trophyId, std::uint64_t tick);
    std::optional<std::uint64_t> UnlockTick(std::int32_t userId, std::uint32_t serviceLabel, std::int32_t trophyId);
    std::size_t UnlockCount(std::int32_t userId, std::uint32_t serviceLabel);
    std::filesystem::path RecordPath(std::int32_t userId, std::uint32_t serviceLabel) const;

private:
    using Key = std::pair<std::int32_t, std::uint32_t>;
    using Unlocks = std::map<std::int32_t, std::uint64_t>;

    Unlocks& record(const Key& key);
    void save(const Key& key, const Unlocks& unlocks) const;

    std::filesystem::path directory;
    std::mutex mutex;
    std::map<Key, Unlocks> records;
};

TrophyStore& TitleStore();

class StatStore {
public:
    explicit StatStore(std::filesystem::path directory);

    void Set(std::int32_t userId, std::uint32_t serviceLabel, std::uint64_t statId, std::uint64_t value);
    std::optional<std::uint64_t> Value(std::int32_t userId, std::uint32_t serviceLabel, std::uint64_t statId);
    std::map<std::uint64_t, std::uint64_t> Values(std::int32_t userId, std::uint32_t serviceLabel);
    std::filesystem::path RecordPath(std::int32_t userId, std::uint32_t serviceLabel) const;

private:
    using Key = std::pair<std::int32_t, std::uint32_t>;
    using Stats = std::map<std::uint64_t, std::uint64_t>;

    Stats& record(const Key& key);

    std::filesystem::path directory;
    std::mutex mutex;
    std::map<Key, Stats> records;
};

StatStore& TitleStats();

struct ContextOwner {
    std::int32_t userId = 0;
    std::uint32_t serviceLabel = 0;
};

int CreateContext(std::int32_t userId, std::uint32_t serviceLabel);
void DestroyContext(int context);
std::optional<ContextOwner> FindContext(int context);
std::optional<std::uint64_t> ContextUnlockTick(int context, std::int32_t trophyId);
std::size_t ContextUnlockCount(int context);
bool RecordUnlock(std::int32_t userId, std::uint32_t serviceLabel, std::int32_t trophyId);

}

extern "C" {

bool NpTrophy2RecordUnlock_nid_no_patch(std::int32_t userId, std::uint32_t serviceLabel, std::int32_t trophyId);
std::uint64_t NpTrophy2AddUnlockListener_nid_no_patch(NpTrophy2UnlockListener listener, void* context);
void NpTrophy2RemoveUnlockListener_nid_no_patch(std::uint64_t id);
void NpTrophy2ApplyEvent_nid_no_patch(std::int32_t userId, std::uint32_t serviceLabel, const NpTrophy2Event& event);

}

#endif

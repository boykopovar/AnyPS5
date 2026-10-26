#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"
#include "prx/libSceNpTrophy2/include/TitleTrophies.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

namespace NpTrophy2 {

namespace {

template <typename TValue>
bool parseNumber(std::string_view text, TValue& value) {
    const auto* end = text.data() + text.size();
    const auto [stop, error] = std::from_chars(text.data(), end, value);
    return error == std::errc{} && stop == end;
}

template <typename TKey>
bool negative(TKey key) {
    if constexpr (std::is_signed_v<TKey>) return key < 0;
    return false;
}

template <typename TKey>
std::map<TKey, std::uint64_t> readRecord(const std::filesystem::path& path, const std::string& what, const char* pair) {
    std::map<TKey, std::uint64_t> values;
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) throw std::filesystem::filesystem_error("cannot check the " + what, path, error);
    if (!exists) return values;
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot read the " + what + " " + path.string());
    std::string line;
    std::size_t number = 0;
    while (std::getline(file, line)) {
        ++number;
        const auto space = line.find(' ');
        TKey key{};
        std::uint64_t value = 0;
        const std::string_view text(line);
        if (space == std::string::npos || !parseNumber(text.substr(0, space), key) || negative(key) || !parseNumber(text.substr(space + 1), value) || !values.emplace(key, value).second) {
            throw std::runtime_error("the " + what + " " + path.string() + " has a line " + std::to_string(number) + " that is not a new '" + pair + "' pair");
        }
    }
    if (file.bad()) throw std::runtime_error("cannot read the " + what + " " + path.string());
    return values;
}

template <typename TKey>
void writeRecord(const std::filesystem::path& directory, const std::filesystem::path& path, const std::map<TKey, std::uint64_t>& values, const std::string& what) {
    std::error_code error;
    if (std::filesystem::create_directories(directory, error)) RecordWrittenPath_nid_no_patch(directory);
    if (error) throw std::filesystem::filesystem_error("cannot create the trophy directory", directory, error);
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        for (const auto& [key, value] : values) file << key << ' ' << value << '\n';
        file.flush();
        if (!file) throw std::runtime_error("cannot write the " + what + " " + temporary.string());
    }
    std::filesystem::rename(temporary, path);
    RecordWrittenPath_nid_no_patch(path);
}

std::string recordName(std::int32_t userId, std::uint32_t serviceLabel) {
    return std::to_string(userId) + "_" + std::to_string(serviceLabel);
}

struct Listener {
    std::uint64_t id = 0;
    NpTrophy2UnlockListener callback = nullptr;
    void* context = nullptr;
};

struct Listeners {
    std::mutex mutex;
    std::recursive_mutex dispatch;
    std::vector<Listener> entries;
    std::uint64_t next = 1;
};

bool registered(Listeners& registry, std::uint64_t id) {
    std::lock_guard lock(registry.mutex);
    return std::any_of(registry.entries.begin(), registry.entries.end(), [id](const Listener& entry) { return entry.id == id; });
}

Listeners& listeners() {
    static Listeners registry;
    return registry;
}

}

std::uint64_t CurrentTick() {
    const auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return UnixEpochTick + static_cast<std::uint64_t>(now);
}

TrophyStore::TrophyStore(std::filesystem::path directory) : directory(std::move(directory)) {}

std::filesystem::path TrophyStore::RecordPath(std::int32_t userId, std::uint32_t serviceLabel) const {
    return directory / (recordName(userId, serviceLabel) + ".txt");
}

TrophyStore::Unlocks& TrophyStore::record(const Key& key) {
    if (const auto found = records.find(key); found != records.end()) return found->second;
    auto unlocks = readRecord<std::int32_t>(RecordPath(key.first, key.second), "trophy record", "<trophy id> <unlock tick>");
    return records.emplace(key, std::move(unlocks)).first->second;
}

void TrophyStore::save(const Key& key, const Unlocks& unlocks) const {
    writeRecord(directory, RecordPath(key.first, key.second), unlocks, "trophy record");
}

bool TrophyStore::Unlock(std::int32_t userId, std::uint32_t serviceLabel, std::int32_t trophyId, std::uint64_t tick) {
    if (trophyId < 0) throw std::invalid_argument("trophy " + std::to_string(trophyId) + " cannot be unlocked: trophy ids are not negative");
    std::lock_guard lock(mutex);
    const Key key{userId, serviceLabel};
    auto& unlocks = record(key);
    if (unlocks.contains(trophyId)) return false;
    auto updated = unlocks;
    updated.emplace(trophyId, tick);
    save(key, updated);
    unlocks = std::move(updated);
    return true;
}

std::optional<std::uint64_t> TrophyStore::UnlockTick(std::int32_t userId, std::uint32_t serviceLabel, std::int32_t trophyId) {
    std::lock_guard lock(mutex);
    const auto& unlocks = record({userId, serviceLabel});
    const auto found = unlocks.find(trophyId);
    if (found == unlocks.end()) return std::nullopt;
    return found->second;
}

std::size_t TrophyStore::UnlockCount(std::int32_t userId, std::uint32_t serviceLabel) {
    std::lock_guard lock(mutex);
    return record({userId, serviceLabel}).size();
}

TrophyStore& TitleStore() {
    static TrophyStore store(ResolvePath_nid_no_patch(TrophyRootGuest));
    return store;
}

StatStore::StatStore(std::filesystem::path directory) : directory(std::move(directory)) {}

std::filesystem::path StatStore::RecordPath(std::int32_t userId, std::uint32_t serviceLabel) const {
    return directory / (recordName(userId, serviceLabel) + ".stats.txt");
}

StatStore::Stats& StatStore::record(const Key& key) {
    if (const auto found = records.find(key); found != records.end()) return found->second;
    auto stats = readRecord<std::uint64_t>(RecordPath(key.first, key.second), "stat record", "<stat id> <value>");
    return records.emplace(key, std::move(stats)).first->second;
}

void StatStore::Set(std::int32_t userId, std::uint32_t serviceLabel, std::uint64_t statId, std::uint64_t value) {
    std::lock_guard lock(mutex);
    const Key key{userId, serviceLabel};
    auto& stats = record(key);
    if (const auto found = stats.find(statId); found != stats.end() && found->second == value) return;
    auto updated = stats;
    updated[statId] = value;
    writeRecord(directory, RecordPath(userId, serviceLabel), updated, "stat record");
    stats = std::move(updated);
}

std::uint64_t StatStore::Raise(std::int32_t userId, std::uint32_t serviceLabel, std::uint64_t statId, std::uint64_t value) {
    std::lock_guard lock(mutex);
    const Key key{userId, serviceLabel};
    auto& stats = record(key);
    if (const auto found = stats.find(statId); found != stats.end() && found->second >= value) return found->second;
    auto updated = stats;
    updated[statId] = value;
    writeRecord(directory, RecordPath(userId, serviceLabel), updated, "stat record");
    stats = std::move(updated);
    return value;
}

std::optional<std::uint64_t> StatStore::Value(std::int32_t userId, std::uint32_t serviceLabel, std::uint64_t statId) {
    std::lock_guard lock(mutex);
    const auto& stats = record({userId, serviceLabel});
    const auto found = stats.find(statId);
    if (found == stats.end()) return std::nullopt;
    return found->second;
}

std::map<std::uint64_t, std::uint64_t> StatStore::Values(std::int32_t userId, std::uint32_t serviceLabel) {
    std::lock_guard lock(mutex);
    return record({userId, serviceLabel});
}

StatStore& TitleStats() {
    static StatStore store(ResolvePath_nid_no_patch(TrophyRootGuest));
    return store;
}

std::optional<std::uint64_t> ContextUnlockTick(int context, std::int32_t trophyId) {
    const auto owner = FindContext(context);
    if (!owner || trophyId < 0) return std::nullopt;
    return TitleStore().UnlockTick(owner->userId, owner->serviceLabel, trophyId);
}

std::size_t ContextUnlockCount(int context) {
    const auto owner = FindContext(context);
    return owner ? TitleStore().UnlockCount(owner->userId, owner->serviceLabel) : 0;
}

bool RecordUnlock(std::int32_t userId, std::uint32_t serviceLabel, std::int32_t trophyId) {
    const auto title = GetAppTitleId_nid_postfix();
    const std::string_view titleId(title.value, static_cast<std::size_t>(std::find(std::begin(title.value), std::end(title.value), '\0') - std::begin(title.value)));
    const NpTrophy2Unlock unlock{std::string(titleId), userId, serviceLabel, trophyId, CurrentTick()};
    if (!TitleStore().Unlock(userId, serviceLabel, trophyId, unlock.tick)) return false;
    std::fprintf(stderr, "[trophy] %s unlocked trophy %d for user %d\n", unlock.titleId.c_str(), trophyId, userId);
    auto& registry = listeners();
    std::lock_guard dispatching(registry.dispatch);
    std::vector<Listener> current;
    {
        std::lock_guard lock(registry.mutex);
        current = registry.entries;
    }
    for (const auto& entry : current) {
        if (registered(registry, entry.id)) entry.callback(entry.context, unlock);
    }
    const auto* package = Package(serviceLabel);
    if (package == nullptr) return true;
    if (const auto platinum = PlatinumToUnlock(*package, trophyId, UnlockedBy(userId, serviceLabel))) RecordUnlock(userId, serviceLabel, *platinum);
    return true;
}

}

extern "C" bool NpTrophy2RecordUnlock_nid_no_patch(std::int32_t userId, std::uint32_t serviceLabel, std::int32_t trophyId) {
    if (const auto* package = NpTrophy2::Package(serviceLabel)) {
        if (const auto refusal = NpTrophy2::PostedUnlockRefusal(*package, trophyId)) {
            std::fprintf(stderr, "[trophy] ignored the posted unlock of trophy %d for user %d: %s\n", trophyId, userId, refusal->c_str());
            return false;
        }
    }
    return NpTrophy2::RecordUnlock(userId, serviceLabel, trophyId);
}

extern "C" std::uint64_t NpTrophy2AddUnlockListener_nid_no_patch(NpTrophy2UnlockListener listener, void* context) {
    if (listener == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    auto& registry = NpTrophy2::listeners();
    std::lock_guard lock(registry.mutex);
    const auto id = registry.next++;
    registry.entries.push_back({id, listener, context});
    return id;
}

extern "C" void NpTrophy2RemoveUnlockListener_nid_no_patch(std::uint64_t id) {
    auto& registry = NpTrophy2::listeners();
    std::lock_guard dispatching(registry.dispatch);
    std::lock_guard lock(registry.mutex);
    const auto removed = std::erase_if(registry.entries, [id](const NpTrophy2::Listener& entry) { return entry.id == id; });
    if (removed == 0) throw std::invalid_argument("no trophy unlock listener has id " + std::to_string(id));
}

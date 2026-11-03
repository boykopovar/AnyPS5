#include "prx/libSceNpTrophy2/include/Api.hpp"
#include "prx/libSceNpTrophy2/include/NpTrophy2Types.hpp"
#include "prx/libSceNpTrophy2/include/TitleTrophies.hpp"
#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/TemporaryFile.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

constexpr std::int32_t InitialUser = 0x10000000;

void check(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template <typename TAction>
bool throws(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

std::string read(const char* path) {
    std::string text;
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) return text;
    char buffer[256];
    for (std::size_t count; (count = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, count);
    std::fclose(file);
    return text;
}

void write(const char* path, const std::string& text) {
    std::FILE* file = std::fopen(path, "wb");
    check(file != nullptr, "test file was not created");
    const bool written = std::fwrite(text.data(), 1, text.size(), file) == text.size();
    check(std::fclose(file) == 0 && written, "test file was not written");
}

void putBigEndian(std::string& data, std::size_t offset, std::uint64_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) data[offset + i] = static_cast<char>((value >> (8 * (size - 1 - i))) & 0xFF);
}

std::string ucp(const std::vector<std::pair<std::string, std::string>>& files) {
    std::size_t offset = 0x60 + files.size() * 0x40;
    std::size_t size = offset;
    for (const auto& file : files) size += file.second.size();
    std::string data(size, '\0');
    putBigEndian(data, 0, 0xB228C60A, 4);
    putBigEndian(data, 4, 1, 4);
    putBigEndian(data, 8, size, 8);
    putBigEndian(data, 0x10, files.size(), 4);
    putBigEndian(data, 0x14, 0x40, 4);
    std::size_t entry = 0x60;
    for (const auto& [name, contents] : files) {
        data.replace(entry, name.size(), name);
        putBigEndian(data, entry + 0x20, offset, 8);
        putBigEndian(data, entry + 0x28, contents.size(), 8);
        data.replace(offset, contents.size(), contents);
        offset += contents.size();
        entry += 0x40;
    }
    return data;
}

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = TemporaryFile::Open();
        check(file != nullptr, "cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        check(saved >= 0 && _dup2(_fileno(file), _fileno(stderr)) == 0, "cannot capture stderr");
#else
        saved = dup(fileno(stderr));
        check(saved >= 0 && dup2(fileno(file), fileno(stderr)) == fileno(stderr), "cannot capture stderr");
#endif
    }
    std::string Finish() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::rewind(file);
        std::string text;
        char buffer[4096];
        for (std::size_t count; (count = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, count);
        std::fclose(file);
        std::fwrite(text.data(), 1, text.size(), stderr);
        return text;
    }

private:
    std::FILE* file = nullptr;
    int saved = -1;
};

void testPersistence() {
    const std::filesystem::path directory("store");
    {
        NpTrophy2::TrophyStore store(directory);
        check(!store.UnlockTick(InitialUser, 0, 12) && store.UnlockCount(InitialUser, 0) == 0, "a new store has unlocks");
        check(store.Unlock(InitialUser, 0, 12, 1000), "trophy 12 was not unlocked");
        check(!store.Unlock(InitialUser, 0, 12, 2000), "trophy 12 was unlocked twice");
        check(store.Unlock(InitialUser, 0, 3, 3000), "trophy 3 was not unlocked");
        check(store.Unlock(InitialUser + 1, 0, 12, 4000) && store.Unlock(InitialUser, 1, 12, 5000), "another user or service label shares unlocks");
        check(throws([&store] { store.Unlock(InitialUser, 0, -1, 6000); }), "a negative trophy id was unlocked");
        check(store.RecordPath(InitialUser, 0) == directory / "268435456_0.txt", "the record is not named after the user and service label");
    }
    check(read("store/268435456_0.txt") == "3 3000\n12 1000\n", "the record does not list the unlocks");
    NpTrophy2::TrophyStore restarted(directory);
    check(restarted.UnlockTick(InitialUser, 0, 12) == std::optional<std::uint64_t>(1000) && restarted.UnlockTick(InitialUser, 0, 3) == std::optional<std::uint64_t>(3000),
          "unlock times did not survive a restart");
    check(restarted.UnlockCount(InitialUser, 0) == 2 && restarted.UnlockCount(InitialUser + 1, 0) == 1 && restarted.UnlockCount(InitialUser, 1) == 1 && restarted.UnlockCount(InitialUser, 2) == 0,
          "unlock counts did not survive a restart");
    check(!restarted.Unlock(InitialUser, 0, 12, 7000) && restarted.UnlockTick(InitialUser, 0, 12) == std::optional<std::uint64_t>(1000), "a restart forgot an unlock");
    check(restarted.Unlock(InitialUser, 0, 7, 8000) && read("store/268435456_0.txt") == "3 3000\n7 8000\n12 1000\n", "an unlock after a restart lost earlier unlocks");
}

void testDamagedRecords() {
    const std::filesystem::path directory("damaged");
    for (const char* text : {"12\n", "12 x\n", "x 5\n", "-1 5\n", "12 5\n12 6\n", "12 5 7\n", "\n", "12 5\r\n"}) {
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        write("damaged/268435456_0.txt", text);
        NpTrophy2::TrophyStore store(directory);
        check(throws([&store] { store.UnlockCount(InitialUser, 0); }), "a damaged record was read");
    }
}

struct Seen {
    std::vector<NpTrophy2Unlock> unlocks;
};

void remember(void* context, const NpTrophy2Unlock& unlock) {
    static_cast<Seen*>(context)->unlocks.push_back(unlock);
}

NpTrophy2Data trophyData(int context, int trophyId) {
    NpTrophy2Data data{};
    check(sceNpTrophy2GetTrophyInfo(context, 1, trophyId, nullptr, &data) == 0, "GetTrophyInfo failed");
    check(data.trophy_id == trophyId, "GetTrophyInfo answered for another trophy");
    return data;
}

std::uint32_t unlockedTrophies(int context) {
    NpTrophy2GameData data{};
    check(sceNpTrophy2GetGameInfo(context, 1, nullptr, &data) == 0, "GetGameInfo failed");
    return data.unlocked_trophies;
}

struct SlowListener {
    std::atomic<bool> running{false};
    std::atomic<bool> finished{false};
};

void slowly(void* context, const NpTrophy2Unlock&) {
    auto* state = static_cast<SlowListener*>(context);
    state->running = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    state->finished = true;
}

void testListenerRemovalWaitsForItsCallback() {
    SlowListener state;
    const auto listener = NpTrophy2AddUnlockListener_nid_no_patch(slowly, &state);
    std::thread unlocking([] { NpTrophy2RecordUnlock_nid_no_patch(InitialUser, 2, 1); });
    while (!state.running) std::this_thread::yield();
    NpTrophy2RemoveUnlockListener_nid_no_patch(listener);
    const bool finished = state.finished;
    unlocking.join();
    check(finished, "a listener was removed while its callback was still running, so its context could be freed under it");
}

void testQueries() {
    int context = 0;
    check(sceNpTrophy2CreateContext(&context, InitialUser, 0, 0) == 0, "context was not created");
    int other = 0;
    check(sceNpTrophy2CreateContext(&other, InitialUser + 1, 0, 0) == 0 && other != context, "second context was not created");
    const auto locked = trophyData(context, 12);
    check(!locked.unlocked && locked.timestamp_tick == 0, "trophy 12 is unlocked before its event");
    check(unlockedTrophies(context) == 0, "the game has unlocks before any event");

    Seen seen;
    const auto listener = NpTrophy2AddUnlockListener_nid_no_patch(remember, &seen);
    const auto before = NpTrophy2::CurrentTick();
    check(NpTrophy2RecordUnlock_nid_no_patch(InitialUser, 0, 12), "trophy 12 was not recorded");
    check(!NpTrophy2RecordUnlock_nid_no_patch(InitialUser, 0, 12), "trophy 12 was recorded twice");
    check(NpTrophy2RecordUnlock_nid_no_patch(InitialUser, 0, 0), "trophy 0 was not recorded");
    const auto after = NpTrophy2::CurrentTick();
    NpTrophy2RemoveUnlockListener_nid_no_patch(listener);
    check(NpTrophy2RecordUnlock_nid_no_patch(InitialUser, 0, 5) && seen.unlocks.size() == 2, "the listener did not see each new unlock once");
    const auto& first = seen.unlocks.front();
    check(first.titleId == "PPSA00000" && first.userId == InitialUser && first.serviceLabel == 0 && first.trophyId == 12 && first.tick >= before && first.tick <= after,
          "the listener did not get the unlock's title, user, trophy and time");
    check(before > NpTrophy2::UnixEpochTick, "the current tick is not counted from year 1");

    const auto unlocked = trophyData(context, 12);
    check(unlocked.unlocked && unlocked.timestamp_tick == first.tick, "trophy 12 is not reported unlocked at its time");
    check(!trophyData(context, 13).unlocked, "trophy 13 is reported unlocked");
    check(unlockedTrophies(context) == 3, "the game does not count its unlocks");
    NpTrophy2Data listed{};
    std::uint32_t count = 0;
    check(sceNpTrophy2GetTrophyInfoArray(context, 1, 0, 1, nullptr, &listed, &count) == 0 && count == 1 && listed.trophy_id == 0 && listed.unlocked,
          "the trophy list does not report trophy 0 unlocked");
    check(!trophyData(other, 12).unlocked && unlockedTrophies(other) == 0, "another user sees the unlocks");
    check(read("_trophy/268435456_0.txt").find("12 ") != std::string::npos, "the unlock was not stored in the title folder");
    check(throws([] { NpTrophy2AddUnlockListener_nid_no_patch(nullptr, nullptr); }), "a null listener was added");
    check(throws([listener] { NpTrophy2RemoveUnlockListener_nid_no_patch(listener); }), "a removed listener was removed again");
    check(sceNpTrophy2DestroyContext(context) == 0 && sceNpTrophy2DestroyContext(other) == 0, "contexts were not destroyed");
}

void testPostedUnlockRules() {
    std::filesystem::create_directories("app0/sce_sys/trophy2");
    std::filesystem::create_directories("app0/sce_sys/uds");
    write("app0/sce_sys/trophy2/trophy04.ucp", ucp({{"tropconf.json", R"({"defaultLanguage":"en-US","trophies":[
        {"id":"0","grade":"P"},
        {"id":"1","grade":"G","platinumTrophyId":"0"},
        {"id":"2","grade":"B","platinumTrophyId":"0","unlockCondition":{"udsStatId":"40","comparator":"ge","targetValue":"5","progressive":true}},
        {"id":"3","grade":"B"}]})"},
                                                     {"tropmeta_en-US.json", R"({"metadata":{"titleMetadata":{"name":"Rules"},"trophyMetadata":[]}})"}}));
    write("app0/sce_sys/uds/uds04.ucp", ucp({{"stats_definition.json", R"({"statDefinitionArray":[{"statId":40,"dataType":"uint32","aggregation":"latest"}]})"},
                                             {"stats_extraction.json", R"({"statsExtractionRuleArray":[{"condition":{"eventName":"STEPS"},"action":{"input":"$.steps","output":{"statId":40}}}]})"}}));
    check(NpTrophy2::Package(4) != nullptr, "the trophy data of service label 4 was not loaded");
    Seen seen;
    const auto listener = NpTrophy2AddUnlockListener_nid_no_patch(remember, &seen);
    StderrCapture capture;
    const bool unknown = NpTrophy2RecordUnlock_nid_no_patch(InitialUser, 4, 9);
    const bool platinum = NpTrophy2RecordUnlock_nid_no_patch(InitialUser, 4, 0);
    const bool progressive = NpTrophy2RecordUnlock_nid_no_patch(InitialUser, 4, 2);
    const auto log = capture.Finish();
    check(!unknown && !platinum && !progressive, "an unknown, platinum or progressive trophy posted through UDS was recorded");
    check(NpTrophy2::TitleStore().UnlockCount(InitialUser, 4) == 0 && seen.unlocks.empty(), "a refused unlock reached the record or a listener");
    for (const char* line : {"[trophy] ignored the posted unlock of trophy 9 for user 268435456: the title's trophy data has no such trophy\n",
                             "[trophy] ignored the posted unlock of trophy 0 for user 268435456: it is the platinum, which unlocks with the last trophy that names it\n",
                             "[trophy] ignored the posted unlock of trophy 2 for user 268435456: it is progressive and unlocks through its statistic\n"}) {
        check(log.find(line) != std::string::npos, "a refused unlock did not log its trophy and reason");
    }
    check(NpTrophy2RecordUnlock_nid_no_patch(InitialUser, 4, 1) && NpTrophy2RecordUnlock_nid_no_patch(InitialUser, 4, 3), "an ordinary trophy posted through UDS was not recorded");
    check(!NpTrophy2::TitleStore().UnlockTick(InitialUser, 4, 0), "the platinum unlocked before every trophy that names it");
    NpTrophy2ApplyEvent_nid_no_patch(InitialUser, 4, NpTrophy2Event{"STEPS", {{"steps", 5, "5"}}});
    check(NpTrophy2::TitleStore().UnlockTick(InitialUser, 4, 2).has_value(), "the progressive trophy did not unlock through its statistic");
    check(NpTrophy2::TitleStore().UnlockTick(InitialUser, 4, 0).has_value(), "the platinum did not unlock with the last trophy that names it");
    NpTrophy2RemoveUnlockListener_nid_no_patch(listener);
    std::vector<std::int32_t> seenIds;
    for (const auto& unlock : seen.unlocks) seenIds.push_back(unlock.trophyId);
    check(seenIds == std::vector<std::int32_t>{1, 3, 2, 0}, "the listener did not see the posted, statistic and platinum unlocks once, in order");
}

}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-trophy-store-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root / "app0" / "sce_sys");
    std::filesystem::current_path(root);
    write("app0/sce_sys/param.json", R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}}})");
    testPersistence();
    testDamagedRecords();
    testQueries();
    testPostedUnlockRules();
    testListenerRemovalWaitsForItsCallback();
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    std::puts("Trophy store tests passed");
    return 0;
}

#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include "prx/libSceNpTrophy2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNpTrophy2/include/NpTrophy2Types.hpp"
#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

struct NpUniversalDataSystemEvent;
struct NpUniversalDataSystemEventPropertyObject;

static constexpr int InitialUser = 0x10000000;

static const char TropConf[] = R"({"defaultLanguage":"en-US","trophies":[
    {"id":"0","grade":"P"},
    {"id":"1","grade":"G","platinumTrophyId":"0","unlockCondition":{"udsStatId":"10","comparator":"ge","targetValue":"10","progressive":false}},
    {"id":"2","grade":"S","platinumTrophyId":"0","unlockCondition":{"udsStatId":"20","comparator":"gt","targetValue":"100"}},
    {"id":"3","grade":"B","platinumTrophyId":"0","unlockCondition":{"udsStatId":"30","comparator":"le","targetValue":"5"}},
    {"id":"4","grade":"B","platinumTrophyId":"0","unlockCondition":{"udsStatId":"30","comparator":"lt","targetValue":"3"}},
    {"id":"5","grade":"B","platinumTrophyId":"0","unlockCondition":{"udsStatId":"40","comparator":"ge","targetValue":"5","progressive":true}},
    {"id":"6","grade":"B","groupId":"1","hidden":true,"hasReward":true},
    {"id":"7","grade":"B","groupId":"1"}
]})";

static const char TropMeta[] = R"({"metadata":{"titleMetadata":{"name":"Synthetic Game"},
    "trophyMetadata":[{"id":"0","name":"All of them"},{"id":"1","name":"Ten","detail":"Reach ten"},{"id":"5","name":"Five steps"},
                      {"id":"6","name":"Secret","detail":"A hidden one","reward":"A hat"}],
    "groupMetadata":[{"id":"1","name":"Expansion"}]}})";

static const char StatsDefinition[] = R"({"statDefinitionArray":[
    {"statId":10,"dataType":"uint64","aggregation":"latest"},{"statId":20,"dataType":"uint64","aggregation":"latest"},
    {"statId":30,"dataType":"uint32","aggregation":"latest"},{"statId":40,"dataType":"uint32","aggregation":"latest"}]})";

static const char SumDefinition[] = R"({"statDefinitionArray":[{"statId":10,"dataType":"uint64","aggregation":"sum"}]})";

static const char StatsExtraction[] = R"({"statsExtractionRuleArray":[
    {"condition":{"eventName":"COLLECT"},"action":{"input":"$.counter","output":{"statId":10}}},
    {"condition":{"eventName":"COLLECT"},"action":{"input":"$.total","output":{"statId":20}}},
    {"condition":{"eventName":"TIME"},"action":{"input":"$.seconds","output":{"statId":30}}},
    {"condition":{"eventName":"STEPS"},"action":{"input":"$.player.steps","output":{"statId":40}}}]})";

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

static void PutBigEndian(std::string& data, std::size_t offset, std::uint64_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) data[offset + i] = static_cast<char>((value >> (8 * (size - 1 - i))) & 0xFF);
}

static std::string Ucp(const std::vector<std::pair<std::string, std::string>>& files) {
    std::size_t offset = 0x60 + files.size() * 0x40;
    std::size_t size = offset;
    for (const auto& file : files) size += file.second.size();
    std::string data(size, '\0');
    PutBigEndian(data, 0, 0xB228C60A, 4);
    PutBigEndian(data, 4, 1, 4);
    PutBigEndian(data, 8, size, 8);
    PutBigEndian(data, 0x10, files.size(), 4);
    PutBigEndian(data, 0x14, 0x40, 4);
    std::size_t entry = 0x60;
    for (const auto& [name, contents] : files) {
        data.replace(entry, name.size(), name);
        PutBigEndian(data, entry + 0x20, offset, 8);
        PutBigEndian(data, entry + 0x28, contents.size(), 8);
        data.replace(offset, contents.size(), contents);
        offset += contents.size();
        entry += 0x40;
    }
    return data;
}

static void Write(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::FILE* file = std::fopen(path.string().c_str(), "wb");
    Require(file != nullptr, "a test file was not created");
    const bool written = std::fwrite(text.data(), 1, text.size(), file) == text.size();
    Require(std::fclose(file) == 0 && written, "a test file was not written");
}

static std::string Read(const char* path) {
    std::string text;
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) return text;
    char buffer[256];
    for (std::size_t count; (count = std::fread(buffer, 1, sizeof(buffer), file)) != 0;) text.append(buffer, count);
    std::fclose(file);
    return text;
}

static bool Exists(const char* path) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) return false;
    std::fclose(file);
    return true;
}

class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        file = std::tmpfile();
        Require(file != nullptr, "cannot open a temporary file for stderr");
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        Require(saved >= 0 && _dup2(_fileno(file), _fileno(stderr)) == 0, "cannot capture stderr");
#else
        saved = dup(fileno(stderr));
        Require(saved >= 0 && dup2(fileno(file), fileno(stderr)) == fileno(stderr), "cannot capture stderr");
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

struct UdsSession {
    int context = 0;
    int handle = 0;
};

static UdsSession OpenUds(std::uint32_t serviceLabel) {
    UdsSession uds;
    Require(sceNpUniversalDataSystemCreateContext(&uds.context, InitialUser, serviceLabel, 0) == 0, "UDS context was not created");
    Require(sceNpUniversalDataSystemCreateHandle(&uds.handle) == 0, "UDS handle was not created");
    Require(sceNpUniversalDataSystemRegisterContext(uds.context, uds.handle, 0) == 0, "UDS context was not registered");
    return uds;
}

template <typename TSet>
static void Post(const UdsSession& uds, const char* name, TSet set) {
    NpUniversalDataSystemEventPropertyObject* properties = nullptr;
    Require(sceNpUniversalDataSystemCreateEventPropertyObject(&properties) == 0, "property object was not created");
    set(properties);
    NpUniversalDataSystemEvent* event = nullptr;
    Require(sceNpUniversalDataSystemCreateEvent(name, properties, &event, nullptr) == 0, "event was not created");
    Require(sceNpUniversalDataSystemPostEvent(uds.context, uds.handle, event, 0) == 0, "event was not posted");
    Require(sceNpUniversalDataSystemDestroyEvent(event) == 0, "event was not destroyed");
    Require(sceNpUniversalDataSystemDestroyEventPropertyObject(properties) == 0, "property object was not destroyed");
}

static int TrophyContext(std::uint32_t serviceLabel) {
    int context = 0;
    Require(sceNpTrophy2CreateContext(&context, InitialUser, serviceLabel, 0) == 0, "trophy context was not created");
    return context;
}

static NpTrophy2Data Trophy(int context, int trophyId, NpTrophy2Details* details = nullptr) {
    NpTrophy2Data data{};
    Require(sceNpTrophy2GetTrophyInfo(context, 1, trophyId, details, &data) == 0, "trophy info was not returned");
    Require(data.trophy_id == trophyId, "trophy info is for another trophy");
    return data;
}

static NpTrophy2GameData Game(int context, NpTrophy2GameDetails* details = nullptr) {
    NpTrophy2GameData data{};
    Require(sceNpTrophy2GetGameInfo(context, 1, details, &data) == 0, "game info was not returned");
    return data;
}

struct Unlocks {
    std::vector<std::pair<std::uint32_t, std::int32_t>> trophies;
};

static void Remember(void* context, const NpTrophy2Unlock& unlock) {
    Require(unlock.titleId == "PPSA00000" && unlock.userId == InitialUser && unlock.tick != 0, "an unlock does not carry its title, user and time");
    static_cast<Unlocks*>(context)->trophies.emplace_back(unlock.serviceLabel, unlock.trophyId);
}

static void KeepStatsAcrossRuns(int context) {
    NpTrophy2Details details{};
    const auto steps = Trophy(context, 5, &details);
    Require(details.target.type == 1 && details.target.value == 5 && !steps.unlocked && steps.progress.type == 1 && steps.progress.value == 2,
            "the progressive trophy does not report the stat kept from the last run");
}

static void UnlockThroughStats(int context, Unlocks& unlocks) {
    const auto uds = OpenUds(0);
    Post(uds, "COLLECT", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt64(properties, "counter", 9) == 0, "counter was not set");
        Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt64(properties, "total", 100) == 0, "total was not set");
    });
    Require(!Trophy(context, 1).unlocked && !Trophy(context, 2).unlocked && unlocks.trophies.empty(), "ge or gt unlocked below its target");
    Post(uds, "COLLECT", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt32(properties, "counter", 10) == 0, "counter was not set");
    });
    Require(Trophy(context, 1).unlocked && !Trophy(context, 2).unlocked, "ge did not unlock at its target");
    Post(uds, "COLLECT", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetInt32(properties, "counter", 3) == 0, "counter was not set");
        Require(sceNpUniversalDataSystemEventPropertyObjectSetInt64(properties, "total", 101) == 0, "total was not set");
    });
    Require(Trophy(context, 2).unlocked, "gt did not unlock past its target");

    Post(uds, "TIME", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt32(properties, "seconds", 6) == 0, "seconds was not set");
    });
    Require(!Trophy(context, 3).unlocked && !Trophy(context, 4).unlocked, "le or lt unlocked above its target");
    Post(uds, "TIME", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt32(properties, "seconds", 5) == 0, "seconds was not set");
    });
    Require(Trophy(context, 3).unlocked && !Trophy(context, 4).unlocked, "le did not unlock at its target");
    Post(uds, "TIME", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetInt32(properties, "seconds", -1) == 0, "seconds was not set");
    });
    Post(uds, "TIME", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetFloat32(properties, "seconds", 1.0f) == 0, "seconds was not set");
    });
    Require(!Trophy(context, 4).unlocked, "a negative or fractional value reached lt");
    Post(uds, "TIME", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt32(properties, "seconds", 2) == 0, "seconds was not set");
    });
    Require(Trophy(context, 4).unlocked, "lt did not unlock below its target");

    Post(uds, "NOTHING", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt32(properties, "steps", 50) == 0, "steps was not set");
    });
    const auto stepped = [&uds](std::uint32_t steps) {
        Post(uds, "STEPS", [steps](NpUniversalDataSystemEventPropertyObject* properties) {
            NpUniversalDataSystemEventPropertyObject* player = nullptr;
            Require(sceNpUniversalDataSystemEventPropertyObjectSetObject(properties, "player", nullptr, &player) == 0, "player was not set");
            Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt32(player, "steps", steps) == 0, "steps was not set");
        });
    };
    stepped(4);
    const auto partway = Trophy(context, 5);
    Require(!partway.unlocked && partway.progress.type == 1 && partway.progress.value == 4 && !Trophy(context, 0).unlocked, "the progressive trophy does not report its progress");
    stepped(5);
    const auto done = Trophy(context, 5);
    Require(done.unlocked && done.progress.value == 5, "the progressive trophy did not unlock at its target");
    Require(Trophy(context, 0).unlocked, "the platinum did not unlock with the last trophy that names it");

    const std::vector<std::pair<std::uint32_t, std::int32_t>> expected{{0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 0}};
    Require(unlocks.trophies == expected, "the listener did not see each stat unlock and the platinum once, in order");
    const auto game = Game(context);
    Require(game.unlocked_trophies == 6 && game.unlocked_platinum == 1 && game.unlocked_gold == 1 && game.unlocked_silver == 1 && game.unlocked_bronze == 3 &&
                game.progress_percentage == (6 + 2 + 3) * 100 / (6 + 2 + 5),
            "the game does not count the unlocks by grade");
    Require(Read("_trophy/268435456_0.stats.txt") == "10 3\n20 101\n30 2\n40 5\n", "the stats were not stored next to the unlocks");
    const auto record = Read("_trophy/268435456_0.txt");
    for (const char* line : {"0 ", "1 ", "2 ", "3 ", "4 ", "5 "}) Require(record.find(std::string("\n") + line) != std::string::npos || record.starts_with(line), "an unlock was not stored");
}

static void IgnoreMissingAndMalformedFiles(Unlocks& unlocks) {
    const auto missing = TrophyContext(1);
    NpTrophy2GameDetails game{};
    Game(missing, &game);
    Require(std::strcmp(game.title, "Game") == 0, "a title without trophy files does not report placeholders");
    Post(OpenUds(1), "COLLECT", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt32(properties, "counter", 10) == 0, "counter was not set");
    });
    Require(!Exists("_trophy/268435456_1.stats.txt"), "a title without stat rules stored a stat");

    const auto brokenTrophies = TrophyContext(2);
    Game(brokenTrophies, &game);
    Require(std::strcmp(game.title, "Game") == 0, "a malformed trophy package was used");
    Post(OpenUds(2), "COLLECT", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt32(properties, "counter", 10) == 0, "counter was not set");
    });
    Require(Read("_trophy/268435456_2.stats.txt") == "10 10\n" && !Exists("_trophy/268435456_2.txt"), "a malformed trophy package disabled the stat rules, or stats unlocked a trophy without one");

    const auto brokenRules = TrophyContext(3);
    Game(brokenRules, &game);
    Require(std::strcmp(game.title, "Synthetic Game") == 0, "malformed stat rules disabled the trophy package");
    Post(OpenUds(3), "COLLECT", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt32(properties, "counter", 10) == 0, "counter was not set");
    });
    Require(!Exists("_trophy/268435456_3.stats.txt") && !Trophy(brokenRules, 1).unlocked, "malformed stat rules were used");
    Require(unlocks.trophies.size() == 6, "a missing or malformed file unlocked a trophy");
}

static void IgnoreOtherAggregations(Unlocks& unlocks) {
    StderrCapture capture;
    const auto context = TrophyContext(5);
    Post(OpenUds(5), "COLLECT", [](NpUniversalDataSystemEventPropertyObject* properties) {
        Require(sceNpUniversalDataSystemEventPropertyObjectSetUInt32(properties, "counter", 10) == 0, "counter was not set");
    });
    const auto log = capture.Finish();
    Require(log.find("[trophy] /app0/sce_sys/uds/uds05.ucp: stat 10 has aggregation \"sum\"; only \"latest\" is supported") != std::string::npos,
            "a stat with a sum aggregation was not reported");
    Require(!Trophy(context, 1).unlocked && !Exists("_trophy/268435456_5.stats.txt") && unlocks.trophies.size() == 6, "a stat with a sum aggregation was stored or unlocked a trophy");
}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-trophy-stats-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    Write("app0/sce_sys/param.json", R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}}})");
    const auto trophies = Ucp({{"tropconf.json", TropConf}, {"tropmeta_en-US.json", TropMeta}});
    const auto rules = Ucp({{"stats_definition.json", StatsDefinition}, {"stats_extraction.json", StatsExtraction}});
    Write("app0/sce_sys/trophy2/trophy00.ucp", trophies);
    Write("app0/sce_sys/uds/uds00.ucp", rules);
    Write("app0/sce_sys/trophy2/trophy02.ucp", "not a package");
    Write("app0/sce_sys/uds/uds02.ucp", rules);
    Write("app0/sce_sys/trophy2/trophy03.ucp", trophies);
    Write("app0/sce_sys/uds/uds03.ucp", rules.substr(0, rules.size() - 1));
    Write("app0/sce_sys/trophy2/trophy05.ucp", trophies);
    Write("app0/sce_sys/uds/uds05.ucp", Ucp({{"stats_definition.json", SumDefinition}, {"stats_extraction.json", StatsExtraction}}));
    Write("_trophy/268435456_0.stats.txt", "40 2\n");

    Unlocks unlocks;
    const auto listener = NpTrophy2AddUnlockListener_nid_no_patch(Remember, &unlocks);
    const auto context = TrophyContext(0);
    KeepStatsAcrossRuns(context);
    UnlockThroughStats(context, unlocks);
    IgnoreMissingAndMalformedFiles(unlocks);
    IgnoreOtherAggregations(unlocks);
    NpTrophy2RemoveUnlockListener_nid_no_patch(listener);

    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    std::puts("Trophy stat tests passed");
    return 0;
}

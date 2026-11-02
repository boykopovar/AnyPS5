#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include "prx/libSceNpTrophy2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNpTrophy2/include/NpTrophy2Types.hpp"
#include "prx/libSceNpTrophy2/include/TrophyStore.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct NpUniversalDataSystemEvent;
struct NpUniversalDataSystemEventPropertyObject;

static constexpr int InitialUser = 0x10000000;
static constexpr int InvalidTrophyId = static_cast<int>(0x8055390Au);

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

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template <typename TCall>
static bool ThrowsInvalidArgument(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
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

static void PostUnlock(int trophyId) {
    static int context = 0;
    static int handle = 0;
    if (context == 0) {
        Require(sceNpUniversalDataSystemCreateContext(&context, InitialUser, 0, 0) == 0, "UDS context was not created");
        Require(sceNpUniversalDataSystemCreateHandle(&handle) == 0, "UDS handle was not created");
        Require(sceNpUniversalDataSystemRegisterContext(context, handle, 0) == 0, "UDS context was not registered");
    }
    NpUniversalDataSystemEventPropertyObject* properties = nullptr;
    Require(sceNpUniversalDataSystemCreateEventPropertyObject(&properties) == 0, "property object was not created");
    Require(sceNpUniversalDataSystemEventPropertyObjectSetInt32(properties, "_trophy_id", trophyId) == 0, "trophy id was not set");
    NpUniversalDataSystemEvent* event = nullptr;
    Require(sceNpUniversalDataSystemCreateEvent("_UnlockTrophy", properties, &event, nullptr) == 0, "event was not created");
    Require(sceNpUniversalDataSystemPostEvent(context, handle, event, 0) == 0, "event was not posted");
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
    std::vector<std::int32_t> trophies;
};

static void Remember(void* context, const NpTrophy2Unlock& unlock) {
    Require(unlock.titleId == "PPSA00000" && unlock.userId == InitialUser && unlock.serviceLabel == 0 && unlock.tick != 0, "an unlock does not carry its title, user, label and time");
    static_cast<Unlocks*>(context)->trophies.push_back(unlock.trophyId);
}

static void ReportTrophyData(int context) {
    NpTrophy2GameDetails game{};
    const auto progress = Game(context, &game);
    Require(std::strcmp(game.title, "Synthetic Game") == 0 && game.num_groups == 2 && game.num_trophies == 8 && game.num_platinum == 1 && game.num_gold == 1 && game.num_silver == 1 && game.num_bronze == 5,
            "the game details are not the title's");
    Require(progress.unlocked_trophies == 0 && progress.progress_percentage == 0, "the game has unlocks before any event");

    NpTrophy2Details details{};
    Trophy(context, 1, &details);
    Require(std::strcmp(details.name, "Ten") == 0 && std::strcmp(details.description, "Reach ten") == 0 && details.trophy_grade == 2 && details.group_id == -1 && !details.hidden &&
                details.target.type == 0 && details.target.value == 0,
            "trophy 1 does not report its name, grade and group");
    Trophy(context, 6, &details);
    Require(std::strcmp(details.name, "Secret") == 0 && std::strcmp(details.reward, "A hat") == 0 && details.trophy_grade == 4 && details.group_id == 1 && details.hidden && details.has_reward,
            "trophy 6 does not report its hidden flag, reward and group");
    Trophy(context, 0, &details);
    Require(details.trophy_grade == 1 && std::strcmp(details.name, "All of them") == 0, "the platinum is not reported");
    const auto steps = Trophy(context, 5, &details);
    Require(details.target.type == 1 && details.target.value == 5 && !steps.unlocked && steps.progress.type == 1 && steps.progress.value == 0, "the progressive trophy does not report its target");
    const auto untouched = [](const void* bytes, std::size_t size) {
        const auto* first = static_cast<const unsigned char*>(bytes);
        return std::all_of(first, first + size, [](unsigned char byte) { return byte == 0x5a; });
    };
    for (const int unknownId : {8, 99, -1}) {
        NpTrophy2Details unknownDetails{};
        NpTrophy2Data unknown{};
        std::memset(&unknownDetails, 0x5a, sizeof(unknownDetails));
        std::memset(&unknown, 0x5a, sizeof(unknown));
        Require(sceNpTrophy2GetTrophyInfo(context, 1, unknownId, &unknownDetails, &unknown) == InvalidTrophyId, "a trophy that is not in the package did not return the invalid trophy id error");
        Require(untouched(&unknownDetails, sizeof(unknownDetails)) && untouched(&unknown, sizeof(unknown)), "a trophy that is not in the package wrote its details or data");
    }

    std::vector<NpTrophy2Details> list(10);
    std::vector<NpTrophy2Data> listed(10);
    std::uint32_t count = 0;
    Require(sceNpTrophy2GetTrophyInfoArray(context, 1, 0, 10, list.data(), listed.data(), &count) == 0 && count == 8, "the trophy list does not hold every trophy");
    for (std::uint32_t i = 0; i < count; ++i) Require(list[i].trophy_id == static_cast<int>(i) && listed[i].trophy_id == static_cast<int>(i), "the trophy list is not in id order");
    Require(std::strcmp(list[5].name, "Five steps") == 0 && listed[5].progress.type == 1, "the trophy list does not carry names and progress");
    Require(sceNpTrophy2GetTrophyInfoArray(context, 1, 6, 5, list.data(), nullptr, &count) == 0 && count == 2 && list[0].trophy_id == 6 && list[1].trophy_id == 7,
            "the trophy list does not start at its offset");
    Require(sceNpTrophy2GetTrophyInfoArray(context, 1, 8, 5, nullptr, listed.data(), &count) == 0 && count == 0, "the trophy list goes past its end");

    NpTrophy2GroupDetails group{};
    NpTrophy2GroupData groupData{};
    Require(sceNpTrophy2GetGroupInfo(context, 1, 1, &group, &groupData) == 0 && std::strcmp(group.title, "Expansion") == 0 && group.num_trophies == 2 && group.num_bronze == 2 && groupData.group_id == 1,
            "group 1 is not reported");
    NpTrophy2GroupData unknownGroup{};
    Require(ThrowsInvalidArgument([context, &unknownGroup] { sceNpTrophy2GetGroupInfo(context, 1, 7, nullptr, &unknownGroup); }), "a group that is not in the package was reported");
    std::vector<NpTrophy2GroupDetails> groups(4);
    Require(sceNpTrophy2GetGroupInfoArray(context, 1, 0, 4, groups.data(), nullptr, &count) == 0 && count == 2 && groups[0].group_id == -1 && std::strcmp(groups[0].title, "Synthetic Game") == 0 &&
                groups[0].num_trophies == 6 && groups[1].group_id == 1,
            "the group list does not hold the base game and group 1");
}

static void PostUnlocks(int context) {
    Unlocks unlocks;
    const auto listener = NpTrophy2AddUnlockListener_nid_no_patch(Remember, &unlocks);
    for (const int trophyId : {1, 2, 3, 6}) PostUnlock(trophyId);
    const auto partway = Game(context);
    Require(!Trophy(context, 0).unlocked && partway.unlocked_trophies == 4 && partway.unlocked_gold == 1 && partway.unlocked_silver == 1 && partway.unlocked_bronze == 2 &&
                partway.progress_percentage == (6 + 2 + 2) * 100 / (6 + 2 + 5),
            "the game does not count the unlocks by grade");
    PostUnlock(4);
    Require(!Trophy(context, 0).unlocked, "the platinum unlocked before every trophy that names it");
    for (const int trophyId : {5, 0, 42}) PostUnlock(trophyId);
    const auto refused = Game(context);
    Require(!Trophy(context, 5).unlocked && !Trophy(context, 0).unlocked && refused.unlocked_trophies == 5 && refused.unlocked_platinum == 0 && refused.unlocked_bronze == 3 &&
                refused.progress_percentage == (6 + 2 + 3) * 100 / (6 + 2 + 5),
            "a progressive, platinum or unknown trophy posted through UDS unlocked, or changed the counts");
    NpTrophy2RemoveUnlockListener_nid_no_patch(listener);
    Require(unlocks.trophies == (std::vector<std::int32_t>{1, 2, 3, 6, 4}), "the listener did not see each posted unlock once, in order, or saw a refused one");
}

static void ReportPlaceholders() {
    for (const std::uint32_t serviceLabel : {1u, 2u}) {
        const auto context = TrophyContext(serviceLabel);
        NpTrophy2GameDetails game{};
        Game(context, &game);
        NpTrophy2Details details{};
        Trophy(context, 12, &details);
        std::uint32_t count = 0;
        NpTrophy2Data listed{};
        Require(std::strcmp(game.title, "Game") == 0 && std::strcmp(details.name, "Trophy") == 0 &&
                    sceNpTrophy2GetTrophyInfoArray(context, 1, 0, 10, nullptr, &listed, &count) == 0 && count == 1,
                "a missing or malformed trophy package does not leave the placeholders");
    }
}

static void ReportExportLinked() {
    auto* volatile linked = &sceNpUniversalDataSystemIntInitialize;
    Require(linked != nullptr, "sceNpUniversalDataSystemIntInitialize is not linked");
}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-trophy-data-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    Write("app0/sce_sys/param.json", R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}}})");
    Write("app0/sce_sys/trophy2/trophy00.ucp", Ucp({{"tropconf.json", TropConf}, {"tropmeta_en-US.json", TropMeta}}));
    Write("app0/sce_sys/trophy2/trophy02.ucp", "not a package");

    const auto context = TrophyContext(0);
    ReportTrophyData(context);
    PostUnlocks(context);
    ReportPlaceholders();
    ReportExportLinked();

    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    std::puts("Trophy data tests passed");
    return 0;
}

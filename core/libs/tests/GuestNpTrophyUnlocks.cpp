#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNpTrophy2/include/NpTrophy2Types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>

struct NpUniversalDataSystemEvent;
struct NpUniversalDataSystemEventPropertyObject;

extern "C" {
int APS5_VABI sceNpUniversalDataSystemCreateContext(int*, int, uint32_t, uint64_t);
int APS5_VABI sceNpUniversalDataSystemRegisterContext(int, int, uint64_t);
int APS5_VABI sceNpUniversalDataSystemCreateHandle(int*);
int APS5_VABI sceNpUniversalDataSystemCreateEvent(const char*, const NpUniversalDataSystemEventPropertyObject*, NpUniversalDataSystemEvent**, NpUniversalDataSystemEventPropertyObject**);
int APS5_VABI sceNpUniversalDataSystemDestroyEvent(NpUniversalDataSystemEvent*);
int APS5_VABI sceNpUniversalDataSystemPostEvent(int, int, const void*, uint64_t);
int APS5_VABI sceNpUniversalDataSystemCreateEventPropertyObject(NpUniversalDataSystemEventPropertyObject**);
int APS5_VABI sceNpUniversalDataSystemDestroyEventPropertyObject(NpUniversalDataSystemEventPropertyObject*);
int APS5_VABI sceNpUniversalDataSystemEventPropertyObjectSetInt32(NpUniversalDataSystemEventPropertyObject*, const char*, int32_t);
int APS5_VABI sceNpTrophy2CreateContext(int*, int, uint32_t, uint64_t);
int APS5_VABI sceNpTrophy2GetGameInfo(int, int, NpTrophy2GameDetails*, NpTrophy2GameData*);
int APS5_VABI sceNpTrophy2GetTrophyInfo(int, int, int, NpTrophy2Details*, NpTrophy2Data*);
}

static constexpr int InitialUser = 0x10000000;

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
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

static void Post(int context, int handle, const char* name, int trophyId) {
    NpUniversalDataSystemEventPropertyObject* properties = nullptr;
    Require(sceNpUniversalDataSystemCreateEventPropertyObject(&properties) == 0, "property object was not created");
    Require(sceNpUniversalDataSystemEventPropertyObjectSetInt32(properties, "_trophy_id", trophyId) == 0, "trophy id was not set");
    NpUniversalDataSystemEvent* event = nullptr;
    Require(sceNpUniversalDataSystemCreateEvent(name, properties, &event, nullptr) == 0, "event was not created");
    Require(sceNpUniversalDataSystemPostEvent(context, handle, event, 0) == 0, "event was not posted");
    Require(sceNpUniversalDataSystemDestroyEvent(event) == 0, "event was not destroyed");
    Require(sceNpUniversalDataSystemDestroyEventPropertyObject(properties) == 0, "property object was not destroyed");
}

static NpTrophy2Data Trophy(int context, int handle, int trophyId) {
    NpTrophy2Data data{};
    Require(sceNpTrophy2GetTrophyInfo(context, handle, trophyId, nullptr, &data) == 0, "trophy info was not returned");
    Require(data.trophy_id == trophyId, "trophy info is for another trophy");
    return data;
}

static std::uint32_t Unlocked(int context, int handle) {
    NpTrophy2GameData data{};
    Require(sceNpTrophy2GetGameInfo(context, handle, nullptr, &data) == 0, "game info was not returned");
    return data.unlocked_trophies;
}

static void UnlockThroughEvents() {
    std::filesystem::create_directories("app0/sce_sys");
    std::FILE* param = std::fopen("app0/sce_sys/param.json", "wb");
    Require(param != nullptr, "param.json was not created");
    std::fputs(R"({"titleId":"PPSA00000","localizedParameters":{"en-US":{"titleName":"Example"}}})", param);
    std::fclose(param);
    int udsContext = 0;
    int handle = 0;
    Require(sceNpUniversalDataSystemCreateContext(&udsContext, InitialUser, 0, 0) == 0, "UDS context was not created");
    Require(sceNpUniversalDataSystemCreateHandle(&handle) == 0, "UDS handle was not created");
    Require(sceNpUniversalDataSystemRegisterContext(udsContext, handle, 0) == 0, "UDS context was not registered");
    int trophyContext = 0;
    Require(sceNpTrophy2CreateContext(&trophyContext, InitialUser, 0, 0) == 0, "trophy context was not created");

    Require(!Trophy(trophyContext, handle, 12).unlocked && Unlocked(trophyContext, handle) == 0, "trophy 12 is unlocked before its event");
    Post(udsContext, handle, "_UnlockTrophy", 12);
    const auto unlocked = Trophy(trophyContext, handle, 12);
    Require(unlocked.unlocked && unlocked.timestamp_tick != 0, "the posted unlock is not reported with its time");
    Require(Unlocked(trophyContext, handle) == 1, "the game does not count the posted unlock");

    Post(udsContext, handle, "_ChallengeComplete", 13);
    Require(!Trophy(trophyContext, handle, 13).unlocked, "an unknown event unlocked a trophy");
    Post(udsContext, handle, "_UnlockTrophy", 12);
    Require(Trophy(trophyContext, handle, 12).timestamp_tick == unlocked.timestamp_tick && Unlocked(trophyContext, handle) == 1, "a repeated unlock changed the record");

    Require(Read("_trophy/268435456_0.txt") == "12 " + std::to_string(unlocked.timestamp_tick) + "\n", "the unlock was not stored in the title folder");
}

int main() {
    const auto previous = std::filesystem::current_path();
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-trophy-unlocks-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    std::filesystem::current_path(root);
    UnlockThroughEvents();
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
    return 0;
}

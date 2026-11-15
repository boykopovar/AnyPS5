#include "prx/libSceNpTrophy2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNpTrophy2/include/NpTrophy2Constants.hpp"
#include "prx/libSceNpTrophy2/include/NpTrophy2Types.hpp"
#include "tests/ExportLookup.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

static void APS5_VABI OnUnlock(int, int, void*) {}

static void ResolvePendingExports(const char* path) {
    struct Export {
        int (APS5_VABI* function)(void);
        const char* name;
    };
    const Export exports[] = {
        {sceNpTrophy2SystemAbortHandle, "sceNpTrophy2SystemAbortHandle"},
        {sceNpTrophy2SystemBuildGroupIconUri, "sceNpTrophy2SystemBuildGroupIconUri"},
        {sceNpTrophy2SystemBuildRewardIconUri, "sceNpTrophy2SystemBuildRewardIconUri"},
        {sceNpTrophy2SystemBuildTrophyIconUri, "sceNpTrophy2SystemBuildTrophyIconUri"},
        {sceNpTrophy2SystemCheckCallback, "sceNpTrophy2SystemCheckCallback"},
        {sceNpTrophy2SystemCheckNetSyncTitles, "sceNpTrophy2SystemCheckNetSyncTitles"},
        {sceNpTrophy2SystemCreateHandle, "sceNpTrophy2SystemCreateHandle"},
        {sceNpTrophy2SystemDebugLockTrophy, "sceNpTrophy2SystemDebugLockTrophy"},
        {sceNpTrophy2SystemDebugUnlockTrophy, "sceNpTrophy2SystemDebugUnlockTrophy"},
        {sceNpTrophy2SystemDestroyContext, "sceNpTrophy2SystemDestroyContext"},
        {sceNpTrophy2SystemGetTrophyDetails, "sceNpTrophy2SystemGetTrophyDetails"},
        {sceNpTrophy2SystemGetTrophyGroupConf, "sceNpTrophy2SystemGetTrophyGroupConf"},
        {sceNpTrophy2SystemGetTrophySetArray, "sceNpTrophy2SystemGetTrophySetArray"},
        {sceNpTrophy2SystemGetTrophyTitleConf, "sceNpTrophy2SystemGetTrophyTitleConf"},
        {sceNpTrophy2SystemGetTrophyTitleDetails, "sceNpTrophy2SystemGetTrophyTitleDetails"},
        {sceNpTrophy2SystemGetTrpIconByUri, "sceNpTrophy2SystemGetTrpIconByUri"},
        {sceNpTrophy2SystemIsServerAvailable, "sceNpTrophy2SystemIsServerAvailable"},
        {sceNpTrophy2SystemRegisterTitleSyncedCallback, "sceNpTrophy2SystemRegisterTitleSyncedCallback"},
        {sceNpTrophy2SystemRegisterTitleUpdateCallback, "sceNpTrophy2SystemRegisterTitleUpdateCallback"},
        {sceNpTrophy2SystemRemoveAll, "sceNpTrophy2SystemRemoveAll"},
        {sceNpTrophy2SystemRemoveTitleData, "sceNpTrophy2SystemRemoveTitleData"},
        {sceNpTrophy2SystemRemoveUserData, "sceNpTrophy2SystemRemoveUserData"},
    };
    Require(ExportsResolveByName(path, exports, "sceNpTrophy2SystemNotExported"));
}

template <typename Call> static bool ThrowsInvalidArgument(Call call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template <typename Call> static bool RefusesKeepingErrno(Call call) {
    errno = 71;
    return ThrowsInvalidArgument(call) && errno == 71;
}

int main(int argc, char** argv) {
    int userdata = 0;
    Require(sceNpTrophy2UnregisterUnlockCallback() == 0);
    Require(sceNpTrophy2RegisterUnlockCallback(reinterpret_cast<void*>(&OnUnlock), &userdata) == 0);
    Require(sceNpTrophy2UnregisterUnlockCallback() == 0);
    Require(sceNpTrophy2RegisterUnlockCallback(reinterpret_cast<void*>(&OnUnlock), &userdata) == 0);
    Require(sceNpTrophy2UnregisterUnlockCallback() == 0);

    Require(sceNpTrophy2ShowTrophyList(1) == 0);

    auto* volatile linked = &sceNpTrophy2SystemCreateContext;
    Require(linked != nullptr);
    ResolvePendingExports(argc > 1 ? argv[1] : nullptr);

    NpTrophy2GameDetails details{};
    NpTrophy2GameData data{};
    Require(sceNpTrophy2GetGameInfo(1, 1, &details, nullptr) == 0);
    Require(details.num_trophies != 0);
    Require(sceNpTrophy2GetGameInfo(1, 1, nullptr, &data) == 0);
    Require(ThrowsInvalidArgument([] { sceNpTrophy2GetGameInfo(1, 1, nullptr, nullptr); }));

    NpTrophy2GroupDetails groupDetails{};
    NpTrophy2GroupData groupData{};
    Require(sceNpTrophy2GetGroupInfo(1, 1, 2, &groupDetails, nullptr) == 0);
    Require(groupDetails.group_id == 2);
    Require(sceNpTrophy2GetGroupInfo(1, 1, 2, nullptr, &groupData) == 0);
    Require(groupData.group_id == 2);
    Require(ThrowsInvalidArgument([] { sceNpTrophy2GetGroupInfo(1, 1, 2, nullptr, nullptr); }));

    NpTrophy2Details trophyDetails{};
    NpTrophy2Data trophyData{};
    Require(sceNpTrophy2GetTrophyInfo(1, 1, 3, &trophyDetails, nullptr) == 0);
    Require(trophyDetails.trophy_id == 3);
    Require(sceNpTrophy2GetTrophyInfo(1, 1, 3, nullptr, &trophyData) == 0);
    Require(trophyData.trophy_id == 3);
    Require(ThrowsInvalidArgument([] { sceNpTrophy2GetTrophyInfo(1, 1, 3, nullptr, nullptr); }));
    std::size_t iconSize = 1234;
    Require(sceNpTrophy2GetRewardIcon(1, 1, 1, nullptr, &iconSize) == SCE_NP_TROPHY2_ERROR_ICON_FILE_NOT_FOUND);
    Require(iconSize == 0);
    Require(sceNpTrophy2GetRewardIcon(1, 1, 1, nullptr, nullptr) == SCE_NP_TROPHY2_ERROR_ICON_FILE_NOT_FOUND);

    int handle = -1;
    Require(sceNpTrophy2CreateHandle(&handle) == SCE_NP_TROPHY2_OK);
    Require(handle == NP_TROPHY2_HANDLE_DEFAULT);
    Require(sceNpTrophy2DestroyHandle(handle) == SCE_NP_TROPHY2_OK);
    Require(sceNpTrophy2RegisterContext(0, handle, 0) == SCE_NP_TROPHY2_OK);
    Require(sceNpTrophy2RegisterContext(-1, -1, ~0ull) == SCE_NP_TROPHY2_OK);
    Require(sceNpTrophy2AbortHandle(handle) == SCE_NP_TROPHY2_OK);
    Require(sceNpTrophy2AbortHandle(0) == SCE_NP_TROPHY2_OK);

    auto* bad = reinterpret_cast<void*>(0x10);
    Require(RefusesKeepingErrno([bad] { sceNpTrophy2CreateHandle(static_cast<int*>(bad)); }));
    int first = -1;
    int second = -1;
    Require(sceNpTrophy2CreateContext(&first, 1, 0, 0) == SCE_NP_TROPHY2_OK);
    Require(RefusesKeepingErrno([bad] { sceNpTrophy2CreateContext(static_cast<int*>(bad), 1, 0, 0); }));
    Require(sceNpTrophy2CreateContext(&second, 1, 0, 0) == SCE_NP_TROPHY2_OK);
    Require(second == first + 1);
    Require(sceNpTrophy2DestroyContext(first) == SCE_NP_TROPHY2_OK);
    Require(sceNpTrophy2DestroyContext(second) == SCE_NP_TROPHY2_OK);

    Require(RefusesKeepingErrno([bad] { sceNpTrophy2GetGameInfo(1, 1, static_cast<NpTrophy2GameDetails*>(bad), nullptr); }));
    Require(RefusesKeepingErrno([bad, &details] { sceNpTrophy2GetGameInfo(1, 1, &details, static_cast<NpTrophy2GameData*>(bad)); }));
    Require(RefusesKeepingErrno([bad] { sceNpTrophy2GetGroupInfo(1, 1, 2, static_cast<NpTrophy2GroupDetails*>(bad), nullptr); }));
    Require(RefusesKeepingErrno([bad] { sceNpTrophy2GetGroupInfo(1, 1, 2, nullptr, static_cast<NpTrophy2GroupData*>(bad)); }));
    Require(RefusesKeepingErrno([bad] { sceNpTrophy2GetTrophyInfo(1, 1, 3, static_cast<NpTrophy2Details*>(bad), nullptr); }));
    Require(RefusesKeepingErrno([bad] { sceNpTrophy2GetTrophyInfo(1, 1, 3, nullptr, static_cast<NpTrophy2Data*>(bad)); }));

    std::uint32_t count = 99;
    Require(RefusesKeepingErrno([bad, &count] { sceNpTrophy2GetGroupInfoArray(1, 1, 0, 1, static_cast<NpTrophy2GroupDetails*>(bad), nullptr, &count); }));
    Require(RefusesKeepingErrno([bad, &count] { sceNpTrophy2GetGroupInfoArray(1, 1, 0, 1, nullptr, static_cast<NpTrophy2GroupData*>(bad), &count); }));
    Require(RefusesKeepingErrno([bad] { sceNpTrophy2GetGroupInfoArray(1, 1, 0, 1, nullptr, nullptr, static_cast<std::uint32_t*>(bad)); }));
    Require(RefusesKeepingErrno([bad, &count] { sceNpTrophy2GetTrophyInfoArray(1, 1, 0, 1, static_cast<NpTrophy2Details*>(bad), nullptr, &count); }));
    Require(RefusesKeepingErrno([bad, &count] { sceNpTrophy2GetTrophyInfoArray(1, 1, 0, 1, nullptr, static_cast<NpTrophy2Data*>(bad), &count); }));
    Require(RefusesKeepingErrno([bad] { sceNpTrophy2GetTrophyInfoArray(1, 1, 0, 1, nullptr, nullptr, static_cast<std::uint32_t*>(bad)); }));
    Require(count == 99);
    Require(sceNpTrophy2GetTrophyInfoArray(1, 1, 0, 1, nullptr, nullptr, &count) == SCE_NP_TROPHY2_OK && count == 1);
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNpTrophy2/include/NpTrophy2Types.hpp"

#include <Testing/Test.hpp>

#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceNpTrophy2RegisterUnlockCallback(void*, void*);
int APS5_VABI sceNpTrophy2UnregisterUnlockCallback();
int APS5_VABI sceNpTrophy2GetGameInfo(int, int, NpTrophy2GameDetails*, NpTrophy2GameData*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

void APS5_VABI OnUnlock(int, int, void*) {}

void* UnlockCallback() {
    return reinterpret_cast<void*>(&OnUnlock);
}

const Case unregisterWithoutCallback{"UnregisterUnlockCallback_NothingRegistered_Succeeds", [] {
    RequireEqual(sceNpTrophy2UnregisterUnlockCallback(), 0, "unregister without a callback");
}};

const Case registerAndUnregister{"RegisterUnlockCallback_RepeatedRegisterUnregister_Succeeds", [] {
    int userdata = 0;
    for (int round = 1; round <= 2; ++round) {
        RequireEqual(sceNpTrophy2RegisterUnlockCallback(UnlockCallback(), &userdata), 0, "register in round " + std::to_string(round));
        RequireEqual(sceNpTrophy2UnregisterUnlockCallback(), 0, "unregister in round " + std::to_string(round));
    }
}};

const Case gameDetails{"GetGameInfo_DetailsOnly_ReportsTrophies", [] {
    NpTrophy2GameDetails details{};
    RequireEqual(sceNpTrophy2GetGameInfo(1, 1, &details, nullptr), 0, "get game details");
    Require(details.num_trophies != 0, "the game details report no trophies");
}};

const Case gameData{"GetGameInfo_DataOnly_Succeeds", [] {
    NpTrophy2GameData data{};
    RequireEqual(sceNpTrophy2GetGameInfo(1, 1, nullptr, &data), 0, "get game data");
}};

const Case noOutputs{"GetGameInfo_NoOutputs_ThrowsInvalidArgument", [] {
    RequireThrows<std::invalid_argument>([] { sceNpTrophy2GetGameInfo(1, 1, nullptr, nullptr); }, "get game info without outputs");
}};

} // namespace

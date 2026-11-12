#include "prx/libScePlayerSelectionDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libSceUserService/UserService.hpp"

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005);
constexpr int kErrInvalidArg = static_cast<int>(0x80B8000A);
constexpr int kErrNotRunning = static_cast<int>(0x80B8000B);
constexpr int kErrArgNull = static_cast<int>(0x80B8000D);
constexpr int kResultUserCanceled = 1;

}

int main() {
    Require(sceUserServiceInitialize(nullptr) == 0);
    struct GuardedParam {
        PlayerSelectionDialogParam param;
        std::uint8_t guard[8];
    };
    GuardedParam guarded;
    std::memset(&guarded, 0xA5, sizeof(guarded));
    auto& param = guarded.param;
    PlayerSelectionDialogResult result{-1, 99, nullptr, 77, 55};

    Require(scePlayerSelectionDialogTerminate() == kErrNotInitialized);
    Require(scePlayerSelectionDialogTerminate() == kErrNotInitialized);
    Require(scePlayerSelectionDialogOpen(nullptr) == kErrArgNull);
    errno = 71;
    Require(scePlayerSelectionDialogOpen(reinterpret_cast<const PlayerSelectionDialogParam*>(0x10)) == kErrArgNull);
    Require(errno == 71);
    Require(scePlayerSelectionDialogParamInitialize(nullptr) == kErrArgNull);
    Require(scePlayerSelectionDialogParamInitialize(&param) == 0);
    errno = 71;
    Require(scePlayerSelectionDialogParamInitialize(reinterpret_cast<PlayerSelectionDialogParam*>(0x10)) == kErrArgNull);
    Require(errno == 71);
    Require(param.base.size == sizeof(param.base));
    Require(param.base.magic == static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u);
    Require(param.size == sizeof(param));
    Require(std::all_of(std::begin(param.base.reserved), std::end(param.base.reserved), [](std::uint8_t value) { return value == 0; }));
    Require(param.userId == 0);
    Require(std::all_of(std::begin(param.title), std::end(param.title), [](char value) { return value == 0; }));
    Require(param.maxPlayers == 0);
    Require(param.mode == 0);
    Require(param.padding0 == 0);
    Require(param.players == nullptr);
    Require(param.playerCount == 0);
    Require(std::all_of(std::begin(param.reserved), std::end(param.reserved), [](std::uint8_t value) { return value == 0; }));
    Require(param.padding1 == 0);
    Require(std::all_of(std::begin(guarded.guard), std::end(guarded.guard), [](std::uint8_t value) { return value == 0xA5; }));
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    Require(scePlayerSelectionDialogOpen(&param) == kErrNotInitialized);
    Require(scePlayerSelectionDialogClose() == kErrNotInitialized);
    Require(scePlayerSelectionDialogGetResult(nullptr) == kErrArgNull);
    Require(scePlayerSelectionDialogGetResult(&result) == kErrNotInitialized);
    Require(scePlayerSelectionDialogGetStatus() == kStatusNone);
    Require(scePlayerSelectionDialogUpdateStatus() == kStatusNone);

    Require(scePlayerSelectionDialogInitialize() == 0);
    Require(scePlayerSelectionDialogInitialize() == kErrAlreadyInitialized);
    Require(scePlayerSelectionDialogGetStatus() == kStatusInitialized);
    Require(scePlayerSelectionDialogUpdateStatus() == kStatusInitialized);
    Require(scePlayerSelectionDialogClose() == kErrNotRunning);
    Require(scePlayerSelectionDialogGetStatus() == kStatusInitialized);

    Require(scePlayerSelectionDialogOpen(nullptr) == kErrArgNull);
    errno = 71;
    Require(scePlayerSelectionDialogOpen(reinterpret_cast<const PlayerSelectionDialogParam*>(0x10)) == kErrArgNull);
    Require(errno == 71);
    Require(scePlayerSelectionDialogGetResult(&result) == kErrNotFinished);

    param.base.size = sizeof(param.base) - 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.base.size = sizeof(param.base) + 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.base.size = sizeof(param.base);

    const auto savedMagic = param.base.magic;
    param.base.magic = 0;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.base.magic = savedMagic + 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.base.magic = savedMagic;

    param.base.reserved[0] = 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.base.reserved[0] = 0;
    param.base.reserved[35] = 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.base.reserved[35] = 0;

    param.size = 0;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.size = sizeof(param) - 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.size = sizeof(param);

    param.userId = -1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.userId = 0;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.userId = 42;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.userId = 999;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.userId = USER_SERVICE_INITIAL_USER_ID;

    param.maxPlayers = 257;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.maxPlayers = 256;
    Require(scePlayerSelectionDialogOpen(&param) == 0);
    Require(scePlayerSelectionDialogGetStatus() == kStatusFinished);
    Require(scePlayerSelectionDialogClose() == kErrNotRunning);

    param.maxPlayers = 1;
    param.mode = 4;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.mode = 3;
    Require(scePlayerSelectionDialogOpen(&param) == 0);
    param.mode = 0;

    param.playerCount = 257;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.playerCount = 1;
    param.players = nullptr;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);

    PlayerSelectionDialogPlayer singlePlayer{};
    param.players = &singlePlayer;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    singlePlayer.accountId = 1;
    singlePlayer.option0 = 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.maxPlayers = 2;
    singlePlayer.option0 = 2;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    singlePlayer.option0 = 1;
    singlePlayer.option1 = 2;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    singlePlayer.option1 = 1;
    singlePlayer.mode = 3;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    singlePlayer.mode = 2;
    singlePlayer.reserved[0] = 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    singlePlayer.reserved[0] = 0;
    singlePlayer.reserved[63] = 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    singlePlayer.reserved[63] = 0;
    param.reserved[0] = 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.reserved[0] = 0;
    param.reserved[63] = 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    param.reserved[63] = 0;
    Require(scePlayerSelectionDialogOpen(&param) == 0);
    Require(scePlayerSelectionDialogGetStatus() == kStatusFinished);
    Require(scePlayerSelectionDialogUpdateStatus() == kStatusFinished);

    PlayerSelectionDialogPlayer multiPlayers[2]{};
    multiPlayers[0].accountId = 1;
    multiPlayers[0].option0 = 0;
    multiPlayers[0].option1 = 0;
    multiPlayers[0].mode = 0;
    multiPlayers[1].accountId = 2;
    multiPlayers[1].option0 = 0;
    multiPlayers[1].option1 = 0;
    multiPlayers[1].mode = 0;
    param.players = multiPlayers;
    param.playerCount = 2;
    param.maxPlayers = 2;
    Require(scePlayerSelectionDialogOpen(&param) == 0);

    multiPlayers[1].accountId = 0;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    multiPlayers[1].accountId = 2;

    multiPlayers[1].option0 = 2;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    multiPlayers[1].option0 = 0;

    multiPlayers[1].option1 = 2;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    multiPlayers[1].option1 = 0;

    multiPlayers[1].mode = 3;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    multiPlayers[1].mode = 0;

    multiPlayers[1].reserved[0] = 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    multiPlayers[1].reserved[0] = 0;

    multiPlayers[1].reserved[63] = 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    multiPlayers[1].reserved[63] = 0;

    param.maxPlayers = 1;
    multiPlayers[0].option0 = 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    multiPlayers[0].option0 = 0;
    multiPlayers[1].option0 = 1;
    Require(scePlayerSelectionDialogOpen(&param) == kErrInvalidArg);
    multiPlayers[1].option0 = 0;

    param.maxPlayers = 2;
    multiPlayers[0].option0 = 1;
    multiPlayers[1].option0 = 1;
    Require(scePlayerSelectionDialogOpen(&param) == 0);

    Require(scePlayerSelectionDialogGetResult(nullptr) == kErrArgNull);
    errno = 71;
    Require(scePlayerSelectionDialogGetResult(reinterpret_cast<PlayerSelectionDialogResult*>(0x10)) == kErrArgNull);
    Require(errno == 71);
    Require(scePlayerSelectionDialogGetResult(&result) == kResultUserCanceled);
    Require(result.result == kResultUserCanceled);
    Require(result.option == 99 && result.accountIds == nullptr && result.count == 77 && result.padding == 55);

    Require(scePlayerSelectionDialogClose() == kErrNotRunning);
    Require(scePlayerSelectionDialogTerminate() == 0);
    Require(scePlayerSelectionDialogTerminate() == kErrNotInitialized);
    Require(scePlayerSelectionDialogTerminate() == kErrNotInitialized);
    Require(scePlayerSelectionDialogOpen(nullptr) == kErrArgNull);
    errno = 71;
    Require(scePlayerSelectionDialogOpen(reinterpret_cast<const PlayerSelectionDialogParam*>(0x10)) == kErrArgNull);
    Require(errno == 71);
    Require(scePlayerSelectionDialogOpen(&param) == kErrNotInitialized);
    Require(scePlayerSelectionDialogClose() == kErrNotInitialized);
    Require(scePlayerSelectionDialogGetResult(nullptr) == kErrArgNull);
    Require(scePlayerSelectionDialogGetResult(&result) == kErrNotInitialized);
    Require(scePlayerSelectionDialogGetStatus() == kStatusNone);
    Require(scePlayerSelectionDialogUpdateStatus() == kStatusNone);

    Require(scePlayerSelectionDialogInitialize() == 0);
    Require(scePlayerSelectionDialogInitialize() == kErrAlreadyInitialized);
    Require(scePlayerSelectionDialogGetStatus() == kStatusInitialized);
    Require(scePlayerSelectionDialogClose() == kErrNotRunning);
    Require(scePlayerSelectionDialogGetStatus() == kStatusInitialized);
    Require(scePlayerSelectionDialogOpen(&param) == 0);
    Require(scePlayerSelectionDialogGetStatus() == kStatusFinished);
    Require(scePlayerSelectionDialogGetResult(&result) == kResultUserCanceled);
    Require(scePlayerSelectionDialogTerminate() == 0);
    Require(scePlayerSelectionDialogTerminate() == kErrNotInitialized);
    Require(scePlayerSelectionDialogTerminate() == kErrNotInitialized);
    Require(sceUserServiceTerminate() == 0);
}

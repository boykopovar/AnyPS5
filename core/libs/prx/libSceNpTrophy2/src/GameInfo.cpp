#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

#include "prx/libSceNpTrophy2/include/NpTrophy2.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2GetGameInfo(int context, int handle, NpTrophy2GameDetails* details, NpTrophy2GameData* data) {
    if (details == nullptr || data == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)context;
    (void)handle;
    if (details != nullptr) {
        std::memset(details, 0, sizeof(*details));
        details->num_groups = NP_TROPHY2_NUM_GROUPS;
        details->num_trophies = NP_TROPHY2_NUM_TROPHIES;
        details->num_platinum = NP_TROPHY2_NUM_PLATINUM;
        details->num_gold = NP_TROPHY2_NUM_GOLD;
        details->num_silver = NP_TROPHY2_NUM_SILVER;
        details->num_bronze = NP_TROPHY2_NUM_BRONZE;
        std::strncpy(details->title, NP_TROPHY2_GAME_TITLE, sizeof(details->title) - 1);
    }
    if (data != nullptr) {
        std::memset(data, 0, sizeof(*data));
        data->unlocked_trophies = NP_TROPHY2_UNLOCKED_TROPHIES;
        data->unlocked_platinum = NP_TROPHY2_UNLOCKED_PLATINUM;
        data->unlocked_gold = NP_TROPHY2_UNLOCKED_GOLD;
        data->unlocked_silver = NP_TROPHY2_UNLOCKED_SILVER;
        data->unlocked_bronze = NP_TROPHY2_UNLOCKED_BRONZE;
        data->progress_percentage = NP_TROPHY2_PROGRESS_PERCENTAGE;
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2GetGameIcon(int context, int handle, void* buffer, size_t* size) {
    (void)context;
    (void)handle;
    (void)buffer;
    if (size != nullptr) {
        *size = NP_TROPHY2_ICON_SIZE_NONE;
    }
    throw std::runtime_error(std::string(__func__) + ": icon file not found");
}

int APS5_VABI sceNpTrophy2RegisterUnlockCallback(void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    return SCE_NP_TROPHY2_OK;
}

}

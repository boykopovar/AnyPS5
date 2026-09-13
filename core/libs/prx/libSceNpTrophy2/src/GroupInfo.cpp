#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include "prx/libSceNpTrophy2/include/NpTrophy2.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2GetGroupInfo(int context, int handle, int group_id, NpTrophy2GroupDetails* details, NpTrophy2GroupData* data) {
    if (details == nullptr || data == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)context;
    (void)handle;
    const std::int32_t normalized_group_id = (group_id < 0 ? NP_TROPHY2_GROUP_ID_BASE : group_id);
    if (details != nullptr) {
        std::memset(details, 0, sizeof(*details));
        details->group_id = normalized_group_id;
        details->num_trophies = NP_TROPHY2_NUM_TROPHIES;
        details->num_platinum = NP_TROPHY2_NUM_PLATINUM;
        details->num_gold = NP_TROPHY2_NUM_GOLD;
        details->num_silver = NP_TROPHY2_NUM_SILVER;
        details->num_bronze = NP_TROPHY2_NUM_BRONZE;
        std::strncpy(details->title, NP_TROPHY2_GROUP_TITLE, sizeof(details->title) - 1);
    }
    if (data != nullptr) {
        std::memset(data, 0, sizeof(*data));
        data->group_id = normalized_group_id;
        data->unlocked_trophies = NP_TROPHY2_UNLOCKED_TROPHIES;
        data->unlocked_platinum = NP_TROPHY2_UNLOCKED_PLATINUM;
        data->unlocked_gold = NP_TROPHY2_UNLOCKED_GOLD;
        data->unlocked_silver = NP_TROPHY2_UNLOCKED_SILVER;
        data->unlocked_bronze = NP_TROPHY2_UNLOCKED_BRONZE;
        data->progress_percentage = NP_TROPHY2_PROGRESS_PERCENTAGE;
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2GetGroupInfoArray(int context, int handle, uint32_t offset, uint32_t limit, NpTrophy2GroupDetails* details_array, NpTrophy2GroupData* data_array, uint32_t* count) {
    if (count == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)context;
    (void)handle;
    const uint32_t out_count = (offset == 0 && limit != 0 ? 1u : 0u);
    *count = out_count;
    if (out_count != 0 && details_array != nullptr) {
        std::memset(details_array, 0, sizeof(*details_array));
        details_array->group_id = NP_TROPHY2_GROUP_ID_BASE;
        details_array->num_trophies = NP_TROPHY2_NUM_TROPHIES;
        details_array->num_platinum = NP_TROPHY2_NUM_PLATINUM;
        details_array->num_gold = NP_TROPHY2_NUM_GOLD;
        details_array->num_silver = NP_TROPHY2_NUM_SILVER;
        details_array->num_bronze = NP_TROPHY2_NUM_BRONZE;
        std::strncpy(details_array->title, NP_TROPHY2_GROUP_TITLE, sizeof(details_array->title) - 1);
    }
    if (out_count != 0 && data_array != nullptr) {
        std::memset(data_array, 0, sizeof(*data_array));
        data_array->group_id = NP_TROPHY2_GROUP_ID_BASE;
        data_array->unlocked_trophies = NP_TROPHY2_UNLOCKED_TROPHIES;
        data_array->unlocked_platinum = NP_TROPHY2_UNLOCKED_PLATINUM;
        data_array->unlocked_gold = NP_TROPHY2_UNLOCKED_GOLD;
        data_array->unlocked_silver = NP_TROPHY2_UNLOCKED_SILVER;
        data_array->unlocked_bronze = NP_TROPHY2_UNLOCKED_BRONZE;
        data_array->progress_percentage = NP_TROPHY2_PROGRESS_PERCENTAGE;
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2GetGroupIcon(int context, int handle, int group_id, void* buffer, size_t* size) {
    (void)context;
    (void)handle;
    (void)group_id;
    (void)buffer;
    if (size != nullptr) {
        *size = NP_TROPHY2_ICON_SIZE_NONE;
    }
    throw std::runtime_error(std::string(__func__) + ": icon file not found");
}

}

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include "prx/libSceNpTrophy2/include/NpTrophy2.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2GetTrophyInfo(int context, int handle, int trophy_id, NpTrophy2Details* details, NpTrophy2Data* data) {
    if (details == nullptr || data == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)context;
    (void)handle;
    if (details != nullptr) {
        std::memset(details, 0, sizeof(*details));
        details->trophy_id = trophy_id;
        details->trophy_grade = NP_TROPHY2_TROPHY_GRADE_BRONZE;
        details->group_id = NP_TROPHY2_GROUP_ID_BASE;
        details->hidden = false;
        details->has_reward = false;
        details->target.value = NP_TROPHY2_PROGRESS_VALUE_NONE;
        std::strncpy(details->name, NP_TROPHY2_TROPHY_NAME, sizeof(details->name) - 1);
        std::strncpy(details->description, NP_TROPHY2_TROPHY_DESCRIPTION, sizeof(details->description) - 1);
        std::strncpy(details->reward, NP_TROPHY2_TROPHY_REWARD, sizeof(details->reward) - 1);
    }
    if (data != nullptr) {
        std::memset(data, 0, sizeof(*data));
        data->trophy_id = trophy_id;
        data->unlocked = false;
        data->progress.value = NP_TROPHY2_PROGRESS_VALUE_NONE;
        data->timestamp_tick = 0;
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2GetTrophyInfoArray(int context, int handle, uint32_t offset, uint32_t limit, NpTrophy2Details* details_array, NpTrophy2Data* data_array, uint32_t* count) {
    if (count == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)context;
    (void)handle;
    const uint32_t out_count = (offset == 0 && limit != 0 ? 1u : 0u);
    *count = out_count;
    if (out_count != 0 && details_array != nullptr) {
        std::memset(details_array, 0, sizeof(*details_array));
        details_array->trophy_id = NP_TROPHY2_TROPHY_ID_DEFAULT;
        details_array->trophy_grade = NP_TROPHY2_TROPHY_GRADE_BRONZE;
        details_array->group_id = NP_TROPHY2_GROUP_ID_BASE;
        details_array->hidden = false;
        details_array->has_reward = false;
        details_array->target.value = NP_TROPHY2_PROGRESS_VALUE_NONE;
        std::strncpy(details_array->name, NP_TROPHY2_TROPHY_NAME, sizeof(details_array->name) - 1);
        std::strncpy(details_array->description, NP_TROPHY2_TROPHY_DESCRIPTION, sizeof(details_array->description) - 1);
        std::strncpy(details_array->reward, NP_TROPHY2_TROPHY_REWARD, sizeof(details_array->reward) - 1);
    }
    if (out_count != 0 && data_array != nullptr) {
        std::memset(data_array, 0, sizeof(*data_array));
        data_array->trophy_id = NP_TROPHY2_TROPHY_ID_DEFAULT;
        data_array->unlocked = false;
        data_array->progress.value = NP_TROPHY2_PROGRESS_VALUE_NONE;
        data_array->timestamp_tick = 0;
    }
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2GetTrophyIcon(int context, int handle, int trophy_id, void* buffer, size_t* size) {
    (void)context;
    (void)handle;
    (void)trophy_id;
    (void)buffer;
    if (size != nullptr) {
        *size = NP_TROPHY2_ICON_SIZE_NONE;
    }
    throw std::runtime_error(std::string(__func__) + ": icon file not found");
}

}

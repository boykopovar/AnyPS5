#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceNpTrophy2AbortHandle(int handle) {
 (void)handle;
 return 0;
}

int sceNpTrophy2CreateContext(int* context, int user_id, uint32_t service_label, uint64_t options) {
 (void)context;
 (void)user_id;
 (void)service_label;
 (void)options;
 return 0;
}

int sceNpTrophy2CreateHandle(int* handle) {
 (void)handle;
 return 0;
}

int sceNpTrophy2DestroyContext(int context) {
 (void)context;
 return 0;
}

int sceNpTrophy2DestroyHandle(int handle) {
 (void)handle;
 return 0;
}

int sceNpTrophy2GetGameIcon(int context, int handle, void* buffer, size_t* size) {
 (void)context;
 (void)handle;
 (void)buffer;
 (void)size;
 return 0;
}

int sceNpTrophy2GetGameInfo(int context, int handle, NpTrophy2GameDetails* details, NpTrophy2GameData* data) {
 (void)context;
 (void)handle;
 (void)details;
 (void)data;
 return 0;
}

int sceNpTrophy2GetGroupIcon(int context, int handle, int group_id, void* buffer, size_t* size) {
 (void)context;
 (void)handle;
 (void)group_id;
 (void)buffer;
 (void)size;
 return 0;
}

int sceNpTrophy2GetGroupInfo(int context, int handle, int group_id, NpTrophy2GroupDetails* details, NpTrophy2GroupData* data) {
 (void)context;
 (void)handle;
 (void)group_id;
 (void)details;
 (void)data;
 return 0;
}

int sceNpTrophy2GetGroupInfoArray(int context, int handle, uint32_t offset, uint32_t limit, NpTrophy2GroupDetails* details_array, NpTrophy2GroupData* data_array, uint32_t* count) {
 (void)context;
 (void)handle;
 (void)offset;
 (void)limit;
 (void)details_array;
 (void)data_array;
 (void)count;
 return 0;
}

int sceNpTrophy2GetTrophyIcon(int context, int handle, int trophy_id, void* buffer, size_t* size) {
 (void)context;
 (void)handle;
 (void)trophy_id;
 (void)buffer;
 (void)size;
 return 0;
}

int sceNpTrophy2GetTrophyInfo(int context, int handle, int trophy_id, NpTrophy2Details* details, NpTrophy2Data* data) {
 (void)context;
 (void)handle;
 (void)trophy_id;
 (void)details;
 (void)data;
 return 0;
}

int sceNpTrophy2GetTrophyInfoArray(int context, int handle, uint32_t offset, uint32_t limit, NpTrophy2Details* details_array, NpTrophy2Data* data_array, uint32_t* count) {
 (void)context;
 (void)handle;
 (void)offset;
 (void)limit;
 (void)details_array;
 (void)data_array;
 (void)count;
 return 0;
}

int sceNpTrophy2RegisterContext(int context, int handle, uint64_t options) {
 (void)context;
 (void)handle;
 (void)options;
 return 0;
}

int sceNpTrophy2RegisterUnlockCallback(void* callback, void* userdata) {
 (void)callback;
 (void)userdata;
 return 0;
}

}

#include <cstddef>
#include <cstdint>

#include "prx/libSceNpTrophy2/include/NpTrophy2.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2GetGroupInfo(int context, int handle, int group_id, NpTrophy2GroupDetails* details, NpTrophy2GroupData* data) {
    (void)context;
    (void)handle;
    (void)group_id;
    (void)details;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2GetGroupInfoArray(int context, int handle, uint32_t offset, uint32_t limit, NpTrophy2GroupDetails* details_array, NpTrophy2GroupData* data_array, uint32_t* count) {
    (void)context;
    (void)handle;
    (void)offset;
    (void)limit;
    (void)details_array;
    (void)data_array;
    (void)count;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2GetGroupIcon(int context, int handle, int group_id, void* buffer, size_t* size) {
    (void)context;
    (void)handle;
    (void)group_id;
    (void)buffer;
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

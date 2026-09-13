#include <cstddef>
#include <cstdint>

#include "NpTrophy2.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2GetTrophyInfo(int context, int handle, int trophy_id, NpTrophy2Details* details, NpTrophy2Data* data) {
    (void)context;
    (void)handle;
    (void)trophy_id;
    (void)details;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2GetTrophyInfoArray(int context, int handle, uint32_t offset, uint32_t limit, NpTrophy2Details* details_array, NpTrophy2Data* data_array, uint32_t* count) {
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

int APS5_VABI sceNpTrophy2GetTrophyIcon(int context, int handle, int trophy_id, void* buffer, size_t* size) {
    (void)context;
    (void)handle;
    (void)trophy_id;
    (void)buffer;
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

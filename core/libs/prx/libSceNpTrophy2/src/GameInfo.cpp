#include <cstddef>

#include "NpTrophy2.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2GetGameInfo(int context, int handle, NpTrophy2GameDetails* details, NpTrophy2GameData* data) {
    (void)context;
    (void)handle;
    (void)details;
    (void)data;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2GetGameIcon(int context, int handle, void* buffer, size_t* size) {
    (void)context;
    (void)handle;
    (void)buffer;
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2RegisterUnlockCallback(void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

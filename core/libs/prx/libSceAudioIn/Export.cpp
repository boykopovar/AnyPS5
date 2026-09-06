#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceAudioInGetSilentState(int handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAudioInInput(int handle, void* dest) {
 (void)handle;
 (void)dest;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceAudioInOpen(int user_id, uint32_t type, uint32_t index, uint32_t len, uint32_t freq, uint32_t param) {
 (void)user_id;
 (void)type;
 (void)index;
 (void)len;
 (void)freq;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

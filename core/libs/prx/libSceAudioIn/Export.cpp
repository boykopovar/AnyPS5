#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceAudioInGetSilentState(int handle) {
 (void)handle;
 return 0;
}

int sceAudioInInput(int handle, void* dest) {
 (void)handle;
 (void)dest;
 return 0;
}

int sceAudioInOpen(int user_id, uint32_t type, uint32_t index, uint32_t len, uint32_t freq, uint32_t param) {
 (void)user_id;
 (void)type;
 (void)index;
 (void)len;
 (void)freq;
 (void)param;
 return 0;
}

}

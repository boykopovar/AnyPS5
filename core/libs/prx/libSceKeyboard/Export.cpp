#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceKeyboardClose(int32_t handle) {
 (void)handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKeyboardGetKey2Char(int32_t handle, int32_t arrange, uint32_t led, uint32_t modifier_key, uint16_t key_code, KeyboardCharData* char_data) {
 (void)handle;
 (void)arrange;
 (void)led;
 (void)modifier_key;
 (void)key_code;
 (void)char_data;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKeyboardInit(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKeyboardOpen(int user_id, int32_t type, int32_t index, const void* param) {
 (void)user_id;
 (void)type;
 (void)index;
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKeyboardRead(int32_t handle, KeyboardData* data, int32_t num) {
 (void)handle;
 (void)data;
 (void)num;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceKeyboardReadState(int32_t handle, KeyboardData* data) {
 (void)handle;
 (void)data;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

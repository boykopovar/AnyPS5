#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceMouseClose(int32_t handle) {
 (void)handle;
 return 0;
}

int sceMouseInit(void) {
 return 0;
}

int sceMouseOpen(int user_id, int32_t type, int32_t index, const void* param) {
 (void)user_id;
 (void)type;
 (void)index;
 (void)param;
 return 0;
}

int sceMouseRead(int32_t handle, MouseData* data, int32_t num) {
 (void)handle;
 (void)data;
 (void)num;
 return 0;
}

}

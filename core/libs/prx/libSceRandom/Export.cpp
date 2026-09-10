#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceRandomGetRandomNumber(void* buf, size_t size) {
 (void)buf;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

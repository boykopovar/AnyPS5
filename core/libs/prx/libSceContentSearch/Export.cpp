#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceContentSearchInit(const ContentSearchInitParam* init_param) {
 (void)init_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

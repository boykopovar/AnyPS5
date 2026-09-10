#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* init_param) {
 (void)init_param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

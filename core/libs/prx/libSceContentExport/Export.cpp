#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceContentExportInit2(const ContentExportInitParam2* init_param) {
 (void)init_param;
 return 0;
}

}

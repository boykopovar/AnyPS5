#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

static_assert(sizeof(LibcUtimbuf) == 16);

extern "C" int APS5_VABI utime_nid_postfix(const char* path, const LibcUtimbuf* times) {
    int* const error = __error_nid_postfix();
    const int saved = *error;
    const KernelTimeval values[2] = {{times ? times->actime : 0, 0}, {times ? times->modtime : 0, 0}};
    const int result = utimes_nid_postfix(path, times ? values : nullptr);
    if (result == 0) *error = saved;
    return result;
}

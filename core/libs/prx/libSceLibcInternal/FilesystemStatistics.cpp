#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceLibcInternal/include/Statvfs.hpp"
#include <cstring>

extern "C" int APS5_VABI fstatvfs_nid_postfix(int descriptor, LibcStatvfs* output) {
    int* const error = __error_nid_postfix();
    const int saved = *error;
    KernelStatfs volume{};
    const int status = _fstatfs_nid_postfix(descriptor, &volume);
    if (status != 0) return status;
    if (output == nullptr) {
        *error = 14;
        return -1;
    }
    const LibcStatvfs result = StatvfsFromStatfs(volume);
    std::memcpy(output, &result, sizeof(result));
    *error = saved;
    return 0;
}

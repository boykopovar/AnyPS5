#include <cstddef>
#include <cerrno>
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceLibcBacktraceGetBufferSize_nid_postfix(int, int* depth, std::size_t* size) {
    if (depth != nullptr) *depth = 0;
    if (size != nullptr) *size = 0;
    errno = ENOTSUP;
    return -1;
}

int APS5_VABI sceLibcBacktraceSelf_nid_postfix(int, void*, std::size_t, int* count) {
    if (count != nullptr) *count = 0;
    errno = ENOTSUP;
    return -1;
}

}

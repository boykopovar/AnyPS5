#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI __fflush_nid_postfix(FileStream* stream) {
    return fflush_nid_postfix(stream);
}

}

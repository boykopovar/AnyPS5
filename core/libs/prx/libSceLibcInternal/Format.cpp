#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI __vfprintf_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    if (!stream) {
        *__error_nid_postfix() = 9;
        return -1;
    }
    return vfprintf_nid_postfix(stream, format, args);
}

}

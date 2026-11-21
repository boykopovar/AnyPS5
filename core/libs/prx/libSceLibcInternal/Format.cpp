#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI __vfprintf_nid_postfix(FileStream* stream, const char* format, VaList* args) {
    if (!stream) {
        *_Geterrno_nid_postfix() = 9;
        *__error_nid_postfix() = 9;
        return -1;
    }
    const int result = vfprintf_nid_postfix(stream, format, args);
    if (result < 0) {
        *_Geterrno_nid_postfix() = *__error_nid_postfix();
    }
    return result;
}

}

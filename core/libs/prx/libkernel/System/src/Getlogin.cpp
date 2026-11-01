#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" char* APS5_VABI getlogin_nid_postfix(void) {
    static char logname[33];
    if (const int error = getlogin_r_nid_postfix(logname, static_cast<int>(sizeof(logname))); error != 0) {
        *__error_nid_postfix() = error;
        return nullptr;
    }
    return logname[0] != '\0' ? logname : nullptr;
}

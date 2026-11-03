#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
void APS5_VABI _ZN3PAL10systemBeepEv() {
#ifdef _WIN32
    MessageBeep(static_cast<UINT>(-1));
#endif
}

}

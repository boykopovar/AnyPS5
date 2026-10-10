#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceFontFtSupportTrueType();
int APS5_VABI sceFontFtSupportOpenType();
int APS5_VABI sceFontFtSupportSystemFonts();
int APS5_VABI sceFontFtSupportBdf();
}

int main() {
    if (sceFontFtSupportTrueType() != 0 || sceFontFtSupportOpenType() != 0 ||
        sceFontFtSupportSystemFonts() != 0 || sceFontFtSupportBdf() != 0) {
        std::abort();
    }
    return 0;
}

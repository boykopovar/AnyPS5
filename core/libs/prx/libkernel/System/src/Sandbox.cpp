#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" const char* APS5_VABI sceKernelGetFsSandboxRandomWord() {
    static constexpr char word[] = "gkbHtXc3Wq";
    return word;
}

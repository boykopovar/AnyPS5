#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpEAAccessInitialize(void) {
    throw std::runtime_error("sceNpEAAccessInitialize: unknown signature");
}

int APS5_VABI sceNpEAAccessTerminate(void) {
    throw std::runtime_error("sceNpEAAccessTerminate: unknown signature");
}

}

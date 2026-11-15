#include "prx/libSceGLSlimVSH/libSceGLSlimVSH.hpp"
#include <cstdlib>

int main() {
    for (int i = 0; i < 2; ++i) {
        if (sceSlimglIPCSetup() != 0) std::abort();
        if (sceSlimglClientStartIPC() != 0) std::abort();
    }
}

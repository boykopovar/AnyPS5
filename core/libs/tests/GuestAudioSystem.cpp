#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceAudioSystemSystemResume();
int APS5_VABI sceAudioSystemSystemSuspend();
}

static void Require(bool value) { if (!value) std::abort(); }

namespace {

void TestLifecycle() {
    Require(sceAudioSystemSystemSuspend() == 0);
    Require(sceAudioSystemSystemResume() == 0);
    Require(sceAudioSystemSystemResume() == 0);
    Require(sceAudioSystemSystemSuspend() == 0);
    Require(sceAudioSystemSystemSuspend() == 0);
    Require(sceAudioSystemSystemResume() == 0);
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    Require(std::strcmp(argv[1], "lifecycle") == 0);
    TestLifecycle();
}

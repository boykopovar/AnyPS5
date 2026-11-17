#include "prx/libSceNpSpeechToText/libSceNpSpeechToText.hpp"
#include "Testing.hpp"

#include <cstdlib>

int main() {
    constexpr int notInitialized = static_cast<int>(0x8193F000u);
    for (int call = 0; call < 2; ++call) {
        Require(sceNpSpeechToTextCreateContext() == notInitialized);
        Require(sceNpSpeechToTextDestroyContext() == notInitialized);
        Require(sceNpSpeechToTextGetEvent() == notInitialized);
        Require(sceNpSpeechToTextStart() == notInitialized);
        Require(sceNpSpeechToTextStop() == notInitialized);
        Require(sceNpSpeechToTextTerminate() == notInitialized);
    }
    return 0;
}

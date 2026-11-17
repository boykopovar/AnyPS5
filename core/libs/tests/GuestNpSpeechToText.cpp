#include "prx/libSceNpSpeechToText/libSceNpSpeechToText.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notInitialized = static_cast<int>(0x8193F000u);
    constexpr int invalidArgument = static_cast<int>(0x8193F003u);
    alignas(8) unsigned char param[0x40]{};
    Require(sceNpSpeechToTextInitialize(nullptr) == invalidArgument);
    Require(sceNpSpeechToTextInitialize(param) == invalidArgument);
    param[2] = 0x02;
    param[0] = 0x01;
    Require(sceNpSpeechToTextInitialize(param) == invalidArgument);
    param[0] = 0x00;
    param[0x3f] = 1;
    Require(sceNpSpeechToTextInitialize(param) == invalidArgument);
    param[0x3f] = 0;
    param[4] = 1;
    Require(sceNpSpeechToTextInitialize(param) == invalidArgument);
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

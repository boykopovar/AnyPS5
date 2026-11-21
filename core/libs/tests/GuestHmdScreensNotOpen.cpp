#include "prx/libSceHmd/libSceHmd.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int NotInitialized = static_cast<int>(0x81110002);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

} // namespace

int main() {
    Require(sceHmdInternalSocialScreenSetOutput({}, {}) == NotInitialized, "sceHmdInternalSocialScreenSetOutput answers 0x81110002 while nothing is open");
    Require(sceHmdInternalSeparateModeGetAudioStatus({}) == NotInitialized, "sceHmdInternalSeparateModeGetAudioStatus answers 0x81110002 while nothing is open");
    Require(sceHmdInternalSocialScreenSetFadeAndSwitch({}) == NotInitialized, "sceHmdInternalSocialScreenSetFadeAndSwitch answers 0x81110002 while nothing is open");
    Require(sceHmdInternalSeparateModeClose() == NotInitialized, "sceHmdInternalSeparateModeClose answers 0x81110002 while nothing is open");
    Require(sceHmdInternalSeparateModeGetVideoStatus({}) == NotInitialized, "sceHmdInternalSeparateModeGetVideoStatus answers 0x81110002 while nothing is open");
    Require(sceHmdInternalSeparateModeSendVideo({}) == NotInitialized, "sceHmdInternalSeparateModeSendVideo answers 0x81110002 while nothing is open");
    Require(sceHmdInternalSocialScreenGetFadeState({}) == NotInitialized, "sceHmdInternalSocialScreenGetFadeState answers 0x81110002 while nothing is open");
    Require(sceHmdInternalSeparateModeSendAudio({}) == NotInitialized, "sceHmdInternalSeparateModeSendAudio answers 0x81110002 while nothing is open");
    Require(sceHmdInternalAnotherScreenSendVideo({}) == NotInitialized, "sceHmdInternalAnotherScreenSendVideo answers 0x81110002 while nothing is open");
    Require(sceHmdInternalAnotherScreenSendAudio({}) == NotInitialized, "sceHmdInternalAnotherScreenSendAudio answers 0x81110002 while nothing is open");
    Require(sceHmdInternalAnotherScreenSetFadeAndSwitch({}) == NotInitialized, "sceHmdInternalAnotherScreenSetFadeAndSwitch answers 0x81110002 while nothing is open");
    Require(sceHmdInternalAnotherScreenClose() == NotInitialized, "sceHmdInternalAnotherScreenClose answers 0x81110002 while nothing is open");
    Require(sceHmdInternalAnotherScreenGetAudioStatus({}) == NotInitialized, "sceHmdInternalAnotherScreenGetAudioStatus answers 0x81110002 while nothing is open");
    Require(sceHmdInternalAnotherScreenGetVideoStatus({}) == NotInitialized, "sceHmdInternalAnotherScreenGetVideoStatus answers 0x81110002 while nothing is open");
    Require(sceHmdInternalAnotherScreenGetFadeState({}) == NotInitialized, "sceHmdInternalAnotherScreenGetFadeState answers 0x81110002 while nothing is open");
    Require(sceHmdReprojectionStartCapture() == NotInitialized, "sceHmdReprojectionStartCapture answers 0x81110002 while nothing is open");
    Require(sceHmdReprojectionFinalizeCapture() == NotInitialized, "sceHmdReprojectionFinalizeCapture answers 0x81110002 while nothing is open");
    Require(sceHmdInternalSeparateModeClose() == NotInitialized, "a second sceHmdInternalSeparateModeClose answers 0x81110002");
    Require(sceHmdInternalAnotherScreenClose() == NotInitialized, "a second sceHmdInternalAnotherScreenClose answers 0x81110002");
    Require(sceHmdReprojectionFinalizeCapture() == NotInitialized, "a second sceHmdReprojectionFinalizeCapture answers 0x81110002");
    return 0;
}

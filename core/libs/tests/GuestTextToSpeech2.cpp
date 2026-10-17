#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
std::int32_t APS5_VABI sceTextToSpeech2Initialize(const void* param);
int APS5_VABI sceTextToSpeech2Open();
int APS5_VABI sceTextToSpeech2Cancel();
int APS5_VABI sceTextToSpeech2Close();
int APS5_VABI sceTextToSpeech2GetSpeechStatus();
int APS5_VABI sceTextToSpeech2Speak();
int APS5_VABI sceTextToSpeech2Terminate();
int APS5_VABI sceTextToSpeech2GetSystemStatus();
int APS5_VABI sceTextToSpeech2RegisterTextConversionItem();
int APS5_VABI sceTextToSpeechCancelImpl();
int APS5_VABI sceTextToSpeechSpeak();
}

int main() {
    constexpr std::int32_t unsupported = static_cast<std::int32_t>(0x8002002D);
    const std::uint32_t param[12]{0x2000000, 0x26c};
    if (sceTextToSpeech2Initialize(param) != unsupported) {
        std::puts("TextToSpeech2: initialization did not report the unsupported operation");
        std::abort();
    }
    if (sceTextToSpeech2Open() != unsupported || sceTextToSpeech2Cancel() != unsupported ||
        sceTextToSpeech2Close() != unsupported || sceTextToSpeech2GetSpeechStatus() != unsupported ||
        sceTextToSpeech2Speak() != unsupported || sceTextToSpeech2Terminate() != unsupported ||
        sceTextToSpeech2GetSystemStatus() != unsupported) {
        std::puts("TextToSpeech2: later calls did not report the unsupported operation");
        std::abort();
    }
    bool threwConversion = false;
    try {
        sceTextToSpeech2RegisterTextConversionItem();
    } catch (const std::runtime_error&) {
        threwConversion = true;
    }
    if (!threwConversion) {
        std::puts("TextToSpeech2: register text conversion did not throw");
        std::abort();
    }
    bool threwCancel = false;
    try {
        sceTextToSpeechCancelImpl();
    } catch (const std::runtime_error&) {
        threwCancel = true;
    }
    if (!threwCancel) {
        std::puts("TextToSpeech2: cancel impl did not throw");
        std::abort();
    }
    bool threwSpeak = false;
    try {
        sceTextToSpeechSpeak();
    } catch (const std::runtime_error&) {
        threwSpeak = true;
    }
    if (!threwSpeak) {
        std::puts("TextToSpeech2: speak did not throw");
        std::abort();
    }
    std::puts("TextToSpeech2 tests passed");
    return 0;
}

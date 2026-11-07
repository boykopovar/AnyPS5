#include "prx/libSceTextToSpeech2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

int main() {
    constexpr std::int32_t unsupported = static_cast<std::int32_t>(0x8002002D);
    const std::uint32_t param[16]{0x2000000, 0x26c};
    if (sceTextToSpeech2Initialize(reinterpret_cast<const TextToSpeechInitParameter*>(param)) != unsupported ||
        sceTextToSpeech2Initialize(nullptr) != unsupported) {
        std::puts("TextToSpeech2: initialization did not report the unsupported operation");
        std::abort();
    }

    std::int32_t status = 0x55AA;
    if (sceTextToSpeech2GetSystemStatus(&status) != unsupported || status != 0x55AA ||
        sceTextToSpeech2GetSpeechStatus(&status) != unsupported || status != 0x55AA) {
        std::puts("TextToSpeech2: status queries did not preserve the output value");
        std::abort();
    }

    const TextToSpeechOpenParameter openParam{};
    const TextToSpeechSpeakParameter speakParam{};
    if (sceTextToSpeech2Open(&openParam) != unsupported || sceTextToSpeech2Speak(&speakParam) != unsupported) {
        std::puts("TextToSpeech2: configured open or speak did not report the unsupported operation");
        std::abort();
    }
    if (sceTextToSpeech2Open(nullptr) != unsupported || sceTextToSpeech2Cancel() != unsupported ||
        sceTextToSpeech2Close() != unsupported || sceTextToSpeech2GetSpeechStatus(nullptr) != unsupported ||
        sceTextToSpeech2Speak(nullptr) != unsupported || sceTextToSpeech2Terminate() != unsupported ||
        sceTextToSpeech2GetSystemStatus(nullptr) != unsupported) {
        std::puts("TextToSpeech2: later calls did not report the unsupported operation");
        std::abort();
    }
    std::puts("TextToSpeech2 tests passed");
    return 0;
}

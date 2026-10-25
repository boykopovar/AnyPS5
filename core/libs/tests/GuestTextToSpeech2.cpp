#include "prx/libSceTextToSpeech2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

int main() {
    constexpr std::int32_t unsupported = static_cast<std::int32_t>(0x8002002D);
    const std::uint32_t param[16]{0x2000000, 0x26c};
    if (sceTextToSpeech2Initialize(reinterpret_cast<const TextToSpeechInitParameter*>(param)) != unsupported) {
        std::puts("TextToSpeech2: initialization did not report the unsupported operation");
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

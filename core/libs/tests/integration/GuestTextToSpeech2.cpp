#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
std::int32_t APS5_VABI sceTextToSpeech2Initialize(const void* param);
int APS5_VABI sceTextToSpeech2Open();
}

int main() {
    const std::uint32_t param[12]{0x2000000, 0x26c};
    if (sceTextToSpeech2Initialize(param) != static_cast<std::int32_t>(0x8002002D)) {
        std::puts("TextToSpeech2: initialization did not report the unsupported operation");
        std::abort();
    }
    bool threw = false;
    try {
        sceTextToSpeech2Open();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!threw) {
        std::puts("TextToSpeech2: open after failed initialization did not throw");
        std::abort();
    }
    std::puts("TextToSpeech2 tests passed");
    return 0;
}

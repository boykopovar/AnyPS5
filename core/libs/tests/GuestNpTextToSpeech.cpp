#include "prx/libSceNpTextToSpeech/libSceNpTextToSpeech.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notInitialized = static_cast<int>(0x8194F009u);
    constexpr int invalidArgument = static_cast<int>(0x8194F004u);
    alignas(8) unsigned char param[0x40]{};
    Require(sceNpTextToSpeechInitialize(nullptr) == invalidArgument);
    Require(sceNpTextToSpeechInitialize(param) == invalidArgument);
    param[2] = 0x02;
    param[0] = 0x01;
    Require(sceNpTextToSpeechInitialize(param) == invalidArgument);
    param[0] = 0x00;
    for (int call = 0; call < 2; ++call) {
        Require(sceNpTextToSpeechCreateContext() == notInitialized);
        Require(sceNpTextToSpeechDestroyContext() == notInitialized);
        Require(sceNpTextToSpeechGetEvent() == notInitialized);
        Require(sceNpTextToSpeechTerminate() == notInitialized);
        Require(libSceNpTextToSpeechUnknown__minus_wGjkwcjXWs_nid_no_patch() == notInitialized);
        Require(libSceNpTextToSpeechUnknown_APYz0RAIugo_nid_no_patch() == notInitialized);
        Require(libSceNpTextToSpeechUnknown_Fp7i5mx3JWM_nid_no_patch() == notInitialized);
        Require(libSceNpTextToSpeechUnknown_i3MidGD_minus_Zfk_nid_no_patch() == notInitialized);
        Require(libSceNpTextToSpeechUnknown_oIMRm6q4pUM_nid_no_patch() == notInitialized);
        Require(libSceNpTextToSpeechUnknown_rIXr2WILhUE_nid_no_patch() == notInitialized);
    }
    return 0;
}

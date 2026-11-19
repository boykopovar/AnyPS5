#include "prx/libSceTextToSpeech/libSceTextToSpeech.hpp"

#include <climits>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int invalidId = static_cast<int>(0x814900FFu);
    constexpr int notInitialized = static_cast<int>(0x81490020u);
    using Call = int (*)(std::uint32_t);
    const Call anyId[] = {
        [](std::uint32_t id) { return sceTextToSpeechCancelImpl(id); },
        [](std::uint32_t id) { return sceTextToSpeechCloseImpl(id); },
        [](std::uint32_t id) { return sceTextToSpeechOpenImpl(id); },
        [](std::uint32_t id) { return sceTextToSpeechSpeakImpl(id); },
    };
    const Call playingId[] = {
        [](std::uint32_t id) { return sceTextToSpeechPauseImpl(id); },
        [](std::uint32_t id) { return sceTextToSpeechResumeImpl(id); },
        [](std::uint32_t id) { return libSceTextToSpeechUnknown_6ZLFpZLuqEk_nid_no_patch(id); },
    };
    unsigned char block[16] = {};
    for (int round = 0; round < 2; ++round) {
        for (const Call call : anyId) {
            Require(call(0) == notInitialized);
            Require(call(1) == notInitialized);
            Require(call(2) == notInitialized);
            Require(call(3) == invalidId);
            Require(call(0xFFFFFFFFu) == invalidId);
        }
        for (const Call call : playingId) {
            Require(call(0) == invalidId);
            Require(call(1) == notInitialized);
            Require(call(2) == notInitialized);
            Require(call(3) == invalidId);
            Require(call(0xFFFFFFFFu) == invalidId);
        }
        Require(sceTextToSpeechSpeakContinuousImpl(0) == invalidId);
        Require(sceTextToSpeechSpeakContinuousImpl(-1) == invalidId);
        Require(sceTextToSpeechSpeakContinuousImpl(INT_MIN) == invalidId);
        Require(sceTextToSpeechSpeakContinuousImpl(1) == notInitialized);
        Require(sceTextToSpeechSpeakContinuousImpl(3) == notInitialized);
        Require(sceTextToSpeechSpeakContinuousImpl(INT_MAX) == notInitialized);
        Require(sceTextToSpeechTermImpl(3) == invalidId);
        Require(sceTextToSpeechTermImpl(0) == notInitialized);
        Require(sceTextToSpeechGetStatusImpl(3, block) == invalidId);
        Require(sceTextToSpeechGetStatusImpl(0xFFFFFFFFu, block) == invalidId);
        Require(sceTextToSpeechGetStatusImpl(3, nullptr) == invalidId);
        Require(sceTextToSpeechGetStatusImpl(0, nullptr) == static_cast<int>(0x81490014u));
        Require(sceTextToSpeechGetStatusImpl(2, nullptr) == static_cast<int>(0x81490014u));
        Require(sceTextToSpeechInitImpl(3, block) == invalidId);
        Require(sceTextToSpeechInitImpl(0xFFFFFFFFu, block) == invalidId);
        Require(sceTextToSpeechInitImpl(3, nullptr) == invalidId);
        Require(sceTextToSpeechInitImpl(0, nullptr) == static_cast<int>(0x81490010u));
        Require(sceTextToSpeechInitImpl(2, nullptr) == static_cast<int>(0x81490010u));
        Require(libSceTextToSpeechUnknown_L_minus_pHC7qtEug_nid_no_patch(0, block) == invalidId);
        Require(libSceTextToSpeechUnknown_L_minus_pHC7qtEug_nid_no_patch(3, block) == invalidId);
        Require(libSceTextToSpeechUnknown_L_minus_pHC7qtEug_nid_no_patch(0xFFFFFFFFu, block) == invalidId);
        Require(libSceTextToSpeechUnknown_L_minus_pHC7qtEug_nid_no_patch(0, nullptr) == invalidId);
        Require(libSceTextToSpeechUnknown_L_minus_pHC7qtEug_nid_no_patch(1, nullptr) == static_cast<int>(0x8149001Au));
        Require(libSceTextToSpeechUnknown_L_minus_pHC7qtEug_nid_no_patch(2, nullptr) == static_cast<int>(0x8149001Au));
    }
    return 0;
}

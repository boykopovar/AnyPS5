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
    }
    return 0;
}

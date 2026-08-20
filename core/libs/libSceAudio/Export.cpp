#include <cstdint>

extern "C" {

    using SceAudioOutPortId = std::int32_t;

    std::int32_t sceAudioOutInit(void) { return 0; }

    SceAudioOutPortId sceAudioOutOpen(std::int32_t userId, std::int32_t type, std::int32_t index, std::uint32_t len, std::uint32_t freq, std::uint32_t param) {
        (void)userId; (void)type; (void)index; (void)len; (void)freq; (void)param;
        return 0;
    }

    std::int32_t sceAudioOutOutput(SceAudioOutPortId handle, const void* ptr) {
        (void)handle; (void)ptr;
        return 0;
    }

    std::int32_t sceAudioOutSetVolume(SceAudioOutPortId handle, std::int32_t flag, std::int32_t* vol) {
        (void)handle; (void)flag; (void)vol;
        return 0;
    }

    std::int32_t sceAudioOutClose(SceAudioOutPortId handle) {
        (void)handle;
        return 0;
    }

}

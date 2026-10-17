#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceVoiceInit(VoiceInitParam*, std::int32_t);
int APS5_VABI sceVoiceStart(const VoiceStartParam*);
int APS5_VABI sceVoiceEnd_nid_postfix();
int APS5_VABI sceVoiceCreatePort(std::uint32_t*, const VoicePortParam*);
int APS5_VABI sceVoiceDeletePort(std::uint32_t);
int APS5_VABI sceVoiceConnectIPortToOPort(std::uint32_t, std::uint32_t);
int APS5_VABI sceVoiceDisconnectIPortFromOPort(std::uint32_t, std::uint32_t);
int APS5_VABI sceVoiceGetVolume(std::uint32_t, float*);
int APS5_VABI sceVoiceSetMuteFlag(std::uint32_t, bool);
int APS5_VABI sceVoiceGetMuteFlag(std::uint32_t, bool*);
int APS5_VABI sceVoiceGetPortInfo(std::uint32_t, VoicePortInfo*);
int APS5_VABI sceVoiceReadFromOPort(std::uint32_t, void*, std::uint32_t*);
int APS5_VABI sceVoiceResetPort(std::uint32_t);
int APS5_VABI sceVoiceEnableChat(void);
int APS5_VABI sceVoiceDisableChat(void);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TFunction>
void RequireThrows(TFunction function) {
    try { function(); } catch (const std::logic_error&) { return; }
    std::abort();
}

std::uint32_t CreatePort(std::int32_t type, float volume) {
    VoicePortParam param{};
    param.port_type = type;
    param.volume = volume;
    if (type == 2 || type == 4) param.voice.bitrate = 16000;
    std::uint32_t id = 0;
    Require(sceVoiceCreatePort(&id, &param) == 0);
    return id;
}

} // namespace

int main() {
    RequireThrows([] { sceVoiceResetPort(0); });
    RequireThrows([] { sceVoiceEnableChat(); });
    RequireThrows([] { sceVoiceDisableChat(); });
    VoiceInitParam init{};
    VoiceStartParam start{};
    Require(sceVoiceInit(&init, 0) == 0);
    Require(sceVoiceStart(&start) == 0);
    Require(sceVoiceEnableChat() == 0);
    Require(sceVoiceDisableChat() == 0);
    const auto input = CreatePort(2, 0.5f);
    const auto output = CreatePort(3, 0.25f);
    Require(sceVoiceConnectIPortToOPort(input, output) == 0);
    Require(sceVoiceSetMuteFlag(output, true) == 0);
    VoicePortInfo before{};
    Require(sceVoiceGetPortInfo(input, &before) == 0);

    Require(sceVoiceResetPort(input) == 0);
    Require(sceVoiceResetPort(output) == 0);
    RequireThrows([&] { sceVoiceResetPort(output + 1); });

    VoicePortInfo after{};
    Require(sceVoiceGetPortInfo(input, &after) == 0);
    Require(after.port_type == before.port_type && after.state == before.state && after.frame_size == before.frame_size);
    float volume = 0;
    bool muted = false;
    Require(sceVoiceGetVolume(output, &volume) == 0 && volume == 0.25f);
    Require(sceVoiceGetMuteFlag(output, &muted) == 0 && muted);
    std::uint8_t data[64]{};
    std::uint32_t size = sizeof(data);
    Require(sceVoiceReadFromOPort(output, data, &size) == 0 && size == 0);
    Require(sceVoiceDisconnectIPortFromOPort(input, output) == 0);

    Require(sceVoiceDeletePort(input) == 0);
    RequireThrows([&] { sceVoiceResetPort(input); });
    Require(sceVoiceResetPort(output) == 0);
    Require(sceVoiceEnd_nid_postfix() == 0);
    RequireThrows([&] { sceVoiceResetPort(output); });
    RequireThrows([] { sceVoiceEnableChat(); });
    RequireThrows([] { sceVoiceDisableChat(); });
}
